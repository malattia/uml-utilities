# 
# Copyright (C) 2002 Jeff Dike (jdike@karaya.com)
# Licensed under the GPL
#

package hppfs;

use Socket;
use IO::Select;

use strict;

sub new {
    my $class = shift;
    my $base = shift;

    !defined($base) and $base = ".";
    my $me = { files => { }, handles => { }, base => $base };

    bless($me, $class);
    return($me);
}

sub add {
    my $me = shift;

    while(@_){
	my $file = shift;
	my $handler = shift;

	$me->{files}->{$file} = $handler;
    }
}

sub handler {
    my $me = shift;
    my $s = IO::Select->new();

    foreach my $file (keys(%{$me->{files}})){
	my (undef, $mode) = @{$me->{files}->{$file}};

	my $full = $me->{base} . "/proc/$file";
	if(! -d $full){
	    unlink $full;
	    my $out = `mkdir -p $full 2>&1`;
	    $? and die "mkdir '$full' failed : $out";
	}
	$full .= "/$mode";
	unlink $full;

	my $sock = sockaddr_un($full);
	!defined($sock) and die "sockaddr_un of '$sock' failed : $!";

	!defined(socket(my $fh, AF_UNIX, SOCK_STREAM, 0)) and 
	    die "socket failed : $!";

	!defined(bind($fh, $sock)) and die "bind failed : $!";

	!defined(listen($fh, 5)) and die "listen failed : $!";

	$s->add(\*$fh);
	$me->{handles}->{fileno(\*$fh)} = $file;
    }

    while(1){
	my @ready = $s->can_read();
	
	foreach my $sock (@ready){
	    my $file = $me->{handles}->{fileno($sock)};
	    !defined($file) and die "Couldn't map from socket to file";

	    !accept(CONN, $sock) and die "accept failed : $!";

	    my ($handler, $mode) = @{$me->{files}->{$file}};

	    (!defined($handler) || !defined($mode)) and 
		die "Couldn't map from file to handler";

	    my $output;

	    if($mode eq "rw"){
		my $input = join("", <CONN>);
		$output = $handler->($input);
	    }
	    else {
		$output = $handler->();
	    }

	    print CONN $output;
	    close CONN;
	}
    }
}

1;
