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

/*!\file Provider.h
 * \ingroup CXX_SECURITY_m
 */

#ifndef _CLASS_PROVIDER_H
#define _CLASS_PROVIDER_H

#ifdef __cplusplus

#include "beecrypt/c++/mutex.h"
using beecrypt::mutex;
#include "beecrypt/c++/lang/String.h"
using beecrypt::lang::String;
#include "beecrypt/c++/util/Properties.h"
using beecrypt::util::Properties;

#include <unicode/ucnv.h>
#include <map>
using std::map;

namespace beecrypt {
	namespace security {
		class BEECRYPTCXXAPI Provider : public Properties
		{
			friend class Security;

			private:
				String _name;
				String _info;
				double _vers;

				mutex _lock;
				UConverter* _conv;

				typedef void* (*instantiator)();
				typedef map<String,instantiator> instantiator_map;

				instantiator_map _imap;

				instantiator getInstantiator(const String& name) const;

			protected:
				#if WIN32
				HANDLE _dlhandle;
				#else
				void* _dlhandle;
				#endif

				Provider(const String& name, double version, const String& info);

			public:
				virtual ~Provider();

				void put(const String& key, const String& value);

				const String& getName() const throw ();
				const String& getInfo() const throw ();
				double getVersion() const throw ();
		};
	}
}

#endif

#endif
