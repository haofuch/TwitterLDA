#pragma once

/*
	File schema

	text_file:
		char text[];
		(\r\n r \n\r or \n or \r)

	tweet_file:
		var_int user;
		var_int word_count;
		var_int words[word_count]

	tweet_param_file:
		var_int topic;
		var_int word_count;
		char word_tags[ceil(word_count / 8)]

	user_param_file:
		var_int user;
		sparse_var_int_array topic_counts;

	topic_param_file:
		sparse_var_int_array word_counts;

	tweet_id_file:
		var_int64 tweet_id;
*/

#include <cstdio>

struct file_item
{
	char *data;
	size_t size;
};

class file_reader
{
public:
	file_reader(const char *path, size_t buffer_size);
	~file_reader();

	file_item get_item(bool fixed_buffer);
	bool unget_item(file_item item);
	void trim();
	void reset();
	void close();
	size_t buffer_size() const;
	long long size() const;
	long long position() const;

	virtual size_t segment(char *data, size_t size) = 0;

protected:
	size_t _buffer_size;
	size_t _buffer_offset;
	size_t _buffer_count;
	char *_buffer;
	FILE *_fp;
	long long _size;
	long long _position;
};

class text_file_reader : public file_reader
{
public:
	text_file_reader(const char *path, size_t buffer_size = 16 << 20);
	size_t segment(char *data, size_t size);
};

class tweet_file_reader : public file_reader
{
public:
	tweet_file_reader(const char *path, size_t buffer_size = 16 << 20);
	size_t segment(char *data, size_t size);
};

class tweet_param_file_reader : public file_reader
{
public:
	tweet_param_file_reader(const char *path, size_t buffer_size = 16 << 20);
	size_t segment(char *data, size_t size);
};

class user_param_file_reader : public file_reader
{
public:
	user_param_file_reader(const char *path, int topic_num, size_t buffer_size = 16 << 20);
	~user_param_file_reader();
	size_t segment(char *data, size_t size);
	int topic_num() const;
protected:
	int _topic_num;
	int *_temp_buffer;
};

class topic_param_file_reader : public file_reader
{
public:
	topic_param_file_reader(const char *path, int word_num, size_t buffer_size = 16 << 20);
	~topic_param_file_reader();
	size_t segment(char *data, size_t size);
	int word_num() const;
protected:
	int _word_num;
	int *_temp_buffer;
};

class tweet_id_file_reader : public file_reader
{
public:
	tweet_id_file_reader(const char *path, size_t buffer_size = 16 << 20);
	size_t segment(char *data, size_t size);
};
