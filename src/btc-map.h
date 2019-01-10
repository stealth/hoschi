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

#ifndef hoschi_btcmap_h
#define hoschi_btcmap_h

#include <string>
#include <cstring>
#include <map>
#include <time.h>
#include <cstdint>
#include <cerrno>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "filter.h"
#include "global.h"
#include "misc.h"

#include <iostream>

namespace hoschi {


enum btc_states : int32_t {
	STATE_FAIL	= -1,
	STATE_NONE	= 0,
	STATE_CONNECTING,
	STATE_CONNECTED,
	STATE_SEND_VERSION,
	STATE_GENERIC_READ,
	STATE_GENERIC_WRITE
};


class btc_node {

	std::string m_ip{""}, m_sport{""}, m_sversion{""}, m_err{""};
	std::string m_rx_msg{""}, m_tx_msg{""};

	// how many bytes left until a received msg is complete?
	uint32_t m_rx_needed{0};

	uint32_t m_version{0};
	uint16_t m_port{0};

	btc_states m_state{STATE_NONE};

	int m_sfd{-1}, m_family{AF_INET};

	time_t m_last_access{0};

	filter *m_filter{nullptr};

	// no ownership, just a pointer to existing parent to lookup some things
	class btc_scan *m_parent_engine{nullptr};

	template<class T>
	T build_error(const std::string &msg, T r)
	{
		m_err = "btc_node::";
		m_err += msg;

		if (errno) {
			m_err += ":";
			m_err += strerror(errno);
		}
		errno = 0;
		return r;
	}

public:

	btc_node(const std::string &ip, uint16_t port, int sock, int family)
		: m_ip(ip), m_port(port), m_sfd(sock), m_family(family)
	{
		char tmp[32] = {0};
		snprintf(tmp, sizeof(tmp) - 1, "%hu", port);
		m_sport = tmp;
	}

	void engine(btc_scan *e)
	{
		m_parent_engine = e;
	}

	btc_scan *engine()
	{
		return m_parent_engine;
	}

	virtual ~btc_node()
	{
		delete m_filter;
		close(m_sfd);
	}

	time_t timer()
	{
		return m_last_access;
	}

	void timer(time_t t)
	{
		m_last_access = t;
	}

	btc_states state()
	{
		return m_state;
	}

	void state(btc_states s)
	{
		m_state = s;
	}

	std::string node()
	{
		return "[" + m_ip + "]:" + m_sport;
	}

	std::string ip()
	{
		return m_ip;
	}

	int finish_connect();

	int sock()
	{
		return m_sfd;
	}

	void set_msg(const std::string &m)
	{
		m_tx_msg = m;
	}

	std::string get_msg()
	{
		return m_rx_msg;
	}

	int tx_ready()
	{
		return m_tx_msg.size() == 0;
	}

	int rx_ready()
	{
		return m_rx_needed == 0;
	}

	std::string parse_msg();

	// write one btc message
	int write1();

	// rcv one btc message
	int read1();

	int dump_filter()
	{
		if (m_filter)
			return m_filter->dump();
		return build_error("dump_filter: No filter installed.", -1);
	}

	const char *why()
	{
		return m_err.c_str();
	}
};


class btc_scan {

	std::string m_err{""};

	std::map<std::string, time_t> m_handled_nodes, m_learned_nodes;

	bool m_out_of_sockets{0};

	pollfd *m_pfds{nullptr};
	btc_node **m_nodes{nullptr};

	int m_first_fd{0}, m_max_fd{-1};

	uint32_t m_reconnects{numbers::btc_reconnects};

	time_t m_now{0}, m_reconnect_timeout{timeouts::fin_wait};

	addrinfo *m_baddr{nullptr}, *m_baddr6{nullptr};

	template<class T>
	T build_error(const std::string &msg, T r)
	{
		m_err = "btc_scan::";
		m_err += msg;

		if (errno) {
			m_err += ":";
			m_err += strerror(errno);
		}
		errno = 0;
		return r;
	}

	int calc_max_fd();

	int cleanup(int, bool can_reconnect = 0);

	btc_node *connect(const std::string &ip, const std::string &port);

	btc_node *connect(const std::string &ip, uint16_t port);

	btc_node *connect(const std::string &node);

public:

	btc_scan()
	{
		m_reconnects = numbers::btc_reconnects;
	}

	virtual ~btc_scan()
	{
		freeaddrinfo(m_baddr);
		freeaddrinfo(m_baddr6);

		delete [] m_nodes;
		delete [] m_pfds;
	}

	const char *why()
	{
		return m_err.c_str();
	}

	int init(const std::string &, const std::string &, const std::string &, const std::string &);

	int loop();

	bool out_of_sockets()
	{
		return m_out_of_sockets;
	}

	int restore_nodes(const std::string &);

	bool learned_node(const std::string &s)
	{
		return m_learned_nodes.count(s) > 0;
	}

	void learn_node(const std::string &s)
	{
		// only learn if not handled
		if (m_handled_nodes.count(s) == 0)
			m_learned_nodes.emplace(s, 1);
	}

	void seed_nodes(const std::map<std::string, int> &seeds)
	{
		for (const auto &it : seeds)
			learn_node(it.first);
	}

	bool handled_node(const std::string &s)
	{
		return m_handled_nodes.count(s) > 0;
	}
};


}

#endif

