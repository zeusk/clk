#include <stdio.h>
#include <string.h>

unsigned char hexVal(char c)
{
	switch(c)
	{
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			return c-'0';
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			return c - 'a' + 10;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			return c - 'A' + 10;
	}
	return 0;
}
int main() {
	char buff[0x40000];

	while(!feof(stdin) && fgetc(stdin)!='\n' );

	while(!feof(stdin))
	{
		size_t len = fread(buff, 1, 5, stdin);
		int hLen = -1;
		if(strncmp(buff, "INFO", 4)==0) hLen = buff[4];
		if(strncmp(buff, "(boot", 5)==0)
		{
			len += fread(buff+5, 1, 9, stdin);
			if(strncmp(buff, "(bootloader) ", 13)==0) hLen = buff[13];
		}
		if(hLen!=-1)
		{
			for(;hLen>0;hLen--) fputc(hexVal(fgetc(stdin))<<4 | hexVal(fgetc(stdin)), stdout);
			fgetc(stdin);// read \n appended by fastboot
			continue;
		}
		fwrite(buff, 1, len, stdout);
		break;
	}

	while(!feof(stdin))
	{
		size_t len = fread(buff, 1, sizeof(buff), stdin);
		fwrite(buff, 1, len, stdout);
	}
}
