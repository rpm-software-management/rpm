/*
 * Copyright (c) 2004 Beeyond Software Holding BV
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define BEECRYPT_CXX_DLL_EXPORT

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/timestamp.h"
#include "beecrypt/c++/util/Date.h"

#include <unicode/datefmt.h>

namespace {
	#if WIN32
	__declspec(thread) String* result = 0;
	__declspec(thread) DateFormat* format = 0;
	#else
	# if __GNUC__ && __GNUC_PREREQ (3, 3)
	__thread String* result = 0;
	__thread DateFormat* format = 0;
	# else
	#  warning Date.toString() method routine is not multi-thread safe
	String* result = 0;
	DateFormat* format = 0;
	# endif
	#endif
}

using namespace beecrypt::util;

Date::Date() throw ()
{
	_time = timestamp();
}

Date::Date(javalong time) throw ()
{
	_time = time;
}

const Date& Date::operator=(const Date& set) throw ()
{
	_time = set._time;
	return *this;
}

bool Date::operator==(const Date& cmp) const throw ()
{
	return _time == cmp._time;
}

bool Date::operator!=(const Date& cmp) const throw ()
{
	return _time != cmp._time;
}

bool Date::after(const Date& cmp) const throw ()
{
	return _time > cmp._time;
}

bool Date::before(const Date& cmp) const throw ()
{
	return _time < cmp._time;
}

javalong Date::getTime() const throw ()
{
	return _time;
}

void Date::setTime(javalong time) throw ()
{
	_time = time;
}

const String& Date::toString() const
{
	if (!format)
		format = DateFormat::createDateTimeInstance();

	if (!result)
		result = new String();

	*result = format->format((UDate) _time, *result);

	return *result;
}
