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

#include <string>
#include <stdint.h>
#include <memory>
#include <utility>
#include <new>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <iostream>
#include "btc-map.h"
#include "protocol.h"
#include "filter.h"
#include "global.h"
#include "misc.h"

#include <iostream>


using namespace std;


namespace hoschi {


int btc_node::finish_connect()
{
	if (m_state != STATE_CONNECTING) {
		m_state = STATE_FAIL;
		return build_error("finish_connect: wrong state?!", -1);
	}

	int e = 0;
	socklen_t elen = sizeof(e);
	if (getsockopt(m_sfd, SOL_SOCKET, SO_ERROR, &e, &elen) < 0) {
		m_state = STATE_FAIL;
		return build_error("finish_connect::getsockopt:", -1);
	}
	if (e != 0) {
		m_state = STATE_FAIL; errno = e;
		return build_error("finish_connect:", -1);
	}

	if (!(m_filter = new (nothrow) addr_filter(this)))
		return build_error("finish_connect: OOM", -1);

	return 0;
}


// 0 on incomplete write, 1 on complete write, -1 on error
int btc_node::write1()
{
	if (m_tx_msg.size() == 0)
		return build_error("write1::write: Logic error. Calling write1 w/o message to send", -1);

	string::size_type n = numbers::max_send_size;
	if (n > m_tx_msg.size())
		n = m_tx_msg.size();
	ssize_t r = write(m_sfd, m_tx_msg.c_str(), n);
	if (r <= 0) {
		if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS))
			return 0;
		return build_error("write1::write:", -1);
	}

	m_tx_msg.erase(0, r);

	return m_tx_msg.size() == 0;
}


// 0 on incomplete read, 1 on complete read, -1 on error
int btc_node::read1()
{
	char buf[numbers::max_rx_size] = {0};

	size_t n = sizeof(buf);
	if (n > m_rx_needed && m_rx_needed > 0)
		n = m_rx_needed;

	// expecting an entirely new packet? only slurp hdr first
	if (m_rx_needed == 0)
		n = sizeof(btc_header::header);

	ssize_t r = read(m_sfd, buf, n);
	if (r <= 0) {
		// this should not happen, as we only get here via POLLIN, but
		// heavy loaded kernels seem to sometimes mispredict readyness on sockets
		// Furthermore, it may return EINPROGRESS, even if the manpage doesnt say so
		if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS))
			return 0;
		return build_error("read1::read:", -1);
	}

	m_rx_msg += string(buf, r);

	// have a complete hdr?
	if (m_rx_msg.size() < sizeof(btc_header::header)) {
		// we need at minimum a complete header, let rx_ready() tell that if asked
		m_rx_needed = sizeof(btc_header::header) - m_rx_msg.size();
		return 0;
	}

	const auto *hdr = reinterpret_cast<const btc_header::header *>(m_rx_msg.c_str());

	if (btctoh32(hdr->paylen) > numbers::max_paylen)
		return build_error("read1: Peer wants to send insane large payload", -1);
	m_rx_needed = sizeof(btc_header::header) + btctoh32(hdr->paylen) - m_rx_msg.size();

	// hdr + payload complete?
	if (m_rx_needed == 0)
		return 1;

	return 0;
}


string btc_node::parse_msg()
{
	string reply = "error";

	btc_header hdr;
	if (hdr.parse(m_rx_msg) < 0) {
		m_rx_msg = "";
		return build_error("parse_msg:" + string(hdr.why()), reply);
	}

	const string &cmd = hdr.command();

	m_filter->collect(m_version, move(node()), cmd, m_rx_msg);

	if (cmd == "version") {
		if (sizeof(btc_header::header) + sizeof(btc_messages::version) > m_rx_msg.size()) {
			m_rx_msg = "";
			return build_error("parse_msg: Version message too short.", reply);;
		} else {
			m_version = btctoh32(*reinterpret_cast<const uint32_t *>(m_rx_msg.c_str() + sizeof(btc_header::header)));
			reply = make_verack();
		}
	} else if (cmd == "verack") {
		reply = make_getaddr();
	} else if (cmd == "addr") {
		reply = "end";
	} else if (cmd == "ping") {
		reply = make_pong(m_rx_msg.substr(sizeof(btc_header::header), sizeof(uint64_t)));
	} else
		reply = "";

	m_rx_msg = "";
	return reply;
}


int btc_scan::calc_max_fd()
{
	// find the highest fd that is in use
	for (int i = m_max_fd; i >= m_first_fd; --i) {
		if (m_nodes[i] && m_nodes[i]->state() != STATE_NONE) {
			m_max_fd = i;
			return m_max_fd;
		}
		if (m_pfds[i].fd != -1) {
			m_max_fd = i;
			return m_max_fd;
		}
	}

	// return -1 if no fd's are ready to poll at all
	return -1;
}


int btc_scan::cleanup(int fd, bool can_reconnect)
{
	if (m_nodes[fd]) {
		m_nodes[fd]->dump_filter();
		// no more reconnects for this (bad) node
		if (!can_reconnect) {
			m_handled_nodes[m_nodes[fd]->node()] = m_reconnects;
		} else {
			global::logger.logit("btcmap:", "Enqueing node " + m_nodes[fd]->node() + " for reconnect.", m_now);
			m_learned_nodes[m_nodes[fd]->node()] = m_now;
		}
	}

	delete m_nodes[fd];

	m_nodes[fd] = nullptr;
	m_pfds[fd].fd = -1;
	m_pfds[fd].events = m_pfds[fd].revents = 0;

	// not needed: cleanup() will be inside loop and logic says
	// that you won't save fd's to check calling it here
	//calc_max_fd();

	errno = 0;

	return 0;
}


int btc_scan::init(const string &laddr, const string &lport, const string &laddr6, const string &lport6)
{

	// if not connecting from a fixed port, we don't need to wait timeouts::fin_wait
	// seconds for a reconnect to the same node
	if (lport.size() == 0 && lport6.size() == 0)
		m_reconnect_timeout = 2;

	// allocate poll array
	struct rlimit rl;
	rl.rlim_cur = (1<<16);
	rl.rlim_max = (1<<16);

	// as user we cant set it higher
	if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
		errno = 0;
		if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
			return build_error("init::getrlimit:", -1);
		rl.rlim_cur = rl.rlim_max;
		if (setrlimit(RLIMIT_NOFILE, &rl) < 0)
			return build_error("init::getrlimit:", -1);
	}

	if ((m_pfds = new (nothrow) pollfd[rl.rlim_cur]) == nullptr)
		return build_error("init::new: OOM", -1);
	memset(m_pfds, 0, sizeof(struct pollfd) * rl.rlim_cur);
	for (unsigned int i = 0; i < rl.rlim_cur; ++i)
		m_pfds[i].fd = -1;

	if ((m_nodes = new (nothrow) btc_node*[rl.rlim_cur]) == nullptr)
		return build_error("init::new: OOM", -1);
	memset(m_nodes, 0, rl.rlim_cur*sizeof(btc_node *));

	// Now, make the bind addresses ready to later binding when calling connect()

	int r = 0;
	addrinfo hint;

	if (laddr.size() > 0) {
		memset(&hint, 0, sizeof(hint));
		hint.ai_socktype = SOCK_STREAM;
		hint.ai_family = AF_INET;

		if ((r = getaddrinfo(laddr.c_str(), lport.c_str(), &hint, &m_baddr)) != 0)
			return build_error("init::getaddrinfo:" + string(gai_strerror(r)), -1);

		if (m_baddr->ai_family != AF_INET)
			return build_error("init: laddr config option is not a valid IPv4 address.", -1);

	}

	if (laddr6.size() == 0) {
		if (laddr.size() == 0)
			return build_error("init: Neither IPv4 nor IPv6 adddress given to bind to!", -1);
		return 0;
	}

	// Now the same for the IPv6 bind address
	memset(&hint, 0, sizeof(hint));
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_family = AF_INET6;

	if ((r = getaddrinfo(laddr6.c_str(), lport6.c_str(), &hint, &m_baddr6)) != 0)
		return build_error("init::getaddrinfo:" + string(gai_strerror(r)), -1);

	if (m_baddr6->ai_family != AF_INET6)
		return build_error("init: laddr6 config option is not a valid IPv6 address.", -1);

	return 0;
}


btc_node *btc_scan::connect(const std::string &ip, const std::string &port)
{
	m_out_of_sockets = 0;

	int r = 0, sock_fd = -1;
	addrinfo hint, *tai = nullptr;
	memset(&hint, 0, sizeof(hint));
	hint.ai_socktype = SOCK_STREAM;

	if ((r = getaddrinfo(ip.c_str(), port.c_str(), &hint, &tai)) < 0)
		return build_error("connect::getaddrinfo:" + string(gai_strerror(r)), nullptr);

	free_ptr<addrinfo> ai(tai, freeaddrinfo);

	if (ai->ai_family == AF_INET && !m_baddr)
		return build_error("connect: Not bound to IPv4 socket but IPv4 node requested.", nullptr);
	if (ai->ai_family == AF_INET6 && !m_baddr6)
		return build_error("connect: Not bound to IPv6 socket but IPv6 node requested.", nullptr);
	if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
		return build_error("connect: Invalid address family.", nullptr);

	if ((sock_fd = socket(ai->ai_family, SOCK_STREAM, 0)) < 0) {
		m_out_of_sockets = 1;
		return build_error("connect::socket:", nullptr);
	}

	int flags = fcntl(sock_fd, F_GETFL);
	fcntl(sock_fd, F_SETFL, flags|O_NONBLOCK);

	int one = 1;
	setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
	one = 1;
	setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

	// Bind to the right local address (v4 vs. v6) depending on peer node is v4 or v6
	if (ai->ai_family == AF_INET) {
		if (::bind(sock_fd, m_baddr->ai_addr, m_baddr->ai_addrlen) < 0)
			return build_error("connect::bind:", nullptr);
	} else if (ai->ai_family == AF_INET6) {
		if (::bind(sock_fd, m_baddr6->ai_addr, m_baddr6->ai_addrlen) < 0)
			return build_error("connect::bind:", nullptr);
	} else
		return build_error("connect::bind: Unknown address family.", nullptr);

	if (::connect(sock_fd, ai->ai_addr, ai->ai_addrlen) < 0 && errno != EINPROGRESS) {
		close(sock_fd);
		return build_error("connect::connect:", nullptr);
	}


	uint16_t portnum = (uint16_t)strtoul(port.c_str(), nullptr, 10);

	unique_ptr<btc_node> peer(new (nothrow) btc_node(ip, portnum, sock_fd, ai->ai_family));

	if (!peer.get())
		return build_error("connect::new: OOM", nullptr);

	peer->engine(this);	// who is your parent scan engine?
	peer->state(STATE_CONNECTING);
	peer->timer(m_now);

	if (m_first_fd == 0)
		m_first_fd = sock_fd;

	if (sock_fd > m_max_fd)
		m_max_fd = sock_fd;

	m_pfds[sock_fd].fd = sock_fd;
	m_pfds[sock_fd].events = POLLIN|POLLOUT;
	m_pfds[sock_fd].revents = 0;

	return peer.release();
}


btc_node *btc_scan::connect(const std::string &node)
{
	if (node.find("[") != 0)
		return build_error("connect: Invalid node format.", nullptr);

	string::size_type pidx = node.find("]:");
	if (pidx == string::npos)
		return build_error("connect: Invalid node format.", nullptr);

	string ip = node.substr(1, pidx - 1);
	string port = node.substr(pidx + 2);

	return connect(ip, port);
}


btc_node *btc_scan::connect(const string &ip, uint16_t port)
{
	char sport[32] = {0};
	snprintf(sport, sizeof(sport) - 1, "%hu", port);
	return connect(ip, sport);
}


int btc_scan::loop()
{
	int r = 0;

	for (;;) {
		if ((r = poll(m_pfds, m_max_fd + 1, 1000)) < 0)
			continue;

		m_now = time(nullptr);

		for (int i = m_first_fd; i <= m_max_fd; ++i) {

			if (!m_nodes[i] || m_pfds[i].fd == -1)
				continue;

			if (m_pfds[i].revents == 0) {
				if (m_nodes[i]->state() == STATE_CONNECTING && m_now - m_nodes[i]->timer() > timeouts::connect) {
					global::logger.logit("btcmap:", "connect timeout on node " + m_nodes[i]->node(), m_now);
					cleanup(i);
				} else if (m_now - m_nodes[i]->timer() > timeouts::dead) {
					global::logger.logit("btcmap:", "dead timeout on node " + m_nodes[i]->node(), m_now);
					cleanup(i);
				} else if (m_nodes[i]->state() == STATE_FAIL)
					cleanup(i);

				continue;
			}

			if ((m_pfds[i].revents & ~(POLLIN|POLLOUT)) != 0 || m_nodes[i]->state() == STATE_FAIL) {
				global::logger.logit("btcmap:", "poll error on node " + m_nodes[i]->node());
				cleanup(i);
				continue;
			}

			bool tx_complete = 0, rx_complete = 0;

			if (m_pfds[i].revents & POLLIN) {
				if ((r = m_nodes[i]->read1()) < 0) {
					global::logger.logit("btcmap:", "read from node " + m_nodes[i]->node() + " returned error: " + m_nodes[i]->why(), m_now);
					cleanup(i);
					continue;
				}
				// dont update timer here. read1() may return 0 on EINPROGRESS,
				// so we need to check against timeouts::rx_complete and in order to
				// do that later, we can't update time here
				//m_nodes[i]->timer(m_now);

				rx_complete = (r == 1);
			}

			if ((m_pfds[i].revents & POLLOUT) && m_nodes[i]->state() != STATE_CONNECTING) {
				if ((r = m_nodes[i]->write1()) < 0) {
					global::logger.logit("btcmap:", "write to node " + m_nodes[i]->node() + " returned error: " + m_nodes[i]->why(), m_now);
					cleanup(i);
					continue;
				}

				// see above
				//m_nodes[i]->timer(m_now);

				tx_complete = (r == 1);
			}

			m_pfds[i].revents = 0;

			// The FSM
			switch (m_nodes[i]->state()) {
			case STATE_NONE:
				continue;
			case STATE_CONNECTING:
				if (m_nodes[i]->finish_connect() < 0) {
					global::logger.logit("btcmap:", "error when finish_connect on node " + m_nodes[i]->node());
					cleanup(i);
					break;
				}
				m_nodes[i]->state(STATE_CONNECTED);
			// fallthrough
			case STATE_CONNECTED:
				global::logger.logit("btcmap:", "connected to node " + m_nodes[i]->node(), m_now);
				m_nodes[i]->set_msg(make_version(m_nodes[i]->node()));
				m_nodes[i]->timer(m_now);
				m_nodes[i]->state(STATE_SEND_VERSION);
				m_pfds[i].events = POLLOUT;
				break;
			case STATE_SEND_VERSION:
				if (tx_complete) {
					m_nodes[i]->timer(m_now);
					m_nodes[i]->state(STATE_GENERIC_READ);
					m_pfds[i].events = POLLIN;	// expect verack
				} else if (m_now - m_nodes[i]->timer() > timeouts::tx_complete) {
					global::logger.logit("btcmap:", "verack timeout on node " + m_nodes[i]->node(), m_now);
					cleanup(i);
				}
				break;
			case STATE_GENERIC_READ:
				if (rx_complete) {
					m_pfds[i].events = POLLIN;
					m_nodes[i]->timer(m_now);

					string reply = m_nodes[i]->parse_msg();
					if (reply == "error") {
						global::logger.logit("btcmap:", "parse_msg() returned error on node " + m_nodes[i]->node() + ": " + m_nodes[i]->why(), m_now);
						cleanup(i);
						break;
					} else if (reply == "end") {
						// make node re-usable for re-connect and let main connect loop decide about
						// actually doing the reconnect or removal of inode based on connect-count
						cleanup(i, 1);
						break;
					}

					if (reply.size() > 0) {
						m_nodes[i]->state(STATE_GENERIC_WRITE);
						m_nodes[i]->set_msg(reply);
						m_pfds[i].events = POLLOUT;
					}
				} else if (m_now - m_nodes[i]->timer() > timeouts::rx_complete) {
					global::logger.logit("btcmap:", "rx_complete timeout on node " + m_nodes[i]->node(), m_now);
					cleanup(i);
				}
				break;
			case STATE_GENERIC_WRITE:
				if (tx_complete) {
					m_nodes[i]->timer(m_now);
					m_nodes[i]->state(STATE_GENERIC_READ);
					m_pfds[i].events = POLLIN;
				} else if (m_now - m_nodes[i]->timer() > timeouts::tx_complete) {
					global::logger.logit("btcmap:", "wx_complete timeout on node " + m_nodes[i]->node());
					cleanup(i);
				}
				break;
			case STATE_FAIL:
				cleanup(i);
				break;
			}
		}

		int cnt = 0, max_connects = 256;
		for (auto it = m_learned_nodes.begin(); it != m_learned_nodes.end() && cnt < max_connects;) {

			// reconnects are put into m_learned_nodes again by FSM, so check if a sock/bind/connect would make sense
			// in terms of addr:port re-use (we may used fixed src port). The initial 'time' (->second)
			// when learning the node is set to 1, so this will work with newly learned nodes as well.
			// the FSM sets this field to "time of closing"
			if (m_now - it->second <= m_reconnect_timeout) {
				++it;
				continue;
			}

			const string &node = it->first;

			btc_node *bn = nullptr;

			auto h_it = m_handled_nodes.find(node);

			// connect() also sets correct state for FSM. Keep node in learned_nodes map if we
			// have trial-connect errors, since we may be out of fd'd for a periode
			if (h_it == m_handled_nodes.end() || h_it->second < m_reconnects) {

				usleep(15000);

				if (h_it == m_handled_nodes.end())
					global::logger.logit("btcmap:", "Trying 1st connect to node " + node);
				else
					global::logger.logit("btcmap:", "Trying reconnect to node " + node);

				if ((bn = connect(node))) {
					m_nodes[bn->sock()] = bn;
					++m_handled_nodes[node];
					++cnt;
				} else {
					if (out_of_sockets()) {
						global::logger.logit("btcmap:", "Out of file descriptors.", m_now);
						break;
					} else
						global::logger.logit("btcmap:", "Connect error on node " + node + " :" + string(this->why()));
				}
			} else if (h_it != m_handled_nodes.end() && h_it->second >= m_reconnects)
				global::logger.logit("btcmap:", "Node " + node + " reached max reconnect count. Not handling again.", m_now);

			it = m_learned_nodes.erase(it);
		}

		// nothing more to scan?
		if (calc_max_fd() == -1 && m_learned_nodes.size() == 0)
			break;
	}

	return 0;
}


int btc_scan::restore_nodes(const string &path)
{
	free_ptr<FILE> f(fopen(path.c_str(), "r"), [](FILE *fp){fclose(fp);});
	if (!f.get())
		return build_error("restore_nodes:", -1);

	const int lsize = 0x10000;
	unique_ptr<char[]> buf(new (nothrow) char[lsize]);
	if (!buf.get())
		return build_error("restore_nodes: OOM",  -1);

	string::size_type comma = 0;
	for (;!feof(f.get());) {
		memset(buf.get(), 0, lsize);
		if (!fgets(buf.get(), lsize - 1, f.get()))
			break;
		string line = buf.get();

		// replace newline by comma to have easier search loop to split off nodes later
		if (line.size() < 5)
			continue;
		if (line[line.size() - 1] == '\n')
			line[line.size() - 1] = ',';
		else
			line += ",";

		if ((comma = line.find(",")) == string::npos)
			continue;

		string node = line.substr(0, comma);

		if (m_handled_nodes.count(node) == 0) {
			m_handled_nodes.emplace(node, 1);
			global::logger.logit("restore_nodes:", "Adding " + node + " to list of handled nodes.");
		} else
			++m_handled_nodes[node];

		// skip version=...,
		if (line.find("version=", comma) != string::npos)
			comma = line.find(",", comma + 1);
		// skip agent=...,
		if (line.find("agent=", comma) != string::npos)
			comma = line.find(",", comma + 1);

		for (auto prev = comma + 1; prev < line.size();) {
			if ((comma = line.find(",", prev)) == string::npos)
				break;
			if (comma - prev < 5)
				continue;
			node = line.substr(prev, comma - prev - 1);
			if (m_handled_nodes.count(node) == 0 || m_handled_nodes[node] < m_reconnects) {
				m_learned_nodes.emplace(node, 1);
				global::logger.logit("restore_nodes:", "Adding " + node + " to list of learned nodes.");
			}
			prev = comma + 1;
		}
	}

	return 0;
}


}



