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
  $self->{db} = RPM2::_open_rpm_db($params{-path}, $params{-readwrite} ? 1 : 0);

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

  if (wantarray) {
    return ($hdr, $sigs);
  }
  else {
    return $hdr;
  }
}

sub close_rpm_db {
  my $self = shift;
  die "db not open" unless $self->{db};

  RPM2::_close_rpm_db($self->{db});
  $self->{db} = undef;
}

sub find_all_iter {
  my $self = shift;

  return $self->iterator("RPMTAG_NAME")
}

sub find_all {
  my $self = shift;

  return $self->iterator()->expand_iter();
}

sub find_by_name_iter {
  my $self = shift;
  my $name = shift;

  return $self->iterator("RPMTAG_NAME", $name);
}
sub find_by_name {
  my $self = shift;
  my $name = shift;

  return $self->find_by_name_iter($name)->expand_iter;
}

sub find_by_provides_iter {
  my $self = shift;
  my $name = shift;

  return $self->iterator("RPMTAG_PROVIDES", $name);
}
sub find_by_provides {
  my $self = shift;
  my $name = shift;

  return $self->find_by_provides_iter($name)->expand_iter;
}

sub find_by_file_iter {
  my $self = shift;
  my $name = shift;

  return $self->iterator("RPMTAG_BASENAMES", $name);
}
sub find_by_file {
  my $self = shift;
  my $name = shift;

  return $self->find_by_file_iter($name)->expand_iter;
}

sub iterator {
  my $self = shift;
  my $tag = shift;
  my $str = shift;

  die "db closed" unless $self->{db};
  my $iter = RPM2::PackageIterator->new_iterator($self->{db}, $tag, $str);

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

sub is_source_package {
  my $self = shift;

  return RPM2::_header_is_source($self->{header});
}

sub as_nvre {
  my $self = shift;
  my $epoch = $self->tag('epoch');
  my $epoch_str = '';

  $epoch_str = "$epoch:" if defined $epoch;

  my $ret = $epoch_str . join("-", map { $self->tag($_) } qw/name version release/);

  return $ret;
}

foreach my $tag (keys %tagmap) {
  $tag =~ s/^RPMTAG_//g;

  my $sub = q {
    sub [[method]] {
      my $self = shift;
      return $self->tag("[[tag]]");
    }
  };

  my $method = lc $tag;
  $sub =~ s/\[\[method\]\]/$method/g;
  $sub =~ s/\[\[tag\]\]/$tag/g;
  eval $sub;

  if ($@) {
    die $@;
  }
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

sub new_iterator {
  my $class = shift;
  my $db = shift;
  my $tag = shift;
  my $key = shift;

  my $self = bless {}, $class;
  $self->{iter} = RPM2::_init_iterator($db, $tagmap{$tag}, $key, defined $key ? length $key : 0);
  $self->{db} = $db;

  return $self;
}

sub next {
  my $self = shift;

  return unless $self->{iter};

  my $hdr = RPM2::_iterator_next($self->{iter});
  return unless $hdr;

  return RPM2::Header->_new_raw($hdr, 1);
}

sub expand_iter {
  my $self = shift;

  my @ret;
  while (my $h = $self->next) {
    push @ret, $h;
  }

  return @ret;
}

sub DESTROY {
  my $self = shift;

  if ($self->{iter}) {
    RPM2::_destroy_iter($self->{iter});
  }
}

# Preloaded methods go here.

1;
__END__
# Below is stub documentation for your module. You better edit it!

=head1 NAME

RPM2 - Perl bindings for the RPM Package Manager API

=head1 SYNOPSIS

  use RPM2;

  my $db = RPM2->open_rpm_db();

  my $i = $db->find_all_iter();
  print "The following packages are installed (aka, 'rpm -qa'):\n";
  while (my $pkg = $i->next) {
    print $pkg->as_nvre, "\n";
  }

  $i = $db->find_by_name_iter("kernel");
  print "The following kernels are installed (aka, 'rpm -q kernel'):\n";
  while (my $pkg = $i->next) {
    print $pkg->as_nvre, " ", int($pkg->size()/1024), "k\n";
  }

  $i = $db->find_by_provides_iter("kernel");
  print "The following packages provide 'kernel' (aka, 'rpm -q --whatprovides kernel'):\n";
  while (my $pkg = $i->next) {
    print $pkg->as_nvre, " ", int($pkg->size()/1024), "k\n";
  }

  print "The following packages are installed (aka, 'rpm -qa' once more):\n";
  foreach my $pkg ($db->find_by_file("/bin/sh")) {
    print $pkg->as_nvre, "\n";
  }

  my $pkg = RPM2->open_package_file("/tmp/XFree86-4.1.0-15.src.rpm");
  print "Package opened: ", $pkg->as_nvre(), ", is source: ", $pkg->is_source_package, "\n";

=head1 DESCRIPTION

The RPM2 module provides an object-oriented interface to querying both
the installed RPM database as well as files on the filesystem.

TODO: Everything, including:

The above methods need documenting.

Package installation and removal.

Signature validation.

=head1 HISTORY

=over 8

=item 0.01
Initial release

=back


=head1 AUTHOR

Chip Turner E<lt>cturner@redhat.comE<gt>

=head1 SEE ALSO

L<perl>.
The original L<RPM> module.

=cut
