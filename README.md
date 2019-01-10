hoschi
======

*Hoschi* (common German 90's slang for *dude* that rhymes with *satoshi*) is
a BTC mapping tool which can enumerate all IP's inside the bitcoin network
for further analysis.

It's recommended to use IPv4 as well as IPv6 addresses, since a lot of the
BTC nodes will be IPv6 and you'd get a lot of connect errors without IPv6
connectivity.

You need a good seed-node to start with. If you connect to edge-nodes at
first, your mapping will get stuck.

build
-----

```
stealth@map:hoschi$ make
[...]

```


run
---

```
stealth@map:hoschi$ src/build/hoschi

hoschi v0.1 (C) Sebastian Krahmer -- https://github.com/stealth/hoschi

Usage:

hoschi <-4 ip4> <-6 ip6> [-p lport] [-r node-file] [-d node-file] [-l logfile] <-s seed-node> [-s seednode] ...
        -4 -- local IPv4 address to bind to
        -6 -- local IPv6 address to bind to
        -p -- local port to bind to (default any)
        -r -- restore from previous mapping's result dumped into '-d'
        -d -- dump (append) found nodes to this file; default: nodemap.txt
        -l -- log what we do to this file; default: btclog.txt
        -s -- seed with this node. format is [ip]:port where ip is v4 or v6. [127.0.0.1]:8333 if you run a local bitcoind

```

Note that *hoschi* will map `-testnet` unless you change the magic values in
`protocol.cc` to use the main BTC network.

*Hoschi* has small runtime footprint (C++11! :), although it may handle 10k's
of connections simultaneously. There's a `usleep()` delay in the connect loop
since a lot of cable modems may otherwise loose packets if you connect too
fast. Mapping the entire BTC main network with that delay took 2h on a
100MBit/s up-link on the (resource-)cheapest VPS machine that I found.

There are some Perl scripts inside `contrib` that can map the IP addresses to
Geo locations and build `geojson` maps which can be loaded into
*Open Street-view*, *Google Maps* or others. You most likely need to cluster
it, otherwise you will just see red dots everywhere. Some maps from a mapping
at Jan 2019 are available here.

![testnet3](https://embed.github.com/view/geojson/stealth/hoschi/master/testnet3.geojson)

