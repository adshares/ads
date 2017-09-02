#!/usr/bin/perl

use strict;
use warnings;
use lib "./";
use NET; # qw(:ALL); # ($hostname @nodes $bin $peers)

my $dir=$NET::nodes[0]."/cli";
mkdir($dir);
chdir($dir)||die "ERROR: failed to chdir to $dir\n";
$NET::esc="../esc";

my @do;
for(my $n=2;$n<=@NET::nodes;$n++){
  push @do,qw|{"run":"create_node"}|;}
if(@do){
  &NET::call_esc($NET::secret,1,0,@do);}

exit;
