/*
 * This file is part of the hoschi p2p scan engine.
 *
 * (C) 2019 by Sebastian Krahmer,
 *             sebastian [dot] krahmer [at] gmail [dot] com
 *
 * hoschi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * hoschi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with hoschi. If not, see <http://www.gnu.org/licenses/>.
 */

#include <map>
#include <string>
#include <cstring>
#include <cstdlib>
#include <signal.h>
#include <iostream>
#include "config.h"
#include "global.h"
#include "btc-map.h"


using namespace std;
using namespace hoschi;


void usage()
{
	cout<<"Usage:\n\nhoschi <-4 ip4> <-6 ip6> [-p lport] [-r node-file] [-d node-file] [-l logfile] <-s seed-node> [-s seednode] ...\n"
	    <<"\t-4 -- local IPv4 address to bind to\n"
	    <<"\t-6 -- local IPv6 address to bind to\n"
	    <<"\t-p -- local port to bind to (default any)\n"
	    <<"\t-r -- restore from previous mapping's result dumped into '-d'\n"
	    <<"\t-d -- dump (append) found nodes to this file; default: nodemap.txt\n"
	    <<"\t-l -- log what we do to this file; default: btclog.txt\n"
	    <<"\t-s -- seed with this node. format is [ip]:port where ip is v4 or v6. [127.0.0.1]:8333 if you run a local bitcoind\n\n";

	exit(1);
}


int main(int argc, char **argv)
{
	int c = 0;
	map<string, int> seeds;
	struct sigaction sa;
	string l4addr = "", l6addr = "", lport = "";

	cout<<"\nhoschi v0.1 (C) Sebastian Krahmer -- https://github.com/stealth/hoschi\n\n";

	for (;(c = getopt(argc, argv, "r:d:s:4:6:p:")) != -1;) {
		switch (c) {
		case 'r':
			config::restore_file = optarg;
			break;
		case 'd':
			config::dump_file = optarg;
			break;
		case 'l':
			config::log_file = optarg;
			break;
		case 's':
			seeds.emplace(optarg, 1);
			break;
		case '4':
			l4addr = optarg;
			break;
		case '6':
			l6addr = optarg;
			break;
		case 'p':
			lport = optarg;
			break;
		default:
			usage();
		}
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &sa, nullptr);
	sigaction(SIGPIPE, &sa, nullptr);

	if (!l4addr.size() && !l6addr.size())
		usage();

	global::logger.init(config::log_file);
	global::logger.logit("main:", "Starting scan.");

	cout<<"Starting scan. Check "<<config::log_file<<" for progress.\n";

	btc_scan btcm;
	if (btcm.init(l4addr, lport, l6addr, lport) < 0) {
		cerr<<"Error "<<btcm.why()<<endl;
		exit(1);
	}

	if (seeds.size() > 0)
		btcm.seed_nodes(seeds);

	if (config::restore_file.size() > 0)
		btcm.restore_nodes(config::restore_file);

	if (btcm.loop() < 0)
		cerr<<"Error in scan engine: "<<btcm.why()<<endl;

	cout<<"scan engine exited gracefully.\n";
	global::logger.logit("main:", "Graceful end of scan.");
	return 0;
}

