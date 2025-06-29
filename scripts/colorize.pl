# SPDX-License-Identifier: MIT
#
# File: colorize.pl
# Description: Colorize the output of Cppcheck
# Author: Jakob Kastelic
# Copyright (c) 2025 Stanford Research Systems, Inc.

use strict;
use warnings;
use feature 'say';
use IPC::Open3;
use Symbol qw(gensym);

# ANSI color codes
my %colors = (
   error   => "\033[31m",
   warning => "\033[33m",
   reset   => "\033[0m",
   green   => "\033[32m",
   cyan    => "\033[36m",
   bold    => "\033[1m",
);

# Regex to capture optional path, filename, line and column numbers
my $filename_re = qr{^(.*[\\/])?([^\\/]+):(\d+):(\d+):};

# Setup to capture stderr and stdout together
my $stderr = gensym;
my $pid = open3(undef, \*CHLD_OUT, $stderr, 'cppcheck', @ARGV);

# Merge both stdout and stderr
my @streams = (\*CHLD_OUT, $stderr);
foreach my $fh (@streams) {
   while (my $line = <$fh>) {
      chomp $line;
      say colorize_line($line);
   }
}

# Wait for child process and return its exit code
waitpid($pid, 0);
exit $? >> 8;

# === Subroutines ===

sub colorize_line {
   my ($line) = @_;

   if ($line =~ $filename_re) {
      my ($path, $filename, $line_no, $col_no) = ($1 // '', $2, $3, $4);

      my $colored_path     = $colors{cyan} . $path;
      my $colored_filename = $colors{bold} . $colors{cyan} . $filename . $colors{reset};
      my $colored_linecol  = $colors{green} . ":$line_no:$col_no:" . $colors{reset};

      my $rest = substr($line, length($&));
      my $colored_rest;

      if (lc($rest) =~ /error/) {
         $colored_rest = $colors{error} . $rest . $colors{reset};
      } elsif (lc($rest) =~ /warning/) {
         $colored_rest = $colors{warning} . $rest . $colors{reset};
      } else {
         $colored_rest = $rest;
      }

      return $colored_path . $colored_filename . $colored_linecol . $colored_rest;
   }

   # Fallback: match error/warning anywhere in line
   if ($line =~ /error/i) {
      return $colors{error} . $line . $colors{reset};
   }
   if ($line =~ /warning/i) {
      return $colors{warning} . $line . $colors{reset};
   }

   return $line;
}

