#include "Path.h"
#include "Utils.h"

#include <assert.h>
#include <algorithm>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include <iostream>

const char*
trim(char* string)
{
	while (isspace(*string))
		string++;

	ssize_t length = ::strlen(string);
	char* endOfString = string + length - 1;
	while (isspace(*endOfString))
		endOfString--;

	endOfString++;
	*endOfString  = '\0';

	return string;
}


void
path_dos_to_unix(char* path)
{
	char* c;
	while ((c = ::strchr(path, '\\')) != NULL)
		*c = '/';
}


const char*
extension(const char* path)
{
	if (path == NULL)
		return NULL;

	const char* point = ::strrchr(path, '.');
	if (point == NULL)
		return NULL;

	return point + 1;
}


size_t
get_unquoted_string(char* dest, char* source, size_t size)
{
	char* name = source;
	char* nameEnd = name + size;
	while (*name == '"')
		name++;
	while (*nameEnd != '"')
		nameEnd--;
	*nameEnd = '\0';
	::strncpy(dest, name, size);

	return nameEnd - source;
}

		
bool
is_bit_set(uint32 value, int bitPos)
{
	return value & (1 << bitPos);
}


void
set_bit(uint32& value, int bitPos)
{
	value |= (1 << bitPos);
}


void
clear_bit(uint32& value, int bitPos)
{
	value &= ~(1 << bitPos);
}


void
assert_size(size_t size, size_t controlValue)
{
	if (size != controlValue) {
		printf("size is %zu\n", size);
		assert(size == controlValue);
	}
}
