#include <stdlib.h>
#include <string.h>

char *
strndup(const char *str, size_t n)
{	
	size_t len = (strlen(str) > n ? n : strlen(str));
	char *res = malloc(len + 1);

	if (res != NULL){
		memcpy(res, str, (len + 1));
		res[len] = '\0';
	}

	return res;
}

