#!/usr/bin/perl

use strict;
use warnings;

if(@ARGV<2){
  die"USAGE: $0 svid num_of_accounts\n";}

my $svid=$ARGV[0];
my $num=$ARGV[1];
my $skey="/workspace/leszek/net/key";
my $user="/workspace/leszek/net/user";
my $file="/tmp/in_$$-$svid";
my %skey;
my $pkey="";

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
#print join('',@file);
my $node="";
foreach(@file){
  if($_=~/"node": "(\d+)/){
    $node=sprintf("%04X",$1+0);
    next;}
  if($_=~/"public_key": "([A-F0-9]+)/){
    $pkey=$1;
    next;}
  if($_=~/"network_account"/){
    last;}}
if($node eq""){
  die"ERROR, connection failed\n";}
print "NODE: $node\n";
print "PKEY: $pkey\n";

for(my $i=1;$i<$num;$i++){
  my $accn=sprintf("%4s-%08X-XXXX",$node,$i);
  open(FILE,">$file");
  print FILE qq|{"run":"get_account","address":"$accn"}\n|;
  close(FILE);
  open(FILE,"$user --address $myaccn < $file 2>/dev/null |");
  my @file=<FILE>;
  close(FILE);
  my $upkey=""; 
  my $balance=0.0; 
  foreach(@file){
    if($_=~/"public_key": "([A-F0-9]+)/){
      $upkey=$1;
      next;}
    if($_=~/"balance": "([0-9\.]+)/){
      $balance=$1+0.0;
      next;}
    if($_=~/"network_account"/){
      last;}}
  if($upkey eq ""){
    print "CREATE $accn\n";
    open(FILE,">$file");
    print FILE qq|{"run":"get_me"}\n{"run":"create_account","node":"$node"}\n{"run":"send_one","address":"$accn","amount":"100.0","message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}\n{"run":"get_account","address":"$accn"}\n|;
    close(FILE);
    open(FILE,"$user --address $myaccn < $file 2>/dev/null |");
    @file=<FILE>;
    close(FILE);
    $upkey=""; 
    $balance=0;
    my $uaccn="-----";
    foreach(@file){
      if($_=~/"address": "(....-........-)...."/){
        $uaccn=$1;
        next;}
      if($accn=~/^$uaccn/ && $_=~/"public_key": "([A-F0-9]+)/){
        $upkey=$1;
        next;}
      if($accn=~/^$uaccn/ && $_=~/"balance": "([0-9\.]+)/){
        $balance=$1+0.0;
        next;}
      if($accn=~/^$uaccn/ && $_=~/"network_account"/){
        last;}}
    if($upkey eq ""){
      die"ERROR, failed to access account $accn\n".join("",@file);}}
  if($balance<1.0){
    print "SEND FUNDS to $accn\n";
    open(FILE,">$file");
    print FILE qq|{"run":"get_me"}\n{"run":"send_one","address":"$accn","amount":"100.0","message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}\n{"run":"get_account","address":"$accn"}\n|;
    close(FILE);
    open(FILE,"$user --address $myaccn < $file 2>/dev/null |");
    @file=<FILE>;
    close(FILE);
    my $uaccn="-----";
    foreach(@file){
      if($_=~/"address": "(....-........-)...."/){
        $uaccn=$1;
        next;}
      if($accn=~/^$uaccn/ && $_=~/"balance": "([0-9\.]+)/){
        $balance=$1+0.0;
        next;}
      if($accn=~/^$uaccn/ && $_=~/"network_account"/){
        last;}}
    if($balance<1.0){
      die"ERROR, failed to send funds to $accn\n".join("",@file);}}
  if($upkey eq $pkey){
    print "CHANGE KEY $accn\n";
    open(FILE,"$skey $accn|");
    @file=<FILE>;
    close(FILE);
    if(@file != 3){
      die"ERROR, unexpected $skey result\n".join("",@file);}
    $file[0]=~s/^SK: //; chomp($file[0]);
    $file[1]=~s/^PK: //; chomp($file[1]);
    $file[2]=~s/^SG: //; chomp($file[2]);
    $skey{$i}=$file[0];
    open(FILE,">$file");
    print FILE qq|{"run":"get_me"}\n{"run":"change_account_key","pkey":"$file[1]","signature":"$file[2]"}\n|;
    close(FILE);
    open(FILE,qq{$user --address $accn < $file 2>/dev/null |});
    #open(FILE,qq{$user --address "$accn" < $file |});
    my @fila=<FILE>;
    close(FILE);
    open(FILE,">$file");
    print FILE qq|{"run":"get_account","address":"$accn"}\n|;
    close(FILE);
    open(FILE,qq{$user --address $myaccn < $file 2>/dev/null |});
    @file=<FILE>;
    close(FILE);
    $upkey=""; 
    foreach(@file){
      if($_=~/"public_key": "([A-F0-9]+)/){
        $upkey=$1;
        next;}
      if($_=~/"network_account"/){
        last;}}
    if($upkey eq ""){
      die"ERROR, failed to access account $accn\n".join("",@file);}
    if($upkey eq $pkey){
      die"ERROR, failed to change key $accn, try:\n".qq{$user --address $accn < $file\n}.join("",@fila)."ME:\n".join("",@file);}}
  print sprintf("%04X PKEY: %s %.9f\n",$i,$upkey,$balance);}
unlink($file);
