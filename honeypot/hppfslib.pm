# 
# Copyright (C) 2002 Jeff Dike (jdike@karaya.com)
# Licensed under the GPL
#

package hppfslib;

use Exporter   ();
use vars       qw(@ISA @EXPORT);

@ISA         = qw(Exporter);
@EXPORT      = qw(&remove_line &host &proc);

sub remove_line {
    my $str = shift;

    return( [ sub { my $input = shift;
		    $input =~ s/^.*$str.*\n$//m;
		    return($input) }, 
	      "rw" ] );
}

sub host {
    my $file = shift;

    return( [ sub { return(`cat $file`); },
	      "r" ] );
}

sub proc {
    my $file = shift;

    return(host("/proc/$file"));
}

1;
