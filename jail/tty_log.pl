use strict;

my $TTY_LOG_OPEN = 1;
my $TTY_LOG_CLOSE = 2;
my $TTY_LOG_WRITE = 3;

!@ARGV and die "Usage : perl tty_log.pl log-file";

my $file = $ARGV[0];
open FILE, "<$file" or die "Couldn't open $file : $!";

my $log = join("", <FILE>);

my @ops = ();

while($log){
    (my $op, my $tty, my $len, $log) = unpack("iIia*", $log);
    (my $data, $log) = unpack("a${len}a*", $log);
    if($op == $TTY_LOG_OPEN){
	my ($old_tty) = unpack("I", $data);
	push @ops, { op => "open", tty => $tty, old_tty => $old_tty };
    }
    elsif($op == $TTY_LOG_CLOSE){
	push @ops, { op => "close", tty => $tty };
    }
    elsif($op == $TTY_LOG_WRITE){
	push @ops, { op => "write", tty => $tty, string => $data };
    }
    else {
	die "Bad tty_log op - $op";
    }
}

foreach my $op (@ops){
    if($op->{op} eq "open"){
	printf("Opening new tty 0x%x from tty 0x%x\n", $op->{tty}, 
	       $op->{old_tty});
    }
    elsif($op->{op} eq "close"){
	printf("Closing tty 0x%x\n", $op->{tty});
    }
    elsif($op->{op} eq "write"){
	printf("Write to tty 0x%x - '%s'\n", $op->{tty}, $op->{string});
    }
    else {
	die "Bad op - " . $op->{op};
    }
}
