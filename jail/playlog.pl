# 
# Copyright (C) 2003 Jeff Dike (jdike@karaya.com)
# Licensed under the GPL
#
# Translated from playlog.py, by Upi Tamminen

use Time::HiRes qw(usleep);
use tty_log;
use IO::Handle;

use strict;

my $usage_string = 
"Usage : perl playlog.pl [-f] [-n] [-a] log-file [tty-id]
	-f - follow the log, similar to 'tail -f'
	-n - full-speed playback, without mimicing the original timing
	-a - all traffic, including both tty reads and writes
By default, playlog will retain the original timing in the log.  -n will
just dump the log out without that timing.  Also by default, only tty writes
will be output.  This will provide an authentic view of what the original
user saw, but it will leave non-echoed characters, such as passwords, out.  
-a will output ttys reads as well, but this has the side-effect of duplicating
all normal, echoed, user input.
";

sub Usage {
    print $usage_string;
    exit(1);
}

my $follow = 0;
my $fast = 0;
my $all = 0;

while(@ARGV){
    my $arg = shift @ARGV;
    if($arg eq "-f"){
	$follow = 1;
    }
    elsif($arg eq "-n"){
	$fast = 1;
    }
    elsif($arg eq "-a"){
	$all = 1;
    }
    else {
	unshift @ARGV, $arg;
	last;
    }
}

!@ARGV and Usage();

my $file = shift @ARGV;

@ARGV > 1 and Usage();

my $tty_id;
@ARGV and my $tty_id = $ARGV[0];

open FILE, "<$file" or die "Couldn't open $file : $!";

my @ops = ();

while(1){
    my $op = read_log_line($file, \*FILE, 0);
    !defined($op) and last;

    push @ops, $op;
}

my @ttys = map { $_->{op} eq "open" && (($_->{old_tty} == 0) || 
					($_->{old_tty} == $_->{tty})) ? 
					    $_->{tty} : () } @ops;

my %unique_ttys = ();
foreach my $tty (@ttys){
    $unique_ttys{$tty} = 1;
}

@ttys = keys(%unique_ttys);

if(@ttys > 1 and !defined($tty_id)){
    print "You have the following ttys to choose from:\n";
    print join(" ", @ttys) . "\n";
    exit(0);
}

!defined($tty_id) and $tty_id = $ttys[0];

STDOUT->autoflush(1);

my $base;
my $cur;

foreach my $op (@ops){
    (($op->{op} ne "write") || ($op->{tty} ne $tty_id)) and next;
    (!$all && ($op->{direction} ne "write")) and next;

    !defined($base) and $base = $op->{secs};

    my $next = ($op->{secs} - $base) * 1000 * 1000 + $op->{usecs};
    !$fast && defined($cur) and usleep($next - $cur);
    print $op->{string};
    $cur = $next;
}

!$follow and exit 0;

while(1){
    my $op = read_log_line($file, \*FILE, 1);

    (($op->{op} ne "write") || ($op->{tty} ne $tty_id)) and next;
    (!$all && ($op->{direction} ne "write")) and next;

    print $op->{string};
}
