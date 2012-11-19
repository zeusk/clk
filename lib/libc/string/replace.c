#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char* replace(const char *str, const char *oldstr, const char *newstr, int *count)
{
	const char *tmp = str;
	char *result;
	int   found = 0;
	int   length, reslen;
	int   oldlen = strlen(oldstr);
	int   newlen = strlen(newstr);
	int   limit = (count != NULL && *count > 0) ? *count : -1; 

	tmp = str;
	while ((tmp = strstr(tmp, oldstr)) != NULL && found != limit) {
		found++;
		tmp += oldlen;
	}
	
	length = strlen(str) + found * (newlen - oldlen);
	if ( (result = (char *)malloc(length+1)) == NULL) {
		found = -1;
	} else {
		tmp = str;
		limit = found; // Countdown
		reslen = 0; // Length of current result
		// Replace each old string found with new string
		while ((limit-- > 0) && (tmp = strstr(tmp, oldstr)) != NULL) {
			length = (tmp - str); // Number of chars to keep intouched
			strncpy(result + reslen, str, length); // Original part keeped
			strcpy(result + (reslen += length), newstr); // Insert new string
			reslen += newlen;
			tmp += oldlen;
			str = tmp;
		}
		strcpy(result + reslen, str); // Copies last part and ending nul char
	}
	
	if (count != NULL)
		*count = found;
	
	return result;
}

char* sreplace(char *buf, int c, int pos)
{
	buf[pos]=(char)c;
	
	return buf;
}