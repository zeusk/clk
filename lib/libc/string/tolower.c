#include <string.h>
#include <sys/types.h>

char
tolower(char c)
{
	return (c >= (char) 65 && c <= (char) 90) ? c + 32 : c;
}
