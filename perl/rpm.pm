package rpm;

use strict;
use Carp;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK $AUTOLOAD);

require Exporter;
require DynaLoader;
require AutoLoader;

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(

);
$VERSION = '0.01';

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "& not defined" if $constname eq 'constant';
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
		croak "Your vendor has not defined rpm macro $constname";
	}
    }
    no strict 'refs';
    *$AUTOLOAD = sub () { $val };
    goto &$AUTOLOAD;
}

bootstrap rpm $VERSION;

# Preloaded methods go here.

sub Header::ItemByName {
  my $header = shift;
  my $item = shift;
  my $item_index = shift;

  if (defined $item_index) {
    return @{$header->ItemByNameRef($item)}[$item_index];
  } else {
    return @{$header->ItemByNameRef($item)};
  }
}

sub Header::ItemByVal {
  my $header = shift;
  my $item = shift;
  my $item_index = shift;

  if (defined $item_index) {
    return @{$header->ItemByValRef($item)}[$item_index];
  } else {
    return @{$header->ItemByValRef($item)};
  }
}

sub Header::List {
  my $header = shift;

  return %{$header->ListRef()};
}

sub Header::Tags {
  my $header = shift;

  return @{$header->TagsRef()};
}

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is the stub of documentation for your module. You better edit it!

=head1 NAME

rpm - Perl extension for blah blah blah

=head1 SYNOPSIS

  use rpm;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for rpm was created by h2xs. It looks like the
author of the extension was negligent enough to leave the stub
unedited.

Blah blah blah.

=head1 Exported constants


=head1 AUTHOR

Cristian Gafton, gafton@redhat.com

=head1 SEE ALSO

perl(1), rpm(8).

=cut
