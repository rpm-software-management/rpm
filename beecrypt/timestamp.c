/*
 * timestamp.c
 *
 * Java compatible 64-bit timestamp, code
 *
 * Copyright (c) 1999-2000 Virtual Unlimited B.V.
 *
 * Author: Bob Deblier <bob@virtualunlimited.com>
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
 *
 */

#define BEECRYPT_DLL_EXPORT

#include "timestamp.h"

#if HAVE_TIME_H
# include <time.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

javalong timestamp()
{
	#if HAVE_SYS_TIME_H
	# if HAVE_GETTIMEOFDAY
	struct timeval now;
	gettimeofday(&now, 0);
	return (now.tv_sec * 1000LL) + (now.tv_usec / 1000);
	# endif
	#elif HAVE_TIME_H
	return time(0) * 1000LL;
	#else
	# error implement other time function
	#endif
}
