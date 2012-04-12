#include <string.h>
#include <sys/types.h>

char
toupper(char c)
{
	return (c >= (char) 97 && c <= (char) 122) ? c - 32 : c;
}
