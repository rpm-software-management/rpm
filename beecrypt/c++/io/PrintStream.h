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

/*!\file PrintStream.h
 * \ingroup CXX_IO_m
 */

#ifndef _CLASS_PRINTSTREAM_H
#define _CLASS_PRINTSTREAM_H

#ifdef __cplusplus

#include "beecrypt/c++/array.h"
using beecrypt::array;
using beecrypt::bytearray;
#include "beecrypt/c++/io/FilterOutputStream.h"
using beecrypt::io::FilterOutputStream;

#include <unicode/ucnv.h>

namespace beecrypt {
	namespace io {
		class BEECRYPTCXXAPI PrintStream : public FilterOutputStream
		{
			private:
				bool _closed;
				bool _error;
				bool _flush;
				UConverter* _loc;

				void print(const UChar*, size_t) throw ();

			protected:
				void setError() throw ();

			public:
				PrintStream(OutputStream& out, bool autoflush = false, const char* encoding = 0);
				virtual ~PrintStream();

				virtual void close() throw ();
				virtual void flush() throw ();
				virtual void write(byte) throw ();
				virtual void write(const byte*, size_t, size_t) throw ();

				bool checkError() throw ();
				void print(bool) throw ();
				void print(javachar) throw ();
				void print(javaint) throw ();
				void print(javalong) throw ();
				void print(javashort) throw ();
		//		void print(javafloat) throw ();
		//		void print(javadouble) throw ();
		//		void print(const char*) throw ();
				void print(const array<javachar>&) throw ();
				void print(const String&) throw ();
				void println() throw ();
				void println(bool) throw ();
				void println(javachar) throw ();
				void println(javaint) throw ();
				void println(javalong) throw ();
				void println(javashort) throw ();
		//		void println(javafloat) throw ();
		//		void println(javadouble) throw ();
		//		void println(const char*) throw ();
				void println(const array<javachar>&) throw ();
				void println(const String&) throw ();
		};
	}
}

#endif

#endif
