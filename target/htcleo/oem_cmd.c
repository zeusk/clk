/*
 * License: GPL
 * Author: cedesmith
 */
#include <debug.h>
#include <string.h>
#include <stdlib.h>
#include <reg.h>

void fastboot_okay(const char *info);
void fastboot_fail(const char *reason);
int fastboot_write(void *buf, unsigned len);
void fastboot_register(const char *prefix, void (*handle)(const char *arg, void *data, unsigned sz));

static char charVal(char c)
{
	c&=0xf;
	if(c<=9) return '0'+c;
	return 'A'+(c-10);
}

static void send_mem(char* start, int len)
{
	char response[64];
	while(len>0)
	{
		int slen = len > 29 ? 29 : len;
		memcpy(response, "INFO", 4);
		response[4]=slen;

		for(int i=0; i<slen; i++)
		{
			response[5+i*2] = charVal(start[i]>>4);
			response[5+i*2+1]= charVal(start[i]);
		}
		response[5+slen*2+1]=0;
		fastboot_write(response, 5+slen*2);

		start+=slen;
		len-=slen;
	}
}

static void cmd_oem_smesg()
{
	send_mem((char*)0x1fe00018, MIN(0x200000, readl(0x1fe00000)));
	fastboot_okay("");
}

static void cmd_oem_dmesg()
{
	if(*((unsigned*)0x2FFC0000) == 0x43474244 /* DBGC */  ) //see ram_console_buffer in kernel ram_console.c
	{
		send_mem((char*)0x2FFC000C, *((unsigned*)0x2FFC0008));
	}
	fastboot_okay("");
}

static int str2u(const char *x)
{
	while(*x==' ')x++;

	unsigned base=10;
	int sign=1;
    unsigned n = 0;

    if(strstr(x,"-")==x) { sign=-1; x++;};
    if(strstr(x,"0x")==x) {base=16; x+=2;}

    while(*x) {
    	char d=0;
        switch(*x) {
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
            d = *x - '0';
            break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            d = (*x - 'a' + 10);
            break;
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            d = (*x - 'A' + 10);
            break;
        default:
            return sign*n;
        }
        if(d>=base) return sign*n;
        n*=base;n+=d;
        x++;
    }

    return sign*n;
}
static void cmd_oem_dumpmem(const char *arg)
{
	char *sStart = strtok((char*)arg, " ");
	char *sLen = strtok(NULL, " ");
	if(sStart==NULL || sLen==NULL)
	{
		fastboot_fail("usage:oem dump start len");
		return;
	}

	send_mem((char*)str2u(sStart), str2u(sLen));
	fastboot_okay("");
}
static void cmd_oem_set(const char *arg)
{
	char type=*arg; arg++;
	char *sAddr = strtok((char*) arg, " ");
	char *sVal = strtok(NULL, "\0");
	if(sAddr==NULL || sVal==NULL)
	{
		fastboot_fail("usage:oem set[s,c,w] address value");
		return;
	}
	char buff[64];
	switch(type)
	{
		case 's':
			memcpy((void*)str2u(sAddr), sVal, strlen(sVal));
			send_mem((char*)str2u(sAddr), strlen(sVal));
			break;
		case 'c':
			*((char*)str2u(sAddr)) = (char)str2u(sVal);
			sprintf(buff, "%x", *((char*)str2u(sAddr)));
			send_mem(buff, strlen(buff));
			break;
		case 'w':
		default:
			*((int*)str2u(sAddr)) = str2u(sVal);
			sprintf(buff, "%x", *((int*)str2u(sAddr)));
			send_mem(buff, strlen(buff));
	}
	fastboot_okay("");
}

static void cmd_oem(const char *arg, void *data, unsigned sz)
{
	while(*arg==' ') arg++;
	if(memcmp(arg, "dmesg", 5)==0) cmd_oem_dmesg();
	if(memcmp(arg, "smesg", 5)==0) cmd_oem_smesg();
	if(memcmp(arg, "pwf ", 4)==0) cmd_oem_dumpmem(arg+4);
	if(memcmp(arg, "set", 3)==0) cmd_oem_set(arg+3);
}

void cmd_oem_register()
{
	fastboot_register("oem", cmd_oem);
}
