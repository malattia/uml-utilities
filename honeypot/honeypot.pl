use hppfs;
use strict;

my $dir;

@ARGV and $dir = $ARGV[0];

sub interrupts {
    return `cat /proc/interrupts`;
}

sub cmdline {
    return `cat /proc/cmdline`;
}

sub filesystems {
    my $input = shift;

    return $input . "\tfoobarfs\n";
}

my $hppfs = hppfs->new($dir);

$hppfs->add("interrupts" => [ \&interrupts, "r" ],
	    "cmdline" => [ \&cmdline, "r" ],
	    "filesystems" => [ \&filesystems, "rw" ]);

$hppfs->handler();
