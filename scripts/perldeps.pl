#!/usr/bin/perl -w
use strict;
use 5.006001;

use Getopt::Long;
my ($show_provides, $show_requires, $verbose, @ignores);

my $result = GetOptions("provides" => \$show_provides,
			"requires" => \$show_requires,
			"verbose" => \$verbose,
			"ignore=s" => \@ignores);
my %ignores = map { $_ => 1 } @ignores;

if (not $result) {
  exit 1;
}

my $deps = new DependencyParser;
for my $file (grep /^[^-]/, @ARGV) {
  $deps->process_file($file);
}

if ($show_requires) {
  for my $req ($deps->requires) {
    my $verbage = "";
    if (not exists $ignores{$req->to_string}) {
      printf "%s%s\n", $req->to_string, $verbage;
    }
  }
}

if ($show_provides) {
  for my $prov ($deps->provides) {
    my $verbage = "";
    if (not exists $ignores{$prov->to_string}) {
      printf "%s%s\n", $prov->to_string, $verbage;
    }
  }
}

package Dependency;
sub new {
  my $class = shift;
  my $type = shift;
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

  if ($self->type eq 'perl version') {
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

  my $dep = new Dependency "provide", $params{-provide};
  $dep->filename($params{-filename});
  $dep->type($params{-type});
  $dep->line_number($params{-line}) if $params{-line};

  push @{$self->{provides}}, $dep;
}

sub add_require {
  my $self = shift;
  my %params = @_;
  die "DependencyParser->add_require requires -filename, -require, and -type"
    if not exists $params{-filename} or not exists $params{-require} or not exists $params{-type};

  my $dep = new Dependency "require", $params{-require};
  $dep->filename($params{-filename});
  $dep->type($params{-type});
  $dep->line_number($params{-line}) if $params{-line};

  push @{$self->{requires}}, $dep;
}

sub process_file {
  my $self = shift;
  my $filename = shift;

  if (not open FH, "<$filename") {
    warn "Can't open $filename: $!";
    return;
  }

  while (<FH>) {
    next if m(^=(head1|head2|pod|item)) .. m(^=cut);
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
  }
}
