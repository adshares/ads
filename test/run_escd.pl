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
my $host=$NET::hostname;
if($host eq ""){
  $host=`hostname -I`;
  $host=~s/ .*//;
  chomp($host);}
my $port=&NET::port($svid);
my $offi=&NET::offi($svid);
if(!-d $ndir){
  system("mkdir -p $ndir");
  if(!-d $ndir){
    die "ERROR: failed to create $ndir directory\n";}}
if(-e "$ndir/.lock"){
  die "ERROR: node lock file $ndir/.lock exists, remove if node not running\n";}
my $check_screen=`screen -S escd_node$svid -ls`;
if($check_screen!~/No Sockets found/){
  die "ERROR: screen -S node$svid running? try: screen -S escd_node$svid -X kill\n";}

print "SVID: $svid\n";
print "NDIR: $ndir\n";
print "HOST: $host\n";
print "PORT: $port\n";
print "OFFI: $offi\n";

my $options="svid=$svid\naddr=$host\nport=$port\noffi=$offi\n";
my $peers="";
for(my $n=1;$n<=@NET::nodes;$n++){
  if($n==$svid){
    next;}
  my $dir=$NET::nodes[$n-1];
  if(-f "$dir/.lock"){
    open(FILE,"$dir/.lock");
    my $peer=<FILE>;
    close(FILE);
    chomp($peer);
    if($peer=~/:\d+/){
      $peers.="peer=$peer\n";}}}
open(FILE,">$ndir/options.cfg")||die;
print FILE $options.$peers;
close(FILE);

if(!-e "$ndir/escd" || ((-C"$NET::bin/escd")<(-C"$ndir/escd"))){
  system("cp $NET::bin/escd $ndir/escd")&&
    die "ERROR: failed to copy $NET::bin/escd to $ndir/escd\n";}

if(!-e "$ndir/esc" || ((-C"$NET::bin/esc")<(-C"$ndir/esc"))){
  system("cp $NET::bin/esc $ndir/esc")&&
    die "ERROR: failed to copy $NET::bin/esc to $ndir/esc\n";}

if($svid>1){
  if(!-d "$ndir/key"){
    mkdir("$ndir/key")||die"ERROR: failed to create $ndir/key directory\n";}
  if(!-f "$ndir/key/key.txt"){ # copy keys from node 1 if no keys found
    if(-r "$NET::nodes[0]/key/key.txt"){
      system("cp $NET::nodes[0]/key/key.txt $ndir/key/key.txt")&&
        die "ERROR: failed to copy $NET::nodes[0]/key/key.txt to $ndir/key/key.txt\n";}
    else{
      die "ERROR: node can not start yet, $NET::nodes[0]/key/key.txt missing (maybe start node 1 first)\n";}}
  if(!-d "$ndir/blk" && !-f "$ndir/servers.srv"){ # copy servers.srv from node 1 if not found
    if(-r "$NET::nodes[0]/msid.txt"){
      open(FILE,"$NET::nodes[0]/msid.txt")||die "ERROR: failed to open $NET::nodes[0]/msid.txt\n";
      my $msid=<FILE>;
      close(FILE);
      if($msid=~/^........ (...)(.....) /){
        system("cp $NET::nodes[0]/blk/$1/$2/servers.srv $ndir/servers.srv")&&
          die "ERROR: failed to copy $NET::nodes[0]/blk/$1/$2/servers.srv to $ndir/servers.srv\n";}
      else{
        die "ERROR: failed to parse $NET::nodes[0]/msid.txt\n";}}
    else{
      die "ERROR: node can not start yet, $NET::nodes[0]/msid.txt missing (maybe start node 1 first)\n";}}}

chdir($ndir)||die "ERROR: failed to change dir to $ndir\n";
#now, the node creates a .lock file with same format
#open(FILE,">.lock")||die "ERROR: failed to create lock file\n";
#print FILE "$host:$port\n";
#close(FILE);
if($peers eq'' && $svid==1){
  print "screen -S escd_node$svid -d -m ./escd --init 1\n";
  system("screen -S escd_node$svid -d -m ./escd --init 1");} # init if node1 and no nodes running
else{
  if(-s "usr/0001.dat"){
    print "screen -S escd_node$svid -d -m ./escd -m 1\n";
    system("screen -S escd_node$svid -d -m ./escd -m 1");} # full sync if started before
  else{
    print "screen -S escd_node$svid -d -m ./escd -m 1 -f 1\n";
    system("screen -S escd_node$svid -d -m ./escd -m 1 -f 1");}} # resync fast first time

exit;
