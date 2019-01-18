#include "file_reader.h"
#include "utility.h"
#include <cstdio>
#include <cstring>

file_reader::file_reader(const char *path, size_t buffer_size)
{
	_fp = fopen(path, "rb");
	_buffer = new char[buffer_size];
	_buffer_offset = 0;
	_buffer_count = 0;
	_buffer_size = buffer_size;

	_position = 0;
#ifdef  _WIN64
	_fseeki64(_fp, 0, SEEK_END);
	_size = _ftelli64(_fp);
	_fseeki64(_fp, 0, SEEK_SET);
#else
	fseek(_fp, 0, SEEK_END);
	_size = ftell(_fp);
	fseek(_fp, 0, SEEK_SET);
#endif
}

file_reader::~file_reader()
{
	close();
}

void file_reader::trim()
{
	memmove(_buffer, _buffer + _buffer_offset, _buffer_count);
	_buffer_offset = 0;
}

void file_reader::close()
{
	if (_fp != nullptr)
	{
		fclose(_fp);
		_fp = nullptr;
		delete[] _buffer;
		_buffer = nullptr;
	}
}

void file_reader::reset()
{
	if (_fp != nullptr)
	{
		fseek(_fp, 0, SEEK_SET);
		_buffer_offset = 0;
		_buffer_count = 0;
		_position = 0;
	}
}

size_t file_reader::buffer_size() const
{
	return _buffer_size;
}

long long file_reader::size() const
{
	return _size;
}

long long file_reader::position() const
{
	return _position;
}

file_item file_reader::get_item(bool fixed_buffer)
{
	file_item item;
	item.data = _buffer + _buffer_offset;
	item.size = segment(item.data, _buffer_count);
	_buffer_offset += item.size;
	_buffer_count -= item.size;
	_position += item.size;
	if (item.size > 0 || fixed_buffer) return item;

	// incomplete item in buffer, read more data
	trim();
	while (true)
	{
		size_t more = utility::fread(_buffer + _buffer_count, _buffer_size - _buffer_count, _fp);
		if (more == 0) return item; // reach end of file, item is still incomplete

		_buffer_count += more;
		item.data = _buffer;
		item.size = segment(item.data, _buffer_count);
		_buffer_offset += item.size;
		_buffer_count -= item.size;
		_position += item.size;
		if (item.size > 0) return item;
		if (_buffer_count < _buffer_size) return item; // incomplete item reaches end of file

		// big item, enlarge buffer
		char *new_buffer = new char[_buffer_size * 2];
		memcpy(new_buffer, _buffer, _buffer_count);
		delete[] _buffer;
		_buffer = new_buffer;
		_buffer_size *= 2;
	}
}

bool file_reader::unget_item(file_item item)
{
	if (_buffer + _buffer_offset != item.data + item.size) return false;
	_buffer_offset -= item.size;
	_buffer_count += item.size;
	_position -= item.size;
	return true;
}

text_file_reader::text_file_reader(const char *path, size_t buffer_size) : file_reader(path, buffer_size)
{
}

size_t text_file_reader::segment(char *data, size_t size)
{
	size_t n = 0;
	while (n < size && data[n] != '\r' && data[n] != '\n') ++n;
	if (n >= size) return 0;
	char tail = data[n];
	data[n] = '\0';
	if (n + 1 < size && data[n + 1] != tail && (data[n + 1] == '\r' || data[n + 1] == '\n')) ++n;
	return n + 1;
}

tweet_file_reader::tweet_file_reader(const char *path, size_t buffer_size) : file_reader(path, buffer_size)
{
	reset();
}

size_t tweet_file_reader::segment(char *data, size_t size)
{
	utility::read_buffer buffer(data, size);
	int user;
	if (buffer.read_varint(&user) == 0) return 0;
	int count;
	if (buffer.read_varint(&count) == 0) return 0;
	for (int i = 0; i < count; ++i)
	{
		int word;
		if (buffer.read_varint(&word) == 0) return 0;
	}
	return buffer.offset();
}

tweet_param_file_reader::tweet_param_file_reader(const char *path, size_t buffer_size) : file_reader(path, buffer_size)
{
}

size_t tweet_param_file_reader::segment(char *data, size_t size)
{
	utility::read_buffer buffer(data, size);
	int topic;
	if (buffer.read_varint(&topic) == 0) return 0;
	int count;
	if (buffer.read_varint(&count) == 0) return 0;
	size_t num = (count + 7) / 8;
	if (buffer.offset() + num > size) return 0;
	return buffer.offset() + num;
}

user_param_file_reader::user_param_file_reader(const char *path, int topic_num, size_t buffer_size) : file_reader(path, buffer_size)
{
	_topic_num = topic_num;
	_temp_buffer = new int[topic_num];
}

user_param_file_reader::~user_param_file_reader()
{
	delete[] _temp_buffer;
}

int user_param_file_reader::topic_num() const
{
	return _topic_num;
}

size_t user_param_file_reader::segment(char *data, size_t size)
{
	utility::read_buffer buffer(data, size);
	int user;
	if (buffer.read_varint(&user) == 0) return 0;
	if (buffer.read_sparse_array(_temp_buffer, _topic_num) == 0) return 0;
	return buffer.offset();
}

topic_param_file_reader::topic_param_file_reader(const char *path, int word_num, size_t buffer_size) : file_reader(path, buffer_size)
{
	_word_num = word_num;
	_temp_buffer = new int[word_num];
}

topic_param_file_reader::~topic_param_file_reader()
{
	delete[] _temp_buffer;
}

size_t topic_param_file_reader::segment(char *data, size_t size)
{
	utility::read_buffer buffer(data, size);
	if (buffer.read_sparse_array(_temp_buffer, _word_num) == 0) return 0;
	return buffer.offset();
}

int topic_param_file_reader::word_num() const
{
	return _word_num;
}

tweet_id_file_reader::tweet_id_file_reader(const char *path, size_t buffer_size) : file_reader(path, buffer_size)
{
}

size_t tweet_id_file_reader::segment(char *data, size_t size)
{
	long long id;
	return utility::get_varint(data, &id, size);
}
