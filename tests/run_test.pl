#!/usr/bin/perl

use strict;
use warnings;
use threads;

use Getopt::Long;
use File::Find;

sub help {
    # always useful
    exit
}

my ($o_suite, $o_help, $o_exe);
my $status = Getopt::Long::GetOptions(
    'suite=s' => \$o_suite,
    'exe=s'   => \$o_exe,
    'help'    => \$o_help,
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


# Round 1: Collect the tests
my $g_test_db;
print "Info: search test in $o_suite\n";
find(\&add_test_cmd_for_elf, $o_suite);

# Round 2: Run the tests (later in thread)
foreach my $t (keys(%$g_test_db)) {
    my $info = $g_test_db->{$t};
    run_elf($t, $info->{"CFG_DIR"}, $info->{"OUT"});
}

# Round 3: Collect the results

#####################################################

sub generate_cfg {
}

sub collect_result {
}

sub add_test_cmd_for_elf {
    my $file = $_;
    return 0 unless ($file =~ /\.elf/);
    return 0 unless ($file =~ /branchdelay/);

    my $dir = $File::Find::dir;
    print "INFO: found $file in $dir\n";

    $g_test_db->{$File::Find::name}->{"CFG_DIR"} = $File::Find::dir;
    $g_test_db->{$File::Find::name}->{"EXPECTED"} = $File::Find::name =~ s/\.elf/.expected/r;
    $g_test_db->{$File::Find::name}->{"OUT"} = $File::Find::name =~ s/\.elf/.PCSX2.out/r;

    return 1;
}

sub run_elf {
    my $elf = shift;
    my $cfg = shift;
    my $out = shift;

    my $line;
    my $dump = 0;
    open(my $run, ">$out") or die "Impossible to open $!";

    # FIXME timeout + cfg support
    my $pid = open(my $log, "$o_exe --elf $elf |") or die "Impossible to pipe $!";
    print "Info:  Execute $elf (PID=$pid) with cfg ($cfg)\n";

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
