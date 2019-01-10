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

#include <cstdio>
#include <cstring>
#include <string>
#include <time.h>
#include "log.h"

using namespace std;

namespace hoschi {


int log::init(const string &path)
{
	if (!(f = fopen(path.c_str(), "a")))
		return -1;

	return 0;
}


extern "C" struct tm *localtime_r(const time_t *, struct tm *);

int log::logit(const string &tag, const string &s, time_t t)
{
	if (!f)
		return -1;

	if (!t)
		t = time(nullptr);
	struct tm tm;
	memset(&tm, 0, sizeof(tm));

	localtime_r(&t, &tm);
	char local_date[64] = {0};
	strftime(local_date, sizeof(local_date) - 1, "%a, %d %b %Y %H:%M:%S", &tm);

	string msg = s;
	string::size_type nl = 0;
	if (msg.size() > 1 && (nl = msg.find_last_of("\n")) == msg.size() - 1)
		msg.erase(nl, 1);
	for (string::size_type i = 0; i < msg.size(); ++i) {
		if (!isprint(msg[i]))
			msg[i] = '?';
	}

	fprintf(f, "%s: %s %s\n", local_date, tag.c_str(), msg.c_str());
	fflush(f);
	return 0;
}




} // namespace hoschi

