#!/usr/bin/perl

use strict;
use English;

sub Usage {
    print "Usage : jailer.pl uml-binary root-filesystem uid " .
	"[ -net host-ip uml-ip] uml arguments ...\n";
    exit 1;
}

my ($uml, $rootfs, $uid, @uml_args) = @ARGV;
my ($tap, $host_ip, $uml_ip);

while(1){
    if($uml_args[0] eq "-net"){
	(undef, $host_ip, $uml_ip, @uml_args) = @uml_args;
	(!defined($uml_ip) or !defined($host_ip)) and Usage();
    }
    else {
	last;
    }
}

(!defined($uml) || !defined($rootfs)) and Usage();

my $sudo = "";
$UID != 0 and $sudo = "sudo";

my $n = 0;

while(-e "cell$n"){
    $n++;
}
my $cell = "cell$n";

my $out;
my @more_args = ();

if(defined($host_ip)){
    $out = `tunctl -u $uid 2>&1`;
    $? ne 0 and die "Couldn't configure tap device : '$out'";
    if($out =~ /(tap\d+)/){
	$tap = $1;
	push @more_args, "eth0=tuntap,$tap";

	$out = `$sudo ifconfig $tap $host_ip up 2>&1`;
	$? ne 0 and die "Failed to ifconfig $tap : '$out'";

	$out = `$sudo route add -host $uml_ip dev $tap 2>&1`;
	$? ne 0 and die "Failed to set route to $uml_ip through $tap : '$out'";
    }
    else {
	die "Couldn't find tap device name in '$out'";
    }
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
print "	Extra arguments : '" . join(" ", @uml_args) . "'\n";
print "\n";

$out = `mkdir $cell 2>&1`;
$? ne 0 and die "Couldn't mkdir $cell : '$out'";

my $cell_tar = "cell.tar";
$out = `cd $cell ; $sudo tar xpf ../$cell_tar 2>&1`;
$? ne 0 and die "Couldn't populate $cell : '$out'";

print "Copying '$uml' and '$rootfs' to '$cell'...";
$out = `cp $uml $rootfs $cell 2>&1`;
$? ne 0 and die "Couldn't copy kernel and filesystem into $cell : '$out'";
print "done\n\n";

$uml = `basename $uml`;
chomp $uml;

$rootfs = `basename $rootfs`;
chomp $rootfs;

my @args = ("./jail_uml", $cell, $uid, "/$uml", "ubd0=/$rootfs", @uml_args,
	    @more_args );
$sudo ne "" and unshift @args, $sudo;

system @args;

$out = `rm -rf $cell 2>&1`;
$? ne 0 and die "Failed to clean up cell after inmate's demise : '$out'";

if(defined($tap)){
    $out = `$sudo ifconfig $tap down 2>&1`;
    $? ne 0 and die "Failed to down $tap - status = $? : '$out'";

    $out = `$sudo tunctl -d $tap`;
    $? ne 0 and die "Failed to make $tap non-persistent : '$out'";
}

return 0;
