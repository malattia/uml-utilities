use hppfs;
use hppfslib;
use strict;

my $dir;

@ARGV and $dir = $ARGV[0];

my $hppfs = hppfs->new($dir);

$hppfs->add("devices" => remove_line("ubd"),
	    "uptime" => proc("uptime"),
	    "exitcode" => "remove",
	    "filesystems" => remove_line("hppfs"),
	    "interrupts" => proc("interrupts"),
	    "iomem" => proc("iomem"),
	    "ioports" => proc("ioports"),
	    "pid/mounts" => remove_line("hppfs"),
	    "mounts" => remove_line("hppfs"),
	    "version" => proc("version") );

$hppfs->handler();
