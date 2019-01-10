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
#include <arpa/inet.h>

#include "missing.h"
#include "btc-map.h"
#include "protocol.h"
#include "global.h"
#include "misc.h"

extern "C" {
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
}


using namespace std;
using namespace hoschi::btc_messages;

namespace hoschi {


uint32_t btc_header::m_expected_magic{htobtc32(numbers::testnet3)};

int btc_header::parse(const string &pkt)
{
	if (pkt.size() < sizeof(m_header))
		return build_error("parse: Header too short", -1);

	memcpy(&m_header, pkt.c_str(), sizeof(m_header));
	if (btctoh32(m_header.magic) != m_expected_magic)
		return build_error("parse: Invalid header magic", -1);
	if (btctoh32(m_header.paylen) > numbers::max_paylen)
		return build_error("parse: Insane large paylen", -1);
	m_header.command[11] = 0;

	return 0;
}


uint32_t btc_header::checksum(const string &payload)
{
	unsigned int hlen = payload.size();
	unsigned char digest[32] = {0}, digest2[32] = {0}, *dptr = digest;
	const void *ptr = payload.c_str();

	free_ptr<EVP_MD_CTX> md_ctx(EVP_MD_CTX_create(), EVP_MD_CTX_delete);
	if (!md_ctx.get())
		return 0;
	for (int i = 0; i < 2; ++i) {
		if (EVP_DigestInit_ex(md_ctx.get(), EVP_sha256(), nullptr) != 1)
			return -1;
		if (EVP_DigestUpdate(md_ctx.get(), ptr, hlen) != 1)
			return -1;
		if (EVP_DigestFinal_ex(md_ctx.get(), dptr, &hlen) != 1)
			return -1;
		ptr = digest;
		dptr = digest2;
	}

	m_header.checksum = htobtc32(*reinterpret_cast<uint32_t *>(dptr));
	m_header.paylen = htobtc32(payload.size());
	return m_header.checksum;
}


static bool is_valid_ip(const char *ip)
{
	// OK, maybe in 2000 i wouldn't have done the validity check
	// with strings but with netmasks, but I think its fast enough,
	// as the bottleneck isn't in our program anyway (but in the net and remote peers).
	// the IP address string was obtained via inet_ntop() so its canonicalized
	// and can't be "010.1.2.03" or similar.
	// Also, the worst case is that we may try to connect to an private IP, which
	// may be annoying but not risky anyway.

	// IPv4 mapped IPv6 addresses
	if (strstr(ip, "::ffff:") == ip || (strstr(ip, ":") && strstr(ip, ".")))
		return 0;

	if (strstr(ip, "10.") == ip)
		return 0;
	if (strstr(ip, "192.168.") == ip)
		return 0;

	if (strstr(ip, "172.16.") == ip)
		return 0;
	if (strstr(ip, "172.17.") == ip)
		return 0;
	if (strstr(ip, "172.18.") == ip)
		return 0;
	if (strstr(ip, "172.19.") == ip)
		return 0;

	// 172.20.x.y - 172.29.x.y
	if (strstr(ip, "172.2") == ip && ip[6] == '.')
		return 0;

	if (strstr(ip, "172.30.") == ip)
		return 0;
	if (strstr(ip, "172.31.") == ip)
		return 0;

	if (strstr(ip, "127.") == ip)
		return 0;

	// LAN multicast
	if (strstr(ip, "224.0.0.") == ip)
		return 0;

	if (strstr(ip, "fc00:") == ip)
		return 0;
	if (strstr(ip, "fd00:") == ip)
		return 0;
	if (strstr(ip, "fe80:") == ip)
		return 0;
	if (strcmp(ip, "::1") == 0 || strcmp(ip, "::") == 0)
		return 0;

	return 1;
}


static bool is_valid_port(uint16_t p)
{
	return p > 1024;
}


int parse_netaddr(const string &s, string &node)
{
	int r = AF_INET6;

	if (s.size() < sizeof(net_addr))
		return -1;

	const auto *na = reinterpret_cast<const net_addr *>(s.c_str());

	char dst[256] = {0};
	if (!inet_ntop(AF_INET6, na->addr_bytes, dst, sizeof(dst) - 1))
		return -1;
	// IPv4 mapped IPv6 address?
	if (string(dst).find("::ffff:") == 0) {
		if (!inet_ntop(AF_INET, na->addr_bytes + 12, dst, sizeof(dst) - 1))
			return -1;
		r = AF_INET;
	}

	if (is_valid_ip(dst) != 1 || is_valid_port(ntohs(na->port)) != 1)
		return -1;

	node = "[";
	node += dst;
	char tmp[32] = {0};
	snprintf(tmp, sizeof(tmp) - 1, "]:%hu", ntohs(na->port));
	node += tmp;
	return r;
}


int parse_netaddr_version(const string &s, string &node)
{
	// a netaddr inside version packet is just missing 4 bytes at the beginning,
	// so prepend some nonsense and call orig function
	return parse_netaddr("1234" + s , node);
}


int make_netaddr_version(const string &node, net_addr_version &na)
{
	string::size_type idx = 0;

	if (node.find("[") != 0)
		return -1;
	if ((idx = node.find("]:")) == string::npos)
		return -1;
	string ip = node.substr(1, idx - 1);
	uint16_t port = 0;
	if (sscanf(node.c_str() + idx + 2, "%hu", &port) != 1)
		return -1;

	if (ip.find(":") == string::npos) {
		ip = "::ffff:" + ip;
	}
	inet_pton(AF_INET6, ip.c_str(), na.addr_bytes);
	na.port = htons(port);
	return 0;
}


string make_version(const string &node)
{
	btc_header hdr("version");

	btc_messages::version vers;
	vers.timestamp = htobtc64(time(nullptr));
	vers.services = htobtc64(numbers::node_network|numbers::node_witness);		// fake it
//	vers.nonce = 0x373723527393;

	make_netaddr_version(node, vers.addr_recv);

	string payload = string(reinterpret_cast<char *>(&vers), sizeof(vers));
	payload += make_valstring(global::client_name);
	uint32_t start_height = 0;
	payload += string(reinterpret_cast<char *>(&start_height), sizeof(start_height));

	if (numbers::btcmap_version >= 70001) {
		char relay = 0;
		payload += string(&relay, 1);
	}

	hdr.checksum(payload);

	return hdr.header_string() + payload;
}


string make_verack()
{
	btc_header hdr("verack");
	hdr.checksum("");

	return hdr.header_string();
}


string make_pong(const string &payload)
{
	btc_header hdr("pong");
	hdr.checksum(payload);

	return hdr.header_string() + payload;
}


string make_getaddr()
{
	btc_header hdr("getaddr");
	hdr.checksum("");

	return hdr.header_string();
}


// doesn't handle uint64_t by intention, as that would exceed our max bufsizes anyway
uint32_t get_valint(const char *data, uint64_t datalen, uint8_t &valsize)
{
	uint32_t r = 0xffffffff;
	valsize = 0;	// signal error

	if (datalen < 1)
		return r;

	unsigned char c = *reinterpret_cast<const unsigned char *>(data);

	if (c < 0xfd) {
		r = c;
		valsize = 1;
	} else if (c < 0xfe) {
		if (datalen >= 3) {
			r =  btctoh16(*reinterpret_cast<const uint16_t *>(data + 1));
			valsize = 3;
		}
	} else if (c == 0xfe) {
		if (datalen >= 5) {
			r = btctoh32(*reinterpret_cast<const uint32_t *>(data + 1));
			valsize = 5;
		}
	}

	return r;
}


string make_valint(uint32_t i)
{
	if (i < 0xfd) {
		char c = i & 0xff;
		return string(&c, 1);
	} else if (i < 0xffff) {
		char s[3] = {char(0xfd), char(i & 0xff), char((i>>8) & 0xff)};
		return string(s, 3);
	} else {
		char s[5] = {char(0xfe), char(i & 0xff), char((i>>8) & 0xff), char((i>>16) & 0xff), char((i>>24) & 0xff)};
		return string(s, 5);
	}
}


string make_valstring(const string &s)
{
	return make_valint(s.size()) + s;
}


}

