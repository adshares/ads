#!/usr/bin/perl

use strict;
use warnings;
use lib "./";
use NET; # qw(:ALL); # ($hostname @nodes $bin $peers)

if(!@ARGV || !($ARGV[0]>0 && $ARGV[0]<=@NET::nodes)){
  die "USAGE: $0 node_id_starting_from_1_ending_at_".scalar(@NET::nodes)."\n";}
my $svid=$ARGV[0];
if($svid>@NET::nodes){
  die "ERROR: node id too high ($svid), only ".scalar(@NET::nodes)." nodes defined.\n";}
my $ndir=$NET::nodes[$svid-1];

if(-r "$ndir/msid.txt"){
  open(FILE,"$ndir/msid.txt")||die "ERROR: failed to open $ndir/msid.txt\n";
  my $msid=<FILE>;
  close(FILE);
  if($msid=~/^........ (...)(.....) /){
    system("cp $ndir/blk/$1/$2/servers.srv ./servers.srv")&&
      die "ERROR: failed to copy $ndir/blk/$1/$2/servers.srv to ./servers.srv\n";}
  else{
    die "ERROR: failed to parse $ndir/msid.txt\n";}}
else{
  die "ERROR: node can not start yet, $ndir/msid.txt missing (maybe start node 1 first)\n";}
