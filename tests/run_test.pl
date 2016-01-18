#!/usr/bin/perl

use strict;
use warnings;
use threads;
use threads::shared;

use Getopt::Long;
use File::Find;
use File::Spec;
use Cwd;
use Cwd 'abs_path';
use Term::ANSIColor;

sub help {
    my $msg = << 'EOS';

    The script run_test.pl is a test runner that work in conjunction with ps2autotests (https://github.com/unknownbrackets/ps2autotests)

    Mandatory Option
        --exe <STRING>   : the PCSX2 binary that you want to test
        --cfg <STRING>   : a path to the a default ini configuration of PCSX2
        --suite <STRING> : a path to ps2autotests test (root directory)

    Optional Option
        --cpu=1          : the number of parallel tests launched. Might create additional issue
        --timeout=20     : a global timeout for hang tests
        --show_diff      : show debug information

        --debug_me       : print script info
EOS
    print $msg;

    exit
}

my $mt_timeout :shared;
my ($o_suite, $o_help, $o_exe, $o_cfg, $o_max_cpu, $o_timeout, $o_show_diff, $o_debug_me);

$o_max_cpu = 1;
$o_timeout = 20;
$o_help = 0;
$o_debug_me = 0;

my $status = Getopt::Long::GetOptions(
    'cfg=s'         => \$o_cfg,
    'cpu=i'         => \$o_max_cpu,
    'debug_me'      => \$o_debug_me,
    'exe=s'         => \$o_exe,
    'help'          => \$o_help,
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
    help();
}

unless (defined $o_exe) {
    print "Error: require a PCSX2 exe\n";
    help();
}

unless (defined $o_cfg) {
    print "Error: require a default cfg directory\n";
    help();
}

$o_exe = abs_path($o_exe);
$o_cfg = abs_path($o_cfg);
$mt_timeout = $o_timeout;


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

    if ($info->{"STATUS"} == 0) {
        print color('bold green');
        print "   OK   | $test\n";
    } else {
        if ($info->{"STATUS"} == 0xBADBEEF) {
            print color('bold blue');
            print "  Tout  | $test\n";
        } else {
            print color('bold red');
            print "   KO   | $test\n";
        }
        if ($o_show_diff) {
            print color('bold magenta'); print "-----------------------------------------------------------------------\n"; print color('reset');
            print test_cmd($test, $cfg) . "\n\n";
            system("diff -u $out $exp");
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

        return unless (-s $out);

        my $end = `tail -1 $out`;
        if ($end =~ /-- TEST END/) {
            system("diff $out $exp -q");
            $info->{"STATUS"} = $?; # potentially not thread safe
        }
    }
}

sub add_test_cmd_for_elf {
    my $file = $_;
    return 0 unless ($file =~ /\.elf/);
    # Fast test
    #return 0 unless ($file =~ /branchdelay/);

    my $dir = $File::Find::dir;
    print "INFO: found $file in $dir\n" if $o_debug_me;

    $g_test_db->{$File::Find::name}->{"CFG_DIR"} = $File::Find::name =~ s/\.elf/_cfg/r;
    $g_test_db->{$File::Find::name}->{"EXPECTED"} = $File::Find::name =~ s/\.elf/.expected/r;
    $g_test_db->{$File::Find::name}->{"OUT"} = $File::Find::name =~ s/\.elf/.PCSX2.out/r;
    $g_test_db->{$File::Find::name}->{"STATUS"} = 0xBADBEEF;

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

    #system("rm -fr $out_dir");
    system("cp -a --remove-destination --no-target-directory $o_cfg $out_dir");
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

    foreach my $option (keys(%sed)) {
        my $v = $sed{$option};
        system("sed -i -e 's/$option=.*/$option=$v/' $ui_ini");
        system("sed -i -e 's/$option=.*/$option=$v/' $vm_ini");
    }
}

sub run_elf {
    my $elf = shift;
    my $cfg = shift;
    my $out = shift;

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
    return "$o_exe --elf $elf --cfgpath=$cfg"
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
