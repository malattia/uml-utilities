#!/usr/bin/perl

use strict;
use English;

# Make sure things like ifconfig and route are accessible
$ENV{PATH} .= ":/sbin:/usr/sbin";

sub Usage {
    print "Usage : jailer.pl uml-binary root-filesystem uid " .
	"[ -net host-ip uml-ip] [ -tty-log ] uml arguments ...\n";
    exit 1;
}

my ($uml, $rootfs, $uid, @uml_args) = @ARGV;
my ($tap, $host_ip, $uml_ip);
my @net_cmds = ();
my $tty_log = 0;

while(1){
    if($uml_args[0] eq "-net"){
	(undef, $host_ip, $uml_ip, @uml_args) = @uml_args;
	(!defined($uml_ip) or !defined($host_ip)) and Usage();
	@net_cmds = ( "tunctl", "ifconfig", "route" );
    }
    elsif($uml_args[0] eq "-tty-log"){
	shift @uml_args;
	$tty_log = 1;
    }
    else {
	last;
    }
}

(!defined($uml) || !defined($rootfs) || !defined($uid)) and Usage();

my @cmds = ("mkdir", "tar", "cp", "basename", "rm", @net_cmds );

my $sudo = "";
if($UID != 0){
    $sudo = "sudo";
    push @cmds, $sudo;
}

my @dont_have = ();

foreach my $cmd (@cmds){
    `which $cmd 2>&1 > /dev/null`;
    $? != 0 and push @dont_have, $cmd;
}

if(@dont_have){
    print "Can't find the following utilities: " . 
	join(" ", @dont_have) . "\n";
    exit 1;
}

my $n = 0;

while(-e "cell$n"){
    $n++;
}
my $cell = "cell$n";

my $out;
my @more_args = ();

sub run {
    my $cmd = shift;
    my $out = `$cmd 2>&1`;

    $? ne 0 and die "Running '$cmd' failed : output = '$out'";
    return($out);
}

if(defined($host_ip)){
    $out = run("tunctl -u $uid");
    if($out =~ /(tap\d+)/){
	$tap = $1;
	push @more_args, "eth0=tuntap,$tap";

	run("$sudo ifconfig $tap $host_ip up");
	run("$sudo route add -host $uml_ip dev $tap");
	run("$sudo bash -c 'echo 1 > /proc/sys/net/ipv4/ip_forward'");
    }
    else {
	die "Couldn't find tap device name in '$out'";
    }
}

if($tty_log == 1){
    push @more_args, "tty_log_fd=3"
}

print "New inmate assigned to '$cell'\n";
print "	UML image : $uml\n";
print "	Root filesystem : $rootfs\n";
if(defined($tap)){
    print "	Network : $tap, host == $host_ip, uml == $uml_ip\n";
}
else {
    print "	No network configured\n";
}
if($tty_log == 1){
    print "	TTY logging to tty_log_$cell\n";
    push @more_args, "3>tty_log_$cell";
}
print "	Extra arguments : '" . join(" ", @uml_args) . "'\n";
print "\n";

run("mkdir $cell");
run("chmod 755 $cell");

my $cell_tar = "cell.tar";
run("cd $cell ; $sudo tar xpf ../$cell_tar");

run("$sudo chown $uid $cell/tmp");
run("$sudo chmod 777 $cell/tmp");

print "Copying '$uml' and '$rootfs' to '$cell'...";
run("cp $uml $rootfs $cell");
print "done\n\n";

if(-e "/proc/mm"){
    run("mkdir $cell/proc");
    run("chmod 755 $cell/proc");
    run("touch $cell/proc/mm");
    run("$sudo mount --bind /proc/mm $cell/proc/mm");
}

$uml = `basename $uml`;
chomp $uml;

$rootfs = `basename $rootfs`;
chomp $rootfs;

run("$sudo chmod 666 $cell/$rootfs");
run("$sudo chmod 755 $cell/$uml");

my @args = ( "bash", "-c", "./jail_uml $cell $uid /$uml ubd0=/$rootfs " . 
	     join(" ", @uml_args, @more_args) );

$sudo ne "" and unshift @args, $sudo;

system @args;

-e "$cell/proc/mm" and run("$sudo umount $cell/proc/mm");
run("rm -rf $cell");

if(defined($tap)){
    run("$sudo ifconfig $tap down");
    run("$sudo tunctl -d $tap");
}

exit 0;
