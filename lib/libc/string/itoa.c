#include <string.h>
#include <sys/types.h>

char*
itoa(int n, char* s, int b)
{
	static char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	int i=0, sign;

	if ((sign = n) < 0) {
		n = -n;
	}

	do {
		s[i++] = digits[n % b];
	} while ((n /= b) > 0);

	if (sign < 0) {
		s[i++] = '-';
	}

	s[i] = '\0';

	return strrev(s);
}
