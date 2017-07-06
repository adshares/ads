#!/usr/bin/perl

use strict;
use warnings;

if(@ARGV<5){
  die"USAGE: $0 svid num_of_nodes num_of_accounts num_of_threads num_of_txs\n";}

my $svid=$ARGV[0];
my $nod=$ARGV[1];
my $num=$ARGV[2];
my $thr=$ARGV[3];
my $txs=$ARGV[4];
my $hnam=`hostname`;chomp($hnam);
my $skey="/workspace/leszek/net/key";
my $user="/workspace/leszek/net/user";
my $file="/tmp/in_$$-$hnam";
my $node=0;
my %skey;
my $pkey;

chdir("/workspace/leszek/net/uxx")||die;

if(!-e "/workspace/leszek/net/nxx/peer.$svid"){
  print STDERR "ERROR, /workspace/leszek/net/nxx/peer.$svid missing\n";
  exit(0);}
my $host=`cat /workspace/leszek/net/nxx/peer.$svid`;
chomp($host);
$user="$user --host $host";
my $myaccn=sprintf("%04X-00000000-XXXX",$svid);

open(FILE,">$file");
print FILE qq|{"run":"get_me"}\n|;
close(FILE);
open(FILE,"$user --address $myaccn < $file 2>/dev/null |");
my @file=<FILE>;
close(FILE);
unlink($file);
#print join('',@file);
foreach(@file){
  if($_=~/"node": "(\d+)/){
    $node=sprintf("%04X",$1+0);
    next;}
  if($_=~/"public_key": "([A-F0-9]+)/){
    $pkey=$1;
    next;}
  if($_=~/"network_account"/){
    last;}}
if(!$node){
  die"ERROR, connection failed\n";}
print "NODE: $node\n";
print "PKEY: $pkey\n";

$skey{0}="14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0";
if($num<=1024){
  for(my $i=1;$i<$num;$i++){
    my $accn=sprintf("%4s-%08X-XXXX",$node,$i);
    open(FILE,"$skey $accn|");
    my @file=<FILE>;
    close(FILE);
    if(@file != 3){
    die"ERROR, unexpected $skey result\n".join("",@file);}
    $file[0]=~s/^SK: //; chomp($file[0]);
    #$file[1]=~s/^PK: //; chomp($file[1]);
    #$file[2]=~s/^SG: //; chomp($file[2]);
    $skey{$i}=$file[0];}}

open(FILE,">/tmp/get_me.json");
print FILE qq|{"run":"get_me"}\n|;
close(FILE);

my $pn=0;
while(1){
  if(!fork){
    my $anum=0;
    if($num>1){
      $anum=int(rand($num));
      if($anum>=$num){
        $anum=$num-1;}}
    my $accn=sprintf("%4s-%08X-XXXX",$node,$anum);
    my $uskey;
    if($num<=1024){
      $uskey=$skey{$anum};}
    else{
      open(FILE,"$skey $accn|");
      my @file=<FILE>;
      close(FILE);
      if(@file != 3){
      die"ERROR, unexpected $skey result\n".join("",@file);}
      $file[0]=~s/^SK: //; chomp($file[0]);
      #$file[1]=~s/^PK: //; chomp($file[1]);
      #$file[2]=~s/^SG: //; chomp($file[2]);
      $uskey=$file[0];}
    open(FILE,qq{$user --address "$accn" --secret "$uskey" < /tmp/get_me.json 2>/dev/null |});
    my @file=<FILE>;
    close(FILE);
    my $balance=0;
    foreach(@file){
      if($_=~/"balance": "([0-9\.]+)/){
        $balance=$1+0.0;
        last;}}
    if($balance<1.0){
      print "$accn $balance LOW!\n";
      sleep(5);
      exit;}
    print "$accn $balance\n";
    open(FILE,qq{|$user --address "$accn" --secret "$uskey" >/dev/null 2>&1});
    print FILE qq|{"run":"get_me"}\n|;
    for(my $i=0;$i<$txs;$i++){
      my $tonod=int(rand($nod))+1;
      my $tonum=int(rand($num));
      my $toaccn=sprintf("%4s-%08X-XXXX",$tonod,$tonum);
      print FILE qq|{"run":"send_one","address":"$toaccn","amount":0.000002,"message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}\n|;}
    close(FILE);
    #sleep(1);
    exit;}
  if(++$pn>=$thr){
    wait();}}
while(wait()>=0){}
exit(0);
