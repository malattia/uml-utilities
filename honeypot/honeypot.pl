# 
# Copyright (C) 2002, 2003 Jeff Dike (jdike@karaya.com)
# Licensed under the GPL
#

use hppfs;
use hppfslib;
use strict;

my $dir;

@ARGV and $dir = $ARGV[0];

my $hppfs = hppfs->new($dir);

my $remove_filesystems = remove_lines("hppfs", "hostfs");

$hppfs->add("devices" => remove_lines("ubd"),
	    "uptime" => proc("uptime"),
	    "exitcode" => "remove",
	    "filesystems" => $remove_filesystems,
	    "interrupts" => proc("interrupts"),
	    "iomem" => proc("iomem"),
	    "ioports" => proc("ioports"),
	    "pid/mounts" => $remove_filesystems,
	    "mounts" => $remove_filesystems,
	    "version" => proc("version") );

$hppfs->handler();
