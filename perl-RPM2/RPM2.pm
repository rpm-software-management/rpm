package RPM2;

use 5.00503;
use strict;
use DynaLoader;
use Data::Dumper;

use vars qw/$VERSION/;
$VERSION = '0.01';
use vars qw/@ISA/;
@ISA = qw/DynaLoader/;

bootstrap RPM2 $VERSION;

my %tagmap;

RPM2::_init_rpm();
RPM2::_populate_header_tags(\%tagmap);

sub open_rpm_db {
  my $class = shift;
  my %params = @_;

  my $self = bless { }, $class;
  $self->{db} = RPM2::_open_rpm_db($params{-path}, $params{-read_only} ? 0 : 1);

  return $self;
}

sub open_package_file {
  my $class = shift;
  my $file = shift;

  open FH, "<$file"
    or die "Can't open $file: $!";

  my ($hdr, $sigs) = RPM2::_read_package_info(*FH);
  close FH;

  $hdr = RPM2::Header->_new_raw($hdr, 1);
  $sigs = RPM2::Header->_new_raw($sigs, 1);

  return ($hdr, $sigs);
}

sub close_rpm_db {
  my $self = shift;
  die "db not open" unless $self->{db};

  RPM2::_close_rpm_db($self->{db});
  $self->{db} = undef;
}

sub iterator {
  my $self = shift;
  my $str = shift;

  die "db closed" unless $self->{db};
  my $iter = RPM2::PackageIterator->new_iterator($self->{db}, $str);

  return $iter;
}

sub DESTROY {
  my $self = shift;

  if ($self->{db}) {
    $self->close_rpm_db();
  }
}

package RPM2::Header;

sub _new_raw {
  my $class = shift;
  my $c_header = shift;
  my $need_free = shift;

  my $self = bless { }, $class;
  $self->{header} = $c_header;
  $self->{need_free} = $need_free;

  return $self;
}

sub tag {
  my $self = shift;
  my $tag = shift;

  $tag = uc "RPMTAG_$tag";

  die "tag $tag invalid"
    unless exists $tagmap{$tag};

  return RPM2::_header_tag($self->{header}, $tagmap{$tag});
}

sub as_nvre {
  my $self = shift;
  my $epoch = $self->tag('epoch');
  my $epoch_str = '';

  $epoch_str = "$epoch:" if defined $epoch;

  my $ret = $epoch_str . join("-", map { $self->tag($_) } qw/name version release/);

  return $ret;
}

sub files {
  my $self = shift;

  if (not exists $self->{files}) {
    my @base_names = $self->tag('basenames');
    my @dir_names = $self->tag('dirnames');
    my @dir_indexes = $self->tag('dirindexes');

    my @files;
    foreach (0 .. $#base_names) {
      push @files, $dir_names[$dir_indexes[$_]] . $base_names[$_];
    }

    $self->{files} = \@files;
  }

  return @{$self->{files}};
}

sub DESTROY {
  my $self = shift;

  if ($self->{need_free}) {
    RPM2::_free_header(delete $self->{header});
  }
}

package RPM2::PackageIterator;

sub RPMDBI_PACKAGES { 0; }

sub new_iterator {
  my $class = shift;
  my $db = shift;
  my $key = shift;

  my $self = bless {}, $class;
  $self->{iter} = RPM2::_init_iterator($db, RPM2::PackageIterator::RPMDBI_PACKAGES, $key, defined $key ? length $key : 0);
  $self->{db} = $db;

  return $self;
}

sub next {
  my $self = shift;

  die "no iterator" unless $self->{iter};

  my $hdr = RPM2::_iterator_next($self->{iter});
  return unless $hdr;

  return RPM2::Header->_new_raw($hdr, 1);
}

sub _cleanup {
  my $self = shift;
  return unless $self->{iter};

  RPM2::_destroy_iterator($self->{iter});

  delete $self->{$_} foreach keys %$self;
}

sub DESTROY {
  my $self = shift;

  $self->_cleanup();
}

# Preloaded methods go here.

1;
__END__
# Below is stub documentation for your module. You better edit it!

=head1 NAME

RPM2 - Perl extension for blah blah blah

=head1 SYNOPSIS

  use RPM2;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for RPM2, created by h2xs. It looks like the
author of the extension was negligent enough to leave the stub
unedited.

Blah blah blah.

=head2 EXPORT

None by default.


=head1 HISTORY

=over 8

=item 0.01

Original version; created by h2xs 1.21 with options

  -AC
	RPM2

=back


=head1 AUTHOR

A. U. Thor, E<lt>a.u.thor@a.galaxy.far.far.awayE<gt>

=head1 SEE ALSO

L<perl>.

=cut
