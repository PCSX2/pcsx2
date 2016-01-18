#!/usr/bin/perl

use strict;
use warnings;
use threads;

use Getopt::Long;
use File::Find;
use File::Spec;
use Cwd;
use Cwd 'abs_path';
use Term::ANSIColor;

sub help {
    # always useful
    exit
}

my ($o_suite, $o_help, $o_exe, $o_cfg, $o_max_cpu);
$o_max_cpu = 1;
my $status = Getopt::Long::GetOptions(
    'cfg=s'   => \$o_cfg,
    'cpu=i'   => \$o_max_cpu,
    'exe=s'   => \$o_exe,
    'help'    => \$o_help,
    'suite=s' => \$o_suite,
);

unless ($status) {
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
    print "Error: require a deafult cfg directory\n";
    help();
}
$o_cfg = abs_path($o_cfg);


# Round 1: Collect the tests
my $g_test_db;
print "INFO: search tests in $o_suite and run them in $o_max_cpu CPU)\n";
find({ wanted => \&add_test_cmd_for_elf, no_chdir => 1 },  $o_suite);

# Round 2: Run the tests (later in thread)
foreach my $test (keys(%$g_test_db)) {
    # wait free CPU slot
    while( scalar(threads->list() >= $o_max_cpu) ) {
        if (close_joinnable_threads() == 0) {
            sleep(1); # test are often fast so 1s is more than enough
        }
    }

    create_thread($test);
}
wait_all_threads();

# Round 3: Collect the results (not thread safe)
collect_result();

# Pretty print
print "\n\n Status | =================  Test ======================\n";
foreach my $test (sort(keys(%$g_test_db))) {
    my $info = $g_test_db->{$test};
    if ($info->{"STATUS"} != 0) {
        print color('bold red');
        print "   KO   | $test\n";
    } else {
        print color('bold green');
        print "   OK   | $test\n";
    }
}
print color('reset');
print "\n";

#####################################################

sub collect_result {
    foreach my $test (keys(%$g_test_db)) {
        my $info = $g_test_db->{$test};
        my $out = $info->{"OUT"};
        my $exp = $info->{"EXPECTED"};

        system("diff $out $exp -q");
        $info->{"STATUS"} = $?; # not thread safe
    }
}

sub add_test_cmd_for_elf {
    my $file = $_;
    return 0 unless ($file =~ /\.elf/);
    # Fast test
    #return 0 unless ($file =~ /branchdelay/);

    my $dir = $File::Find::dir;
    print "INFO: found $file in $dir\n";

    $g_test_db->{$File::Find::name}->{"CFG_DIR"} = $File::Find::name =~ s/\.elf/_cfg/r;
    $g_test_db->{$File::Find::name}->{"EXPECTED"} = $File::Find::name =~ s/\.elf/.expected/r;
    $g_test_db->{$File::Find::name}->{"OUT"} = $File::Find::name =~ s/\.elf/.PCSX2.out/r;

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

    # FIXME timeout
    my $pid = open(my $log, "$o_exe --elf $elf --cfgpath=$cfg |") or die "Impossible to pipe $!";
    print "INFO:  Execute $elf (PID=$pid) with cfg ($cfg)\n";

    while ($line = <$log>) {
        $line =~ s/\e\[\d+(?>(;\d+)*)m//g;
        if ($line =~ /-- TEST BEGIN/) {
            $dump = 1;
        }
        if ($dump == 1) {
            print $run $line;
        }
        if ($line =~ /-- TEST END/) {
            $dump = 0;
            print "Info kill process $pid\n";
            kill 'KILL', $pid;
        }
    }
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
    foreach my $thr (threads->list()) {
        $thr->join();
        $g_counter--;
    }
}
