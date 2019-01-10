#!/usr/bin/perl -w

use strict;
use warnings;
use LWP::UserAgent;
use JSON;

my $apikey = shift || undef;
my $ipdir = shift || undef;
my $nodemap = shift || "nodemap.txt";

my $baseurl = "http://api.ipstack.com";

if (!defined $apikey or !defined $ipdir) {
	die("Usage nodemap2ipstack <api-key> <storage dir> [nodemap file]");
}

my $line = "";
my $ip = "";
my $bulk_ips = "";
my $bulk_ip_cnt = 0;

use constant MAX_BULK_IPS => 50;

my $ua = LWP::UserAgent->new(
	'agent' => 'Mozilla',
	'ssl_opts' => {'verify_hostname' => 1},
	'proxy' => undef
) or die $!;

$ua->timeout(3);

open(I,"<$nodemap") or die $!;

for (;;) {
	$line = <I>;
	last if !defined $line;
	chomp($line);
	$line = $line.",";

	my $done = 0;
	while ($done == 0) {
		if ($line =~ s/\[([0-9a-f\.\:]+)\]:\d+,//) {
			$ip = $1;
			next if $ip =~ /\.\./;
			print "Checking ${ip} ...\n";
			if (!-e "${ipdir}/_${ip}") {
				print "Not found in cache. Adding to lookup list ...\n";
				if ($bulk_ip_cnt++ < MAX_BULK_IPS) {
					$bulk_ips .= ",${ip}";
				}
			} else {
				print "Already cached.\n";
				next;
			}

			if ($bulk_ip_cnt >= MAX_BULK_IPS) {
				print "MAX_BULK_IPS reached, doing lookup.\n";
				write_to_cache($bulk_ips);
				$bulk_ip_cnt = 0;
				$bulk_ips = "";
			}
		} else {
			$done = 1;
		}
	}
}

close(I);

# write remaining IP's, if total amount isnt 0 mod MAX_BULK_IPS
if ($bulk_ip_cnt > 0) {
	write_to_cache($bulk_ips);
}


sub write_to_cache
{
	my $bulk_ips = $_[0];

	$bulk_ips =~ s/,//;	# remove first comma
	my $response = $ua->get("${baseurl}/${bulk_ips}?access_key=${apikey}");
	if ($response->is_success) {
		print "Successfull request!\n";

		my $data = decode_json($response->decoded_content);

		# return may be an array for multiple ip lookups or single hashref
		if (ref $data ne 'ARRAY') {
			my @array = ($data);
			$data = \@array;
		}

		for my $elem (@$data) {
			my $ip = $elem->{'ip'};
			if (!defined $ip) {
				die "Weird! No IP defined?! Check raw content";
			}
			next if !$ip =~ /^[0-9a-f\.\:]+$]/;
			next if $ip =~ /\.\./;
			print "Adding ${ip} to DB.\n";
			open(O,">${ipdir}/_${ip}") or die $!;
			print O encode_json($elem);
			close(O);
		}
	} else {
		print $response->status_line;
	}
}

