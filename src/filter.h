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

#ifndef hoschi_filter_h
#define hoschi_filter_h

#include <string>
#include <iostream>
#include "btc-map.h"

namespace hoschi {

class filter {

protected:

	// to which node we belong, no ownership
	class btc_node *m_parent_node{nullptr};

public:

	filter(btc_node *p)
	 : m_parent_node(p)
	{
	}

	virtual ~filter()
	{
	}

	virtual int collect(uint32_t, const std::string&, const std::string&, const std::string &) = 0;

	virtual	int dump() = 0;
};


class debug_filter : public filter {


public:

	debug_filter(btc_node *p)
	 : filter(p)
	{
	}

	virtual ~debug_filter() override
	{
	}

	int collect(uint32_t version, const std::string& node, const std::string &cmd, const std::string &data) override
	{
		std::cerr<<version<<" "<<node<<" "<<cmd<<std::endl;
		return 0;
	}

	int dump() override
	{
		return 0;
	}
};


class addr_filter : public filter {

	std::map<std::string, std::string> m_addrs;

public:


	addr_filter(btc_node *p)
	 : filter(p)
	{
	}

	virtual ~addr_filter() override
	{
	}

	int collect(uint32_t, const std::string &, const std::string &, const std::string &) override;

	int dump() override;
};

}	// namespace hoschi

#endif

