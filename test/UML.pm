package UML;

use Expect;
use strict;

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $me = { kernel => 'linux',
	       arguments => '',
	       login_prompt => 'login:',
	       login => 'root',
	       password_prompt => 'Password:',
	       password => 'root',
	       prompt => 'darkstar:.*#',
	       halt => 'halt',
	       expect_handle => undef };

    while(@_){
	my $arg = shift;
	if($arg eq 'kernel'){
	    $me->{kernel} = shift;
	}
	elsif($arg eq 'arguments'){
	    $me->{arguments} = shift;
	}
	elsif($arg eq 'login_prompt'){
	    $me->{login_prompt} = shift;
	}
	elsif($arg eq 'login'){
	    $me->{login} = shift;
	}
	elsif($arg eq 'password_prompt'){
	    $me->{password_prompt} = shift;
	}
	elsif($arg eq 'password'){
	    $me->{password} = shift;
	}
	elsif($arg eq 'prompt'){
	    $me->{prompt} = shift;
	}
	elsif($arg eq 'halt'){
	    $me->{halt} = shift;
	}
	else {
	    die "UML::new : Unknown argument - $arg";
	}
    }
    bless($me, $class);
    return $me;
}

sub boot {
    my $me = shift;

    if(defined($me->{expect_handle})){
	warn "UML::boot : already booted";
	return;
    }
    my $cmd = "$me->{kernel} $me->{arguments}";
    $me->{expect_handle} = Expect->spawn($cmd);
    $me->{expect_handle}->expect(undef, "$me->{login_prompt}");
    $me->{expect_handle}->print("$me->{login}\n");
    $me->{expect_handle}->expect(undef, "$me->{password_prompt}");
    $me->{expect_handle}->print("$me->{password}\n");
    $me->{expect_handle}->expect(undef, "-re", "$me->{prompt}");
}

sub command {
    my $me = shift;
    my $cmd = shift;

    $me->{expect_handle}->print("$cmd\n");
    $me->{expect_handle}->expect(undef, "-re", "$me->{prompt}");
}

sub halt {
    my $me = shift;

    $me->{expect_handle}->print("$me->{halt}\n");
    $me->{expect_handle}->expect(undef);
}

1;

=head1 NAME

UML - class to control User-mode Linux

=head1 SYNOPSIS

 use UML;

 #################
 # class methods #
 #################
 $uml   = UML->new(kernel => $path_to_kernel,		# default "linux"
		   arguments => $kernel_arguments,      # ""
		   login_prompt => $login_prompt,       # "login:"
		   login => $account_name,              # "root"
		   password_prompt => $password_prompt, # "Password:"
		   password => $account_password,       # "root"
		   prompt => $shell_prompt_re,          # "darkstar:.*#"
		   halt => $halt_command);              # "halt"
 $uml->boot();
 $uml->command($command_string);
 $uml->halt();

 #######################
 # object data methods #
 #######################

 ########################
 # other object methods #
 ########################

=head1 DESCRIPTION

The UML class is used to control the execution of a user-mode kernel.
All of the arguments to UML::new are optional and will be defaulted if
not present.  The arguments and their values are as follows:
    kernel - the filename of the kernel executable
    arguments - a string containing the kernel command line
    login_prompt - a string matching the login prompt
    login - the account to log in to
    password_prompt - a string matching the password prompt
    password - the account's password
    prompt - a regular expression matching the shell prompt
    halt - the command used to halt the virtual machine

Once constructed, the UML object may be booted.  UML::boot() will
return after it has successfully logged in.

Then, UML::command may be called as many times as desired.  It will
return when the command has finished and the next shell prompt has
been seen.

When the testing is finished, UML::halt() is called to shut the
virtual machine down.
