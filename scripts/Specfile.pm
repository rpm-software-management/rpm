package RPM::Specfile;

use POSIX;

use strict;

use vars qw/$VERSION/;

$VERSION = '1.02';

sub new {
  my $class = shift;

  my $self = bless { }, $class;

  return $self;
}

my @simple_accessors = qw/name version release epoch license group url description prep build clean install summary buildroot buildrequires file_param packager vendor distribution buildarch/;

foreach my $field (@simple_accessors) {
  my $sub = q {
    sub RPM::Specfile::[[field]] {
      my $self = shift;
      if (@_) {
        my $value = shift;
        $self->{__[[field]]__} = $value;
      }
      return $self->{__[[field]]__};
    }
  };

  $sub =~ s/\[\[field\]\]/$field/g;
  eval $sub;

  if ($@) {
    die $@;
  }
}

my @array_accessors = qw/source patch changelog provide require file/;

foreach my $field (@array_accessors) {
  my $sub = q {
    sub RPM::Specfile::[[field]] {
      my $self = shift;
      $self->{__[[field]]__} ||= [ ];

      if (@_) {
        my $index = shift;
        if (@_) {
          my $value = shift;
          $self->{__[[field]]__}->[$index] = $value;
        }
        return $self->{__[[field]]__}->[$index];
      }
      else {
        return @{$self->{__[[field]]__}};
      }
    }

    sub RPM::Specfile::push_[[field]] {
      my $self = shift;
      my $entry = shift;

      $self->{__[[field]]__} ||= [ ];
      push @{$self->{__[[field]]__}}, $entry;
    }

    sub RPM::Specfile::clear_[[field]] {
      my $self = shift;
      my $entry = shift;

      $self->{__[[field]]__} = [ ];
    }

  };

  $sub =~ s/\[\[field\]\]/$field/g;
  eval $sub;

  if ($@) {
    die $@;
  }
}

sub add_changelog_entry {
  my $self = shift;
  my $who = shift;
  my $entry = shift;

  my $output;
  $output .= strftime("* %a %b %d %Y $who\n", localtime time);
  $output .= "- $entry\n";

  $self->push_changelog($output);
}

sub generate_specfile {
  my $self = shift;

  my $output;

  my %defaults = ( buildroot => "%{_tmppath}/%{name}-root" );
  $self->$_($self->$_() || $defaults{$_}) foreach keys %defaults;

  my %proper_names = ( url => "URL", buildroot => "BuildRoot", "buildrequires" => "BuildRequires" );

  foreach my $tag (qw/name version release epoch packager vendor distribution summary license group url buildroot buildrequires buildarch/) {
    my $proper = $proper_names{$tag} || ucfirst $tag;

    next unless defined $self->$tag();
    $output .= "$proper: " . $self->$tag() . "\n";
  }

  my @reqs = $self->require;
  for my $i (0 .. $#reqs) {
    $output .= "Requires: $reqs[$i]\n";
  }

  my @sources = $self->source;
  for my $i (0 .. $#sources) {
    $output .= "Source$i: $sources[$i]\n";
  }

  my @patches = $self->patch;
  for my $i (0 .. $#patches) {
    $output .= "Patch$i: $patches[$i]\n";
  }

  $output .= "\n";

  foreach my $sect (qw/description prep build clean install/) {
    $output .= "%$sect\n";
    $output .= $self->$sect() . "\n";
  }

  if ($self->file_param) {
    $output .= "%files " . $self->file_param . "\n";
  }
  else {
    $output .= "%files\n";
  }
  $output .= "$_\n" foreach $self->file;

  $output .= "\n%changelog\n";
  $output .= "$_\n" foreach $self->changelog;

  return $output;
}

sub write_specfile {
  my $self = shift;
  my $dest = shift;

  open FH, ">$dest"
    or die "Can't open $dest: $!";

  print FH $self->generate_specfile;

  close FH;
}

1;

__END__
# Below is stub documentation for your module. You better edit it!

=head1 NAME

RPM::Specfile - Perl extension for creating RPM Specfiles

=head1 SYNOPSIS

  use RPM::Specfile;

=head1 DESCRIPTION

Simple module for creation of RPM Spec files

=head2 EXPORT

None by default.

=head1 AUTHOR

Chip Turner <cturner@redhat.com>

=head1 SEE ALSO

L<perl>.

=cut
