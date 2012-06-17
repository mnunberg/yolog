#!/usr/bin/perl

my @LEVELS = qw(debug info warn error crit);
my $indent = " " x 4;

package Yolog::Subsystem;
use Class::Struct;
use strict;
use warnings;

struct __PACKAGE__, [
    name => '$',
    constant => '$'
];

package Yolog::Project;
use Class::Struct;
use strict;
use warnings;
use File::Path qw(mkpath);
use Getopt::Long;

struct __PACKAGE__, [
    "name" => '$',
    "macro_prefix" => '$',
    "subsystems" => '@',
    'c89_strict' => '$',
    'yolog_static' => '$',
    'yologns' => '$',
    
    'header_buf' => '$',
    'source_buf' => '$',
    
    'var_implicit_begin' => '$',
    'var_implicit_end' => '$',
    'var_implicit_log' => '$',
    
    
    'var_logfunc' => '$',
    'var_macro_prefix' => '$',
    'var_ctxarray' => '$',
    'var_ctxtype' => '$',
    
    'var_proj_initfunc' => '$',
    'var_yolog_initfunc' => '$',
    'var_proj_infofunc' => '$',
    'var_proj_countfunc' => '$',
    
    'env_color' => '$',
    'env_debug' => '$',
    'outdir'    => '$',
    'yolog_srcdir' => '$',
];

my @FNAMES = qw(
    implicit_begin
    implicit_end
    implicit_log
    
    logfunc
    proj_initfunc
    proj_infofunc
    proj_countfunc
    
    yolog_initfunc
    
    ctxtype
    ctxpfix
    ctxcount
    ctxarray
);


sub create {
    my ($cls,%opts) = @_;
    $opts{var_logfunc} ||= "<YOLOGNS>_logger";
    $opts{var_ctxtype} ||= "<YOLOGNS>_context";
    
    $opts{var_ctxarray} = "<PROJNS>_logging_contexts";
        
    $opts{var_proj_infofunc}    = "<PROJNS>_subsys_info";
    $opts{var_proj_countfunc}   = "<PROJNS>_subsys_count";
    $opts{var_implicit_begin}   = "<YOLOGNS>_implicit_begin";
    $opts{var_implicit_end}     = "<YOLOGNS>_implicit_end";
    $opts{var_implicit_log}     = "<YOLOGNS>_implicit_logger";
    
    $opts{var_yolog_initfunc}   = "<YOLOGNS>_init_defaults";
    $opts{var_proj_initfunc}    = sprintf("%s_yolog_init", $opts{name});
    
    $opts{macro_prefix} ||= $opts{name};
    $opts{header_buf} = "";
    $opts{source_buf} = "";
    
    return $cls->new(%opts);
}


sub process_template {
    my ($self,$templ) = @_;
    foreach my $fname (@FNAMES) {
        my $re = "<$fname>";
        $re = qr/$re/;
        my $meth = "var_$fname";
        my $replacement = $self->$meth();
        $templ =~ s/$re/$replacement/g;
    }
    return $templ;
}

sub append_source {
    my ($self,$txt) = @_;
    $self->source_buf($self->source_buf . $txt);
}

sub append_header {
    my ($self,$txt) = @_;
    $self->header_buf($self->header_buf . $txt);
}


sub process_namespace {
    my ($self,$buf) = @_;
    
    my $nslc = $self->yologns;
    my $nsuc;
    if ($nslc) {
        $nsuc = uc($nslc);
    }
    
    $nslc ||= "yolog";
    $nsuc ||= "YOLOG";
    my $pns_lc = $self->name . "_yolog";
    my $pns_uc = uc($pns_lc);
    
    
    $buf =~ s/<YOLOGNS>/$nslc/g;
    $buf =~ s/<YOLOGNS_UC>/$nsuc/g;
    $buf =~ s/<PROJNS>/$pns_lc/g;
    $buf =~ s/<PROJNS_UC>/$pns_uc/g;
    return $buf;
}

sub initfunc {
    my $self = shift;
    return sprintf("<PROJNS>_init", $self->name);
}


sub ctxpfix {
    my $self = shift;
    return "<PROJNS_UC>_LOGGING_SUBSYS";
}

{
    no warnings 'once';
    *var_ctxpfix = *ctxpfix;
}

sub var_ctxcount {
    my $self = shift;
    return $self->ctxpfix . "__COUNT";
}

sub add_subsys_name {
    my ($self,$name) = @_;
    my $o = Yolog::Subsystem->new(
        name => $name
    );
    my $cname = sprintf("%s_%s",
                        $self->ctxpfix,
                        uc($o->name));
    
    $o->constant($cname);
    push @{$self->subsystems}, $o;
}

sub gen_macro_name {
    my ($self,$subsys_prefix,$level) = @_;
    my @comps = ($self->macro_prefix,$subsys_prefix,lc($level));
    @comps = grep $_, @comps;
    return join("_", @comps);
}

sub _gen_c89_logger {
    my ($self,$prefix,$ctxvar,$level) = @_;
    
    my $macroname = $self->gen_macro_name($level);
    my $const_level = '<YOLOGNS_UC>_' . uc($level);
    
    my $templ = <<'EOF';

#define STUBMACRO(args) \
if (<implicit_begin>( \
    YO__CTX__, \
    YO__LEVEL__, \
    __FILE__, \
    __LINE__, \
    __func__)) \
{ \
    <implicit_log> args; \
    <implicit_end>(); \
}
EOF
    $templ =~ s/YO__CTX__/$ctxvar/g;    
    $templ =~ s/YO__LEVEL__/$const_level/g;
    $templ =~ s/STUBMACRO/$macroname/g;
    
    $self->append_header($self->process_template($templ));}

sub _gen_c99_logger {
    my ($self,$prefix,$ctxvar,$level) = @_;
    
    my $macro_name = $self->gen_macro_name($prefix, $level);
    my $level_const = sprintf("<YOLOGNS_UC>_%s", uc($level));
    
    my $templ = <<'EOF';

#define STUBMACRO(...) \
<logfunc>(\
    YO__CTX__,\
    YO__LEVEL__, \
    __FILE__, \
    __LINE__, \
    __func__, \
    ## __VA_ARGS__)
EOF
        
    $templ =~ s/YO__CTX__/$ctxvar/g;
    $templ =~ s/YO__LEVEL__/$level_const/;
    $templ =~ s/STUBMACRO/$macro_name/g;
    
    $self->append_header($self->process_template($templ));
}

sub generate_log_macros {
    my ($self,$subsys) = @_;
    my $ctxvar;
    
    my $prefix;
    
    if ($subsys) {
        $ctxvar = sprintf("%s + %s",
                          $self->var_ctxarray,
                          $subsys->constant);
        $prefix = $subsys->name;
    } else {
        $ctxvar = "NULL";
        $subsys = "";
    }
    
    foreach my $level (@LEVELS) {
        
        # if we're using strict C89 mode, then we can't do __VA_ARGS__, and must
        # define an actual symbol for this :/
        if ($self->c89_strict) {
            $self->_gen_c89_logger($prefix, $ctxvar, $level);
        } else {
            $self->_gen_c99_logger($prefix, $ctxvar, $level);
        }
    }
}

sub generate_ctx_offsets {
    my $self = shift;
    $self->append_header(<<"EOF");

/** These macros define the subsystems for logging **/

EOF
    my $counter = 0;
    my $buf = "";
    foreach my $sys (@{$self->subsystems}) {
        $buf .= sprintf("#define %-40s %d\n", $sys->constant, $counter);
        $counter++;
    }
    
    $buf .= sprintf("#define %-40s %d\n", $self->var_ctxcount, $counter);
    $buf .= "\n\n";
    $self->append_header($buf);
}


sub generate_cproto {
    my $self = shift;
    
    my $templ = <<'EOF';


/** Array of context object for each of our subsystems **/
extern <ctxtype>* <ctxarray>;

/** Function called to initialize the logging subsystem **/

void
<proj_initfunc>(void);


/** Macro to retrieve information about a specific subsystem **/

#define <PROJNS>_subsys_ctx(subsys) \
    (<ctxarray> + <ctxpfix>_ ## subsys)

#define <PROJNS>_subsys_count() (<ctxcount>)

EOF

    $templ = $self->process_template($templ);
    $self->append_header($templ);
}

sub generate_cbody {
    my $self = shift;
    
    my $templ = <<'EOF';
    
static <ctxtype> <ctxarray>_real[
    <ctxcount>
] = { { 0 } };

<ctxtype>* <ctxarray> = <ctxarray>_real;

void
<proj_initfunc>(void)
{

    <ctxtype>* ctx;

EOF

    foreach my $sys (@{$self->subsystems}) {
        my $prefix = $sys->name;
        my $offset = $sys->constant;
        
        $templ .= <<"EOF";
   
   ctx = <ctxarray> + $offset;
   ctx->prefix = "$prefix";
EOF
    }
    
    
    my $env_color = sprintf("%s_DEBUG_COLOR", uc $self->name);
    my $env_level = sprintf("%s_DEBUG_LEVEL", uc $self->name);

    $templ .= <<"EOF";
   
   <yolog_initfunc>(
        <ctxarray>,
        <ctxcount>,
        <YOLOGNS_UC>_DEFAULT,
        <YOLOGNS_UC>_FLAGS_DEFAULT,
        "$env_color",
        "$env_level"
    );
    
}   
EOF

    $templ = $self->process_template($templ);
    $self->append_source($templ);
}

my $help = <<'EOH';

-c  --config        Configuration File
-P  --print-only    Only print to screen, don't actually write the files
-p  --prefix        Symbol prefix to use
-s  --subsys        Subsystems. This may be specified more than once
-m  --macros-only   Only print the macros
-S  --static        Configure for static/embedded building
-o  --outdir        Output directory
    --c89           Enforce C89 compliant macros
-y  --yolog-dir     Directory for Yolog Source (for embedding)
EOH

Getopt::Long::Configure("no_ignore_case");
GetOptions(
    'c|config=s' => \my $ConfigFile,
    'P|print-only' => \my $PrintOnly,
    'p|prefix=s' => \my $Prefix,
    's|subsys=s@' =>\my @Subsystems,
    'S|static' => \my $UseStatic,
    'm|macros-only' => \my $MacrosOnly,
    'o|outdir=s' => \my $OutDir,
    'y|yolog-dir=s' => \my $YologDir,
    'c89' => \my $C89Mode,
) or die $help;


my %ConfMap = (
    'env_color' => ['env_color', undef],
    'env_debug' => ['env_debug', undef],
    'c89_strict' => ['c89', \$C89Mode],
    'name' => ['symbol_prefix', \$Prefix],
    'macro_prefix' => ['macro_prefix', undef],
    'outdir' => ['outdir', \$OutDir],
    'yolog_srcdir' => ['yolog_src', \$YologDir],
    'yolog_static' => ['static', \$UseStatic]
);

my %confhash;
my %ctor_opts;

if ($ConfigFile) {
    open my $fh, "<", $ConfigFile or die "$ConfigFile: $!";
    while ((my $line = <$fh>)) {
        next if $line =~ /#|;/;
        my ($k,$v) = split(' ', $line);
        next unless defined $v;
        
        chomp($v);
        if ($k eq 'subsys') {
            push @Subsystems, $v;
            next;
        }
        $confhash{$k} = $v;
    }
}

while (my ($k,$v) = each %ConfMap) {
    my ($ckey,$cliref) = @$v;
    if ($cliref && defined $$cliref) {
        $ctor_opts{$k} = $$cliref;
    } else {
        $ctor_opts{$k} = $confhash{$ckey};
    }
}

$ctor_opts{name} ||= "yolog";

if ($ctor_opts{yolog_static}) {
    if (!$YologDir) {
        die "Can't generate embedded source without Yolog source directory";
    }
    $ctor_opts{yologns} ||= $ctor_opts{name} . '_yolog';
}

my $Project = Yolog::Project->create(%ctor_opts);
{
    my %shtmp = map {$_,$_} @Subsystems;
    @Subsystems = keys %shtmp;
}

$Project->add_subsys_name($_) foreach @Subsystems;

# now define a macro for initializing the actual contexts..
$Project->generate_ctx_offsets();
$Project->generate_cproto();
$Project->generate_cbody();
$Project->generate_log_macros();
$Project->generate_log_macros($_) foreach @{$Project->subsystems};

if (!$Project->outdir) {
    warn "Output directory not specified. Forcing print-only";
    $PrintOnly = 1;
    $Project->outdir("");
}

if (!$PrintOnly) {
    mkpath($Project->outdir);
}

## If we're using static mode, then we need to copy over the entire yolog
## source code.

my $final_src = "";
my $final_hdr = "";


sub preprocess_file {
    my $fname = shift;
    open my $fh, "<", $fname or die "$fname: $!";
    local $/ = undef;
    my $txt = <$fh>;
    $txt =~ s/YOLOG/<YOLOGNS_UC>/g;
    $txt =~ s/yolog/<YOLOGNS>/g;
    return $txt;
}

if ($Project->yolog_static) {
    $final_src = preprocess_file("src/yolog.c");
    $final_hdr = preprocess_file("src/yolog.h");
} else {
    $final_hdr = "#include <yolog.h>\n" . $final_hdr;
    $final_src = sprintf('#include "%s_yolog.h"',
                         $Project->name) . "\n" . $final_src;
}

$final_src .= $Project->source_buf;
$final_hdr .= $Project->header_buf;

foreach (\$final_hdr, \$final_src) {
    $$_ = $Project->process_namespace($$_);
}

foreach my $spec (["c", $final_src], ["h", $final_hdr]) {
    my $fname = sprintf("%s/%s_yolog.%s",
                        $Project->outdir,
                        $Project->name,
                        $spec->[0]);
    if ($PrintOnly) {
        print $spec->[1];
        next;
    }
    open my $fh, ">", $fname or die "$fname: $!";
    print $fh $spec->[1];
}