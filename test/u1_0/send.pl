#!/usr/bin/perl

use strict;
use warnings;

my $imax=10;
my $jmax=10000;

for(my $i=0;$i<$imax;$i++){
  #open(FILE,'|./user');
  open(FILE,'|./user >/dev/null 2>&1');
  print `date`;
  print FILE qq|{"run":"get_me"}\n|;
  for(my $j=0;$j<$jmax;$j++){
    print FILE qq|{"run":"send_one","address":"0003-00000000-XXXX","amount":0.000002,"message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}\n|;}
  close(FILE);}
