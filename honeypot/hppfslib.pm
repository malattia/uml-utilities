# 
# Copyright (C) 2002, 2003 Jeff Dike (jdike@karaya.com)
# Licensed under the GPL
#

package hppfslib;

use Exporter   ();
use vars       qw(@ISA @EXPORT);

use strict;

@ISA         = qw(Exporter);
@EXPORT      = qw(&remove_lines &host &proc);

sub remove_lines {
    my @remove = @_;

    return( [ sub { my $input = shift;

		    foreach my $str (@remove){
			$input =~ s/^.*$str.*\n//mg;
		    }
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
