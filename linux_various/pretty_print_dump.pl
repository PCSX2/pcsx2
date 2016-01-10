#!/usr/bin/perl

use strict;
use warnings;

open(my $in, $ARGV[0]) or die "failed to get first param: $!";

my @pp_name = (
    # GPR
    "0", "0", "0", "0",
    "at", "at", "at", "at",
    "v0", "v0", "v0", "v0",
    "v1", "v1", "v1", "v1",
    "a0", "a0", "a0", "a0",
    "a1", "a1", "a1", "a1",
    "a2", "a2", "a2", "a2",
    "a3", "a3", "a3", "a3",
    "t0", "t0", "t0", "t0",
    "t1", "t1", "t1", "t1",
    "t2", "t2", "t2", "t2",
    "t3", "t3", "t3", "t3",
    "t4", "t4", "t4", "t4",
    "t5", "t5", "t5", "t5",
    "t6", "t6", "t6", "t6",
    "t7", "t7", "t7", "t7",
    "s0", "s0", "s0", "s0",
    "s1", "s1", "s1", "s1",
    "s2", "s2", "s2", "s2",
    "s3", "s3", "s3", "s3",
    "s4", "s4", "s4", "s4",
    "s5", "s5", "s5", "s5",
    "s6", "s6", "s6", "s6",
    "s7", "s7", "s7", "s7",
    "t8", "t8", "t8", "t8",
    "t9", "t9", "t9", "t9",
    "k0", "k0", "k0", "k0",
    "k1", "k1", "k1", "k1",
    "gp", "gp", "gp", "gp",
    "sp", "sp", "sp", "sp",
    "s8", "s8", "s8", "s8",
    "ra", "ra", "ra", "ra",
    "hi", "hi", "hi", "hi",
    "lo", "lo", "lo", "lo",

    # CP0
    "Index"    , "Random"    , "EntryLo0"  , "EntryLo1"  ,
    "Context"  , "PageMask"  , "Wired"     , "Reserved0" ,
    "BadVAddr" , "Count"     , "EntryHi"   , "Compare"   ,
    "Status"   , "Cause"     , "EPC"       , "PRid"      ,
    "Config"   , "LLAddr"    , "WatchLO"   , "WatchHI"   ,
    "XContext" , "Reserved1" , "Reserved2" , "Debug"     ,
    "DEPC"     , "PerfCnt"   , "ErrCtl"    , "CacheErr"  ,
    "TagLo"    , "TagHi"     , "ErrorEPC"  , "DESAVE"    ,

    "sa",
    "IsDelaySlot",
    "pc",
    "code",
    "PERF", "PERF", "PERF", "PERF",

    "eCycle0"  , "eCycle1"  , "eCycle2"  , "eCycle3"  , "eCycle4"  , "eCycle5"  , "eCycle6"  , "eCycle7"  ,
    "eCycle8"  , "eCycle9"  , "eCycle10" , "eCycle11" , "eCycle12" , "eCycle13" , "eCycle14" , "eCycle15" ,
    "eCycle16" , "eCycle17" , "eCycle18" , "eCycle19" , "eCycle20" , "eCycle21" , "eCycle22" , "eCycle23" ,
    "eCycle24" , "eCycle25" , "eCycle26" , "eCycle27" , "eCycle28" , "eCycle29" , "eCycle30" , "eCycle31" ,

    "sCycle0"  , "sCycle1"  , "sCycle2"  , "sCycle3"  , "sCycle4"  , "sCycle5"  , "sCycle6"  , "sCycle7"  ,
    "sCycle8"  , "sCycle9"  , "sCycle10" , "sCycle11" , "sCycle12" , "sCycle13" , "sCycle14" , "sCycle15" ,
    "sCycle16" , "sCycle17" , "sCycle18" , "sCycle19" , "sCycle20" , "sCycle21" , "sCycle22" , "sCycle23" ,
    "sCycle24" , "sCycle25" , "sCycle26" , "sCycle27" , "sCycle28" , "sCycle29" , "sCycle30" , "sCycle31" ,

    "cycle", "interrupt", "branch", "opmode", "tempcycles"
);

my $line;
my $cpu;
while($line = <$in>) {
    if ($line =~ /Dump register data: (0x[0-9a-f]+)/) {
        $cpu = hex($1);
    }
    if ($line =~ /ds:(0x[0-9a-f]+)/) {
        my $mem = hex($1);
        my $offset = $mem - $cpu;
        if ($offset >= 0 && $offset < 980) {
            # Inside the cpuRegisters structure
            my $byte = ($offset >= 544) ? $offset % 4 : $offset % 16;
            my $dw   = $offset / 4;

            # FIXME B doesn't work for duplicated register
            my $pretty = "&$pp_name[$dw]_B$byte";
            #print "AH $pretty\n";
            $line =~ s/ds:0x[0-9a-f]+/$pretty/;
        }
    }
    print $line;
}
