#!/usr/bin/perl

use strict;
use warnings;

if(!@ARGV){
  die "USAGE: $0 svid\n";}
my $svid=$ARGV[0];
print "SVID: $svid\n";
#my $dir=sprintf("n%02d",$svid);
my $options="svid=$svid\n";
if(-e "/workspace/leszek/net/nxx/peer.$svid"){
  die"ERROR, /workspace/leszek/net/nxx/peer.$svid exists\n";}

my $hostname=`hostname -I`;
if($hostname=~/(10\.20\.11\.\d+)/){
  my $addr=$1;
  open(FILE,">/workspace/leszek/net/nxx/peer.$svid")||die;
  print FILE "$addr\n";
  close(FILE);
  $options.="addr=$addr\n";}
else{
  die "ERROR: bad ip $hostname";}

my $peers="";
for(my $n=1;$n<=8;$n++){
  if($n==$svid){
    next;}
  if(-e "/workspace/leszek/net/nxx/peer.$n"){
    open(FILE,"/workspace/leszek/net/nxx/peer.$n");
    my $ip=<FILE>;
    close(FILE);
    if($ip=~/(10\.20\.11\.\d+)/){
      $peers.="peer=$1:9091\n";}}}

system("rm -rf /run/shm/net");
mkdir("/run/shm/net") || die;
chdir("/run/shm/net") || die; 
system("rsync -a /workspace/leszek/net/nxx/key /workspace/leszek/net/nxx/servers.txt ./");
#system("ls -l");
system("echo > in.txt");
open(FILE,">options.cfg")||die;
print FILE $options.$peers;
close(FILE);

if($peers eq ""){
  system("tail -f in.txt | /workspace/leszek/net/main --init 1 &");}
else{
  system("tail -f in.txt | /workspace/leszek/net/main -m 1 -f 1 &");}

while(<STDIN>){
  if($_=~/^\./){
    system("echo '.' >> in.txt");
    sleep(1);
    system("echo '.' >> in.txt");
    sleep(5);
    last;}
  system("lsof -u leszek | grep '^main'");
  system("uptime");
  system("free -m");
  system("df -h ./");}
chdir("/");
system("rm -rf /run/shm/net");
unlink("/workspace/leszek/net/nxx/peer.$svid");
print "DONE\n";
