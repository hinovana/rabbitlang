#/usr/bin/perl
# --------------------------------------------------------------------
use strict;
use warnings;
# --------------------------------------------------------------------
sub get_YYYYMMDD {
	my $time = shift;
	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($time);
	
	sprintf("%04d-%02d-%02d", $year + 1900, $mon + 1, $mday);
}

my $RABBIT_VERSION = "0.1.0";
my $RABBIT_BUILD   = &get_YYYYMMDD( time );

open FH, "> src/version.h" or die "faild file open";
print FH qq[
#ifndef RABBIT_VERSION_H_
#define RABBIT_VERSION_H_

#define RABBIT_VERSION "${RABBIT_VERSION}"
#define RABBIT_BUILD_DATE "${RABBIT_BUILD}"

VALUE rabbit_get_version( VALUE self );

#endif // RABBIT_VERSION_H_
];
close FH;

;1;
