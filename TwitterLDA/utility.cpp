#include "utility.h"
#include "file_reader.h"
#include <cstring>
#include <cstdio>

size_t utility::string_hasher::operator()(const char *str) const
{
	size_t hash = 5381;
	int c;
	while (c = *str++)
	{
		//hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
		hash = c + (hash << 6) + (hash << 16) - hash;
	}
	return hash;
}

bool utility::string_predicate::operator()(const char *str1, const char *str2) const
{
	return strcmp(str1, str2) == 0;
}

char *utility::new_string(char *str)
{
	char *new_str = new char[strlen(str) + 1];
	strcpy(new_str, str);
	return new_str;
}

char *utility::new_string(const char *str1, const char *str2)
{
	size_t len1 = strlen(str1), len2 = strlen(str2);
	char *new_str = new char[len1 + len2 + 1];
	memcpy(new_str, str1, len1);
	memcpy(new_str + len1, str2, len2 + 1);
	return new_str;
}

bool utility::file_exist(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if (fp == nullptr)
	{
		return false;
	}
	else
	{
		fclose(fp);
		return true;
	}
}
