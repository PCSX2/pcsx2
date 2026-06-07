#!/usr/bin/perl

use strict;
use warnings;
use threads;
use threads::shared;

use Cwd;
use Getopt::Long;
use File::Basename;
use File::Find;
use File::Spec;
use File::Copy::Recursive qw(fcopy rcopy dircopy);
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
        --bad                   : only run blacklisted tests
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
        fpuExtraOverflow=enabled          : Full EE FPU
        fpuFullMode=enabled               : Full EE FPU

EOS
    print $msg;

    exit
}

my $mt_timeout :shared;
my ($o_suite, $o_help, $o_exe, $o_cfg, $o_max_cpu, $o_timeout, $o_show_diff, $o_debug_me, $o_test_name, $o_regression, $o_dry_run, %o_pcsx2_opt, $o_cygwin, $o_bad);

# default value
$o_bad = 0;
$o_regression = 0;
$o_cygwin = 0;
$o_max_cpu = 8;
$o_timeout = 30;
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
    'bad'           => \$o_bad,
    'cfg=s'         => \$o_cfg,
    'cpu=i'         => \$o_max_cpu,
    'cygwin'        => \$o_cygwin,
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

# Auto detect cygwin mess
if (-e "/cygdrive") {
    print "INFO: CYGWIN OS detected. Update path accordingly\n";
    $o_cygwin = 1;
}

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

# Default value if the dir exists
$o_cfg = "bin/inis" if (not defined $o_cfg and -d "bin/inis");

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

my %blacklist;
if ($o_regression or $o_bad) {
    # Blacklist bad test

    # FULL FPU rounding to avoid EE FPU test issu
    #$o_pcsx2_opt{"fpuExtraOverflow"} = "enabled";
    #$o_pcsx2_opt{"fpuFullMode"} = "enabled";

    # EE
    $blacklist{"branchdelay"} = 1;
    # EE fpu
    $blacklist{"arithmetic"} = 1;
    $blacklist{"branchdelay"} = 1;
    $blacklist{"compare"} = 1;
    $blacklist{"fcr"} = 1;
    $blacklist{"muldiv"} = 1;
    $blacklist{"sqrt"} = 1;
    # IOP
    $blacklist{"lsudelay"} = 1;
    # Kernel IOP
    $blacklist{"register"} = 1;
    $blacklist{"receive"} = 1;
    $blacklist{"stat"} = 1;
    $blacklist{"send"} = 1;
    # VU
    $blacklist{"triace"} = 1;
}

#####################################################
# Run
#####################################################

# Round 1: Collect the tests
my $cwd = getcwd();

my $g_test_db;
print "INFO: search tests in $o_suite and run them in $o_max_cpu CPU)\n";
find({ wanted => \&add_test_cmd_for_elf, no_chdir => 1 },  $o_suite);
print "\n";

chdir($cwd); # Just to be sure

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
            # Easy copy/past to rerun the test in gdb. Yes lazy guy detected :p
            print "gdb -ex=r --args " . test_cmd($test, $cfg) . "\n";
            print "vi -d $exp $out\n";
            print "\n";
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
sub cyg_abs_path {
    my $p = shift;
    if ($o_cygwin) {
        $p =~ s/\/cygdrive\/(\w)/$1:/;
    }
    return $p;
}

sub collect_result {
    foreach my $test (keys(%$g_test_db)) {
        my $info = $g_test_db->{$test};
        my $cfg = $info->{"CFG_DIR"};
        my $out = $info->{"OUT"};
        my $exp = $info->{"EXPECTED"};

        extract_test_log(File::Spec->catfile($cfg, "emuLog.txt"), $out);
        $info->{"STATUS"} = diff($exp, $out, 1); # potentially not thread safe
    }
}

sub extract_test_log {
    my $in = shift;
    my $out = shift;

    return unless (-e $in);

    open(my $emulog, "<$in");
    my @all_data = <$emulog>;

    open(my $short_log, ">$out") or die "Impossible to open $!";

    my $dump = 0;
    foreach my $line (@all_data) {
        # Remove color
        $line =~ s/\e\[\d+(?>(;\d+)*)m//g;

        if ($line =~ /-- TEST BEGIN/) {
            $dump = 1;
        }
        if ($dump == 1) {
            chomp($line);
            $line =~ s/\r$//g;

            print $short_log "$line\n";
        }
        if ($line =~ /-- TEST END/) {
            $dump = 0;
            last;
        }
    }
}

sub add_test_cmd_for_elf {
    my $file = $_;
    my $ext = "\\.(elf|irx)";

    return 0 unless ($file =~ /$ext/);
    return 0 unless ($file =~ /$o_test_name/i);

    my($test, $dir_, $suffix) = fileparse($file, qw/.elf .irx/);
    return 0 if ($o_regression and exists $blacklist{$test});
    return 0 if ($o_bad and not exists $blacklist{$test});
    # Fast test
    #return 0 unless ($file =~ /branchdelay/);

    my $dir = $File::Find::dir;
    print "INFO: found test $test in $dir\n" if $o_debug_me or $o_dry_run;

    $g_test_db->{$file}->{"CFG_DIR"}  = $file =~ s/$ext/_cfg/r;
    $g_test_db->{$file}->{"EXPECTED"} = $file =~ s/$ext/.expected/r;
    $g_test_db->{$file}->{"OUT"}      = $file =~ s/$ext/.PCSX2.out/r;
    $g_test_db->{$file}->{"STATUS"}   = "T";

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

    print "INFO: Copy dir $o_cfg to $out_dir\n" if $o_debug_me;
    local $File::Copy::Recursive::RMTrgDir = 2;
    dircopy($o_cfg, $out_dir) or die "Failed to copy directory: $!\n";

    my %sed;
    # Enable logging for test
    $sed{".EEout"}  = "enabled";
    $sed{".IOPout"} = "enabled";
    # Redirect log file in the unique cfg dir
    #$sed{"ConsoleToStdio"} = "enabled"; # was to redirect stdio (but windows...) # Still requires to force the flush
    $sed{"Logs"}           = cyg_abs_path($out_dir);
    $sed{"UseDefaultLogs"} = "disabled";

    # FIXME add interpreter vs recompiler
    # FIXME add clamping / rounding option
    # FIXME need separate cfg dir !
    foreach my $k (keys(%o_pcsx2_opt)) {
        my $v = $o_pcsx2_opt{$k};
        $sed{$k} = $v;
    }

    tie my @ui, 'Tie::File', File::Spec->catfile($out_dir, "PCSX2_ui.ini") or die "Fail to tie $!\n";
    for (@ui) {
        foreach my $option (keys(%sed)) {
            my $v = $sed{$option};
            s/$option=.*/$option=$v/;
        }
    }
    untie @ui;

    tie my @vm, 'Tie::File', File::Spec->catfile($out_dir, "PCSX2_vm.ini") or die "Fail to tie $!\n";
    for (@vm) {
        foreach my $option (keys(%sed)) {
            my $v = $sed{$option};
            s/$option=.*/$option=$v/;
        }
    }
    untie @vm;

    # Disable sound emulation (avoid spurious "ALSA lib pcm.c:7843:(snd_pcm_recover) underrun occurred")
    tie my @spu, 'Tie::File', File::Spec->catfile($out_dir, "SPU2.ini") or die "Fail to tie $!\n";
    for (@spu) {
        s/Output_Module=.*/Output_Module=nullout/;
    }
    untie @spu;

}

sub run_elf {
    my $elf = shift;
    my $cfg = shift;
    my $out = shift;

    ######################################################################
    # FORK test
    ######################################################################
    my $command = test_cmd($elf, $cfg);
    print "FORK $command\n" if $o_debug_me;
    return unless ($command ne "");

    my $pid = 0;
    my $log_file = File::Spec->catfile($cfg, "emuLog.txt");

    if ($o_dry_run) {
        print "INFO-DRY: fork process $pid\n";
        # Delete old log
        unlink($out) or die "Impossible to open $!";
        return;
    }

    $pid = open(my $fork, "|$command ") or die "Impossible to fork $!";
    print "INFO: fork process $pid\n";

    # Kill me
    $SIG{'KILL'} = sub {
        print "ERROR: timeout detected on pid $pid.\n";
        unless ($o_dry_run) {
            kill 'KILL', $pid;
        }
        threads->exit();
    };

    ######################################################################
    # Parse test log
    ######################################################################
    my $try = ($o_timeout > 3) ? $o_timeout - 3 : 1;
    while ($try > 0) {
        sleep(1);
        $try--;

        open(my $emulog, "<$log_file") or next;
        my @all_data = <$emulog>;
        close($emulog);

        foreach my $line (@all_data) {
            if ($line =~ /-- TEST END/) {
                $try = 0;
            }
        }
    }

    ######################################################################
    # Test done
    ######################################################################
    # Kill the process
    print "INFO: kill process $pid\n" if $o_debug_me;
    kill 'TERM', $pid;

    threads->exit();
}

sub test_cmd {
    my $elf = shift;
    my $cfg = shift;

    $elf = cyg_abs_path($elf);
    $cfg = cyg_abs_path($cfg);

    if ($elf =~ /\.elf/) {
        return "$o_exe --elf $elf --cfgpath=$cfg"
    } elsif ($elf =~ /\.irx/) {
        return "$o_exe --irx $elf --cfgpath=$cfg"
    } else {
        print "ERROR: bad command parameters $elf $cfg\n";
        return "";
    }
}

sub diff {
    my $ref_ = shift;
    my $out_ = shift;
    my $quiet = shift;

    open (my $ref_h, "<$ref_");
    my @ref = <$ref_h>;
    chomp(@ref);

    open (my $out_h, "<$out_") or return "T";
    my @out = <$out_h>;
    chomp(@out);

    return "T" if (scalar(@out) < 2);
    return "T" if ($out[-1] !~ /-- TEST END/);
    return "KO" if ((scalar(@out) != scalar(@ref)) and $quiet);

    my $status = "OK";
    my $show = 10;
    for (my $l = 0; $l < scalar(@ref); $l++) {
        $ref[$l] =~ s/\r//g;
        $out[$l] =~ s/\r//g;

        if ($ref[$l] ne $out[$l]) {
            $status = "KO";
            if ($o_show_diff and not $quiet and $show > 0) {
                print "EXPECTED: \"$ref[$l]\"\n";
                print "BUT GOT : \"$out[$l]\"\n";
                $show--;
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
            $thr->kill('KILL')->join();
        }
        $mt_timeout = 100;
    }
}
