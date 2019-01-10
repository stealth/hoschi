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

#ifndef hoschi_log_h
#define hoschi_log_h

#include <string>
#include <cstdio>

namespace hoschi {

class log {

	std::FILE *f{nullptr};

public:

	log()
	{
	}

	virtual ~log()
	{
		if (f)
			std::fclose(f);
	}

	int init(const std::string &);

	int logit(const std::string &, const std::string &, time_t t = 0);

};


} // namespace hoschi

#endif

