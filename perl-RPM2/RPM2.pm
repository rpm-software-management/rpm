package RPM2;

use 5.00503;
use strict;
use DynaLoader;
use Data::Dumper;
use Cwd qw/realpath/;

use vars qw/$VERSION/;
$VERSION = '0.61';
use vars qw/@ISA/;
@ISA = qw/DynaLoader/;

bootstrap RPM2 $VERSION;

foreach my $tag (keys %RPM2::constants) {
  my $sub = q {
    sub [[method]] {
      my $self = shift;
      return $RPM2::constants{[[tag]]};
    }
  };

  my $method = lc $tag;
  $method =~ s/^rpm//;
  $sub =~ s/\[\[method\]\]/$method/g;
  $sub =~ s/\[\[tag\]\]/$tag/g;
  eval $sub;

  if ($@) {
    die $@;
  }
}

sub open_rpm_db {
  my $class = shift;
  my %params = @_;

  my $self = bless { }, "RPM2::DB";
  if ($params{-path}) {
    $class->add_macro("_dbpath", $params{-path});
    $self->{c_db} = RPM2::_open_rpm_db($params{-readwrite} ? 1 : 0);
    $class->delete_macro("_dbpath");
  }
  else {
    $self->{c_db} = RPM2::_open_rpm_db($params{-readwrite} ? 1 : 0);
  }

  return $self;
}

sub open_package {
  my $class = shift;
  my $file = shift;
  my $flags = shift;

  if (RPM2->rpm_api_version > 4.0 and not defined $flags) {
    $flags = RPM2->vsf_default;
  }
  $flags ||= 0;

  open FH, "<$file"
    or die "Can't open $file: $!";

  my $hdr = RPM2::_read_package_info(*FH, $flags);
  close FH;

  $hdr = RPM2::Header->_new_raw($hdr, realpath($file));
  return $hdr;
}

package RPM2::DB;

sub find_all_iter {
  my $self = shift;

  return RPM2::PackageIterator->new_iterator($self, "RPMTAG_NAME")
}

sub find_all {
  my $self = shift;

  return RPM2::PackageIterator->new_iterator($self)->expand_iter();
}

sub find_by_name_iter {
  my $self = shift;
  my $name = shift;

  return RPM2::PackageIterator->new_iterator($self, "RPMTAG_NAME", $name);
}
sub find_by_name {
  my $self = shift;
  my $name = shift;

  return $self->find_by_name_iter($name)->expand_iter;
}

sub find_by_provides_iter {
  my $self = shift;
  my $name = shift;

  return RPM2::PackageIterator->new_iterator($self, "RPMTAG_PROVIDES", $name);
}
sub find_by_provides {
  my $self = shift;
  my $name = shift;

  return $self->find_by_provides_iter($name)->expand_iter;
}

sub find_by_requires_iter {
  my $self = shift;
  my $name = shift;

  return RPM2::PackageIterator->new_iterator($self, "RPMTAG_REQUIRENAME", $name);
}

sub find_by_requires {
  my $self = shift;
  my $name = shift;

  return $self->find_by_requires_iter($name)->expand_iter;
}

sub find_by_file_iter {
  my $self = shift;
  my $name = shift;

  return RPM2::PackageIterator->new_iterator($self, "RPMTAG_BASENAMES", $name);
}

sub find_by_file {
  my $self = shift;
  my $name = shift;

  return $self->find_by_file_iter($name)->expand_iter;
}

package RPM2::Header;

use overload '<=>' => \&op_spaceship,
             'bool' => \&op_bool;

sub _new_raw {
  my $class = shift;
  my $c_header = shift;
  my $filename = shift;

  my $self = bless { }, $class;
  $self->{c_header} = $c_header;
  $self->{filename} = $filename if defined $filename;

  return $self;
}

sub tag {
  my $self = shift;
  my $tag = shift;

  $tag = uc "RPMTAG_$tag";

  die "tag $tag invalid"
    unless exists $RPM2::header_tag_map{$tag};

  return $self->{c_header}->tag_by_id($RPM2::header_tag_map{$tag});
}

sub tagformat {
  my $self   = shift;
  my $format = shift;

  return RPM2::C::Header::_header_sprintf($self->{c_header}, $format);
}

sub compare {
  my $h1 = shift;
  my $h2 = shift;

  return RPM2::C::Header::_header_compare($h1->{c_header}, $h2->{c_header});
}

sub op_bool {
  my $self = shift;

  return defined($self) && defined($self->{c_header});
}

sub op_spaceship {
  my $h1 = shift;
  my $h2 = shift;

  my $ret = $h1->compare($h2);

  # rpmvercmp can return any neg/pos number; normalize here to -1, 0, 1
  return  1 if $ret > 0;
  return -1 if $ret < 0;
  return  0;
}

sub is_source_package {
  my $self = shift;

  return RPM2::C::Header::_header_is_source($self->{c_header});
}

sub filename {
  my $self = shift;
  if (exists $self->{filename}) {
    return $self->{filename};
  }
  return;
}

sub as_nvre {
  my $self = shift;
  my $epoch = $self->tag('epoch');
  my $epoch_str = '';

  $epoch_str = "$epoch:" if defined $epoch;

  my $ret = $epoch_str . join("-", map { $self->tag($_) } qw/name version release/);

  return $ret;
}

foreach my $tag (keys %RPM2::header_tag_map) {
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

package RPM2::PackageIterator;

sub new_iterator {
  my $class = shift;
  my $db = shift;
  my $tag = shift;
  my $key = shift;

  my $self = bless { }, $class;
  $self->{c_iter} = RPM2::C::DB::_init_iterator($db->{c_db},
						$RPM2::header_tag_map{$tag},
						$key || "",
						defined $key ? length $key : 0);
  return $self;
}

sub next {
  my $self = shift;

  return unless $self->{c_iter};
  my $hdr = $self->{c_iter}->_iterator_next();
  return unless $hdr;

  my $ret = RPM2::Header->_new_raw($hdr);
  return $ret;
}

sub expand_iter {
  my $self = shift;

  my @ret;
  while (my $h = $self->next) {
    push @ret, $h;
  }

  return @ret;
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

  my $pkg = RPM2->open_package("/tmp/XFree86-4.1.0-15.src.rpm");
  print "Package opened: ", $pkg->as_nvre(), ", is source: ", $pkg->is_source_package, "\n";

=head1 DESCRIPTION

The RPM2 module provides an object-oriented interface to querying both
the installed RPM database as well as files on the filesystem.

=head1 CLASS METHODS

Pretty much all use of the class starts here.  There are two main
entrypoints into the package -- either through the database of
installed rpms (aka the rpmdb) or through a file on the filesystem
(such as kernel-2.4.9-31.src.rpm or kernel-2.4.9-31.i386.rpm

You can have multiple RPM databases open at once, as well as running
multiple queries on each.

=item open_rpm_db(-path => "/path/to/db")

As it sounds, it opens the RPM database, and returns it as an object.

=item open_package("foo-1.1-14.noarch.rpm")

Opens a specific package (RPM or SRPM).  Returns a Header object.

=head1 RPM DB object methods

=item find_all_iter()

Returns an iterator object that iterates over the entire database.

=item find_all()

Returns an list of all of the results of the find_all_iter() method.

=item find_by_file_iter($filename)

Returns an iterator that returns all packages that contain a given file.

=item find_by_file($filename)

Ditto, except it just returns the list

=item find_by_name_iter($package_name)

You get the idea.  This one is for iterating by package name.

=item find_by_name($package_name)

Ditto, except it returns a list.

=item find_by_provides_iter($provides_string)

This one iterates over provides.

=item find_by_provides($provides_string)

Ditto, except it returns a list.

=item find_by_requires_iter($requires_string)

This one iterates over requires.

=item find_by_requires($requires_string)

Ditto, except it returns a list.

=head1 RPM Header object methods

stuff goes here

=head1 TODO

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
