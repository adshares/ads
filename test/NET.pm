#!/usr/bin/perl
package NET;
use strict;
use warnings;
use Exporter qw(import);
our @EXPORT_OK=qw($hostname @nodes $bin $peers);
our %EXPORT_TAGS=(ALL=>\@EXPORT_OK);

#our $hostname="127.0.0.1"; # disable hostname detection
our $secret="14B183205CA661F589AD83809952A692DFA48F5D490B10FD120DA7BF10F2F4A0"; # default esc secret
our $hostname=""; # enable hostname detection
our @nodes=( # define directories for each node
  "/tmp/esc/n1",
  "/tmp/esc/n2",
  "/tmp/esc/n3",
  "/tmp/esc/n4",
  "/tmp/esc/n5",
  "/tmp/esc/n6",
  "/tmp/esc/n7");
#test
#@nodes=("/tmp/esc/n1");

our $bin=".."; # lookup this directory for the executables (escd, esc)
our $peers="/tmp/esc"; # store list of running nodes in this directory
our $escd="./escd"; # not used
our $esc="./esc"; # overwritten by scripts
our $olog=0; # do not write transaction log
my $salt="some weak secret";

sub port
{ return 8080+shift;
}

sub offi
{ return 9090+shift;
}

sub call_esc
{ my $pass=shift(@_);
  my $node=shift(@_)||die;
  my $user=shift(@_);
  my $addr=sprintf("%04X-%08X-XXXX",$node,$user);
  my $offi=&offi($node);
  open(FILE,"|$esc -P $offi -A $addr -o $olog > $addr.out")||
    die "ERROR RUNNING: $esc -P $offi -A $addr -o $olog > $addr.out\n";
  if($pass ne ''){
    print FILE "$pass\n";}
  else{ # change this with correct password
    print FILE "$secret\n";}
  print FILE qq|{"run":"get_me"}\n|;
  while(my $do=shift(@_)){
    print FILE "$do\n";}
}

1;
