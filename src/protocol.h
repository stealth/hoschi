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

#ifndef hoschi_protocol_h
#define hoschi_protocol_h

#include <string>
#include <stdint.h>
#include <cstring>
#include <cerrno>
#include "misc.h"


namespace hoschi {

namespace numbers {

enum {
	main		= 0xD9B4BEF9,
	testnet		= 0xDAB5BFFA,
	testnet3	= 0x0709110B,
	namecoin	= 0xFEB4BEF9
};

enum {
	btcmap_version = 70015
};

enum {
	node_network	= 1,
	node_getutxo	= 2,
	node_bloom	= 4,
	node_witness	= 8,
	node_network_limited	= 1024
};

}	// numbers namespace


class btc_header {

	std::string m_err{""};

	static uint32_t m_expected_magic;

	template<class T>
	T build_error(const std::string &msg, T r)
	{
		m_err = "btc_header::";
		m_err += msg;

		if (errno) {
			m_err += ":";
			m_err += strerror(errno);
		}
		errno = 0;
		return r;
	}

public:

	struct header {
		uint32_t magic{m_expected_magic};	// LE
		char command[12]{0};			// ASCII
		uint32_t paylen{0};			// LE
		uint32_t checksum{0};			// LE
	} __attribute__((packed));

private:

	header m_header;

public:

	btc_header(const std::string &cmd = "nonsense")
	{
		std::string::size_type n = cmd.size();
		if (n >= sizeof(m_header.command))
			n = sizeof(m_header.command) - 1;

		memcpy(m_header.command, cmd.c_str(), n);
		m_header.command[11] = 0;
	}

	virtual ~btc_header()
	{
	}

	std::string command()
	{
		return std::string(m_header.command);
	}

	std::string header_string()
	{
		return std::string(reinterpret_cast<const char *>(&m_header), sizeof(m_header));
	}

	uint32_t checksum(const std::string &);

	int parse(const std::string &);

	const char *why()
	{
		return m_err.c_str();
	}
};


namespace btc_messages {


// net_addr struct as used in version >= 31402
struct net_addr {
	uint32_t time{0};
	uint64_t services{0};
	uint8_t addr_bytes[16]{0};
	uint16_t port{0};
} __attribute__((packed));


// net_addr struct as used in version packet and when version < 31402
struct net_addr_version {
	uint64_t services{0};
	uint8_t addr_bytes[16]{0};
	uint16_t port{0};
} __attribute__((packed));


struct version {
	int32_t version{htobtc32(numbers::btcmap_version)};
	uint64_t services{0};
	int64_t timestamp{0};
	net_addr_version addr_recv;	// where its sent to
	net_addr_version addr_from;	// our address
	uint64_t nonce{0};
	// following the variable portion user_agent, start_height, relay
} __attribute__((packed));

}	// btc_messages namespace


std::string make_version(const std::string &);

std::string make_verack();

std::string make_pong(const std::string &);

std::string make_getaddr();

uint32_t get_valint(const char *, uint64_t, uint8_t &);

std::string make_valint(uint32_t);

std::string make_valstring(const std::string &);

int parse_netaddr(const std::string &, std::string &);

int parse_netaddr_version(const std::string &, std::string &);


}	// hoschi namespace

#endif

