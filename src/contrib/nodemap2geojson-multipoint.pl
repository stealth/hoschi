#!/usr/bin/perl -w

use strict;
use warnings;
use LWP::UserAgent;
use JSON;

my $ipdir = shift || undef;
my $nodemap = shift || "nodemap.txt";

if (!defined $ipdir) {
	die("Usage nodemap2geojson <storage dir> [nodemap file]");
}

my $line = "";
my $ip = "";

my %handled_ips = ();

open(I,"<$nodemap") or die $!;
open(O,">/tmp/geo.json") or die $!;
print O<<EOJ;
{
"type": "FeatureCollection",
"features": [
{
"type": "Feature",
"properties": {
       "marker-color": "#f36205",
       "marker-size": "small",
       "marker-symbol": ""
      },
"geometry": {
       "type": "MultiPoint",
       "coordinates": [
EOJ

my $geojson = "";

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
				print "Not found in cache. Skipping ${ip} ...\n";
			} elsif (!defined $handled_ips{$ip}) {
				my $json = json_from_file("${ipdir}/_${ip}");
				my $href = decode_json($json);
				if (defined $href->{'longitude'} && defined $href->{'latitude'}) {
					$geojson .= geojson_point($ip, $href->{'longitude'}, $href->{'latitude'});
					$handled_ips{$ip} = 1;
				}
			}
		} else {
			$done = 1;
		}
	}
}

close(I);

$geojson =~ s/,$//;
print O $geojson."\n]}}]}\n";
close(O);

sub json_from_file
{
	my $file = $_[0];
	open(J,"<${file}") or die $!;
	my $json = "";
	while(<J>) {
		$json .= $_;
	}
	close(J);
	return $json;
}


sub geojson_point
{
	my ($ip, $longitude, $latitude) = @_;

my $point=<<EOJ;
[${longitude}, ${latitude} ],
EOJ

	return $point;
}

