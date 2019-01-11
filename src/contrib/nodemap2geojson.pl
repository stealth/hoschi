#!/usr/bin/perl -w

use strict;
use warnings;
use LWP::UserAgent;
use JSON;

# where is the geoip DB?
my $ipdir = shift || undef;

my $nodemap = shift || "nodemap.txt";

if (!defined $ipdir) {
	die("Usage nodemap2geojson <storage dir> [nodemap file]");
}

my $line = "";
my $ip = "";
my $node = "";

# how many peers does it need to appear on the map?
# 0 means that all nodes will be mapped, regardless of whether
# they could be connected to (as long as they once have been part
# of the network andtherefore appear in an "addr" frame)
# 1 means that only nodes are mapped that were alive at the time of scan
# ... 1000 means that only nodes are mapped that returned 1000 peers
use constant BIG_NODE => 0;

my %handled_ips = ();
my %geojson_points = ();

# a dictionary of hash refs
my %node_graph = ();

open(I,"<$nodemap") or die $!;
open(O,">${nodemap}.geojson") or die $!;
print O<<EOJ;
{
"type": "FeatureCollection",
"features": [
EOJ

my $geojson = "";
for (;;) {
	$line = <I>;
	last if !defined $line;
	chomp($line);
	$line = $line.",";

	# extract first IP so we can build a graph of who is connected to who
	# the first IP of a nodemap file's line is the IP that the following node addresses
	# are originated from (claimed to be connected to that peer)
	if ($line =~ /\[([0-9a-f\.\:]+)\]:\d+,/) {
		$node = $1;
	}

	my $done = 0;
	while ($done == 0) {
		if ($line =~ s/\[([0-9a-f\.\:]+)\]:\d+,//) {
			$ip = $1;
			next if $ip =~ /\.\./;
			$node_graph{$node}->{$ip} = 1;
			print "Checking ${ip} ...\n";
			if (defined $handled_ips{$ip}) {
				next;
			} elsif (!-e "${ipdir}/_${ip}") {
				print "Not found in cache. Skipping ${ip} ...\n";
			} else {
				$handled_ips{$ip} = 1;
			}
		} else {
			$done = 1;
		}
	}
}

close(I);

print "\nGenerating GEOJSON...\n";

my $mapped_ips = 0;
foreach $ip (keys %handled_ips) {
	print "Mapping $ip ...";
	my $connect_cnt = scalar (keys %{$node_graph{$ip}});
	if ($connect_cnt < BIG_NODE) {
		print " skipping.\n";
		next;
	}
	print "\n";

	my $json = json_from_file("${ipdir}/_${ip}");
	my $href = decode_json($json);
	if (defined $href->{'longitude'} && defined $href->{'latitude'}) {
		++$mapped_ips;
		my $pt = geojson_point($ip, $connect_cnt, $href->{'longitude'}, $href->{'latitude'});
		if (!defined $geojson_points{$pt}) {
			$geojson .= $pt.",\n";

			# more than 1 IP can have same geo coordinates
			#$geojson_points{$pt} = 1; #dont reduce
		}
	}
}

$geojson =~ s/,$//;
print O $geojson."]}\n";
close(O);

print "There are ".scalar (keys %handled_ips)." uniqe nodes, $mapped_ips have been mapped.\n";

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
	my ($ip, $count, $longitude, $latitude) = @_;
	my $properties = "";

	if (BIG_NODE > 0) {
		$properties=<<EOP;
"properties":{"name":"${count}"},
EOP
	}

my $point=<<EOJ;
{"type":"Feature",
${properties}"geometry":{
"type": "Point",
"coordinates": [${longitude},${latitude}]
}}
EOJ
	return $point;
}

