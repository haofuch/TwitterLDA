#pragma once

#include <cstdio>
#include <algorithm>
#include <limits>

namespace utility
{
	char *new_string(char *str);
	char *new_string(const char *str1, const char *str2);
	bool file_exist(const char *path);

	template <class T> T **new_array(size_t n1, size_t n2)
	{
		T **ptr = new T*[n1];
		ptr[0] = new T[n1 * n2];
		for (size_t i = 1; i < n1; ++i)
		{
			ptr[i] = ptr[i - 1] + n2;
		}
		return ptr;
	}

	template <class T> void delete_array(T **ptr)
	{
		delete[] ptr[0];
		delete[] ptr;
	}

	template <class T> size_t get_varint(const char *data, T *value, size_t size = std::numeric_limits<size_t>::max())
	{
		if (size == 0) return 0;
		size_t i = 0, j = 0;
		T v = 0;
		while (true)
		{
			unsigned char x = (unsigned char)data[i++];
			v |= (x & 0x7f) << j;
			j += 7;
			if ((x & 0x80) == 0) break;
			if (i >= size) return 0;
			if (j >= sizeof(T) * 8) return 0;
		}
		*value = v;
		return i;
	}

	template <class T> size_t get_sparse_array(const char *data, T *values, size_t length, size_t size = std::numeric_limits<size_t>::max())
	{
		if (size == 0) return 0;
		size_t offset = 0;
		size_t count;
		size_t more = get_varint(data, &count, size);
		if (more == 0) return 0;
		offset += more;
		for (size_t i = 0, index = 0; i < count; ++i)
		{
			size_t delta;
			more = get_varint(data + offset, &delta, size - offset);
			index += delta;
			if (more == 0 || index >= length) return 0;
			offset += more;
			more = get_varint(data + offset, values + index, size - offset);
			if (more == 0) return 0;
			offset += more;
		}
		return offset;
	}

	template <class T> size_t set_varint(char *data, T value, size_t size = std::numeric_limits<size_t>::max())
	{
		if (size == 0) return 0;
		size_t i = 0;
		while (true)
		{
			char x = value & 0x7f;
			value >>= 7;
			if (value)
			{
				x |= 0x80;
				data[i++] = x;
				if (i >= size) return 0;
			}
			else
			{
				data[i++] = x;
				break;
			}
		}
		return i;
	}

	template <class T> size_t set_sparse_array(char *data, const T *values, size_t length, const T default_value = 0, size_t size = std::numeric_limits<size_t>::max())
	{
		if (size == 0) return 0;
		size_t offset = 0;
		size_t count = 0;
		for (size_t i = 0; i < length; ++i)
		{
			if (values[i] != default_value) ++count;
		}
		size_t more = set_varint(data, count, size);
		if (more == 0) return 0;
		offset += more;
		for (size_t i = 0, index = 0; i < length; ++i)
		{
			if (values[i] == default_value) continue;
			size_t delta = i - index;
			index = i;
			more = set_varint(data + offset, delta, size - offset);
			if (more == 0) return 0;
			offset += more;
			more = set_varint(data + offset, values[i], size - offset);
			if (more == 0) return 0;
			offset += more;
		}
		return offset;
	}

	class read_buffer
	{
	public:
		read_buffer()
		{
			_offset = 0;
			_size = 0;
			_buffer = nullptr;
		}

		read_buffer(char *buffer, size_t size)
		{
			_offset = 0;
			_size = size;
			_buffer = buffer;
		}

		size_t skip(size_t count)
		{
			if (_offset + count > _size) return 0;
			_offset += count;
			return count;
		}

		template <class T> size_t read_varint(T *value)
		{
			if (_offset >= _size) return 0;
			size_t more = get_varint(_buffer + _offset, value, _size - _offset);
			_offset += more;
			return more;
		}

		template <class T> size_t read(T *value)
		{
			if (_offset + sizeof(T) > _size) return 0;
			*value = *((T*)(_buffer + _offset));
			_offset += sizeof(T);
			return sizeof(T);
		}

		template <class T> size_t read_sparse_array(T *values, size_t length)
		{
			if (_offset >= _size) return 0;
			size_t more = get_sparse_array(_buffer + _offset, values, length, _size - _offset);
			_offset += more;
			return more;
		}

		void reset()
		{
			_offset = 0;
		}

		char *buffer() const
		{
			return _buffer;
		}

		size_t size() const
		{
			return _size;
		}

		size_t offset() const
		{
			return _offset;
		}

	private:
		char *_buffer;
		size_t _offset, _size;
	};

	class write_buffer
	{
	public:
		write_buffer(size_t capacity = 1 << 20)
		{
			_offset = 0;
			_capacity = capacity;
			_buffer = new char[capacity];
		}

		~write_buffer()
		{
			delete[] _buffer;
			_buffer = nullptr;
		}

		template <class T> size_t write_varint(const T value)
		{
			while (true)
			{
				if (_offset < _capacity)
				{
					size_t more = set_varint(_buffer + _offset, value, _capacity - _offset);
					if (more > 0)
					{
						_offset += more;
						return more;
					}
				}
				expand();
			}
		}

		template <class T> size_t write(const T value)
		{
			while (true)
			{
				if (_offset + sizeof(T) <= _capacity)
				{
					*((T*)(_buffer + _offset)) = value;
					_offset += sizeof(T);
					return sizeof(T);
				}
				expand();
			}
		}

		template <class T> size_t write_sparse_array(const T *values, size_t length, const T default_value = 0)
		{
			while (true)
			{
				if (_offset < _capacity)
				{
					size_t more = set_sparse_array(_buffer + _offset, values, length, default_value, _capacity - _offset);
					if (more > 0)
					{
						_offset += more;
						return more;
					}
				}
				expand();
			}
		}

		void clear()
		{
			_offset = 0;
		}

		char *buffer() const
		{
			return _buffer;
		}

		size_t size() const
		{
			return _offset;
		}

		size_t offset() const
		{
			return _offset;
		}

	private:
		char *_buffer;
		size_t _offset, _capacity;

		void expand()
		{
			size_t new_capacity = _capacity * 2;
			if (new_capacity == 0) new_capacity = 1;
			char *new_buffer = new char[new_capacity];
			memcpy(new_buffer, _buffer, _offset);
			delete[] _buffer;
			_buffer = new_buffer;
			_capacity = new_capacity;
		}
	};

	template <class T>
	size_t fread(T *data, size_t num, FILE *fp)
	{
		const size_t block_size = (1 << 30) / sizeof(T);
		size_t count = 0;
		while (num > 0)
		{
			size_t more = std::min(block_size, num);
			size_t got = fread(data, sizeof(T), more, fp);
			if (got == 0) break;
			count += got;
			num -= got;
			data += got;
		}
		return count;
	}

	template<class T>
	size_t fwrite(T *data, size_t num, FILE *fp)
	{
		const size_t block_size = (1 << 30) / sizeof(T);
		size_t count = 0;
		while (num > 0)
		{
			size_t more = std::min(block_size, num);
			size_t got = fwrite(data, sizeof(T), more, fp);
			count += got;
			num -= got;
			data += got;
			if (got != more) break;
		}
		return count;
	}

	struct string_hasher
	{
		size_t operator()(const char *str) const;
	};

	struct string_predicate
	{
		bool operator()(const char *str1, const char *str2) const;
	};

	template <class T>
	struct index_comparer
	{
		const T& _keys;
		bool _reversed;

		index_comparer(T& keys, bool reversed) : _keys(keys), _reversed(reversed)
		{
		}

		bool operator()(int i, int j) const
		{
			return _reversed ? (_keys[j] < _keys[i]) : (_keys[i] < _keys[j]);
		}
	};
};
