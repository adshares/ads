#!/usr/bin/perl
package NET;
use strict;
use warnings;
use Exporter qw(import);
our @EXPORT_OK=qw($hostname @nodes $bin $peers);
our %EXPORT_TAGS=(ALL=>\@EXPORT_OK);

#our $hostname = "127.0.0.1"; # disable hostname detection
our $hostname = ""; # enable hostname detection
our @nodes = ( # define directories for each node
  "/tmp/esc/n1",
  "/tmp/esc/n2",
  "/tmp/esc/n3",
  "/tmp/esc/n4");
our $bin = "../"; # lookup this directory for the executables (escd, esc)
our $peers = "/tmp/esc"; # store list of running nodes in this directory

1;
