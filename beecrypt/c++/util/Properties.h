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

/*!\file Properties.h
 * \ingroup CXX_UTIL_m
 */

#ifndef _CLASS_PROPERTIES_H
#define _CLASS_PROPERTIES_H

#ifdef __cplusplus

#include "beecrypt/c++/mutex.h"
using beecrypt::mutex;
#include "beecrypt/c++/io/InputStream.h"
using beecrypt::io::InputStream;
#include "beecrypt/c++/io/OutputStream.h"
using beecrypt::io::OutputStream;
#include "beecrypt/c++/lang/String.h"
using beecrypt::lang::String;
#include "beecrypt/c++/util/Enumeration.h"
using beecrypt::util::Enumeration;

#include <map>
using std::map;

namespace beecrypt {
	namespace util {
		class BEECRYPTCXXAPI Properties
		{
			private:
				typedef map<String,String> properties_map;

				class PropEnum : public Enumeration
				{
					public:
						properties_map::const_iterator _it;
						properties_map::const_iterator _end;

					public:
						PropEnum(const properties_map&) throw ();
						virtual ~PropEnum() throw ();

						virtual bool hasMoreElements() throw ();
						virtual const void* nextElement() throw (NoSuchElementException);
				};

				properties_map _pmap;

				mutex _lock;

			protected:
				const Properties* defaults;

			public:
				Properties();
				Properties(const Properties& copy);
				Properties(const Properties* defaults);
				~Properties();

				const String* getProperty(const String& key) const throw ();
				const String* getProperty(const String& key, const String& defaultValue) const throw ();

				void setProperty(const String& key, const String& value) throw ();

				Enumeration* propertyNames() const;

				void load(InputStream& in) throw (IOException);
				void store(OutputStream& out, const String& header) throw (IOException);
		};
	}
}

#endif

#endif
