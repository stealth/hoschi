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
#include <cstdio>
#include "btc-map.h"
#include "protocol.h"
#include "filter.h"
#include "global.h"
#include "config.h"
#include "misc.h"


using namespace std;
using namespace hoschi::btc_messages;

namespace hoschi {



int addr_filter::collect(uint32_t version, const string &node, const string &cmd, const string &data)
{
	global::logger.logit("addr_filter:", node + " " + cmd);

	if (cmd != "addr")
		return 0;

	int nsize = sizeof(net_addr);
	if (version < 31402)
		nsize = sizeof(net_addr_version);	// missing the time field

	if (data.size() < sizeof(btc_header::header) + nsize + 1)
		return -1;

	const char *payload = data.c_str() + sizeof(btc_header::header);

	uint8_t intsize = 0;
	uint32_t naddrs = get_valint(payload, data.size() - sizeof(btc_header::header), intsize);

	// parse error for valint?
	if (!intsize || naddrs > numbers::max_paylen)
		return -1;

	// can't wrap b/c of check above
	if (naddrs*nsize + sizeof(btc_header::header) + intsize > data.size())
		return -1;

	int family = 0;
	for (unsigned int i = 0; i < naddrs; ++i) {
		string na = string(payload + intsize + i*nsize, nsize), lnode = "";
		if (nsize == sizeof(net_addr))
			family = parse_netaddr(na, lnode);
		else
			family = parse_netaddr_version(na, lnode);

		// family may be -1 for private IP address space. Only learn node if not already handled.
		// Otherwise we may add nodes that are already in STATE_CONNECTING, causing double-connects
		// and/or errors for port-reuse.
		if (family >= 0) {
			if (!m_parent_node->engine()->handled_node(lnode) && !m_parent_node->engine()->learned_node(lnode)) {
				global::logger.logit("addr_filter:", "learned node " + lnode + " from " + node);
				m_parent_node->engine()->learn_node(lnode);
			}

			auto it = m_addrs.find(node);
			if (it != m_addrs.end()) {
				if (it->second.find(lnode) == string::npos)
					it->second += "," + lnode;
			} else
				m_addrs.emplace(node, lnode);
		}
	}

	return 0;
}


int addr_filter::dump()
{
	free_ptr<FILE> f(fopen(config::dump_file.c_str(), "a"), [](FILE *fp){fclose(fp);});
	if (!f.get())
		return -1;
	for (const auto &it : m_addrs)
		fprintf(f.get(), "%s,%s\n", it.first.c_str(), it.second.c_str());

	return 0;
}


} // namespace hoschi

