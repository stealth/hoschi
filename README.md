hoschi
======

The BTC census project
----------------------

*Hoschi* (common German 90's slang for *dude* that rhymes with *satoshi*) is
a BTC mapping tool which can enumerate all IP's inside the bitcoin network
for further analysis.

It's recommended to use IPv4 as well as IPv6 addresses, since a lot of the
BTC nodes will be IPv6 and you'd get a lot of connect errors without IPv6
connectivity.

You need a good seed-node to start with. If you connect to edge-nodes at
first, your mapping will get stuck.

Build
-----

```
stealth@map:hoschi$ make
[...]

```


Run
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
*Open Street Map*, *Google Maps* or others. You most likely need to cluster
it, otherwise you will just see red dots everywhere. Some maps from a mapping
at Jan 2019 are available down below (click to actually render the map).

[![testnet3](https://github.com/stealth/maps/blob/master/testnet3.jpg)](https://github.com/stealth/maps/blob/master/testnet3.geojson)


Hints
-----

* The `btclog.txt` will be very verbose when the mapper is run. Nevermind the
many `poll()` errors, these happen when the port on a node is closed. The
BTC network is very volatile and therefore lot of nodes distribute outdated
node information.

* I counted ~62k nodes in testnet and ~272k nodes in mainnet. Many of these
are IPv6 nodes, so this technique may be one stepping stone to solve the IPv6
network-scanning problem.

* Do not confuse the amount of nodes running `bitcoind` with the amount of
wallets or BTC addresses. These may easily be found by searching the (local)
blockchain DB.

* When you use a fixed port for outgoing connections (`-p`), the engine needs
to wait the amount of seconds mentioned in `/proc/sys/net/ipv4/tcp_fin_timeout`
before a new reconnect can be done. *Hoschi* is doing seven reconnects per node,
as internal `bitcoind` logic sends only 23% of connected peer addresses per
connection. Nevermind the `fin_wait` delay, it's neglectible for the large amount
of nodes scanned (will just add 60s to the overall mapping time at the end) but
its one parameter that may be tuned if a higher mapping rate is desired
(but its not recommended in order to not flood the network).


