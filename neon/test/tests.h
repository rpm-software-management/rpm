/* 
   Stupidly simple test framework
   Copyright (C) 2001, Joe Orton <joe@manyfish.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

#ifndef TESTS_H
#define TESTS_H 1

#include <stdio.h>

/* prototype of test function. */
typedef int (*test_func)(void);

/* array of test functions to call. */
extern test_func tests[];

void i_am(const char *testname);

#define OK 0
#define FAIL 1
#define FAILHARD 2 /* fail and skip succeeding tests. */

/* are __FUNCTION__ and __LINE__ gcc-isms? Probably. */

#define ON(x) do { char _buf[50]; sprintf(_buf, "%s line %d", __FUNCTION__, __LINE__ ); i_am(_buf); if ((x)) return FAIL; } while(0)

#define TESTFUNC do { i_am(__FUNCTION__); } while(0)

#define ONN(n,x) do { i_am(n); if ((x)) return FAIL; } while(0)

#define CALL(x) do { int ret = (x); if (ret != OK) return ret; } while (0)

#endif /* TESTS_H */
