#!/usr/bin/perl

use strict;
use warnings;
use lib "./";
use NET; # qw(:ALL); # ($hostname @nodes $bin $peers)

if(!@ARGV || ($ARGV[0] ne 'all' && !($ARGV[0]>0 && $ARGV[0]<=@NET::nodes))){
  die "USAGE: $0 node_id_starting_from_1_ending_at_".scalar(@NET::nodes)." | all\n";}
my $svid=$ARGV[0];
my @screen=`screen -ls`;
my @kill;
my @delete;
#print "--".join("--",@screen)."\n\n";
foreach(@screen){
  if($_=~/^\s*(\d+)\.escd_node(\d+)\s/){
    if($svid eq 'all' || $svid == $2){
      push @kill,"$1.escd_node$2";
      print "Kill screen $1.escd_node$2\n";}}}
if($svid eq "all"){
  for(my $n=1;$n<=@NET::nodes;$n++){
    if(-d $NET::nodes[$n-1]){
      print "Delete content of directory ".$NET::nodes[$n-1]."\n";
      push @delete,$NET::nodes[$n-1];}}}
elsif(-d $NET::nodes[$svid-1]){
  print "Delete escd content of directory ".$NET::nodes[$svid-1]."\n";
  push @delete,$NET::nodes[$svid-1];}
if(!@kill && !@delete){
  print "Nothing to do\n";
  exit;}
print "Execute ? [Y/N] ";
my $ok=<STDIN>;
if($ok!~/^\s*[Yy]/){
  print "Canceled\n";
  exit;}

foreach(@kill){
  system("screen -S $_ -X kill");}
foreach(@delete){
  if($_!~/^\//){
    print STDERR "ERROR: ignore relative directory $_\n";
    next;}
  if(!chdir($_)){
    print STDERR "ERROR: failed to change dir to $_\n";
    next;}
  system("rm -rf blk esc escd inx key log log.txt msid.txt ofi options.cfg servers.srv usr vip cli .lock core");}

exit;
