/*
 * oem.c
 * This file is part of Little Kernel
 *
 * Copyright (C) 2010~2011 - Cedesmith
 * Copyright (C) 2011 - Shantanu gupta 
 *
 * Little Kernel is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Little Kernel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Little Kernel; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */


 
#include <arch/arm.h>
#include <arch/ops.h>
#include <kernel/thread.h>
#include <platform/iomap.h>
#include <platform/timer.h>
#include <sys/types.h>
#include <app.h>
#include <bits.h>
#include <compiler.h>
#include <debug.h>
#include <err.h>
#include <hsusb.h>
#include <platform.h>
#include <reg.h>
#include <smem.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dev/flash.h>
#include <dev/fbcon.h>
#include <dev/keys.h>
#include <dev/udc.h>
#include <lib/ptable.h>
#include <lib/vptable.h>
#include <kernel/thread.h>
#include <kernel/timer.h>

#include "aboot.h"
#include "bootimg.h"
#include "fastboot.h"
#include "recovery.h"
#include "oem.h"
#include "menu.h"

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
unsigned flash_erase_ptn(const char *arg)
{
	struct ptentry *ptn;
	struct ptable *ptable;

	ptable = flash_get_ptable();
	if (ptable == NULL) {
		return -3;
	}

	ptn = ptable_find(ptable, arg);
	if (ptn == NULL) {
		return -2;
	}

	if (flash_erase(ptn)) {
		return -1;
	}
	return 1;
}
static void cmd_erase(const char *arg, void *data, unsigned sz)
{
	if(target_is_emmc_boot() != 0)
	{
		fastboot_fail("This version does not support emmc targets");
		return;
	}
	switch (flash_erase_ptn(arg))
	{
		case -3:
			fastboot_fail("partition table doesn't exist");
			break;
		case -2:
			fastboot_fail("unknown partition name");
			break;
		case -1:
			fastboot_fail("failed to erase partition");
			break;
		case 1:
			fastboot_okay("");
			break;
		default:
			fastboot_fail("Unknown error occured");
			break;
	}
	return;
}

static void cmd_flash(const char *arg, void *data, unsigned sz)
{
	struct ptentry *ptn;
	struct ptable *ptable;
	unsigned extra = 0;

	ptable = flash_get_ptable();
	if (ptable == NULL) {
		fastboot_fail("partition table doesn't exist");
		return;
	}

	ptn = ptable_find(ptable, arg);
	if (ptn == NULL) {
		fastboot_fail("unknown partition name");
		return;
	}

	if (!strcmp(ptn->name, "boot") || !strcmp(ptn->name, "recovery")) {
		if (memcmp((void *)data, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
			fastboot_fail("image is not a boot image");
			return;
		}
	}

	if (!strcmp(ptn->name, "system") || !strcmp(ptn->name, "userdata")
	    || !strcmp(ptn->name, "persist"))
		extra = ((get_page_sz() >> 9) * 16);
	else
		sz = ROUND_TO_PAGE(sz, get_page_mask());

	dprintf(INFO, "writing %d bytes to '%s'\n", sz, ptn->name);
	if (flash_write(ptn, extra, data, sz)) {
		fastboot_fail("flash write failure");
		return;
	}
	dprintf(INFO, "partition '%s' updated\n", ptn->name);
	fastboot_okay("");
}

static void cmd_continue(const char *arg, void *data, unsigned sz)
{
	fastboot_okay("");
	target_battery_charging_enable(0, 1);
	udc_stop();
	boot_linux_from_flash();
}

static void cmd_reboot(const char *arg, void *data, unsigned sz)
{
	dprintf(INFO, "rebooting the device\n");
	fastboot_okay("");
	reboot_device(0);
}

static void cmd_reboot_bootloader(const char *arg, void *data, unsigned sz)
{
	dprintf(INFO, "rebooting the device\n");
	fastboot_okay("");
	reboot_device(FASTBOOT_MODE);
}

static void cmd_boot(const char *arg, void *data, unsigned sz)
{
	unsigned kernel_actual;
	unsigned ramdisk_actual;
	static struct boot_img_hdr hdr;
	char *ptr = ((char*) data);

	if (sz < sizeof(hdr)) {
		fastboot_fail("invalid bootimage header");
		return;
	}

	memcpy(&hdr, data, sizeof(hdr));

	/* ensure commandline is terminated */
	hdr.cmdline[BOOT_ARGS_SIZE-1] = 0;

	kernel_actual = ROUND_TO_PAGE(hdr.kernel_size, get_page_mask());
	ramdisk_actual = ROUND_TO_PAGE(hdr.ramdisk_size, get_page_mask());

	memmove((void*) KERNEL_ADDR, ptr + get_page_sz(), hdr.kernel_size);
	memmove((void*) RAMDISK_ADDR, ptr + get_page_sz() + kernel_actual, hdr.ramdisk_size);

	fastboot_okay("");
	target_battery_charging_enable(0, 1);
	udc_stop();

	boot_linux((void*) KERNEL_ADDR, (void*) TAGS_ADDR,
		   (const char*) hdr.cmdline, board_machtype(),
		   (void*) RAMDISK_ADDR, hdr.ramdisk_size);
}
/* Depreciated Functions, XXXTODOXXX: REWRITE FOLLOWING */
void cmd_oem_cls(){
	draw_menu();
	fastboot_okay("");
}

void cmd_oem_part_add(const char *arg){
	dev_info_add(arg);
	dev_info_list();
	fastboot_okay("");
}

void cmd_oem_part_resize(const char *arg){
	dev_info_resize(arg);
	dev_info_list();
	fastboot_okay("");
}

void cmd_oem_part_del(const char *arg){
	dev_info_del(arg);
	dev_info_list();
	fastboot_okay("");
}

void cmd_oem_part_commit(){
	dev_info_commit();
	printf("\n Partition changes saved! Will Reboot device in 2 secs\n");
	fastboot_okay("");
	thread_sleep(1980);
	reboot_device(FASTBOOT_MODE);
}

void cmd_oem_part_read(){
	dev_info_read();
	dev_info_list();
	fastboot_okay("");
}

void cmd_oem_part_list(){
	dev_info_list();
	fastboot_okay("");
}

void cmd_oem_part_clear(){
	dev_info_clear();

	dev_info_list();
	fastboot_okay("");
}

void cmd_oem_part_create_default(){
	dev_info_clear();
	dev_info_create_default();

	dev_info_list();
	fastboot_okay("");
}
void prnt_nand_stat(void)
{
	struct flash_info *flash_info;
	flash_info = flash_get_info();
	unsigned start_block=0x219;
	//unsigned vpt_start_block;
	unsigned nand_num_blocks;
	unsigned extrom_offset; // Space allocated by ExtROM and si backup on WinCE -  Used as cache partition
	unsigned extrom_size = 191 ; // 191 blocks = ~24 Mb
	nand_num_blocks = flash_info->num_blocks;
	blk_pmb = (1024*1024)/flash_info->block_size; // Number of blocks per 1Mb
	// Offset for partition with part table
	extrom_offset = nand_num_blocks - extrom_size; // Extrom start offset  NAND Size - 24Mb
	// Usable permanent flash storage - Complete size with extrom
	flash_size_blk =(nand_num_blocks - start_block); // Max available flash size for user partitions
	dprintf(INFO,"\n==============================================================================\n\n");
	dprintf(INFO,"  Full flash size: %i blocks - %i Mb\n",nand_num_blocks, (int)(nand_num_blocks/blk_pmb));
	dprintf(INFO,"  Usable flash size: %i blocks - %i Mb\n",flash_size_blk, (int)(flash_size_blk/blk_pmb));
	dprintf(INFO,"  Flash ExtROM: offset:%x  blocks:%i  size:%i\n",extrom_offset, extrom_size, (int)(extrom_size/blk_pmb));
	dprintf(INFO,"  Flash spare size: %i\n",flash_info->spare_size);
	dprintf(INFO,"  Flash offset: %x\n",start_block);
	dprintf(INFO,"  Partition table offset:%x  size:%x\n",0x216, 2);
	dprintf(INFO,"\n==============================================================================\n\n");
}
void cmd_oem_nand_status(void)
{
	prnt_nand_stat();
	fastboot_okay("");
}
void cmd_oem_part_format_all(){
	
	struct ptentry *ptn, *vpt;
	struct ptable *ptable;
	ptable = flash_get_vptable();
	
	printf("\n\n\nInitializing flash format...\n");
	
	if (ptable == NULL) {
		printf( "ERROR: VPTABLE not found!!!\n");
		return;
	}
	ptn = ptable_find(ptable, "task29");
	vpt = ptable_find(ptable, "vptable");
	if (ptn == NULL) {
		printf("Format failed due to Error:29.1");
		return;
	}
	if (vpt == NULL) {
		printf("Format failed due to Error:29.2");
		return;
	}
	printf("Formating flash...\n");
	
	flash_erase(ptn);
	flash_erase(vpt);

	printf("\nFormat complete ! \nReboot device to create default partition table, or create partitions manualy!\n\n");
	
	fastboot_okay("");
}


void cmd_oem_part_format_vpart(){
	
	struct ptentry *ptn;
	struct ptable *ptable;
	ptable = flash_get_vptable();
	
	printf("\n\n\nInitializing flash format...\n");
	
	if (ptable == NULL) {
		printf( "ERROR: VPTABLE not found!!!\n");
		return;
	}
	ptn = ptable_find(ptable, "vptable");

	if (ptn == NULL) {
		printf( "ERROR: No vptable partition!!!\n");
		return;
	}
	
	printf("Formating vptable...\n");
	
	
	flash_erase(ptn);
	
	
	printf("\nFormat complete ! \nReboot device to create default partition table, or create partitions manualy!\n\n");

	
	fastboot_okay("");
	return;
}

void cmd_test()
{
	int i =0;
	for (i=0;i<100;i++){
		printf("Test line... %i\n", i);
	}
	fastboot_okay("Hello!");
	draw_menu();
}

void cmd_oem_help(){
	fbcon_resetdisp();

	printf("\n================================ FASTBOOT HELP ================================\n");

	printf(" fastboot oem help\n");
	printf("  - This simple help\n\n");
	
	printf(" fastboot oem cls\n");
	printf("  - Clear screen\n\n");
	
	printf(" fastboot oem part-add name:size\n");
	printf("  - Create new partition with given name and size (in Mb, 0 for all space)\n");
	printf("  - example: fastboot oem part-add system:160  - this will create system partition with size 160Mb\n\n");
	
		
	printf(" fastboot oem part-resize name:size\n");
	printf("  - Resize partition with given name to new size (in Mb, 0 for all space)\n");
	printf("  - example: fastboot oem part-resize system:150  - this will resize system partition to 150Mb\n\n");
	
	printf(" fastboot oem part-del name\n");
	printf("  - Delete named partition\n\n");
	
	printf(" fastboot oem part-create-default\n");
	printf("  - Delete curent partition table and create default one\n\n");
	
	printf(" fastboot oem part-read\n");
	printf("  - Reload partition layout from NAND\n\n");
	
	printf(" fastboot oem part-commit\n");
	printf("  - Save curent layout to NAND. All changes to partition layout are tmp. untill commited to nand!\n\n");
	
	printf(" fastboot oem part-list\n");
	printf("  - Display current partition layout\n\n");
	
	printf(" fastboot oem part-clear\n");
	printf("  - Clear current partition layout\n\n");
	
	printf(" fastboot oem format-all\n");
	printf("  - WARNING !!!  THIS COMMAND WILL FORMAT COMPLETE NAND (except bootloader)\n   - ALSO IT WILL DISPLAY BAD SECTORS IF THERE ARE ANY\n");
	printf("  - !!!  THIS IS EQUIVALENT TO TASK 29 !!!\n\n");
	
	fastboot_okay("Look at your Device.");
}
static void cmd_powerdown(void)
{
	fastboot_okay("");
	target_battery_charging_enable(0, 1);
	udc_stop();
	shutdown();
}
/* Depreciation over */
void cmd_set_dev_sn (const char *arg)
{
	char device_serial_buffer[12];
	dprintf(INFO, "TRYING TO SET DEVICE SERIAL:%s\n", arg);
	snprintf(device_serial_buffer, strlen(dev_inf.devsn), "%s", arg);
	snprintf(dev_inf.devsn, strlen(dev_inf.devsn), "%s", device_serial_buffer);
	strcpy(dev_inf.devsn , device_serial_buffer);
	dprintf(INFO, "DEVICE SERIAL:%s\n", dev_inf.devsn);
	fastboot_okay("");
}
static void cmd_get_dev_sn(void)
{
	dprintf(INFO, "DEVICE SERIAL:%s\n", dev_inf.devsn);
	fastboot_okay("");
}
static void cmd_oem(const char *arg, void *data, unsigned sz)
{
	while(*arg==' ') arg++;
	if(memcmp(arg, "cls", 3)==0)                           cmd_oem_cls();
	if(memcmp(arg, "set", 3)==0)                           cmd_oem_set(arg+3);
	if(memcmp(arg, "set", 3)==0)                           cmd_oem_set(arg+3);
	if(memcmp(arg, "pwf ", 4)==0)                          cmd_oem_dumpmem(arg+4);
	if(memcmp(arg, "test", 4)==0)                          cmd_test();
	if(memcmp(arg, "dmesg", 5)==0)                         cmd_oem_dmesg();
	if(memcmp(arg, "smesg", 5)==0)                         cmd_oem_smesg();
	if(memcmp(arg, "nandstat", 8)==0)                      cmd_oem_nand_status();
	if(memcmp(arg, "poweroff", 8)==0)                      cmd_powerdown();
	if(memcmp(arg, "part-add ", 9)==0)                     cmd_oem_part_add(arg+9);
	if(memcmp(arg, "part-del ", 9)==0)                     cmd_oem_part_del(arg+9);
	if(memcmp(arg, "part-read", 9)==0)                     cmd_oem_part_read();
	if(memcmp(arg, "part-list", 9)==0)                     cmd_oem_part_list();
	if(memcmp(arg, "flashlight", 10)==0)                   cmd_flashlight();	
	if(memcmp(arg, "part-clear", 10)==0)                   cmd_oem_part_clear();
	if(memcmp(arg, "format-all", 10)==0)                   cmd_oem_part_format_all();
	if(memcmp(arg, "get-dev-sn", 10)==0)                   cmd_get_dev_sn();
	if(memcmp(arg, "set-dev-sn", 10)==0)                   cmd_set_dev_sn(arg+11);
	if(memcmp(arg, "part-commit", 11)==0)                  cmd_oem_part_commit();
	if(memcmp(arg, "part-resize ", 12)==0)                 cmd_oem_part_resize(arg+12);
	if(memcmp(arg, "format-vpart", 12)==0)                 cmd_oem_part_format_vpart();
	if(memcmp(arg, "part-create-default", 19)==0)          cmd_oem_part_create_default();
	if((memcmp(arg,"help",4)==0)||(memcmp(arg,"?",1)==0))  cmd_oem_help();
}
void cmd_oem_register(void)
{
	fastboot_register("oem", cmd_oem);
	fastboot_register("boot", cmd_boot);
	fastboot_register("flash:", cmd_flash);
	fastboot_register("erase:", cmd_erase);
	fastboot_register("continue", cmd_continue);
	fastboot_register("reboot", cmd_reboot);
	fastboot_register("reboot-bootloader", cmd_reboot_bootloader);
}
