use UML;

use strict;

# The kernel command line - you probably need to change this

my $args = "ubd0=../distros/slackware_7.0/root_fs_slackware_7_0 " .
    "ubd3=../kernel ubd5=../lmbench umn=192.168.0.100 mem=128M";

my $uml = UML->new(arguments => $args);
$uml->boot();
$uml->command('ls -al');
$uml->halt();
