#!/usr/bin/perl -w
use strict;
use 5.006001;

use Getopt::Long;
my ($show_provides, $show_requires, $verbose, @ignores);

my $result = GetOptions("provides" => \$show_provides,
			"requires" => \$show_requires,
			"verbose"  => \$verbose,
			"ignore=s" => \@ignores);
my %ignores = map { $_ => 1 } @ignores;

exit(1) if (not $result);

my $deps = new DependencyParser;
for my $file (grep /^[^-]/, @ARGV) {
  $deps->process_file($file);
}

if ($show_requires) {
  for my $req ($deps->requires) {
    my $verbage = "";
    next if (exists $ignores{$req->to_string});
    printf "%s%s\n", $req->to_string, $verbage;
  }
}

if ($show_provides) {
  for my $prov ($deps->provides) {
    my $verbage = "";
    next if (exists $ignores{$prov->to_string});
    printf "%s%s\n", $prov->to_string, $verbage;
  }
}

exit(0);

####################
# Dependency Class #
####################
package Dependency;
sub new {
  my $class = shift;
  my $type  = shift;
  my $value = shift;

  return bless { type => $type, value => $value }, $class;
}

sub value {
  my $self = shift;

  if (@_) {
    $self->{value} = shift;
  }

  return $self->{value};
}

sub filename {
  my $self = shift;

  if (@_) {
    $self->{filename} = shift;
  }

  return $self->{filename};
}

sub type {
  my $self = shift;

  if (@_) {
    $self->{type} = shift;
  }

  return $self->{type};
}

sub line_number {
  my $self = shift;

  if (@_) {
    $self->{line_number} = shift;
  }

  return $self->{line_number};
}

sub to_string {
  my $self = shift;
  my $type = $self->type;

  if ($type eq 'perl version') {
    # we need to convert a perl release version to an rpm package
    # version

    my $epoch = 0;
    my $version = $self->value;
    $version =~ s/_/./g;
    $version =~ s/0+$//;

    if ($version =~ /^5.00[1-5]/) {
      $epoch = 0;
    }
    elsif ($version =~ /^5.006/ or $version =~ /^5.6/) {
      $version =~ s/00//g;
      $epoch = 1;
    }
    elsif ($version =~ /^5.00[7-9]/ or $version =~ /^5.[7-9]/) {
      $version =~ s/00//g;
      $epoch = 2;
    }
    $version =~ s/\.$//;

    return sprintf "perl >= %d:%s", $epoch, $version;
  }
  elsif ($type eq 'virtual') { 
   	return $self->value; 
  }
  else {
    return sprintf "perl(%s)", $self->value;
  }
}

package DependencyParser;
sub new {
  my $class = shift;
  return bless {}, $class;
}

sub requires {
  return @{shift->{requires} || []};
}

sub provides {
  return @{shift->{provides} || []};
}

sub add_provide {
  my $self = shift;
  my %params = @_;
  die "DependencyParser->add_provide requires -filename, -provide, and -type"
    if not exists $params{-filename} or not exists $params{-provide} or not exists $params{-type};

  #
  # Make sure this one has not been added already
  $self->{'provides_check'} ||= { };
  return if(exists($self->{'provides_check'}->{$params{'-provide'}}));

  #
  # Created dependency object
  my $dep = new Dependency "provide", $params{-provide};
  $dep->filename($params{-filename});
  $dep->type($params{-type});
  $dep->line_number($params{-line}) if $params{-line};

  #
  # Add to requires check list
  $self->{'provides_check'}->{$params{'-provide'}} = 1; 

  #
  # Add to list
  push @{$self->{provides}}, $dep;
}

sub add_require {
  my $self = shift;
  my %params = @_;
  die "DependencyParser->add_require requires -filename, -require, and -type"
    if not exists $params{-filename} or not exists $params{-require} or not exists $params{-type};

  #
  # Make sure this one has not been added already
  $self->{'requires_check'} ||= { };
  return if(exists($self->{'requires_check'}->{$params{'-require'}}));

  #
  # Create dependency object.
  my $dep = new Dependency "require", $params{-require};
  $dep->filename($params{-filename});
  $dep->type($params{-type});
  $dep->line_number($params{-line}) if $params{-line};

  #
  # Add to requires check list
  $self->{'requires_check'}->{$params{'-require'}} = 1; 

  #
  # Add to list
  push @{$self->{requires}}, $dep;
}

sub process_file {
  my $self     = shift;
  my $filename = shift;

  if (not open FH, "<$filename") {
    # XXX: Should be die IMHO...JOO
    warn "Can't open $filename: $!";
    return;
  }

  while (<FH>) {
    next if m(^=(head[1-4]|pod|item)) .. m(^=cut);
    next if m(^=over) .. m(^=back);
    last if m/^__(DATA|END)__$/;

    if (m/^\s*package\s+([\w\:]+)\s*;/) {
      $self->add_provide(-filename => $filename, -provide => $1, -type => "package", -line => $.);
    }
    if (m/^\s*use\s+base\s+(.*)/) {
      # recognize the three main forms: literal string, qw//, and
      # qw().  this is incomplete but largely sufficient.

      my @module_list;
      my $base_params = $1;

      if ($base_params =~ m[qw\((.*)\)]) {
	@module_list = split /\s+/, $1;
      }
      elsif ($base_params =~ m[qw/(.*)/]) {
	@module_list = split /\s+/, $1;
      }
      elsif ($base_params =~ m/(['"])(.*)\1/) { # close '] to unconfuse emacs cperl-mode
	@module_list = ($2);
      }

      $self->add_require(-filename => $filename, -require => $_, -type => "base", -line => $.)
	     for @module_list;
    }
    elsif (m/^\s*(use|require)\s+(v?[0-9\._]+)/) {
      $self->add_require(-filename => $filename, -require => $2, -type => "perl version", -line => $.);
    }
    elsif (m/^\s*use\s+([\w\:]+)/) {
      $self->add_require(-filename => $filename, -require => $1, -type => "use", -line => $.);
    }
    elsif (m/^require\s+([\w\:]+).*;/) {
      $self->add_require(-filename => $filename, -require => $1, -type => "require", -line => $.);
    } 
    #
    # Allow for old perl.req Requires.  Support:
    #
    #    $RPM_Requires = "x y z";
    #    our $RPM_Requires = "x y z";
    # 
    # where the rvalue is a space delimited list of provides.
    elsif (m/^\s*(our\s+)?\$RPM_Requires\s*=\s*["'](.*)['"]/) {
      foreach my $require (split(/\s+/, $2)) {
      	$self->add_require(
           -filename => $filename, 
           -require  => $require, 
           -type     => "virtual", 
           -line     => $.
        );
      }
    }
    #
    # Allow for old perl.req Provides.  Support:
    #
    #    $RPM_Provides = "x y z";
    #    our $RPM_Provides = "x y z";
    # 
    # where the rvalue is a space delimited list of provides.
    elsif ( m/^\s*(our\s+)?\$RPM_Provides\s*=\s*["'](.*)['"]/) {
      foreach my $provide (split(/\s+/, $2)) {
        $self->add_provide(
           -filename => $filename, 
           -provide  => $provide, 
           -type     => "virtual", 
           -line     => $.
        );
      }
    }
  }

  close(FH);
}

#######
# POD #
#######
__END__

=head1 NAME

perldeps.pl - Generate Dependency Sets For a Perl Script

=head1 SYNOPSIS

	perldeps.pl --provides [--verbose] 
		[--ignore=(dep) ... --ignore=(depN)]
	perldeps.pl --requires [--verbose] 
		[--ignore=(dep) ... --ignore=(depN)]

=head1 DESCRIPTION

This script examines a perl script or library and determines what the
set of provides and requires for that file.  Depending on whether you
use the C<--provides> or C<--requires> switch it will print either
the provides or requires it finds.  It will print each dependency 
on a seperate line simular to:

	perl(strict)
	perl(warnings)
	perl(Cmd)
	perl(Dbug)
	perl(Fdisk::Cmd)

This is the standard output that rpm expects from all of its autodependency
scripts.

Provides are determined by C<package> lines such as:

	package Great::Perl::Lib;

Additionally, a script can infrom C<perldeps.pl> that it has additional
provides by creating the variable C<$RPM_Provides>, and setting it to 
a space delimited list of provides.  For instance:

	$RPM_Provides = "great stuff";

Would tell C<perldeps.pl> that this script provides C<great> and C<stuff>.

Requires are picked up from several sources:

=over 4

=item *

C<use> lines.   These can define either libraries to use or the version
of perl required (see C<use> under C<perlfunc(1)).

=item *

C<require> lines.  Defines libraries to be sourced and evaled.

=item *

C<use base> lines.   These define base classes of the libraries and are 
thus dependencies.  It can parse the following forms:

	use base "somelib";
	use base qw(somelib otherlib);
	use base qw/somelib otherlib);

=back

Aditionally, you can define the variable C<$RPM_Requires> to define
additonal non-perl requirments.  For instance your script may require
sendmail, in which case might do:

	$RPM_Requires = "sendmail";

=head1 OPTIONS

=over 4

=item B<--provides>

Print all provides.

=item B<--requires>

Print all requires.

=item B<--ignore=(dep)>

Ignore this dependency if found.

=back

=head1 EXIT STATUS

0 success, 1 failure

=head1 SEE ALSO

/usr/lib/rpm/macros

=head1 BUGS

Does not generate version information on dependencies.  

=head1 AUTHOR

Chip Turner <cturner@redhat.com>
