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

/*!\file array.h
 * \brief Array template class.
 * \author Bob Deblier <bob.deblier@telenet.be>
 * \ingroup CXX_m
 */

#ifndef _TEMPLATE_ARRAY_H
#define _TEMPLATE_ARRAY_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include <new>

namespace beecrypt {

	template <typename T>
		class array
		{
		private:
			T* _data;
			size_t _size;

		public:
			array() throw ()
			{
				_data = 0;
				_size = 0;
			}

			array(size_t size) throw (std::bad_alloc)
			{
				if (size)
				{
					_data = (T*) malloc(size * sizeof(T));
					if (_data == 0)
						throw std::bad_alloc();
				}
				else
					_data = 0;
				_size = size;
			}

			array(const T* data, size_t size) throw (std::bad_alloc)
			{
				_data = (T*) malloc(size * sizeof(T));
				if (_data == 0)
					throw std::bad_alloc();
				_size = size;
				memcpy(_data, data, _size * sizeof(T));
			}

			array(const array& _copy) throw (std::bad_alloc)
			{
				_data = (T*) malloc(_copy._size * sizeof(T));
				if (_data == 0)
					throw std::bad_alloc();
				_size = _copy._size;
				memcpy(_data, _copy._data, _size * sizeof(T));
			}

			~array() throw ()
			{
				if (_data)
					free(_data);
			}

			const array& operator=(const array& _set) throw (std::bad_alloc)
			{
				resize(_set._size);
				if (_size)
					memcpy(_data, _set._data, _size * sizeof(T));

				return *this;
			}

			bool operator==(const array& _cmp) const throw ()
			{
				if (_size != _cmp.size)
					return false;

				if (_size == 0 && _cmp._size == 0)
					return true;

				return !memcmp(_data, _cmp._data, _size * sizeof(T));
			}

			bool operator!=(const array& _cmp) const throw ()
			{
				if (_size != _cmp._size)
					return true;

				if (_size == 0 && _cmp._size == 0)
					return false;

				return memcmp(_data, _cmp._data, _size * sizeof(T));
			}

			T* data() throw ()
			{
				return _data;
			}

			const T* data() const throw ()
			{
				return _data;
			}

			size_t size() const throw ()
			{
				return _size;
			}

			void resize(size_t _newsize) throw (std::bad_alloc)
			{
				if (_newsize)
				{
					_data = (T*) (_data ? realloc(_data, _newsize * sizeof(T)) : malloc(_newsize * sizeof(T)));
					if (_data == 0)
						throw std::bad_alloc();
				}
				else
				{
					if (_data)
					{
						free(_data);
						_data = 0;
					}
				}
				_size = _newsize;
			}

			T& operator[](size_t _n) throw ()
			{
				return _data[_n];
			}

			const T operator[](size_t _n) const throw ()
			{
				return _data[_n];
			}

			const array<T>& operator+=(const array<T>& _rhs) throw ()
			{
				if (_rhs._size)
				{
					size_t _curr = _size;
					resize(_size+_rhs._size);
					memcpy(_data+_curr, _rhs._data, _rhs._size * sizeof(T));
				}
				return *this;
			}
		};

	template<typename T>
		array<T> operator+(const array<T>& _lhs, const array<T>& _rhs)
		{
			array<T> _con(_lhs);

			return _con += _rhs;
		};

	typedef array<byte> bytearray;
	typedef array<javachar> javachararray;
}

#endif

#endif
