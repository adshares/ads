#!/usr/bin/perl

use strict;
use warnings;

if(@ARGV<1){
  die"USAGE: $0 num_of_nodes\n";}

my $num=$ARGV[0];
my $skey="/workspace/leszek/net/key";
my $user="/workspace/leszek/net/user";
my $file="/tmp/in_$$";
my $node=0;
my %skey;
my $pkey;

open(FILE,">$file");
print FILE qq|{"run":"get_me"}\n|;
close(FILE);
open(FILE,"$user < $file 2>/dev/null |");
my @file=<FILE>;
close(FILE);
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

for(my $i=2;$i<=$num;$i++){
  my $accn=sprintf("%4X-00000000-XXXX",$i);
  open(FILE,">$file");
  print FILE qq|{"run":"get_account","address":"$accn"}\n|;
  close(FILE);
  open(FILE,"$user < $file 2>/dev/null |");
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
    print FILE qq|{"run":"get_me"}\n{"run":"create_node"}\n|;
    close(FILE);
    open(FILE,"$user < $file 2>/dev/null |");
    @file=<FILE>;
    close(FILE);
    my $dir=sprintf("../n%02d",$i);
    mkdir($dir);
    system("rsync -qa ../n01/key $dir/");
    next;}
  if($balance<1000000.0){
    print "SEND FUNDS to $accn\n";
    open(FILE,">$file");
    print FILE qq|{"run":"get_me"}\n{"run":"send_one","address":"$accn","amount":"10000000.0","message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}\n{"run":"get_account","address":"$accn"}\n|;
    close(FILE);
    open(FILE,"$user < $file 2>/dev/null |");
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
  print sprintf("%04X %.9f\n",$i,$balance);}
unlink($file);
