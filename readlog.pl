#!/usr/bin/perl

use strict;
use warnings;

use Digest::MD5 qw(md5_hex);

sub padrev($)
{
    my ($a) = @_;
    "0000000$a" =~ /.*(.{8})/  or  die;
    return scalar reverse $1;
}


sub md5spc($)
{
    my $r = md5_hex @_;
    $r =~ s/(\w{8})/$1 /g;
    return $r;
}

my $count = 0;

while (<>) {
    next unless /^H/;
    /^H \d+ \d \d+ \d \d+ (\w+) (\w+) (\w+) \w+ \w+ \w+ (\w+) (\w+) (\w+) \w+ \w+ \w+$/  or  die;
    my @a = ($1, $2, $3);
    my @b = ($4, $5, $6);
    my $a = join '', map { padrev $_ } @a;
    my $b = join '', map { padrev $_ } @b;
    print $a, ' ', md5spc $a, "\n";
    print $b, ' ', md5spc $b, "\n\n";
    ++$count;
}

print "Read $count hits\n";