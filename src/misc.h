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

#ifndef hoschi_misc_h
#define hoschi_misc_h

#include <memory>
#include <endian.h>
#include <byteswap.h>


namespace hoschi {

template<typename T> using free_ptr = std::unique_ptr<T, void (*)(T *)>;


namespace timeouts {

enum : uint32_t {
	none		= 0,
	connect		= 30,
	verack		= 120,
	dead		= 180,
	tx_complete	= dead,
	rx_complete	= dead,
	fin_wait	= 60,		// /proc/sys/net/ipv4/tcp_fin_timeout

};

}

namespace numbers {

enum {
	max_send_size	= 0x1000,
	max_paylen	= 0x10000,
	max_rx_size	= 0x1000,

	btc_reconnects	= 7
};

}

// host to btc and reverse
#define htobtc64(x)	htole64(x)
#define htobtc32(x)	htole32(x)
#define htobtc16(x)	htole16(x)
#define btctoh64(x)	le64toh(x)
#define btctoh32(x)	le32toh(x)
#define btctoh16(x)	le16toh(x)

}	// namespace hoschi

#endif

