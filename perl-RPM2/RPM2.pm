package RPM2;

use 5.00503;
use strict;
use DynaLoader;
use Cwd qw/realpath/;
use File::Basename qw/basename dirname/;
use File::Spec ();

use vars qw/$VERSION/;
$VERSION = '0.65';
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

sub open_hdlist {
  my $class = shift;
  my $file = shift;

  open FH, "<$file"
    or die "Can't open $file: $!";

  my @ret;
  while (1) {
    my ($hdr) = RPM2::_read_from_file(*FH);
    last unless $hdr;

    push @ret, RPM2::Header->_new_raw($hdr);
  }

  close FH;
  return @ret;
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

  my ($filename, $path) = (basename($file), realpath(dirname($file)));
  $hdr = RPM2::Header->_new_raw($hdr, File::Spec->catfile($path, $filename));
  return $hdr;
}

sub create_transaction
{
  my $class = shift;
  my $flags = shift;
  my $t;

  return undef if (RPM2->rpm_api_version <= 4.0); 
  if(not defined $flags) {
    $flags = RPM2->vsf_default;
  }

  $t = RPM2::_create_transaction($flags);
  $t = RPM2::Transaction->_new_raw($t);

  return $t;	
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

use overload '<=>'  => \&op_spaceship,
             'cmp'  => \&op_spaceship,
             'bool' => \&op_bool;

sub _new_raw {
  my $class = shift;
  my $c_header = shift;
  my $filename = shift;
  my $offset   = shift;

  my $self = bless { }, $class;
  $self->{c_header}  = $c_header;
  $self->{filename}  = $filename if defined $filename;
  $self->{db_offset} = $offset   if defined $offset;

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

sub offset {
  my $self = shift;
  if (exists $self->{db_offset}) {
    return $self->{db_offset};
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

sub changelog {
  my $self = shift;

  if (not exists $self->{changelog}) {
    my @cltimes = $self->tag('CHANGELOGTIME');
    my @clnames = $self->tag('CHANGELOGNAME');
    my @cltexts = $self->tag('CHANGELOGTEXT');

    my @changelog;
    foreach (0 .. $#cltimes) {
      push(@changelog,
           { time => $cltimes[$_],
             name => $clnames[$_],
             text => $cltexts[$_],
           });
    }

    $self->{changelog} = \@changelog;
  }

  return @{$self->{changelog}};
}

package RPM2::PackageIterator;

sub new_iterator {
  my $class = shift;
  my $db = shift;
  my $tag = shift;
  my $key = shift;

  my $self = bless { db => $db }, $class;
  $self->{c_iter} = RPM2::C::DB::_init_iterator($db->{c_db},
						$RPM2::header_tag_map{$tag},
						$key || "",
						defined $key ? length $key : 0);
  return $self;
}

sub next {
  my $self = shift;

  return unless $self->{c_iter};
  my ($hdr, $offset) = $self->{c_iter}->_iterator_next();
  return unless $hdr;

  my $ret = RPM2::Header->_new_raw($hdr, undef, $offset);
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

# make sure c_iter is destroyed before {db} so that we always free an
# iterator before we free the db it came from

sub DESTROY {
  my $self = shift;
  delete $self->{c_iter};
}

package RPM2::Transaction;

sub _new_raw {
  my $class         = shift;
  my $c_transaction = shift;

  my $self = bless { }, $class;
  $self->{c_transaction} = $c_transaction;

  return $self;
}

sub add_install {
  my $self    = shift;
  my $h       = shift;
  my $upgrade = shift || 0;
  my $fn;

  #
  # Must have a header to add
  return 0 if(!defined($h));

  #
  # Get filename
  $fn = $h->filename();
  
  # XXX: Need to add relocations at some point, but I think we live
  #      without this for now (until I need it (-;).
  return RPM2::C::Transaction::_add_install($self->{'c_transaction'}, 
	$h->{'c_header'}, $fn, $upgrade)
}

sub add_erase {
  my $self    = shift;
  my $h       = shift;
  my $db_offset;
  my $fn;

  #
  # Must have a header to add
  return 0 if(!defined($h));

  #
  # Get record offset
  $db_offset = $h->offset();
  return 0 if(!defined($db_offset));
 
  # XXX: Need to add relocations at some point, but I think we live
  #      without this for now (until I need it (-;).
  return RPM2::C::Transaction::_add_delete($self->{'c_transaction'}, 
	$h->{'c_header'}, $db_offset)
}

sub element_count {
	my $self = shift;
	
	return $self->{'c_transaction'}->_element_count();
}

sub close_db {
	my $self = shift;
	
	return $self->{'c_transaction'}->_close_db();
}

sub check {
	my $self = shift;
	
	return $self->{'c_transaction'}->_check();
}

sub order {
	my $self = shift;
		
	return $self->{'c_transaction'}->_order();
}

sub elements {
	my $self = shift;
	my $type = shift;

	$type = 0 if(!defined($type));
	
	return $self->{'c_transaction'}->_elements($type);
}

sub run {
  my $self         = shift;
  my $ok_probs     = shift || '';
  my $ignore_probs = shift || 0;

  return RPM2::C::Transaction::_run($self->{'c_transaction'}, $ok_probs, 
	$ignore_probs);
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

Pretty much all use of the class starts here.  There are three main
entrypoints into the package -- either through the database of
installed rpms (aka the rpmdb),  through a file on the filesystem
(such as kernel-2.4.9-31.src.rpm or kernel-2.4.9-31.i386.rpm, or via
an rpm transaction.

You can have multiple RPM databases open at once, as well as running
multiple queries on each.  That being said if you expect to run a transaction
to install or erase some rpms, you will need to cause any RPM2::DB and 
RPM2::PackageIterator objects to go out of scope.  For instance:

	$db = RPM2->open_rpm_db();
	$i  = $db->find_by_name("vim");
	$t  = create_transaction();
	while($pkg = $i->next()) {
	   $t->add_erase($pkg);
	}
	$t->run();

Would end up in a dead lock waiting for $db, and $i (the RPM2::DB and 
RPM2::PackageIterator) objects to releaase their read lock on the database.
The correct way of handling this then would be to do the following 
before running the transaction:

	$db = undef;
	$i  = undef;

That is to explicitly cause the RPM2::DB and RPM2::PackageIterator objects to
go out of scope.

=over 4

=item open_rpm_db(-path => "/path/to/db")

As it sounds, it opens the RPM database, and returns it as an object.
The path to the database (i.e. C<-path>) is optional.

=item open_package("foo-1.1-14.noarch.rpm")

Opens a specific package (RPM or SRPM).  Returns a Header object.

=item create_transaction(RPM2->vsf_default)

Creates an RPM2::Transaction.  This can be used to install and 
remove packages.  It, also, exposes the dependency ordering functionality. 
It takes as an optional argument verify signature flags.  The following 
flags are available:

=item RPM2->vsf_default

You don't ever have to specify this, but you could if you wanted to do so.
This will check headers, not require a files payload, and support all the
various hash and signature formats that rpm supports.

=item RPM2->vsf_nohdrchk

Don't check the header.

=item RPM2->vsf_needpayload

Require that a files payload be part of the RPM (Chip is this right?).

=item RPM2->vsf_nosha1header


=item RPM2->vsf_nomd5header

=item RPM2->vsf_nodsaheader

=item RPM2->vsf_norsaheader

=item RPM2->vsf_nosha1

=item RPM2->vsf_nomd5

=item RPM2->vsf_nodsa

=item RPM2->vsf_norsa

=back

=head1 RPM DB object methods

=over 4

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

=back

=head1 RPM Database Iterator Methods

Once you have a a database iterator, then you simply need to step
through all the different package headers in the result set via the
iterator.

=over 4

=item next()

Return the next package header in the result set.

=item expand_iter()

Return the list of all the package headers in the result set of the iterator.

=back

=head1 RPM Header object methods

In addition to the following methods, all tags have simple accessors;
$hdr->epoch() is equivalent to $hdr->tag('epoch').

The <=> and cmp operators can be used to compare versions of two packages.

=over 4

=item $hdr->tag($tagname)

Returns the value of the tag $tagname.

=item $hdr->tagformat($format)

TODO.

=item $hdr->is_source_package()

Returns a true value if the package is a source package, false otherwise.

=item $hdr->filename()

Returns the filename of the package.

=item $hdr->offset()

Returns the rpm database offset for the package.

=item $hdr->as_nvre()

Returns a string formatted like:

   epoch:name-version-release

If epoch is undefined for this package, it and the leading colon are omitted.

=item $hdr->files()

TODO.

=item $hdr->changelog()

Returns a list of hash refs containing the change log data of the package.
The hash keys represent individual change log entries, and their keys are:
C<time> (the time of the changelog entry), C<name> (the "name", ie. often
the email address of the author of the entry), and C<text> (the text of the
entry).

=back

=head1 Transaction object methods

Transactions are what allow you to install, upgrade, and remove rpms.
Transactions are created, have elements added to them (i.e. package headers)
and are ran.  When run the updates to the system and the rpm database are
treated as on "transaction" which is assigned a transaction id.  This can
be queried in install packages as the INSTALLTID, and for repackaged packages
they have the REMOVETID set.

=over 4

=item add_install($pkg, $upgrade)

Adds a package to a transaction for installation.  If you want this to
be done as a package upgrade, then be sure to set the second optional
parameter to 1.  It will return 0 on failure and 1 on success.  Note,
this should be obvious, but the package header must come from an rpm file,
not from the RPM database.

=item add_erase($pkg)

Adds a package to a transaction for erasure.  The package header should
come from the database (i.e. via an iterator) and not an rpm file.

=item element_count()

Returns the number of elements in a transaction (this is the sum of the
install and erase elements.

=item close_db()

Closes the rpm database.  This is needed for some ordering of
transactions for non-install purposes.

=item check()

Verify that the dependencies for this transaction are met.  Returns
0 on failure and 1 on success.

=item order()

Order the elements in dependency order.

=item elements()

Return a list of elements as they are presently ordered.  Note, this
returns the NEVR's not the package headers.

=item run()

Run the transaction.  This will automatically check for dependency
satisfaction, and order the transaction.

=back

=head1 TODO

Make package installation and removal better (-;.

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
