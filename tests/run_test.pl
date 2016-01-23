#!/usr/bin/perl

use strict;
use warnings;
use threads;
use threads::shared;

use Getopt::Long;
use File::Basename;
use File::Find;
use File::Spec;
use File::Copy::Recursive qw(fcopy rcopy dircopy);
use File::Which;
use Tie::File;
use Cwd;
use Cwd 'abs_path';
use Term::ANSIColor;
use Data::Dumper;

sub help {
    my $msg = << 'EOS';

    The script run_test.pl is a test runner that work in conjunction with ps2autotests (https://github.com/unknownbrackets/ps2autotests)

    Mandatory Option
        --exe <STRING>          : the PCSX2 binary that you want to test
        --cfg <STRING>          : a path to the a default ini configuration of PCSX2
        --suite <STRING>        : a path to ps2autotests test (root directory)

    Optional Option
        --cpu=1                 : the number of parallel tests launched. Might create additional issue
        --timeout=20            : a global timeout for hang tests
        --show_diff             : show debug information

        --test=<REGEXP>         : filter test based on their names
        --regression            : blacklist test that are known to be broken
        --option <KEY>=<VAL>    : overload PCSX2 configuration option

        --debug_me              : print script info
        --dry_run               : don't launch PCSX2

    PCSX2 option
        EnableEE=disabled                 : Use EE interpreter
        EnableIOP=disabled                : Use IOP interpreter
        EnableVU0=disabled                : Use VU0 interpreter
        EnableVU1=disabled                : Use VU1 interpreter
        FPU.Roundmode=3                   : EE FPU round mode
        VU.Roundmode=3                    : VU round mode

EOS
    print $msg;

    exit
}

my $mt_timeout :shared;
my ($o_suite, $o_help, $o_exe, $o_cfg, $o_max_cpu, $o_timeout, $o_show_diff, $o_debug_me, $o_test_name, $o_regression, $o_dry_run, %o_pcsx2_opt, $o_cygwin);

# default value
$o_cygwin = 0;
$o_max_cpu = 1;
$o_timeout = 20;
$o_help = 0;
$o_debug_me = 0;
$o_dry_run = 0;
$o_test_name = ".*";
$o_exe = File::Spec->catfile("bin", "PCSX2");
if (exists $ENV{"PS2_AUTOTESTS_ROOT"}) {
    $o_suite = $ENV{"PS2_AUTOTESTS_ROOT"};
}
if (exists $ENV{"PS2_AUTOTESTS_CFG"}) {
    $o_cfg = $ENV{"PS2_AUTOTESTS_CFG"};
}

my $status = Getopt::Long::GetOptions(
    'cfg=s'         => \$o_cfg,
    'cpu=i'         => \$o_max_cpu,
    'debug_me'      => \$o_debug_me,
    'dry_run'       => \$o_dry_run,
    'exe=s'         => \$o_exe,
    'help'          => \$o_help,
    'option=s'      => \%o_pcsx2_opt,
    'regression'    => \$o_regression,
    'testname=s'    => \$o_test_name,
    'timeout=i'     => \$o_timeout,
    'show_diff'     => \$o_show_diff,
    'suite=s'       => \$o_suite,
);

#####################################################
# Check option
#####################################################
if (not $status or $o_help) {
    help();
}

unless (defined $o_suite) {
    print "Error: require a test suite directory\n";
    print "Note: you could use either use --suite or the env variable \$PS2_AUTOTESTS_ROOT\n";
    help();
}

unless (defined $o_cfg) {
    print "Error: require a default cfg directory\n";
    print "Note: you could use either use --cfg or the env variable \$PS2_AUTOTESTS_CFG\n";
    help();
}

$o_exe = abs_path($o_exe);
$o_cfg = abs_path($o_cfg);
$o_suite = abs_path($o_suite);
$mt_timeout = $o_timeout;

unless (-d $o_suite) {
    print "Error: --suite option requires a directory\n";
    help();
}

unless (-x $o_exe) {
    print "Error: --exe option requires an executable\n";
    help();
}

unless (-d $o_cfg) {
    print "Error: --cfg option requires a directory\n";
    help();
}

if (defined(which("cygpath"))) {
    $o_cygwin = 1;
}

if ($o_cygwin) {
    $o_cfg = `cygpath -w $o_cfg`;
}

my %blacklist;
if (defined $o_regression) {
    # Blacklist bad test
    $blacklist{"branchdelay"} = 1;
    $blacklist{"arithmetic"} = 1;
    $blacklist{"branchdelay"} = 1;
    $blacklist{"compare"} = 1;
    $blacklist{"fcr"} = 1;
    $blacklist{"muldiv"} = 1;
    $blacklist{"sqrt"} = 1;
    $blacklist{"chain"} = 1;
    $blacklist{"interleave"} = 1;
    $blacklist{"normal"} = 1;
    $blacklist{"mode"} = 1;
    $blacklist{"stcycl"} = 1;
    $blacklist{"triace"} = 1;
}

#####################################################
# Run
#####################################################

# Round 1: Collect the tests
my $g_test_db;
print "INFO: search tests in $o_suite and run them in $o_max_cpu CPU)\n";
find({ wanted => \&add_test_cmd_for_elf, no_chdir => 1 },  $o_suite);
print "\n";

# Round 2: Run the tests (later in thread)
foreach my $test (keys(%$g_test_db)) {
    # wait free CPU slot
    while( scalar(threads->list() >= $o_max_cpu) ) {
        if (close_joinnable_threads() == 0) {
            sleep(1); # test are often fast so 1s is more than enough
            $mt_timeout--;
        }
        kill_thread_if_timeout()
    }

    create_thread($test);
}
wait_all_threads();

# Round 3: Collect the results (not thread safe)
collect_result();

# Pretty print
print "\n\n Status | ===========================  Test ================================\n";
foreach my $test (sort(keys(%$g_test_db))) {
    my $info = $g_test_db->{$test};
    my $cfg = $info->{"CFG_DIR"};
    my $out = $info->{"OUT"};
    my $exp = $info->{"EXPECTED"};

    if ($info->{"STATUS"} eq "OK") {
        print color('bold green');
        print "   OK   | $test\n";
    } else {
        if ($info->{"STATUS"} eq "T") {
            print color('bold blue');
            print "  Tout  | $test\n";
        } else {
            print color('bold red');
            print "   KO   | $test\n";
        }
        if ($o_show_diff) {
            print color('bold magenta'); print "-----------------------------------------------------------------------\n"; print color('reset');
            print test_cmd($test, $cfg) . "\n\n";
            diff($exp, $out, 0);
            print color('bold magenta'); print "-----------------------------------------------------------------------\n"; print color('reset');
        }
    }
}
print color('reset');
print "\n";

#####################################################
# Sub helper
#####################################################
sub collect_result {
    foreach my $test (keys(%$g_test_db)) {
        my $info = $g_test_db->{$test};
        my $out = $info->{"OUT"};
        my $exp = $info->{"EXPECTED"};

        $info->{"STATUS"} = diff($exp, $out, 1); # potentially not thread safe
    }
}

sub add_test_cmd_for_elf {
    my $file = $_;
    my $ext = "\\.(elf|irx)";

    return 0 unless ($file =~ /$ext/);
    return 0 unless ($file =~ /$o_test_name/i);

    my($test, $dir_, $suffix) = fileparse($file, qw/.elf .irx/);
    return 0 if (exists $blacklist{$test});
    # Fast test
    #return 0 unless ($file =~ /branchdelay/);

    my $dir = $File::Find::dir;
    print "INFO: found test $test in $dir\n" if $o_debug_me or $o_dry_run;

    $g_test_db->{$File::Find::name}->{"CFG_DIR"} = $File::Find::name =~ s/$ext/_cfg/r;
    $g_test_db->{$File::Find::name}->{"EXPECTED"} = $File::Find::name =~ s/$ext/.expected/r;
    $g_test_db->{$File::Find::name}->{"OUT"} = $File::Find::name =~ s/$ext/.PCSX2.out/r;
    $g_test_db->{$File::Find::name}->{"STATUS"} = "T";

    return 1;
}

sub run_thread {
    my $test = shift;
    my $info = $g_test_db->{$test};

    generate_cfg($info->{"CFG_DIR"});

    run_elf($test, $info->{"CFG_DIR"}, $info->{"OUT"});
}

sub generate_cfg {
    my $out_dir = shift;

    print "Info: Copy dir $o_cfg to $out_dir\n" if $o_debug_me;
    local $File::Copy::Recursive::RMTrgDir = 2;
    dircopy($o_cfg, $out_dir) or die "Failed to copy directory: $!\n";

    # Enable the logging to get the trace log
    my $ui_ini = File::Spec->catfile($out_dir, "PCSX2_ui.ini");
    my $vm_ini = File::Spec->catfile($out_dir, "PCSX2_vm.ini");

    my %sed;
    # Enable logging for test
    $sed{".EEout"} = "enabled";
    $sed{".IOPout"} = "enabled";
    $sed{"ConsoleToStdio"} = "enabled";
    # FIXME add interpreter vs recompiler
    # FIXME add clamping / rounding option
    # FIXME need separate cfg dir !
    foreach my $k (keys(%o_pcsx2_opt)) {
        my $v = $o_pcsx2_opt{$k};
        $sed{$k} = $v;
    }

    tie my @ui, 'Tie::File', File::Spec->catfile($out_dir, "PCSX2_ui.ini") or die "Fail to tie PCSX2_ui.ini $!\n";
    tie my @vm, 'Tie::File', File::Spec->catfile($out_dir, "PCSX2_vm.ini") or die "Fail to tie PCSX2_vm.ini $!\n";

    for (@ui) {
        foreach my $option (keys(%sed)) {
            my $v = $sed{$option};
            s/$option=.*/$option=$v/;
        }
    }
    for (@vm) {
        foreach my $option (keys(%sed)) {
            my $v = $sed{$option};
            s/$option=.*/$option=$v/;
        }
    }

    untie @ui;
    untie @vm;
}

sub run_elf {
    my $elf = shift;
    my $cfg = shift;
    my $out = shift;

    return if $o_dry_run; # Not real

    my $line;
    my $dump = 0;
    open(my $run, ">$out") or die "Impossible to open $!";

    my $command = test_cmd($elf, $cfg);
    my $pid = open(my $log, "$command |") or die "Impossible to pipe $!";
    print "INFO:  Execute $elf (PID=$pid) with cfg ($cfg)\n" if $o_debug_me;

    # Kill me
    $SIG{'KILL'} = sub {
        # FIXME doesn't work (no print, neither kill)
        print "ERROR: timeout detected on pid $pid.\n";
        kill 'KILL', $pid;
        threads->exit();
    };

    while ($line = <$log>) {
        $mt_timeout = $o_timeout; # Keep me alive

        $line =~ s/\e\[\d+(?>(;\d+)*)m//g;
        if ($line =~ /-- TEST BEGIN/) {
            $dump = 1;
        }
        if ($dump == 1) {
            print $run $line;
        }
        if ($line =~ /-- TEST END/) {
            $dump = 0;
            print "INFO: kill process $pid\n" if $o_debug_me;
            kill 'KILL', $pid;
        }
    }
}

sub test_cmd {
    my $elf = shift;
    my $cfg = shift;

    if ($o_cygwin) {
        $elf = `cygpath -w $elf`;
        $cfg = `cygpath -w $cfg`;
    }

    if ($elf =~ /\.elf/) {
        return "$o_exe --elf $elf --cfgpath=$cfg"
    } else {
        return "$o_exe --irx $elf --cfgpath=$cfg"
    }
}

sub diff {
    my $ref_ = shift;
    my $out_ = shift;
    my $quiet = shift;

    open (my $ref_h, "<$ref_");
    my @ref = <$ref_h>;

    open (my $out_h, "<$out_") or return "T";
    my @out = <$out_h>;

    return "T" if (scalar(@out) < 2);
    return "T" if ($out[-1] !~ /-- TEST END/);
    return "KO" if ((scalar(@out) != scalar(@ref)) and $quiet);

    my $status = "OK";
    for (my $l = 0; $l < scalar(@ref); $l++) {
        if (chomp($ref[$l]) ne chomp($out[$l])) {
            $status = "KO";
            if ($o_show_diff and not $quiet) {
                print "EXPECTED: $ref[$l]";
                print "BUT GOT : $out[$l]";
            }
        }
    }
    return $status;
}

#####################################################
# Thread management
#####################################################
my $g_counter = 0;
sub create_thread {
    my $cmd = shift;

    my $thr = threads->create(\&run_thread, $cmd );

    $g_counter++;
}

sub close_joinnable_threads {
    my $closed = 0;
    foreach my $thr (threads->list(threads::joinable)) {
        $thr->join();
        $closed = 1;
        $g_counter--;
    }

    return $closed;
}

sub wait_all_threads {
    # wait free CPU slot
    while( scalar(threads->list() > 0) and $mt_timeout > 0) {
        if (close_joinnable_threads() == 0) {
            sleep(1); # test are often fast so 1s is more than enough
            $mt_timeout--;
        }
    }

    kill_thread_if_timeout()
}

sub kill_thread_if_timeout {
    if ($mt_timeout <= 0) {
        foreach my $thr (threads->list()) {
            # Farewell my friend
            print "ERROR: send kill on timeout process\n";
            $thr->kill('KILL')->detach();
        }
        $mt_timeout = 100;
    }
}
