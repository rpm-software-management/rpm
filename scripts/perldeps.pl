#!/usr/bin/perl -Tw
#
# perldeps.pl -- Analyze dependencies of Perl packages
#
# Michael Jennings
# 7 November 2005
#
# $Id: perldeps.pl,v 1.6 2006/04/04 20:12:03 mej Exp $
#

use strict;
use Config;
use File::Basename;
use File::Find;
use Getopt::Long;
use POSIX;

############### Debugging stolen from Mezzanine::Util ###############
my $DEBUG = 0;

# Debugging output
sub
dprintf(@)
{
    my ($f, $l, $s, $format);
    my @params = @_;

    return if (! $DEBUG);
    $format = shift @params;
    if (!scalar(@params)) {
        return dprint($format);
    }
    (undef, undef, undef, $s) = caller(1);
    if (!defined($s)) {
        $s = "MAIN";
    }
    (undef, $f, $l) = caller(0);
    $f =~ s/^.*\/([^\/]+)$/$1/;
    $s =~ s/^\w+:://g;
    $s .= "()" if ($s =~ /^\w+$/);
    $f = "" if (!defined($f));
    $l = "" if (!defined($l));
    $format = "" if (!defined($format));
    for (my $i = 0; $i < scalar(@params); $i++) {
        if (!defined($params[$i])) {
            $params[$i] = "<undef>";
        }
    }
    printf("[$f/$l/$s] $format", @params);
}

sub
dprint(@)
{
    my ($f, $l, $s);
    my @params = @_;

    return if (! $DEBUG);
    (undef, undef, undef, $s) = caller(1);
    if (!defined($s)) {
        $s = "MAIN";
    }
    (undef, $f, $l) = caller(0);
    $f =~ s/^.*\/([^\/]+)$/$1/;
    $s =~ s/\w+:://g;
    $s .= "()" if ($s =~ /^\w+$/);
    $f = "" if (!defined($f));
    $l = "" if (!defined($l));
    $s = "" if (!defined($s));
    for (my $i = 0; $i < scalar(@params); $i++) {
        if (!defined($params[$i])) {
            $params[$i] = "<undef>";
        }
    }
    print "[$f/$l/$s] ", @params;
}

############### Module::ScanDeps Code ###############
use constant dl_ext  => ".$Config{dlext}";
use constant lib_ext => $Config{lib_ext};
use constant is_insensitive_fs => (
    -s $0 
        and (-s lc($0) || -1) == (-s uc($0) || -1)
        and (-s lc($0) || -1) == -s $0
);

my $CurrentPackage = '';
my $SeenTk;

# Pre-loaded module dependencies
my %Preload = (
    'AnyDBM_File.pm'  => [qw( SDBM_File.pm )],
    'Authen/SASL.pm'  => 'sub',
    'Bio/AlignIO.pm'  => 'sub',
    'Bio/Assembly/IO.pm'  => 'sub',
    'Bio/Biblio/IO.pm'  => 'sub',
    'Bio/ClusterIO.pm'  => 'sub',
    'Bio/CodonUsage/IO.pm'  => 'sub',
    'Bio/DB/Biblio.pm'  => 'sub',
    'Bio/DB/Flat.pm'  => 'sub',
    'Bio/DB/GFF.pm'  => 'sub',
    'Bio/DB/Taxonomy.pm'  => 'sub',
    'Bio/Graphics/Glyph.pm'  => 'sub',
    'Bio/MapIO.pm'  => 'sub',
    'Bio/Matrix/IO.pm'  => 'sub',
    'Bio/Matrix/PSM/IO.pm'  => 'sub',
    'Bio/OntologyIO.pm'  => 'sub',
    'Bio/PopGen/IO.pm'  => 'sub',
    'Bio/Restriction/IO.pm'  => 'sub',
    'Bio/Root/IO.pm'  => 'sub',
    'Bio/SearchIO.pm'  => 'sub',
    'Bio/SeqIO.pm'  => 'sub',
    'Bio/Structure/IO.pm'  => 'sub',
    'Bio/TreeIO.pm'  => 'sub',
    'Bio/LiveSeq/IO.pm'  => 'sub',
    'Bio/Variation/IO.pm'  => 'sub',
    'Crypt/Random.pm' => sub {
        _glob_in_inc('Crypt/Random/Provider', 1);
    },
    'Crypt/Random/Generator.pm' => sub {
        _glob_in_inc('Crypt/Random/Provider', 1);
    },
    'DBI.pm' => sub {
        grep !/\bProxy\b/, _glob_in_inc('DBD', 1);
    },
    'DBIx/SearchBuilder.pm' => 'sub',
    'DBIx/ReportBuilder.pm' => 'sub',
    'Device/ParallelPort.pm' => 'sub',
    'Device/SerialPort.pm' => [ qw(
        termios.ph asm/termios.ph sys/termiox.ph sys/termios.ph sys/ttycom.ph
    ) ],
    'ExtUtils/MakeMaker.pm' => sub {
        grep /\bMM_/, _glob_in_inc('ExtUtils', 1);
    },
    'File/Basename.pm' => [qw( re.pm )],
    'File/Spec.pm'     => sub {
        require File::Spec;
        map { my $name = $_; $name =~ s!::!/!g; "$name.pm" } @File::Spec::ISA;
    },
    'HTTP/Message.pm' => [ qw(
        URI/URL.pm          URI.pm
    ) ],
    'IO.pm' => [ qw(
        IO/Handle.pm        IO/Seekable.pm      IO/File.pm
        IO/Pipe.pm          IO/Socket.pm        IO/Dir.pm
    ) ],
    'IO/Socket.pm'     => [qw( IO/Socket/UNIX.pm )],
    'LWP/UserAgent.pm' => [ qw(
        URI/URL.pm          URI/http.pm         LWP/Protocol/http.pm
        LWP/Protocol/https.pm
    ), _glob_in_inc("LWP/Authen", 1) ],
    'Locale/Maketext/Lexicon.pm'    => 'sub',
    'Locale/Maketext/GutsLoader.pm' => [qw( Locale/Maketext/Guts.pm )],
    'Mail/Audit.pm'                => 'sub',
    'Math/BigInt.pm'                => 'sub',
    'Math/BigFloat.pm'              => 'sub',
    'Module/Build.pm'               => 'sub',
    'Module/Pluggable.pm'           => sub {
        _glob_in_inc("$CurrentPackage/Plugin", 1);
    },
    'MIME/Decoder.pm'               => 'sub',
    'Net/DNS/RR.pm'                 => 'sub',
    'Net/FTP.pm'                    => 'sub',
    'Net/SSH/Perl.pm'               => 'sub',
    'PDF/API2/Resource/Font.pm'     => 'sub',
    'PDF/API2/Basic/TTF/Font.pm'    => sub {
        _glob_in_inc('PDF/API2/Basic/TTF', 1);
    },
    'PDF/Writer.pm'                 => 'sub',
    'POE'                           => [ qw(
        POE/Kernel.pm POE/Session.pm
    ) ],
    'POE/Kernel.pm'                    => [
        map "POE/Resource/$_.pm", qw(
            Aliases Events Extrefs FileHandles
            SIDs Sessions Signals Statistics
        )
    ],
    'Parse/AFP.pm'                  => 'sub',
    'Parse/Binary.pm'               => 'sub',
    'Regexp/Common.pm'              => 'sub',
    'SOAP/Lite.pm'                  => sub {
        (($] >= 5.008 ? ('utf8.pm') : ()), _glob_in_inc('SOAP/Transport', 1));
    },
    'SQL/Parser.pm' => sub {
        _glob_in_inc('SQL/Dialects', 1);
    },
    'SVN/Core.pm' => sub {
        _glob_in_inc('SVN', 1),
        map "auto/SVN/$_->{name}", _glob_in_inc('auto/SVN'),
    },
    'SVK/Command.pm' => sub {
        _glob_in_inc('SVK', 1);
    },
    'SerialJunk.pm' => [ qw(
        termios.ph asm/termios.ph sys/termiox.ph sys/termios.ph sys/ttycom.ph
    ) ],
    'Template.pm'      => 'sub',
    'Term/ReadLine.pm' => 'sub',
    'Tk.pm'            => sub {
        $SeenTk = 1;
        qw( Tk/FileSelect.pm Encode/Unicode.pm );
    },
    'Tk/Balloon.pm'     => [qw( Tk/balArrow.xbm )],
    'Tk/BrowseEntry.pm' => [qw( Tk/cbxarrow.xbm Tk/arrowdownwin.xbm )],
    'Tk/ColorEditor.pm' => [qw( Tk/ColorEdit.xpm )],
    'Tk/FBox.pm'        => [qw( Tk/folder.xpm Tk/file.xpm )],
    'Tk/Toplevel.pm'    => [qw( Tk/Wm.pm )],
    'URI.pm'            => sub {
        grep !/.\b[_A-Z]/, _glob_in_inc('URI', 1);
    },
    'Win32/EventLog.pm'    => [qw( Win32/IPC.pm )],
    'Win32/Exe.pm'         => 'sub',
    'Win32/TieRegistry.pm' => [qw( Win32API/Registry.pm )],
    'Win32/SystemInfo.pm'  => [qw( Win32/cpuspd.dll )],
    'XML/Parser.pm'        => sub {
        _glob_in_inc('XML/Parser/Style', 1),
        _glob_in_inc('XML/Parser/Encodings', 1),
    },
    'XML/Parser/Expat.pm' => sub {
        ($] >= 5.008) ? ('utf8.pm') : ();
    },
    'XML/SAX.pm' => [qw( XML/SAX/ParserDetails.ini ) ],
    'XMLRPC/Lite.pm' => sub {
        _glob_in_inc('XMLRPC/Transport', 1),;
    },
    'diagnostics.pm' => sub {
        _find_in_inc('Pod/perldiag.pod')
          ? 'Pod/perldiag.pl'
          : 'pod/perldiag.pod';
    },
    'utf8.pm' => [
        'utf8_heavy.pl', do {
            my $dir = 'unicore';
            my @subdirs = qw( To );
            my @files = map "$dir/lib/$_->{name}", _glob_in_inc("$dir/lib");

            if (@files) {
                # 5.8.x
                push @files, (map "$dir/$_.pl", qw( Exact Canonical ));
            }
            else {
                # 5.6.x
                $dir = 'unicode';
                @files = map "$dir/Is/$_->{name}", _glob_in_inc("$dir/Is")
                  or return;
                push @subdirs, 'In';
            }

            foreach my $subdir (@subdirs) {
                foreach (_glob_in_inc("$dir/$subdir")) {
                    push @files, "$dir/$subdir/$_->{name}";
                }
            }
            @files;
        }
    ],
    'charnames.pm' => [
        _find_in_inc('unicore/Name.pl') ? 'unicore/Name.pl' : 'unicode/Name.pl'
    ],
);

my $Keys = 'files|keys|recurse|rv|skip|first|execute|compile';
sub scan_deps {
    my %args = (
        rv => {},
        (@_ and $_[0] =~ /^(?:$Keys)$/o) ? @_ : (files => [@_], recurse => 1)
    );

    scan_deps_static(\%args);

    if ($args{execute} or $args{compile}) {
        scan_deps_runtime(
            rv      => $args{rv},
            files   => $args{files},
            execute => $args{execute},
            compile => $args{compile},
            skip    => $args{skip}
        );
    }

    return ($args{rv});
}

sub scan_deps_static {
    my ($args) = @_;
    my ($files, $keys, $recurse, $rv, $skip, $first, $execute, $compile) =
      @$args{qw( files keys recurse rv skip first execute compile )};

    $rv   ||= {};
    $skip ||= {};

    foreach my $file (@{$files}) {
        my $key = shift @{$keys};
        next if $skip->{$file}++;
        next if is_insensitive_fs()
          and $file ne lc($file) and $skip->{lc($file)}++;

        local *FH;
        open FH, $file or die "Cannot open $file: $!";

        $SeenTk = 0;

        # Line-by-line scanning
        LINE:
        while (<FH>) {
            chomp(my $line = $_);
            foreach my $pm (scan_line($line)) {
                last LINE if $pm eq '__END__';

                if ($pm eq '__POD__') {
                    while (<FH>) { last if (/^=cut/) }
                    next LINE;
                }

                $pm = 'CGI/Apache.pm' if /^Apache(?:\.pm)$/;

                add_deps(
                    used_by => $key,
                    rv      => $rv,
                    modules => [$pm],
                    skip    => $skip
                );

                my $preload = $Preload{$pm} or next;
                if ($preload eq 'sub') {
                    $pm =~ s/\.p[mh]$//i;
                    $preload = [ _glob_in_inc($pm, 1) ];
                }
                elsif (UNIVERSAL::isa($preload, 'CODE')) {
                    $preload = [ $preload->($pm) ];
                }

                add_deps(
                    used_by => $key,
                    rv      => $rv,
                    modules => $preload,
                    skip    => $skip
                );
            }
        }
        close FH;

        # }}}
    }

    # Top-level recursion handling {{{
    while ($recurse) {
        my $count = keys %$rv;
        my @files = sort grep -T $_->{file}, values %$rv;
        scan_deps_static({
            files   => [ map $_->{file}, @files ],
            keys    => [ map $_->{key},  @files ],
            rv      => $rv,
            skip    => $skip,
            recurse => 0,
        }) or ($args->{_deep} and return);
        last if $count == keys %$rv;
    }

    # }}}

    return $rv;
}

sub scan_deps_runtime {
    my %args = (
        perl => $^X,
        rv   => {},
        (@_ and $_[0] =~ /^(?:$Keys)$/o) ? @_ : (files => [@_], recurse => 1)
    );
    my ($files, $rv, $execute, $compile, $skip, $perl) =
      @args{qw( files rv execute compile skip perl )};

    $files = (ref($files)) ? $files : [$files];

    my ($inchash, $incarray, $dl_shared_objects) = ({}, [], []);
    if ($compile) {
        my $file;

        foreach $file (@$files) {
            ($inchash, $dl_shared_objects, $incarray) = ({}, [], []);
            _compile($perl, $file, $inchash, $dl_shared_objects, $incarray);

            my $rv_sub = _make_rv($inchash, $dl_shared_objects, $incarray);
            _merge_rv($rv_sub, $rv);
        }
    }
    elsif ($execute) {
        my $excarray = (ref($execute)) ? $execute : [@$files];
        my $exc;
        my $first_flag = 1;
        foreach $exc (@$excarray) {
            ($inchash, $dl_shared_objects, $incarray) = ({}, [], []);
            _execute(
                $perl, $exc, $inchash, $dl_shared_objects, $incarray,
                $first_flag
            );
            $first_flag = 0;
        }

        my $rv_sub = _make_rv($inchash, $dl_shared_objects, $incarray);
        _merge_rv($rv_sub, $rv);
    }

    return ($rv);
}

sub scan_line {
    my $line = shift;
    my %found;

    return '__END__' if $line =~ /^__(?:END|DATA)__$/;
    return '__POD__' if $line =~ /^=\w/;

    $line =~ s/\s*#.*$//;
    $line =~ s/[\\\/]+/\//g;

    foreach (split(/;/, $line)) {
        if (/^\s*package\s+(\w+);/) {
            $CurrentPackage = $1;
            $CurrentPackage =~ s{::}{/}g;
            return;
        }
        return if /^\s*(use|require)\s+[\d\._]+/;

        if (my ($libs) = /\b(?:use\s+lib\s+|(?:unshift|push)\W+\@INC\W+)(.+)/)
        {
            my $archname =
              defined($Config{archname}) ? $Config{archname} : '';
            my $ver = defined($Config{version}) ? $Config{version} : '';
            foreach (grep(/\w/, split(/["';() ]/, $libs))) {
                unshift(@INC, "$_/$ver")           if -d "$_/$ver";
                unshift(@INC, "$_/$archname")      if -d "$_/$archname";
                unshift(@INC, "$_/$ver/$archname") if -d "$_/$ver/$archname";
            }
            next;
        }

        $found{$_}++ for scan_chunk($_);
    }

    return sort keys %found;
}

sub scan_chunk {
    my $chunk = shift;

    # Module name extraction heuristics {{{
    my $module = eval {
        $_ = $chunk;

        return [ 'base.pm',
            map { s{::}{/}g; "$_.pm" }
              grep { length and !/^q[qw]?$/ } split(/[^\w:]+/, $1) ]
          if /^\s* use \s+ base \s+ (.*)/sx;

        return [ 'Class/Autouse.pm',
            map { s{::}{/}g; "$_.pm" }
              grep { length and !/^:|^q[qw]?$/ } split(/[^\w:]+/, $1) ]
          if /^\s* use \s+ Class::Autouse \s+ (.*)/sx
              or /^\s* Class::Autouse \s* -> \s* autouse \s* (.*)/sx;

        return [ 'POE.pm',
            map { s{::}{/}g; "POE/$_.pm" }
              grep { length and !/^q[qw]?$/ } split(/[^\w:]+/, $1) ]
          if /^\s* use \s+ POE \s+ (.*)/sx;

        return [ 'encoding.pm',
            map { _find_encoding($_) }
              grep { length and !/^q[qw]?$/ } split(/[^\w:]+/, $1) ]
          if /^\s* use \s+ encoding \s+ (.*)/sx;

        return $1 if /(?:^|\s)(?:use|no|require)\s+([\w:\.\-\\\/\"\']+)/;
        return $1
          if /(?:^|\s)(?:use|no|require)\s+\(\s*([\w:\.\-\\\/\"\']+)\s*\)/;

        if (   s/(?:^|\s)eval\s+\"([^\"]+)\"/$1/
            or s/(?:^|\s)eval\s*\(\s*\"([^\"]+)\"\s*\)/$1/)
        {
            return $1 if /(?:^|\s)(?:use|no|require)\s+([\w:\.\-\\\/\"\']*)/;
        }

        return "File/Glob.pm" if /<[^>]*[^\$\w>][^>]*>/;
        return "DBD/$1.pm"    if /\b[Dd][Bb][Ii]:(\w+):/;
        if (/(?:(:encoding)|\b(?:en|de)code)\(\s*['"]?([-\w]+)/) {
            my $mod = _find_encoding($2);
            return [ 'PerlIO.pm', $mod ] if $1 and $mod;
            return $mod if $mod;
        }
        return $1 if /(?:^|\s)(?:do|require)\s+[^"]*"(.*?)"/;
        return $1 if /(?:^|\s)(?:do|require)\s+[^']*'(.*?)'/;
        return $1 if /[^\$]\b([\w:]+)->\w/ and $1 ne 'Tk';
        return $1 if /\b(\w[\w:]*)::\w+\(/;

        if ($SeenTk) {
            my @modules;
            while (/->\s*([A-Z]\w+)/g) {
                push @modules, "Tk/$1.pm";
            }
            while (/->\s*Scrolled\W+([A-Z]\w+)/g) {
                push @modules, "Tk/$1.pm";
                push @modules, "Tk/Scrollbar.pm";
            }
            return \@modules;
        }
        return;
    };

    # }}}

    return unless defined($module);
    return wantarray ? @$module : $module->[0] if ref($module);

    $module =~ s/^['"]//;
    return unless $module =~ /^\w/;

    $module =~ s/\W+$//;
    $module =~ s/::/\//g;
    return if $module =~ /^(?:[\d\._]+|'.*[^']|".*[^"])$/;

    $module .= ".pm" unless $module =~ /\./;
    return $module;
}

sub _find_encoding {
    return unless $] >= 5.008 and eval { require Encode; %Encode::ExtModule };

    my $mod = $Encode::ExtModule{ Encode::find_encoding($_[0])->name }
      or return;
    $mod =~ s{::}{/}g;
    return "$mod.pm";
}

sub _add_info {
    my ($rv, $module, $file, $used_by, $type) = @_;
    return unless defined($module) and defined($file);

    $rv->{$module} ||= {
        file => $file,
        key  => $module,
        type => $type,
    };

    push @{ $rv->{$module}{used_by} }, $used_by
      if defined($used_by)
      and $used_by ne $module
      and !grep { $_ eq $used_by } @{ $rv->{$module}{used_by} };
}

sub add_deps {
    my %args =
      ((@_ and $_[0] =~ /^(?:modules|rv|used_by)$/)
        ? @_
        : (rv => (ref($_[0]) ? shift(@_) : undef), modules => [@_]));

    my $rv   = $args{rv}   || {};
    my $skip = $args{skip} || {};
    my $used_by = $args{used_by};

    foreach my $module (@{ $args{modules} }) {
        next if exists $rv->{$module};

        my $file = _find_in_inc($module) or next;
        next if $skip->{$file};
        next if is_insensitive_fs() and $skip->{lc($file)};

        my $type = 'module';
        $type = 'data' unless $file =~ /\.p[mh]$/i;
        _add_info($rv, $module, $file, $used_by, $type);

        if ($module =~ /(.*?([^\/]*))\.p[mh]$/i) {
            my ($path, $basename) = ($1, $2);

            foreach (_glob_in_inc("auto/$path")) {
                next if $skip->{$_->{file}};
                next if is_insensitive_fs() and $skip->{lc($_->{file})};
                next if $_->{file} =~ m{\bauto/$path/.*/};  # weed out subdirs
                next if $_->{name} =~ m/(?:^|\/)\.(?:exists|packlist)$/;
                my $ext = lc($1) if $_->{name} =~ /(\.[^.]+)$/;
                next if $ext eq lc(lib_ext());
                my $type = 'shared' if $ext eq lc(dl_ext());
                $type = 'autoload' if $ext eq '.ix' or $ext eq '.al';
                $type ||= 'data';

                _add_info($rv, "auto/$path/$_->{name}", $_->{file}, $module,
                    $type);
            }
        }
    }

    return $rv;
}

sub _find_in_inc {
    my $file = shift;

    # absolute file names
    return $file if -f $file;

    foreach my $dir (grep !/\bBSDPAN\b/, @INC) {
        return "$dir/$file" if -f "$dir/$file";
    }
    return;
}

sub _glob_in_inc {
    my $subdir  = shift;
    my $pm_only = shift;
    my @files;

    require File::Find;

    foreach my $dir (map "$_/$subdir", grep !/\bBSDPAN\b/, @INC) {
        next unless -d $dir;
        File::Find::find({
                "wanted" => sub {
                    my $name = $File::Find::name;
                    $name =~ s!^\Q$dir\E/!!;
                    return if $pm_only and lc($name) !~ /\.p[mh]$/i;
                    push @files, $pm_only
                        ? "$subdir/$name"
                        : {             file => $File::Find::name,
                                        name => $name,
                                    }
                    if -f;
                },
                "untaint" => 1,
                "untaint_skip" => 1,
                "untaint_pattern" => qr|^([-+@\w./]+)$|
                }, $dir
        );
    }

    return @files;
}

# App::Packer compatibility functions

sub new {
    my ($class, $self) = @_;
    return bless($self ||= {}, $class);
}

sub set_file {
    my $self = shift;
    foreach my $script (@_) {
        my $basename = $script;
        $basename =~ s/.*\///;
        $self->{main} = {
            key  => $basename,
            file => $script,
        };
    }
}

sub set_options {
    my $self = shift;
    my %args = @_;
    foreach my $module (@{ $args{add_modules} }) {
        $module =~ s/::/\//g;
        $module .= '.pm' unless $module =~ /\.p[mh]$/i;
        my $file = _find_in_inc($module) or next;
        $self->{files}{$module} = $file;
    }
}

sub calculate_info {
    my $self = shift;
    my $rv   = scan_deps(
        keys  => [ $self->{main}{key}, sort keys %{ $self->{files} }, ],
        files => [ $self->{main}{file},
            map { $self->{files}{$_} } sort keys %{ $self->{files} },
        ],
        recurse => 1,
    );

    my $info = {
        main => {  file     => $self->{main}{file},
            store_as => $self->{main}{key},
        },
    };

    my %cache = ($self->{main}{key} => $info->{main});
    foreach my $key (sort keys %{ $self->{files} }) {
        my $file = $self->{files}{$key};

        $cache{$key} = $info->{modules}{$key} = {
            file     => $file,
            store_as => $key,
            used_by  => [ $self->{main}{key} ],
        };
    }

    foreach my $key (sort keys %{$rv}) {
        my $val = $rv->{$key};
        if ($cache{ $val->{key} }) {
            push @{ $info->{ $val->{type} }->{ $val->{key} }->{used_by} },
              @{ $val->{used_by} };
        }
        else {
            $cache{ $val->{key} } = $info->{ $val->{type} }->{ $val->{key} } =
              {        file     => $val->{file},
                store_as => $val->{key},
                used_by  => $val->{used_by},
              };
        }
    }

    $self->{info} = { main => $info->{main} };

    foreach my $type (sort keys %{$info}) {
        next if $type eq 'main';

        my @val;
        if (UNIVERSAL::isa($info->{$type}, 'HASH')) {
            foreach my $val (sort values %{ $info->{$type} }) {
                @{ $val->{used_by} } = map $cache{$_} || "!!$_!!",
                  @{ $val->{used_by} };
                push @val, $val;
            }
        }

        $type = 'modules' if $type eq 'module';
        $self->{info}{$type} = \@val;
    }
}

sub get_files {
    my $self = shift;
    return $self->{info};
}

# scan_deps_runtime utility functions

sub _compile {
    my ($perl, $file, $inchash, $dl_shared_objects, $incarray) = @_;

    my $fname = File::Temp::mktemp("$file.XXXXXX");
    my $fhin  = FileHandle->new($file) or die "Couldn't open $file\n";
    my $fhout = FileHandle->new("> $fname") or die "Couldn't open $fname\n";

    my $line = do { local $/; <$fhin> };
    $line =~ s/use Module::ScanDeps::DataFeed.*?\n//sg;
    $line =~ s/^(.*?)((?:[\r\n]+__(?:DATA|END)__[\r\n]+)|$)/
use Module::ScanDeps::DataFeed '$fname.out';
sub {
$1
}
$2/s;
    $fhout->print($line);
    $fhout->close;
    $fhin->close;

    system($perl, $fname);

    _extract_info("$fname.out", $inchash, $dl_shared_objects, $incarray);
    unlink("$fname");
    unlink("$fname.out");
}

sub _execute {
    my ($perl, $file, $inchash, $dl_shared_objects, $incarray, $firstflag) = @_;

    $DB::single = $DB::single = 1;

    my $fname = _abs_path(File::Temp::mktemp("$file.XXXXXX"));
    my $fhin  = FileHandle->new($file) or die "Couldn't open $file";
    my $fhout = FileHandle->new("> $fname") or die "Couldn't open $fname";

    my $line = do { local $/; <$fhin> };
    $line =~ s/use Module::ScanDeps::DataFeed.*?\n//sg;
    $line = "use Module::ScanDeps::DataFeed '$fname.out';\n" . $line;
    $fhout->print($line);
    $fhout->close;
    $fhin->close;

    File::Path::rmtree( ['_Inline'], 0, 1); # XXX hack
    system($perl, $fname) == 0 or die "SYSTEM ERROR in executing $file: $?";

    _extract_info("$fname.out", $inchash, $dl_shared_objects, $incarray);
    unlink("$fname");
    unlink("$fname.out");
}

sub _make_rv {
    my ($inchash, $dl_shared_objects, $inc_array) = @_;

    my $rv = {};
    my @newinc = map(quotemeta($_), @$inc_array);
    my $inc = join('|', sort { length($b) <=> length($a) } @newinc);

    require File::Spec;

    my $key;
    foreach $key (keys(%$inchash)) {
        my $newkey = $key;
        $newkey =~ s"^(?:(?:$inc)/?)""sg if File::Spec->file_name_is_absolute($newkey);

        $rv->{$newkey} = {
            'used_by' => [],
            'file'    => $inchash->{$key},
            'type'    => _gettype($inchash->{$key}),
            'key'     => $key
        };
    }

    my $dl_file;
    foreach $dl_file (@$dl_shared_objects) {
        my $key = $dl_file;
        $key =~ s"^(?:(?:$inc)/?)""s;

        $rv->{$key} = {
            'used_by' => [],
            'file'    => $dl_file,
            'type'    => 'shared',
            'key'     => $key
        };
    }

    return $rv;
}

sub _extract_info {
    my ($fname, $inchash, $dl_shared_objects, $incarray) = @_;

    use vars qw(%inchash @dl_shared_objects @incarray);
    my $fh = FileHandle->new($fname) or die "Couldn't open $fname";
    my $line = do { local $/; <$fh> };
    $fh->close;

    eval $line;

    $inchash->{$_} = $inchash{$_} for keys %inchash;
    @$dl_shared_objects = @dl_shared_objects;
    @$incarray          = @incarray;
}

sub _gettype {
    my $name = shift;
    my $dlext = quotemeta(dl_ext());

    return 'autoload' if $name =~ /(?:\.ix|\.al|\.bs)$/i;
    return 'module'   if $name =~ /\.p[mh]$/i;
    return 'shared'   if $name =~ /\.$dlext$/i;
    return 'data';
}

sub _merge_rv {
    my ($rv_sub, $rv) = @_;

    my $key;
    foreach $key (keys(%$rv_sub)) {
        my %mark;
        if ($rv->{$key} and _not_dup($key, $rv, $rv_sub)) {
            warn "different modules for file: $key: were found" .
                 "(using the version) after the '=>': ".
                 "$rv->{$key}{file} => $rv_sub->{$key}{file}\n";

            $rv->{$key}{used_by} = [
                grep (!$mark{$_}++,
                    @{ $rv->{$key}{used_by} },
                    @{ $rv_sub->{$key}{used_by} })
            ];
            @{ $rv->{$key}{used_by} } = grep length, @{ $rv->{$key}{used_by} };
            $rv->{$key}{file} = $rv_sub->{$key}{file};
        }
        elsif ($rv->{$key}) {
            $rv->{$key}{used_by} = [
                grep (!$mark{$_}++,
                    @{ $rv->{$key}{used_by} },
                    @{ $rv_sub->{$key}{used_by} })
            ];
            @{ $rv->{$key}{used_by} } = grep length, @{ $rv->{$key}{used_by} };
        }
        else {
            $rv->{$key} = {
                used_by => [ @{ $rv_sub->{$key}{used_by} } ],
                file    => $rv_sub->{$key}{file},
                key     => $rv_sub->{$key}{key},
                type    => $rv_sub->{$key}{type}
            };

            @{ $rv->{$key}{used_by} } = grep length, @{ $rv->{$key}{used_by} };
        }
    }
}

sub _not_dup {
    my ($key, $rv1, $rv2) = @_;
    (_abs_path($rv1->{$key}{file}) ne _abs_path($rv2->{$key}{file}));
}

sub _abs_path {
    return join(
        '/',
        Cwd::abs_path(File::Basename::dirname($_[0])),
        File::Basename::basename($_[0]),
    );
}

#####################################################
### Actual perldeps.pl code starts here.

# Print usage information
sub
print_usage_info($)
{
    my $code = shift || 0;
    my ($leader, $underbar);

    print "\n";
    $leader = "$0 Usage Information";
    $underbar = $leader;
    $underbar =~ s/./-/g;
    print "$leader\n$underbar\n";
    print "\n";
    print "  Syntax:   $0 [ options ] [ path(s)/file(s) ]\n";
    print "\n";
    print "    -h --help                        Show this usage information\n";
    print "    -v --version                     Show version and copyright\n";
    print "    -d --debug                       Turn on debugging\n";
    print "    -p --provides                    Find things provided by path(s)/file(s)\n";
    print "    -r --requires                    Find things required by path(s)/file(s)\n";
    #print "                                     \n";
    print "\nNOTE:  Path(s)/file(s) can also be specified on STDIN.  Default is \@INC.\n\n";
    exit($code);
}

# Locate perl modules (*.pm) in given locations.
sub
find_perl_modules(@)
{
    my @locations = @_;
    my %modules;

    foreach my $loc (@locations) {
        if (-f $loc) {
            # It's a file.  Assume it's a Perl module.
            #print "Found module:  $loc.\n";
            $modules{$loc} = 1;
        } elsif (-d $loc) {
            my @tmp;

            # Recurse the directory tree looking for all modules inside it.
            &File::Find::find({
                "wanted" => sub {
                    if ((-s _) && (substr($File::Find::fullname, -3, 3) eq ".pm")) {
                        push @tmp, $File::Find::fullname;
                    }
                },
                "follow_fast" => 1,
                "no_chdir" => 1,
                "untaint" => 1,
                "untaint_skip" => 1,
                "untaint_pattern" => qr|^([-+@\w./]+)$|
                }, $loc);

            # @tmp is now a list with all non-empty *.pm files in and under $loc.
            # Untaint and save in %modules hash.
            foreach my $module (@tmp) {
                if ($module =~ /^([-+@\w.\/]+)$/) {
                    $modules{$1} = 1;
                    #print "Found module:  $1\n";
                }
            }
        } else {
            # Something wicked this way comes.
            print STDERR "$0:  Error:  Don't know what to do with location \"$loc\"\n";
        }
    }
    return keys(%modules);
}

# Generate an RPM-style "Provides:" list for the given modules.
sub
find_provides(@)
{
    my @modules = @_;
    my @prov;

    foreach my $mod (@modules) {
        my (@contents, @pkgs);
        my $mod_path;
        local *MOD;

        $mod_path = dirname($mod);
        if (!open(MOD, $mod)) {
            warn "Unable to read module $mod -- $!\n";
            next;
        }
        @contents = <MOD>;
        if (!close(MOD)) {
            warn "Unable to close module $mod -- $!\n";
        }

        if (!scalar(grep { $_ eq $mod_path } @INC)) {
            push @INC, $mod_path;
        }
        foreach my $line (grep { $_ =~ /^\s*package\s+/ } @contents) {
            if ($line =~ /^\s*package\s+([^\;\s]+)\s*\;/) {
                push @pkgs, $1;
            }
        }

        # Now we have a list of packages.  Load up the modules and get their versions.
        foreach my $pkg (@pkgs) {
            my $ret;
            local ($SIG{"__WARN__"}, $SIG{"__DIE__"});

            # Make sure eval() can't display warnings/errors.
            $SIG{"__DIE__"} = $SIG{"__WARN__"} = sub {0;};
            $ret = eval("no strict ('vars', 'subs', 'refs'); use $pkg (); return $pkg->VERSION || 0.0;");
            if ($@) {
                dprint "Unable to parse version number from $pkg -- $@.  Assuming 0.\n";
                $ret = 0;
            }

            if (! $ret) {
                $ret = 0;
            }
            push @prov, "perl($pkg) = $ret";
        }
    }
    printf("Provides:  %s\n", join(", ", sort(@prov)));
}

# Generate an RPM-style "Requires:" list for the given modules.
sub
find_requires(@)
{
    my @modules = @_;
    my @reqs;
    my $reqs;

    $reqs = &scan_deps("files" => \@modules, "recurse" => 0);
    foreach my $key (grep { $reqs->{$_}{"type"} eq "module" } sort(keys(%{$reqs}))) {
        if (substr($key, -3, 3) eq ".pm") {
            $key = substr($key, 0, -3);
        }
        $key =~ s!/!::!g;
        push @reqs, "perl($key)";
    }
    printf("Requires:  %s\n", join(", ", @reqs));
}

sub
main()
{
    my $VERSION = '1.0';
    my (@locations, @modules);
    my %OPTION;

    # For taint checks
    delete @ENV{("IFS", "CDPATH", "ENV", "BASH_ENV")};
    $ENV{"PATH"} = "/bin:/usr/bin:/sbin:/usr/sbin:/etc:/usr/ucb";
    foreach my $shell ("/bin/bash", "/usr/bin/ksh", "/bin/ksh", "/bin/sh", "/sbin/sh") {
        if (-f $shell) {
            $ENV{"SHELL"} = $shell;
            last;
        }
    }

    $ENV{"LANG"} = "C" if (! $ENV{"LANG"});
    umask 022;
    select STDERR; $| = 1;
    select STDOUT; $| = 1;

    Getopt::Long::Configure("no_getopt_compat", "bundling", "no_ignore_case");
    Getopt::Long::GetOptions(\%OPTION, "debug|d!", "help|h", "version|v", "provides|p", "requires|r");

    # Post-parse the options stuff
    select STDOUT; $| = 1;
    if ($OPTION{"version"}) {
        # Do not edit this variable.  It is updated automatically by CVS when you commit
        my $rcs_info = 'CVS Revision $Revision: 1.6 $ created on $Date: 2006/04/04 20:12:03 $ by $Author: mej $ ';

        $rcs_info =~ s/\$\s*Revision: (\S+) \$/$1/;
        $rcs_info =~ s/\$\s*Date: (\S+) (\S+) \$/$1 at $2/;
        $rcs_info =~ s/\$\s*Author: (\S+) \$ /$1/;
        print "\n";
	print "perldeps.pl $VERSION by Michael Jennings <mej\@eterm.org>\n";
        print "Copyright (c) 2005-2006, Michael Jennings\n";
        print "  ($rcs_info)\n";
        print "\n";
	return 0;
    } elsif ($OPTION{"help"}) {
	&print_usage_info(0);   # Never returns
    }

    push @locations, @ARGV;
    if (!scalar(@ARGV) && !(-t STDIN)) {
        @locations = <STDIN>;
    }
    if (!scalar(@locations)) {
        @locations = @INC;
    }

    if (!($OPTION{"provides"} || $OPTION{"requires"})) {
	&print_usage_info(-1);   # Never returns
    }

    # Catch bogus warning messages like "A thread exited while 2 threads were running"
    $SIG{"__DIE__"} = $SIG{"__WARN__"} = sub {0;};

    @modules = &find_perl_modules(@locations);
    if ($OPTION{"provides"}) {
        &find_provides(@modules);
    }
    if ($OPTION{"requires"}) {
        &find_requires(@modules);
    }
    return 0;
}

exit &main();
