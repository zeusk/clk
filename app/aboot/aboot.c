/*
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * Copyright (c) 2011, Rick@xda-developers.com, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
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
#include <target.h>
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
#include <lib/devinfo.h>

#include <aboot.h>
#include <bootimg.h>
#include <fastboot.h>
#include <recovery.h>

#include <version.h>

#if WITH_LIB_FS
#include <lib/fs.h>
#endif

static struct udc_device surf_udc_device = 
{
	.vendor_id	= 0x18d1,
	.product_id	= 0x0D02,
	.version_id	= 0x0001,
	.manufacturer	= "Google",
	.product	= "Android",
	//.serialno	= "HT9000000000"
};

char charVal(char c)
{
	c&=0xf;
	if(c<=9) return '0'+c;
	return 'A'+(c-10);
}

int str2u(const char *x)
{
	while(*x==' ')x++;

	unsigned base=10;
	int sign=1;
    	unsigned n = 0;

    	if(strstr(x,"-")==x) { sign=-1; x++;};
    	if(strstr(x,"0x")==x) {base=16; x+=2;}

    	while(*x) 
	{
    		char d=0;
        	switch(*x)
		{
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

void hex2ascii(char *hexstr, char *asciistr)
{
	for (int i = 0; i < (int)(strlen(hexstr) - 1); i+=2)
	{
		int firstvalue = hexstr[i] - '0';
		int secondvalue;
		switch(hexstr[i+1])
		{
			case 'A': case 'a':
			{
				secondvalue = 10;
			}break;
			case 'B': case 'b':
			{
				secondvalue = 11;
			}break;
			case 'C': case 'c':
			{
				secondvalue = 12;
			}break;
			case 'D': case 'd':
			{
				secondvalue = 13;
			}break;
			case 'E': case 'e':
			{
				secondvalue = 14;
			}break;
			case 'F': case 'f':
			{
				secondvalue = 15;
			}break;
			default: 
				secondvalue = hexstr[i+1] - '0';
				break;
		}
		int newval;
		newval = ((16 * firstvalue) + secondvalue);
		*asciistr = (char)(newval);
		asciistr++;
	}
}

void dump_mem_to_buf(char* start, int len, char *buf)
{
	while(len>0)
	{
		int slen = len > 29 ? 29 : len;
		for(int i=0; i<slen; i++)
		{
			buf[i*2] = charVal(start[i]>>4);
			buf[i*2+1]= charVal(start[i]);
		}
		buf[slen*2+1]=0;
		start+=slen;
		len-=slen;
	}
}

/*
 * koko : Get device IMEI
 */
char device_imei[64];
char imeiBuffer[64];
void update_device_imei(void)
{
	dump_mem_to_buf((char*)str2u("0x1EF260"), str2u("0xF"), imeiBuffer);

	hex2ascii(imeiBuffer, device_imei);

	return;
}

/*
 * koko : Get radio version
 */
char radio_version[25];
char radBuffer[25];
void update_radio_ver(void)
{
	dump_mem_to_buf((char*)str2u("0x1EF220"), str2u("0xA"), radBuffer);

	/* koko : if radio version is not read 1st char won't be '3' so return */
	char expected_byte[] = "3";
	if(radBuffer[0] != expected_byte[0]){return;}

	hex2ascii(radBuffer, radio_version);

	return;
}

/*
 * koko : Get spl version
 */
char spl_version[24];
char splBuffer[24];
void update_spl_ver(void)
{
	dump_mem_to_buf((char*)str2u("0x1004"), str2u("0x9"), splBuffer);

	hex2ascii(splBuffer, spl_version);

	return;
}

/*
 * koko : Changed the header to display
 *		
 *	eg: 	cLK 1.7.0.0   |   SPL 2.08.HSPL   |   RADIO 2.15.50.14
 *					 NAND flash chipset : Hynix-BC
 *					    Total flash size:    512MB
 *					      Available size:    421MB
 *						      ExtROM: DISABLED
 */
void draw_clk_header(void)
{
	unsigned myfg = 0xffff;
	unsigned mytg = 0x0000;
	unsigned mycfg = 0x65ff;
	if(inverted)
	{
		mycfg = 0x001f;
	}
	fbcon_setfg(mycfg);
	fbcon_settg(mytg);
	printf("                                                            ");
	printf("   cLK %s", PSUEDO_VERSION);
	fbcon_setfg(myfg);
	char expected_byte[] = "3";
	printf("   |   SPL %s   |   RADIO %s   ",
		 spl_version, radBuffer[0] != expected_byte[0] ? ".........." : radio_version);
	printf("                           NAND flash chipset: %7s-%02X                                \
		Total flash size:     %4iMB   ",flash_info->manufactory, flash_info->device, (int)( flash_info->num_blocks / get_blk_per_mb() ));
	printf("                               \
		Available size:      %iMB   ", (int)( get_usable_flash_size() / get_blk_per_mb() ));
	printf("                                       \
		ExtROM:   ");
	if (device_info.extrom_enabled){
		fbcon_setfg(mycfg);printf(" ENABLED   ");
	}else{
		fbcon_setfg(myfg);printf("DISABLED   ");
	}
	if(inverted){
		myfg = 0x0000;
		mytg = 0xffff;
	}
	if (active_menu != NULL)
	{
		fbcon_settg(mytg);fbcon_setfg(myfg);
		_dputs("<> ");
		_dputs(active_menu->MenuId);
		_dputs("\n\n");
	}
}

/*
 * koko : Selected item gets
 *	  different foregroung color
 *	  and ">> " infront of it
 *	  to indicate it is selected - No highlight
 */
void selector_disable(void)
{
	fbcon_set_x_cord(active_menu->item[active_menu->selectedi].x);
	fbcon_set_y_cord(active_menu->item[active_menu->selectedi].y);
	fbcon_forcetg(true);
	fbcon_reset_colors_rgb555();
	char sel_off[64];
	sprintf( sel_off, "   %.s", active_menu->item[active_menu->selectedi].mTitle + 3);
	_dputs(sel_off);
	fbcon_forcetg(false);
}

void selector_enable(void)
{
	if(active_menu->maxarl > 0) // if there are no sub-entries don't enable selection
	{
		fbcon_set_x_cord(active_menu->item[active_menu->selectedi].x);
		fbcon_set_y_cord(active_menu->item[active_menu->selectedi].y);
		unsigned myfg = 0x65ff;
	
		if(inverted)
		{
			myfg = 0x001f;
		}
		fbcon_setfg(myfg);
		char sel_on[64];
		sprintf( sel_on, ">> %.s", active_menu->item[active_menu->selectedi].mTitle + 3);
		_dputs(sel_on);
		fbcon_reset_colors_rgb555();
	}
}

/*
 * koko: Credits added in Main Menu/Info items
 */
void cmd_oem_credits(void)
{
	unsigned myfg = 0xffff;
	if(inverted)
	{
		myfg = 0x0000;
	}
	fbcon_setfg(myfg);
	printf("   cedesmith for his great work on porting LK to HD2\n\
		   and the ppp wrappers,\n");
	printf("   Travis Geiselbrecht for writing LK,\n");
	printf("   Codeaurora and Qualcomm for adding hardware level\n\
		   support for qsd8250,\n");
	printf("   LeTama for his work on the NAND driver and making\n\
		   linux on NAND possible,\n");
	printf("   Martin Johnson for his tinboot which was a great\n\
		   inspiration,\n");
	printf("   bepe, Cotulla and DFT for the HardSPL and their\n\
		   over-all contribution to android on HD2,\n");
	printf("   Martijn Stolk for his kernel segfault solving code,\n");
	printf("   seadersn for helping with the original compilation,\n");
	printf("   spalm for the wp7 style android HD2 bootanimation\n\
		   from which came the htc HD2 logo,\n");
	printf("   stirkac for his fantastic documentation on howto,\n");
	printf("   arif-ali for the version patch and his howto,\n");
	printf("   Dan1j3l for the offmode charging daemon in recovery\n\
		   and for the dynamic partition table,\n");
	printf("   xdmcdmc for his work on nbgen(recovery added in NBH)\n\
		   and on partition table(repartition on the fly),\n");
	printf("   Rick_1995 for his universal work on maintaining\n\
		   original cLK and further developing HBOOT by adding\n\
		   great new features(flashlight,fonts,logo,etc.),\n");
	printf("   ALL who worked on making linux kernel possible on hd2.\n");
}

/*
 * koko : Add menu item at specific index
 */
void add_menu_item(struct menu *xmenu,const char *name,const char *command,const int index)
{
	if( (xmenu->maxarl==MAX_MENU) || (index>MAX_MENU) )
	{
		printf("Menu: is overloaded with entry %s",name);
		return;
	}
	if(index>xmenu->maxarl)
	{
      		strcpy(xmenu->item[index].mTitle,name);
      		strcpy(xmenu->item[index].command,command);
	}else{
		for (int i = (xmenu->maxarl+1); i > (index); i--)
		{
	      		strcpy(xmenu->item[i].mTitle, xmenu->item[i-1].mTitle);
	      		strcpy(xmenu->item[i].command, xmenu->item[i-1].command);
	      	}
	      	strcpy(xmenu->item[index].mTitle,name);
      		strcpy(xmenu->item[index].command,command);
	}

      	xmenu->item[xmenu->maxarl].x = 0;
      	xmenu->item[xmenu->maxarl].y = 0;
      	xmenu->maxarl++;

	return;
}

/*
 * koko : Remove menu item at specific index
 */
void rem_menu_item(struct menu *xmenu,const int index)
{
	for (int i = (index); i < (xmenu->maxarl); i++)
	{
      		strcpy(xmenu->item[i].mTitle, xmenu->item[i+1].mTitle);
      		strcpy(xmenu->item[i].command, xmenu->item[i+1].command);
      	}

      	xmenu->item[xmenu->maxarl].x = 0;
      	xmenu->item[xmenu->maxarl].y = 0;
      	xmenu->maxarl--;

	return;
}

/*
 * koko : Change menu item's name and command
 */
void change_menu_item(struct menu *xmenu,const char *oldname,const char *newname,const char *newcommand)
{
      	for (int i = 0; i < xmenu->maxarl; i++)
	{
      		if (!memcmp(xmenu->item[i].mTitle, oldname, strlen(xmenu->item[i].mTitle)))
		{
      			strcpy(xmenu->item[i].mTitle, newname);
      			strcpy(xmenu->item[i].command, newcommand);
      		}
      	}
	return;
}

void eval_command(void)
{
	char command[32];

	if (active_menu->goback)
	{
		active_menu->goback = 0;
		strcpy(command, active_menu->backCommand);
		if ( strlen( command ) == 0 )
			return;
	}
	else
	{
		strcpy(command, active_menu->item[active_menu->selectedi].command);
	}

	// BOOT ANDROID NAND
	if (!memcmp(command,"boot_pboot", strlen(command)))
	{
		active_menu=NULL;
        	fbcon_resetdisp();
		if(inverted){draw_clk_header();printf(" \n\n");}
        	boot_into_sboot = 0;
		boot_into_tboot = 0;
        	boot_into_recovery = 0;
        	boot_linux_from_flash();
    	}
	// BOOT ANDROID sBOOT
	else if (!memcmp(command,"boot_sboot", strlen(command)))
	{
		active_menu=NULL;
        	fbcon_resetdisp();
		if(inverted){draw_clk_header();printf(" \n\n");}
        	boot_into_sboot = 1;
        	boot_into_tboot = 0;
        	boot_into_recovery = 0;
        	boot_linux_from_flash();
    	}
	// BOOT ANDROID tBOOT
	else if (!memcmp(command,"boot_tboot", strlen(command)))
	{
		active_menu=NULL;
        	fbcon_resetdisp();
		if(inverted){draw_clk_header();printf(" \n\n");}
        	boot_into_tboot = 1;
        	boot_into_sboot = 0;
        	boot_into_recovery = 0;
        	boot_linux_from_flash();
    	}
	// BOOT RECOVERY
	else if (!memcmp(command,"boot_recv", strlen(command)))
	{
		active_menu=NULL;
        	fbcon_resetdisp();
		if(inverted){draw_clk_header();printf(" \n\n");}
        	boot_into_sboot = 0;
        	boot_into_tboot = 0;
        	boot_into_recovery = 1;
        	boot_linux_from_flash();
	}
	// FLASHLIGHT
	else if (!memcmp(command,"init_flsh", strlen(command)))
	{
		redraw_menu();
		cmd_flashlight();
		selector_enable();
	}
	// SETTINGS
	else if (!memcmp(command,"goto_sett", strlen(command)))
	{
		/* koko : Need to link mirror_info with device_info for option "REARRANGE PARTITIONS" to work.
			  The linking is done here so that every time the user enters this submenu he resets mirror_info to device_info.
			  Check devinfo.h for new struct */
		strcpy( mirror_info.tag, "MIRRORINFO" );
		for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
		{
			if(strlen( device_info.partition[i].name ) != 0)
			{
				strcpy( mirror_info.partition[i].name, device_info.partition[i].name );
				mirror_info.partition[i].size = device_info.partition[i].size;
				mirror_info.partition[i].asize = device_info.partition[i].asize;
				mirror_info.partition[i].order = i+1;
			}
		}
		mirror_info.extrom_enabled = device_info.extrom_enabled;
		mirror_info.size_fixed = device_info.size_fixed;
		mirror_info.inverted_colors = device_info.inverted_colors;
		mirror_info.show_startup_info = device_info.show_startup_info;

		active_menu = &sett_menu;
		redraw_menu();
		selector_enable();
		set_usb_status_listener(ON);
    	}
	// RESIZE PARTITIONS
	else if (!memcmp(command,"goto_rept", strlen(command)))
	{
		set_usb_status_listener(OFF);
		rept_menu.top_offset	= 0;
		rept_menu.bottom_offset	= 0;
		rept_menu.maxarl	= 0;
		rept_menu.goback	= 0;

		char command[64];
      		char partname[64];
		int k=0;
      		for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
      		{
      			if( strlen(device_info.partition[i].name)!=0 )
			{
	      			strcpy( command, "rept_" );
	      			strcat( command, device_info.partition[i].name );
	      			sprintf( partname, "   %s", device_info.partition[i].name);
	      			add_menu_item( &rept_menu, partname, command , k++ );
			}
      		}
            	add_menu_item( &rept_menu, "   PRINT PARTITION TABLE", "prnt_stat" ,k++ );
            	add_menu_item( &rept_menu, "   COMMIT CHANGES", "rept_commit" ,k );

		active_menu = &rept_menu;
		redraw_menu();
		selector_enable();
    	}
	// RESIZE PARTITIONS SUBMENU
	else if (!memcmp(command, "rept_", 5 ))
	{
		char* subCommand = command + 5;
		if ( active_menu == &rept_menu )
		{
			if ( !memcmp( subCommand, "commit", strlen( subCommand ) ) )
			{
				cust_menu.top_offset	= 0;
				cust_menu.bottom_offset	= 0;
				cust_menu.selectedi	= 0;	// APPLY by default
				cust_menu.maxarl	= 0;
				cust_menu.goback	= 0;

				sprintf( cust_menu.MenuId, "COMMIT CHANGES" );
				strcpy( cust_menu.backCommand, "goto_rept" );

				add_menu_item(&cust_menu, "   APPLY"	, "rept_write", 0);
				add_menu_item(&cust_menu, "   CANCEL"	, "goto_rept", 1);

				active_menu = &cust_menu;

				redraw_menu();
				selector_enable();
				printf(" \n\n\n");
				device_list();
				return;
			}
			else
			{
				cust_menu.top_offset    = 0;
				cust_menu.bottom_offset = 0;
				cust_menu.selectedi     = 0;
				cust_menu.maxarl	= 0;
				cust_menu.goback	= 0;

				strcpy( cust_menu.data, subCommand );
				strcpy( cust_menu.backCommand, "goto_rept" );

				if(!device_info.partition[device_partition_order(cust_menu.data)-1].asize) // if selected partition has a fixed size
				{
					sprintf( cust_menu.MenuId, "%s = %d MB", cust_menu.data, (int) ( device_partition_size( cust_menu.data ) / get_blk_per_mb() ) );
					
					/* koko : Added more options +25,+5,-5,-25 */
					add_menu_item(&cust_menu, "   +25"			, "rept_add_25", 0);
					add_menu_item(&cust_menu, "   +10"			, "rept_add_10", 1);
					add_menu_item(&cust_menu, "   +5"			, "rept_add_5" , 2);
					add_menu_item(&cust_menu, "   +1"			, "rept_add_1" , 3);
					add_menu_item(&cust_menu, "   -1"			, "rept_rem_1" , 4);
					add_menu_item(&cust_menu, "   -5"			, "rept_rem_5" , 5);
					add_menu_item(&cust_menu, "   -10"			, "rept_rem_10", 6);
					add_menu_item(&cust_menu, "   -25"			, "rept_rem_25", 7);

				}
				else 	// if selected partition is auto-size
				{	// give user the option to make it fixed-size
					sprintf( cust_menu.MenuId, "%s partition is auto-size (%d MB)", cust_menu.data , (int)( device_partition_size( cust_menu.data ) / get_blk_per_mb() ));
					char cancelautosize[32];
					char cancelautosizecmd[32];
					sprintf( cancelautosize, "   Convert partition to fixed-size (%d MB)", (int) ( device_partition_size( cust_menu.data ) / get_blk_per_mb() ) );
					sprintf( cancelautosizecmd, "rept_set_%d", (int) ( device_partition_size( cust_menu.data ) / get_blk_per_mb() ) );
					add_menu_item(&cust_menu, cancelautosize		, cancelautosizecmd, 0);
				}

				active_menu = &cust_menu;
				redraw_menu();
				selector_enable();
			}
		}
		else if ( active_menu == &cust_menu )
		{
			// option to make auto-sized partition -> fixed size
			if ( !memcmp( subCommand, "set_", 4 ) )
			{
				int add = atoi( subCommand + 4 );		// MB
				device_info.partition[device_partition_order(cust_menu.data)-1].asize = 0;
				device_resize_ex( cust_menu.data, add, false );
				sprintf( cust_menu.MenuId, "%s = %d MB", cust_menu.data, add );
				thread_sleep(500);
				active_menu = &rept_menu;
				redraw_menu();
				selector_enable();
			}
			else if ( !memcmp( subCommand, "add_", 4 ) )
			{
				int add = atoi( subCommand + 4 );		// MB
				device_resize_ex( cust_menu.data, (int) device_partition_size( cust_menu.data ) / get_blk_per_mb() + add, false );
				sprintf( cust_menu.MenuId, "%s = %d MB", cust_menu.data, (int) ( device_partition_size( cust_menu.data ) / get_blk_per_mb() ) );
				redraw_menu();
				selector_enable();
			}
			else if ( !memcmp( subCommand, "rem_", 4 ) )
			{
				int rem = atoi( subCommand + 4 );		// MB
				int size = (int) device_partition_size( cust_menu.data ) / get_blk_per_mb() - rem;
				/* koko : User can now set a partition as variable if one doesn't already exist */

				if(!device_variable_exist()) 	// Variable partition doesn't exist
				{				// Can set any partition as variable
					if ( size > 0 )
					{
						device_resize_ex( cust_menu.data, size, false );
						sprintf( cust_menu.MenuId, "%s = %d MB", cust_menu.data, (int) ( device_partition_size( cust_menu.data ) / get_blk_per_mb() ) );
					}
					else if ( size == 0 )	// 0 is accepted
					{
						device_info.partition[device_partition_order(cust_menu.data)-1].asize = 1;
						device_resize_ex( cust_menu.data, size, false );
						sprintf( cust_menu.MenuId, "%s partition is auto-size (%d MB)", cust_menu.data , (int)( device_partition_size( cust_menu.data ) / get_blk_per_mb() ));
						thread_sleep(500);
						active_menu = &rept_menu;
					}
				}
				else 				// Variable partition already exists
				{				// Can not set another partition as variable
					if ( size > 0 )		// 0 is not accepted
					{
						device_resize_ex( cust_menu.data, size, false );
						sprintf( cust_menu.MenuId, "%s = %d MB", cust_menu.data, (int) ( device_partition_size( cust_menu.data ) / get_blk_per_mb() ) );
					}
				}
				redraw_menu();
				selector_enable();
			}
			else if ( !memcmp( subCommand, "write", strlen( subCommand ) ) )
			{
				// Apply changes
				device_resize_asize();
				device_commit();

				/* Load new PTABLE layout without rebooting */
				printf("\n   New PTABLE layout has been successfully loaded.\
					\n   Returning to MAIN MENU in a few seconds...");
				ptable_re_init();
				thread_sleep(5000);
				active_menu = &main_menu;
				redraw_menu();
				selector_enable();
				set_usb_status_listener(ON);
				return;
			}
			else
			{
				redraw_menu();
				selector_enable();
				printf("   HBOOT BUG: Somehow fell through eval_cmd()\n");
				return;
			}
		}
	}
	// REARRANGE PARTITIONS
	else if (!memcmp(command,"goto_rear", strlen(command)))
	{
		set_usb_status_listener(OFF);
		rear_menu.top_offset	= 0;
		rear_menu.bottom_offset	= 0;
		rear_menu.maxarl	= 0;
		rear_menu.goback	= 0;

		char command[64];
      		char partname[64];
		int k=0;
		for ( unsigned i = 1; i < MAX_NUM_PART+1; i++ )
		{
			for ( unsigned j = 0; j < MAX_NUM_PART; j++ )
			{
				if( ((int)i==mirror_info.partition[j].order) && (strlen( mirror_info.partition[j].name ) != 0) )
				{
					strcpy( command, "rear_" );
	      	      			strcat( command, mirror_info.partition[j].name );
	      	      			sprintf( partname, "   %s", mirror_info.partition[j].name);
	      	      			add_menu_item( &rear_menu, partname, command , k++ );
				}
			}
		}
            	add_menu_item( &rear_menu, "   PRINT PARTITION TABLE", "prnt_mirror_info" , k++ );
            	add_menu_item( &rear_menu, "   COMMIT CHANGES", "rear_commit" , k );

		active_menu = &rear_menu;
      		redraw_menu();
		selector_enable();
    	}
	// REARRANGE PARTITIONS SUBMENU
	else if (!memcmp(command, "rear_", 5 ))
	{
		char* subCommand = command + 5;
		if ( active_menu == &rear_menu )
		{
			if ( !memcmp( subCommand, "commit", strlen( subCommand ) ) )
			{
				cust_menu.top_offset	= 0;
				cust_menu.bottom_offset	= 0;
				cust_menu.selectedi	= 0;	// APPLY by default
				cust_menu.maxarl	= 0;
				cust_menu.goback	= 0;

				sprintf( cust_menu.MenuId, "COMMIT CHANGES" );
				strcpy( cust_menu.backCommand, "goto_rear" );

				add_menu_item(&cust_menu, "   APPLY"	, "rear_write", 0);
				add_menu_item(&cust_menu, "   CANCEL"	, "goto_rear" , 1);

				active_menu = &cust_menu;

				redraw_menu();
				selector_enable();
				printf(" \n\n\n");
				printf("    ____________________________________________________ \n\
					   |                  PARTITION  TABLE                  |\n\
					   |____________________________________________________|\n\
					   | MTDBLOCK# |   NAME   | AUTO-SIZE |  BLOCKS  |  MB  |\n");
				printf("   |===========|==========|===========|==========|======|\n");
				int ordercount=0;
				for ( unsigned i = 1; i < MAX_NUM_PART+1; i++ )
				{
					for ( unsigned j = 0; j < MAX_NUM_PART; j++ )
					{
						if( ((int)i==mirror_info.partition[j].order) && (strlen( mirror_info.partition[j].name ) != 0) )
						{
							printf( "   | mtdblock%i | %8s |     %i     |   %4i   | %3i  |\n", ordercount++, mirror_info.partition[j].name,
								mirror_info.partition[j].asize, mirror_info.partition[j].size, mirror_info.partition[j].size / get_blk_per_mb() );
						}
					}
				}
				printf("   |___________|__________|___________|__________|______|\n");
				return;
			}
			else
			{
				if(strcmp(subCommand, "recovery")) // Don't move recovery partition cause then we'll need pc to re-flash it
				{
					strcpy( cust_menu.data, subCommand );
					sprintf( cust_menu.MenuId, "%s as partition #%d", cust_menu.data, (int) ( mirror_partition_order( cust_menu.data ) ) );
					strcpy( cust_menu.backCommand, "goto_rear" );
					/* for every partition listed, the scale of order numbers is from [0] to [number-of-all-partitions]
					   			       0 can be used in order to remove a partition
					   			       Because we don't need to(must mot) remove the default partitions(recovery,misc,boot,system,userdata,cache) 0 is excluded from the scale
					*/
					int orderstart=0; 
					int menuselectfix=1; // fix menu selection if recovery is not the 1st partition
					if( (!strcmp(cust_menu.data, "misc")) || (!strcmp(cust_menu.data, "boot")) || (!strcmp(cust_menu.data, "userdata")) || (!strcmp(cust_menu.data, "system")) || (!strcmp(cust_menu.data, "cache")) )
					{
						if(device_partition_order( "recovery" )==1)
						{
							orderstart=2;menuselectfix=0;
						}
						else
						{
							orderstart=1;
						}
					}
					if( (device_partition_order(cust_menu.data))<(device_partition_order("recovery")) )
					{
						menuselectfix=0;
					}
					
					cust_menu.top_offset    = 0;
					cust_menu.bottom_offset = 0;
					cust_menu.selectedi     = (int)(mirror_partition_order(cust_menu.data) - orderstart - menuselectfix);
					cust_menu.maxarl	= 0;
					cust_menu.goback	= 0;

					char ordernumber[32];
					char ordercmd[32];
					int j=0;
					for ( int i = orderstart; i < (active_menu->maxarl - 1); i++ )
					{
						if(i!=device_partition_order( "recovery" )) // The order of recovery partition can not be available, because we don't move recovery partition
						{
							sprintf( ordernumber, "   %d", i );
							sprintf( ordercmd, "rear_set_%d", i );
							add_menu_item(&cust_menu, ordernumber			, ordercmd, j++);
						}
					}

					active_menu = &cust_menu;
					redraw_menu();
					selector_enable();
				}
			}
		}
		else if ( active_menu == &cust_menu )
		{
			if ( !memcmp( subCommand, "set_", 4 ) )
			{
				int neworder = atoi( subCommand + 4 );
				int oldorder = 0;
				for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
				{
					if ( !memcmp( mirror_info.partition[i].name, cust_menu.data, strlen( cust_menu.data ) ) )
					{
						oldorder = mirror_info.partition[i].order;					// save the initial position of the partition we selected to move
						if(oldorder!=neworder)							// cause after changing the partition's position the initial one is empty
						{
							for ( unsigned j = 0; j < MAX_NUM_PART; j++ )
							{
								if(mirror_info.partition[j].order == neworder)		// find the partition that is in the place where we will move the selected partition
								{
									mirror_info.partition[j].order = oldorder;		// place that partition to the empty initial position of the selected partition
								}
							}
						}
						mirror_info.partition[i].order = neworder;					// change the selected partition's position
						if(neworder==0) // if we set 0 for a partition's order, then that partition will be "removed"
						{	
							if(((int)i > 1) && ((int)i <= active_menu->maxarl)) // if the partition was not the first one, then we use the freed space by expanding the previous one
							{
								mirror_info.partition[i-1].size += mirror_info.partition[i].size;
							}
							else if((int)i == 1) // if the partition was the first one, then we use the freed space by expanding the next one
							{
								mirror_info.partition[i+1].size += mirror_info.partition[i].size;
							}
						}
						sprintf( cust_menu.MenuId, "%s as partition #%d", cust_menu.data, neworder );
					}
					if((oldorder>0) && (neworder==0))
      					{	
      						if(mirror_info.partition[i].order > oldorder)
      						{
      							mirror_info.partition[i].order--;
      						}
      					}
				}
				thread_sleep(500);

				rear_menu.top_offset	= 0;
				rear_menu.bottom_offset	= 0;
				rear_menu.maxarl	= 0;
				rear_menu.goback	= 0;

				char command[64];
		      		char partname[64];
				int k=0;
				for ( unsigned i = 1; i < MAX_NUM_PART+1; i++ )
				{
					for ( unsigned j = 0; j < MAX_NUM_PART; j++ )
					{
						if( ((int)i==mirror_info.partition[j].order) && (strlen( mirror_info.partition[j].name ) != 0) )
						{
							strcpy( command, "rear_" );
			      	      			strcat( command, mirror_info.partition[j].name );
			      	      			sprintf( partname, "   %s", mirror_info.partition[j].name);
			      	      			add_menu_item( &rear_menu, partname, command , k++ );
						}
					}
				}
		            	add_menu_item( &rear_menu, "   PRINT PARTITION TABLE", "prnt_mirror_info" , k++ );
		            	add_menu_item( &rear_menu, "   COMMIT CHANGES", "rear_commit" , k );

				active_menu = &rear_menu;
				redraw_menu();
				selector_enable();
			}
			else if ( !memcmp( subCommand, "write", strlen( subCommand ) ) )
			{
				// Apply changes
				char name_and_size[64];
				device_clear();
				for ( unsigned i = 1; i < MAX_NUM_PART+1; i++ )
				{
					for ( unsigned j = 0; j < MAX_NUM_PART; j++ )
					{
						if( ((int)i==mirror_info.partition[j].order) && (strlen( mirror_info.partition[j].name ) != 0) )
						{
							if(mirror_info.partition[j].asize) // if partition is auto-size
							{
								sprintf( name_and_size, "%s:%d", mirror_info.partition[j].name, 0 );
								device_add(name_and_size);
							}
							else // if partition has a fixed size
							{
								sprintf( name_and_size, "%s:%d", mirror_info.partition[j].name, mirror_info.partition[j].size / get_blk_per_mb() );
								device_add(name_and_size);
							}
						}
					}
				}
				device_resize_asize();
				device_commit();

				/* Load new PTABLE layout without rebooting */
				printf("\n   New PTABLE layout has been successfully loaded.\
					\n   Returning to MAIN MENU in a few seconds...");
				ptable_re_init();
				thread_sleep(5000);
				active_menu = &main_menu;
				redraw_menu();
				selector_enable();
				set_usb_status_listener(ON);
				return;
			}
			else
			{
				redraw_menu();
				selector_enable();
				printf("   HBOOT BUG: Somehow fell through eval_cmd()\n");
				return;
			}
		}
	}
	// PRINT PARTITION TABLE
	else if (!memcmp(command,"prnt_stat", strlen(command)))
	{
		redraw_menu();
		device_list();
		selector_enable();
	}
	// Option to show the partition table as it would be with the new partition order
	else if (!memcmp(command,"prnt_mirror_info", strlen(command)))
	{
		redraw_menu();
		printf("    ____________________________________________________ \n\
			   |                  PARTITION  TABLE                  |\n\
			   |____________________________________________________|\n\
			   | MTDBLOCK# |   NAME   | AUTO-SIZE |  BLOCKS  |  MB  |\n");
		printf("   |===========|==========|===========|==========|======|\n");
		int ordercount=0;
		for ( unsigned i = 1; i < MAX_NUM_PART+1; i++ )
		{
			for ( unsigned j = 0; j < MAX_NUM_PART; j++ )
			{
				if( ((int)i==mirror_info.partition[j].order) && (strlen( mirror_info.partition[j].name ) != 0) )
				{
					printf( "   | mtdblock%i | %8s |     %i     |   %4i   | %3i  |\n", ordercount++, mirror_info.partition[j].name,
						mirror_info.partition[j].asize, mirror_info.partition[j].size, mirror_info.partition[j].size / get_blk_per_mb() );
				}
			}
		}
		printf("   |___________|__________|___________|__________|______|\n");
		selector_enable();
	}
	// PRINT BAD BLOCK TABLE
	else if (!memcmp(command,"prnt_bblocks", strlen(command)))
	{
        	redraw_menu();
		printf("    ____________________________________________________ \n\
			   |                   BAD BLOCK TABLE                  |\n\
			   |____________________________________________________|\n");
		printf("   |       |   HOST    |  POSITION  | POSITION |        |\n\
			   | BLOCK | PARTITION | from START | from END | STATUS |\n");
		printf("   |=======|===========|============|==========|========|\n");
		for ( int j = 0; j < block_tbl.count; j++ )
		{
      			printf( "   | %5d | %9s | %10d | %8d | %6s |\n",
      				block_tbl.blocks[j].pos,
      				block_tbl.blocks[j].partition,
      				block_tbl.blocks[j].pos_from_pstart,
      				block_tbl.blocks[j].pos_from_pend,
				block_tbl.blocks[j].is_marked==1 ? "MARKED" : "ERROR");
		}
		printf("   |_______|___________|____________|__________|________|   ");
		selector_enable();
    	}
	// FORMAT NAND
	else if (!memcmp(command,"format_nand", strlen(command)))
	{
		set_usb_status_listener(OFF);
		redraw_menu();
		printf( "   Initializing flash format..." );
		
		struct ptable *ptable;
		ptable = flash_get_ptable();
		if(ptable == NULL) 
		{
			printf( "   ERROR: PTABLE not found!!!\n");
			return;
		}

		// Format all partitions except for recovery
		time_t ct = current_time();
		for ( int i = 0; i < ptable_size(ptable); i++ )
		{
			if(memcmp(ptable_get(ptable, i)->name,"recovery", strlen(ptable_get(ptable, i)->name)))
			{
				printf( "\n   Formatting %s...", ptable_get(ptable, i)->name );
				flash_erase(ptable_get(ptable, i));
			}
		}
		ct = current_time() - ct;
		printf("\n   Format completed in %i msec!\n", ct);

		selector_enable();
		set_usb_status_listener(ON);
    	}
#if WITH_DEV_SD
	// FLASH LEOREC.IMG
	else if (!memcmp(command,"flash_recovery", strlen(command)))
	{
		set_usb_status_listener(OFF);
		redraw_menu();
		printf( "\n   Initializing flash process..." );

		// Load recovery image from sdcard
		fs_init();
		void *buf;
		fs_load_file("/LEOREC.img", buf);
		fs_stop();

		// Format recovery partition
		struct ptable *ptable;
		ptable = flash_get_ptable();
		if(ptable == NULL) 
		{
			printf( "\n   ERROR: PTABLE not found!!!");
			return;
		}
		for ( int i = 0; i < ptable_size(ptable); i++ )
		{
			if(!memcmp(ptable_get(ptable, i)->name,"recovery", strlen(ptable_get(ptable, i)->name)))
			{
				printf( "\n   Formatting %s...", ptable_get(ptable, i)->name );
				flash_erase(ptable_get(ptable, i));
			}
		}
		printf("\n   Format completed !");

		// Write recovery partition
		struct ptentry *recptn;
		recptn = ptable_find(ptable, "recovery");
		if(recptn == NULL) 
		{
			printf("\n   ERROR: No recovery partition found!!!");
			return;
		}
		if(flash_write(recptn, 0, (void*)buf, (sizeof(buf))))
		{
			printf("\n   ERROR: Cannot write partition!!!");
			return;
		}
		printf("\n   Recovery updated !\n");

		free(buf);

		selector_enable();
		set_usb_status_listener(ON);
    	}
#endif
	// ADD sBOOT
	else if (!memcmp(command,"enable_sboot", strlen(command)))
	{
		set_usb_status_listener(OFF);
		active_menu = &sett_menu;
		change_menu_item(&sett_menu, "   ADD sBOOT", "   REMOVE sBOOT", "disable_sboot");
		add_menu_item(&sett_menu, "   ADD tBOOT", "enable_tboot", 6);
            	redraw_menu();

		char name_and_size[64];
		device_clear();
		for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
		{
			if(strlen( mirror_info.partition[i].name ) != 0)
			{
				if(!memcmp(mirror_info.partition[i].name, "userdata", strlen("userdata")))
				{
					// Create sboot after userdata by taking 5MB from userdata
					if(mirror_info.partition[i].asize)
					{
						device_add("userdata:0");  // if userdata is auto-size
					}
					else
					{
						sprintf( name_and_size, "%s:%d", "userdata", ((mirror_info.partition[i].size / get_blk_per_mb()) - 5) );
						device_add(name_and_size); // if userdata has a fixed size
					}
					device_add( "sboot:5" );
				}
				else
				{
					sprintf( name_and_size, "%s:%d", mirror_info.partition[i].name, mirror_info.partition[i].size / get_blk_per_mb() );
					device_add(name_and_size);
				}
			}
		}
		device_resize_asize();
		device_commit();
		device_list();

		/* Load new PTABLE layout without rebooting */
      		printf("\n   New PTABLE layout has been successfully loaded.\
			\n   Returning to MAIN MENU in a few seconds...");
		ptable_re_init();
		flash_erase(ptable_find(flash_get_ptable(), "sboot"));
		flash_erase(ptable_find(flash_get_ptable(), "cache"));
		thread_sleep(5000);
		active_menu = &main_menu;
		redraw_menu();
		set_usb_status_listener(ON);
		selector_enable();
    	}
	// REMOVE sBOOT
	else if (!memcmp(command,"disable_sboot", strlen(command)))
	{
		set_usb_status_listener(OFF);
		active_menu = &sett_menu;
		change_menu_item(&sett_menu, "   REMOVE sBOOT", "   ADD sBOOT", "enable_sboot");
		rem_menu_item(&sett_menu, 6);
		redraw_menu();
		// Use the freed space for userdata by default
		int new_data_size=(int)( (device_partition_size("userdata")/get_blk_per_mb()) + (device_partition_size("sboot")/get_blk_per_mb()) );
		flash_erase(ptable_find(flash_get_ptable(), "sboot"));
		device_del("sboot");
		if(!mirror_info.partition[device_partition_order("userdata")-1].asize)
		{
			device_resize_ex("userdata", new_data_size, false); // if userdata has a fixed size
		}
		device_resize_asize();
		device_commit();
		device_list();
		
      		/* Load new PTABLE layout without rebooting */
      		printf("\n   New PTABLE layout has been successfully loaded.\
			\n   Returning to MAIN MENU in a few seconds...");
		ptable_re_init();
		thread_sleep(5000);
		active_menu = &main_menu;
		if(active_menu->maxarl>8){rem_menu_item(&main_menu, 1);}
		redraw_menu();
		selector_enable();
		set_usb_status_listener(ON);
    	}
	// ADD tBOOT
	else if (!memcmp(command,"enable_tboot", strlen(command)))
	{
		set_usb_status_listener(OFF);
		active_menu = &sett_menu;
		change_menu_item(&sett_menu, "   ADD tBOOT", "   REMOVE tBOOT", "disable_tboot");
		rem_menu_item(&sett_menu, 5);
		redraw_menu();

		char name_and_size[64];
		device_clear();
		for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
		{
			if(strlen( mirror_info.partition[i].name ) != 0)
			{
				if(!memcmp(mirror_info.partition[i].name, "userdata", strlen("userdata")))
				{
					// Create sboot after userdata by taking 5MB from userdata
					if(mirror_info.partition[i].asize)
					{
						device_add("userdata:0");  // if userdata is auto-size
					}
					else
					{
						sprintf( name_and_size, "%s:%d", "userdata", ((mirror_info.partition[i].size / get_blk_per_mb()) - 5) );
						device_add(name_and_size); // if userdata has a fixed size
					}
					device_add( "tboot:5" );
					device_add( "sboot:5" );
				}
				else
				{
					sprintf( name_and_size, "%s:%d", mirror_info.partition[i].name, mirror_info.partition[i].size / get_blk_per_mb() );
					device_add(name_and_size);
				}
			}
		}
		device_resize_asize();
		device_commit();
		device_list();

		/* Load new PTABLE layout without rebooting */
      		printf("\n   New PTABLE layout has been successfully loaded.\
			\n   Returning to MAIN MENU in a few seconds...");
		ptable_re_init();
		flash_erase(ptable_find(flash_get_ptable(), "tboot"));
		thread_sleep(5000);
		active_menu = &main_menu;
		redraw_menu();
		selector_enable();
		set_usb_status_listener(ON);
    	}
	// REMOVE tBOOT
	else if (!memcmp(command,"disable_tboot", strlen(command)))
	{
		set_usb_status_listener(OFF);
		active_menu = &sett_menu;
		change_menu_item(&sett_menu, "   REMOVE tBOOT", "   ADD tBOOT", "enable_tboot");
		add_menu_item(&sett_menu, "   REMOVE sBOOT", "disable_sboot", 5);
            	redraw_menu();
		// Use the freed space for userdata by default
		int new_data_size=(int)( (device_partition_size("userdata")/get_blk_per_mb()) + (device_partition_size("tboot")/get_blk_per_mb()) );
		flash_erase(ptable_find(flash_get_ptable(), "tboot"));
		device_del("tboot");
		if(!mirror_info.partition[device_partition_order("userdata")-1].asize)
		{
			device_resize_ex("userdata", new_data_size, false); // if userdata has a fixed size
		}
		device_resize_asize();
		device_commit();
		device_list();
		
      		/* Load new PTABLE layout without rebooting */
      		printf("\n   New PTABLE layout has been successfully loaded.\
			\n   Returning to MAIN MENU in a few seconds...");
		ptable_re_init();
		thread_sleep(5000);
		active_menu = &main_menu;
		if(active_menu->maxarl>8){rem_menu_item(&main_menu, 2);}
		redraw_menu();
		selector_enable();
		set_usb_status_listener(ON);
    	}
	// Enabling ExtROM will automatically use the last 24 MB for cache
	else if (!memcmp(command,"enable_extrom", strlen(command)))
	{
		set_usb_status_listener(OFF);
		active_menu = &sett_menu;
		change_menu_item(&sett_menu, "   ENABLE ExtROM", "   DISABLE ExtROM", "disable_extrom");
		redraw_menu();

		/* Delete cache 						-> free		(cache size)		MB
		   Resize userdata by adding to it the freed (cache size)MB 	-> free		0			MB 
		   Enable ExtROM						-> free		24			MB
		   Add cache with 24MB size					-> free		0			MB
		*/
		unsigned cachesize = device_partition_size( "cache" ) / get_blk_per_mb();

		printf( "   Wiping cache..." );
		flash_erase(ptable_find(flash_get_ptable(), "cache"));
		device_del( "cache" );

		printf( "\n   Increasing userdata by %dMB...", cachesize );
		if(!mirror_info.partition[device_partition_order("userdata")-1].asize) // if userdata has a fixed size
		{
			device_resize_ex("userdata", ((int) device_partition_size( "userdata" ) / get_blk_per_mb()) + (int)cachesize, false);
		}
		printf( "\n   Enabling ExtROM..." );
		device_enable_extrom();

		printf( "\n   Assigning last 24MB of ExtROM to cache...\n" );
		device_add( "null:1:b" );
		device_add( "cache:191:b" );

		device_resize_asize();
		device_commit();

      		/* Load new PTABLE layout without rebooting */
      		printf("\n   New PTABLE layout has been successfully loaded.\
			\n   Returning to MAIN MENU in a few seconds...");
		ptable_re_init();
		thread_sleep(5000);
		active_menu = &main_menu;
		redraw_menu();
		selector_enable();
		set_usb_status_listener(ON);
    	}
	// Disabling ExtROM will automatically resize cache
	else if (!memcmp(command,"disable_extrom", strlen(command)))
	{
		set_usb_status_listener(OFF);
		active_menu = &sett_menu;
		change_menu_item(&sett_menu, "   DISABLE ExtROM", "   ENABLE ExtROM", "enable_extrom");
		redraw_menu();

		/* Delete cache 						-> free		(cache size)		MB
		   Disable ExtROM						-> free		(cache size) - 24	MB
		   Resize userdata by adding to it (((cache size) - 24) -5)MB	-> free		5			MB
				    		  reserving 5MB for cache
		   Add cache with 5MB size					-> free		0			MB
		*/
		unsigned cachesize = device_partition_size( "cache" ) / get_blk_per_mb();

		printf( "   Wiping cache..." );
		flash_erase(ptable_find(flash_get_ptable(), "cache"));
		device_del( "null" );
		device_del( "cache" );

		printf( "\n   Disabling ExtROM..." );
		device_disable_extrom();

		if((int)cachesize>24)
		{
			if(cachesize-24>5){
			printf( "\n   Increasing userdata by %dMB...", cachesize-29 );
			}else{
			printf( "\n   Decreasing userdata by %dMB...", cachesize-29 );
			}
			if(!mirror_info.partition[device_partition_order("userdata")-1].asize) // if userdata has a fixed size
			{
				device_resize_ex( "userdata", ((int) device_partition_size( "userdata" ) / get_blk_per_mb()) - 5 + (cachesize-24), false);
			}
			printf( "\n   Assigning last 5MB to cache...\n" );
			device_add( "cache:5" );
		}
		else
		{
			printf( "\n   Decreasing userdata by %dMB...", 5 );
			if(!mirror_info.partition[device_partition_order("userdata")-1].asize) // if userdata has a fixed size
			{
				device_resize_ex( "userdata", ((int) device_partition_size( "userdata" ) / get_blk_per_mb()) - 5, false );
			}
			printf( "\n   Assigning last 5MB to cache...\n" );
			device_add( "cache:5" );
		}

		device_resize_asize();
		device_commit();

      		/* Load new PTABLE layout without rebooting */
      		printf("\n   New PTABLE layout has been successfully loaded.\
			\n   Returning to MAIN MENU in a few seconds...");
		ptable_re_init();
		thread_sleep(5000);
		active_menu = &main_menu;
		redraw_menu();
		selector_enable();
		set_usb_status_listener(ON);
    	}
	// INVERT SCREEN COLORS
	else if (!memcmp(command,"invert_colors", strlen(command)))
	{
		if(inverted)
		{
			inverted = false;
			device_info.inverted_colors = 0;
		}
		else
		{
			inverted = true;
			device_info.inverted_colors = 1;
		}
		device_commit();
		display_init();
		active_menu = &sett_menu;
		redraw_menu();
		selector_enable();
	}
	// REBOOT
	else if (!memcmp(command,"acpu_ggwp", strlen(command)))
	{
		set_usb_status_listener(OFF);
        	reboot_device(0);
	}
	// REBOOT cLK
	else if (!memcmp(command,"acpu_bgwp", strlen(command)))
	{
		set_usb_status_listener(OFF);
		reboot_device(FASTBOOT_MODE);
    	}
	// POWERDOWN
	else if (!memcmp(command,"acpu_pawn", strlen(command)))
	{
		set_usb_status_listener(OFF);
        	shutdown();
	}
	// INFO
	else if (!memcmp(command,"goto_about", strlen(command)))
	{
		active_menu = &about_menu;
		redraw_menu();
		selector_enable();
    	}
      	// HIDE STARTUP INFO
	else if (!memcmp(command,"disable_info", strlen(command)))
	{
		active_menu = &about_menu;
		change_menu_item(&about_menu, "   HIDE STARTUP INFO", "   SHOW STARTUP INFO", "enable_info");
		redraw_menu();

		device_info.show_startup_info = 0;
		device_commit();

		selector_enable();
	}
	// SHOW STARTUP INFO
	else if (!memcmp(command,"enable_info", strlen(command)))
	{
		active_menu = &about_menu;
		change_menu_item(&about_menu, "   SHOW STARTUP INFO", "   HIDE STARTUP INFO", "disable_info");
		redraw_menu();

		device_info.show_startup_info = 1;
		device_commit();

		selector_enable();
	}
	// PRINT NAND INFO
	else if (!memcmp(command,"prnt_nand", strlen(command)))
	{
		redraw_menu();
		prnt_nand_stat();
		selector_enable();
	}
	// CREDITS
	else if (!memcmp(command,"goto_credits", strlen(command)))
	{
		redraw_menu();
		cmd_oem_credits();
		selector_enable();
    	}
	// HELP
	else if (!memcmp(command,"goto_help", strlen(command)))
	{
		redraw_menu();
		oem_help();
		selector_enable();
    	}
	// Return to MAIN MENU
	else if (!memcmp(command,"goto_main", strlen(command)))
	{
		active_menu = &main_menu;
		set_usb_status_listener(ON);
		redraw_menu();
		selector_enable();
    	}
	// ERROR
	else
	{
		redraw_menu();
		selector_enable();
		printf("   HBOOT BUG: Somehow fell through eval_cmd()\n");
	}
}

void redraw_menu(void)
{
	fbcon_resetdisp();
	draw_clk_header();
	
	int current_offset = fbcon_get_y_cord();

	if (active_menu->top_offset < 1)
		active_menu->top_offset = current_offset;
	else
		fbcon_set_y_cord(active_menu->top_offset);

	unsigned mybg = 0x0000;
	if(inverted){mybg = 0xffff;}
			
	if (active_menu->top_offset > 1 && active_menu->bottom_offset > 1)
		fbcon_clear_region(active_menu->top_offset,active_menu->bottom_offset, mybg);
		
	for (uint8_t i=0;; i++)
	{
		if ((strlen(active_menu->item[i].mTitle) != 0) && !(i >= active_menu->maxarl) )
		{
			unsigned myfg = 0xffff;
			if(inverted){myfg = 0x0000;}

			fbcon_setfg(myfg);

			if(active_menu->item[i].x == 0)
				active_menu->item[i].x = fbcon_get_x_cord();
			if(active_menu->item[i].y == 0)
				active_menu->item[i].y = fbcon_get_y_cord();

			_dputs(active_menu->item[i].mTitle);
			_dputs("\n");
		} 
		else {
			break;
		}
	}

	_dputs("\n");
		
	if (active_menu->bottom_offset < 1)
		active_menu->bottom_offset = fbcon_get_y_cord();
	else
		fbcon_set_y_cord(active_menu->bottom_offset);

	if (current_offset > fbcon_get_y_cord())
		fbcon_set_y_cord(current_offset);
}

static int menu_item_down()
{
	thread_set_priority(HIGHEST_PRIORITY);
	
	if (didyouscroll())
		redraw_menu();
	
	int current_y_offset = fbcon_get_y_cord();
	int current_x_offset = fbcon_get_x_cord();
	
	selector_disable();
	
	if (active_menu->selectedi == (active_menu->maxarl-1))
		active_menu->selectedi=0;
	else
		active_menu->selectedi++;
	
	selector_enable();
	
	fbcon_set_y_cord(current_y_offset);
	fbcon_set_x_cord(current_x_offset);
	
	thread_set_priority(DEFAULT_PRIORITY);
	return 0;
}

static int menu_item_up()
{
	thread_set_priority(HIGHEST_PRIORITY);
	
	if (didyouscroll())
		redraw_menu();
	
	int current_y_offset = fbcon_get_y_cord();
	int current_x_offset = fbcon_get_x_cord();
	
	selector_disable();
	
	if ((active_menu->selectedi) == 0)
		active_menu->selectedi=(active_menu->maxarl-1);
	else
		active_menu->selectedi--;
	
	selector_enable();
	
	fbcon_set_y_cord(current_y_offset);
	fbcon_set_x_cord(current_x_offset);
	
	thread_set_priority(DEFAULT_PRIORITY);
	return 0;
}

void eval_keydown(void)
{
	if (keyp == ERR_KEY_CHANGED)
		return;

	switch (keys[keyp])
	{
		case KEY_VOLUMEUP:
			menu_item_up();
			break;
		case KEY_VOLUMEDOWN:
			menu_item_down();
			break;
		case KEY_SEND: // dial
			eval_command();
			break;
		case KEY_CLEAR: // hangup
			break;
		case KEY_BACK:
			active_menu->goback = 1;
			eval_command();
			break;
	}
}

void eval_keyup(void)
{
	if (keyp == ERR_KEY_CHANGED)
		return;

	switch (keys[keyp])
	{
		case KEY_VOLUMEUP:
			break;
		case KEY_VOLUMEDOWN:
			break;
		case KEY_SEND: // dial
			break;
		case KEY_CLEAR: //hangup
			break;
		case KEY_BACK:
			break;
	}
}

int key_repeater(void *arg)
{
	uint16_t last_key_i_remember = keyp;
	uint8_t counter1 = 0;
	
	for(;;)
	{
		if ((keyp == ERR_KEY_CHANGED || (last_key_i_remember!=keyp)))
		{
			thread_exit(0);
			return 0;
		} 
		else 
		{
			thread_sleep(10);
			counter1++;
			if(counter1>75)
			{
				counter1=0;
				break;
			}
		}
	}

	while((keyp!=ERR_KEY_CHANGED)&&(last_key_i_remember==keyp)&&(keys_get_state(keys[keyp])!=0))
	{
		eval_keydown();
		thread_sleep(100);
	}

	thread_exit(0);
	return 0;
}

int key_listener(void *arg)
{
	for (;;)
	{
        for(uint16_t i=0; i< sizeof(keys)/sizeof(uint16_t); i++)
		{
			if (keys_get_state(keys[i]) != 0)
			{
				keyp = i;
				eval_keydown();
				thread_resume(thread_create("key_repeater", &key_repeater, NULL, DEFAULT_PRIORITY, 4096));
				while (keys_get_state(keys[keyp]) !=0)
					thread_sleep(1);
				eval_keyup();
				keyp = 99;
			}
		}
	}
	
	return 0;
}

/*
 * koko: Print USB CABLE STATUS at bottom
 */
void usb_status_checker(void)
{
	if (active_menu != NULL)
	{
		thread_set_priority(HIGHEST_PRIORITY);

		int usb_stat = usb_cable_status();
	
		int current_y_offset = fbcon_get_y_cord();
		int current_x_offset = fbcon_get_x_cord();

		fbcon_clear_region(49, 50 , 0x0000);
		fbcon_set_y_cord(49);fbcon_set_x_cord(28);
		fbcon_settg(0x0000);fbcon_setfg(0xffff);
	
		if(usb_stat == 1){
			_dputs("USB");
		}else{
			_dputs("   ");
		}
	
		fbcon_set_y_cord(current_y_offset);
		fbcon_set_x_cord(current_x_offset);

		thread_set_priority(DEFAULT_PRIORITY);
	}
}

// Check if usb cable is connected every 3000 msec
int usb_status_listener(void *arg)
{
	while(run_usb_listener)
	{
		usb_status_checker();
		mdelay(3000);
	}

	thread_exit(0);
	return 0;
}

void set_usb_status_listener(int state)
{
	switch (state)
	{
		case ON:
		{
			if(run_usb_listener == 1){
				break;
			}else{
				run_usb_listener = 1;
				if(!thread_exist_in_list("start_usbstatuslistener")){
					thread_resume(thread_create("start_usbstatuslistener", &usb_status_listener, NULL, DEFAULT_PRIORITY, DEFAULT_STACK_SIZE));
				}
				break;
			}
		}
		case OFF:
		{
			run_usb_listener = 0;
			break;
		}
	}
}

/* koko : Added "   "(3 spaces) in front of each item's Name 
 *	  because this way the "   "(3 spaces) can be changed to ">> " for the new selection method
 */
void init_menu()
{	
	int i=0;int j=2;int k=0;
	main_menu.top_offset    = 0;
	main_menu.bottom_offset = 0;
	main_menu.maxarl	= 0;
	main_menu.goback	= 0;

	strcpy( main_menu.MenuId, "MAIN MENU" );
	strcpy( main_menu.backCommand, "" );

	//PRIMARY BOOT
      	add_menu_item(&main_menu, "   ANDROID @  BOOT"		, "boot_pboot", i++);
	if (device_partition_exist("sboot")){
      	//SECONDARY BOOT
      	add_menu_item(&main_menu, "   ANDROID @ sBOOT"		, "boot_sboot", i++);
	j++;
	}
	if (device_partition_exist("tboot")){
	//TERTIARY BOOT
      	add_menu_item(&main_menu, "   ANDROID @ tBOOT"		, "boot_tboot", i++);
	j++;
	}
	add_menu_item(&main_menu, "   RECOVERY"			, "boot_recv", i++);
	add_menu_item(&main_menu, "   FLASHLIGHT"		, "init_flsh", i++);
	add_menu_item(&main_menu, "   SETTINGS"			, "goto_sett", i++);
	add_menu_item(&main_menu, "   REBOOT"			, "acpu_ggwp", i++);
	add_menu_item(&main_menu, "   REBOOT cLK"		, "acpu_bgwp", i++);
	add_menu_item(&main_menu, "   POWERDOWN"		, "acpu_pawn", i++);
	add_menu_item(&main_menu, "   INFO"			, "goto_about", i++);

	main_menu.selectedi     = j; // FLASHLIGHT by default

      	sett_menu.top_offset    = 0;
        sett_menu.bottom_offset = 0;
        sett_menu.maxarl	= 0;
        sett_menu.goback	= 0;
      
      	strcpy( sett_menu.MenuId, "SETTINGS" );
      	strcpy( sett_menu.backCommand, "goto_main" );
            
        add_menu_item(&sett_menu, "   RESIZE PARTITIONS"	, "goto_rept",k++);
        add_menu_item(&sett_menu, "   REARRANGE PARTITIONS"	, "goto_rear",k++);
        add_menu_item(&sett_menu, "   PRINT PARTITION TABLE"	, "prnt_stat",k++);
        if( _bad_blocks > 0 ){
        add_menu_item(&sett_menu, "   PRINT BAD BLOCK TABLE"	, "prnt_bblocks",k++);
        }
        add_menu_item(&sett_menu, "   FORMAT NAND"		, "format_nand",k++);
#if WITH_DEV_SD
        add_menu_item(&sett_menu, "   UPDATE RECOVERY"       	, "flash_recovery",k++);
#endif
        if (!device_partition_exist("tboot")){
        	if (device_partition_exist("sboot")){
            	add_menu_item(&sett_menu, "   REMOVE sBOOT"		, "disable_sboot",k++);
            	}else{
            	add_menu_item(&sett_menu, "   ADD sBOOT"		, "enable_sboot",k++);
            	}
      	}
      	if (device_partition_exist("sboot")){
            	if (device_partition_exist("tboot")){
            	add_menu_item(&sett_menu, "   REMOVE tBOOT"	, "disable_tboot",k++);
            	}else{
            	add_menu_item(&sett_menu, "   ADD tBOOT"	, "enable_tboot",k++);
            	}
      	}
        if (device_info.extrom_enabled){
        add_menu_item(&sett_menu, "   DISABLE ExtROM"		, "disable_extrom",k++);
        }else{
        add_menu_item(&sett_menu, "   ENABLE ExtROM"		, "enable_extrom",k++);
        }
        add_menu_item(&sett_menu, "   INVERT SCREEN COLORS"	, "invert_colors",k++);

	strcpy( rept_menu.MenuId, "RESIZE PARTITIONS" );
      	strcpy( rept_menu.backCommand, "goto_sett" );
	
      	strcpy( rear_menu.MenuId, "REARRANGE PARTITIONS" );
      	strcpy( rear_menu.backCommand, "goto_sett" );
	
	about_menu.top_offset    = 0;
      	about_menu.bottom_offset = 0;
      	about_menu.selectedi     = 0;
      	about_menu.maxarl	= 0;
      	about_menu.goback	= 0;
	
	strcpy( about_menu.MenuId, "INFO" );
	strcpy( about_menu.backCommand, "goto_main" );
		
      	if (device_info.show_startup_info){
      	add_menu_item(&about_menu, "   HIDE STARTUP INFO"	, "disable_info", 0);
      	}else{
      	add_menu_item(&about_menu, "   SHOW STARTUP INFO"	, "enable_info", 0);
      	}
	add_menu_item(&about_menu, "   PRINT NAND INFO"		, "prnt_nand", 1);
      	add_menu_item(&about_menu, "   CREDITS"			, "goto_credits", 2);
      	add_menu_item(&about_menu, "   HELP"			, "goto_help", 3);
	      	
      	strcpy( cred_menu.MenuId, "CREDITS" );
      	strcpy( cred_menu.backCommand, "goto_about" );
	      
      	strcpy( help_menu.MenuId, "HELP" );
      	strcpy( help_menu.backCommand, "goto_about" );

      	active_menu = &main_menu;
	redraw_menu();
	selector_enable();
}

static void ptentry_to_tag(unsigned **ptr, struct ptentry *ptn)
{
	struct atag_ptbl_entry atag_ptn;

	if (ptn->type == TYPE_MODEM_PARTITION)
		return;
	memcpy(atag_ptn.name, ptn->name, 16);
	atag_ptn.name[15] = '\0';
	atag_ptn.offset = ptn->start;
	atag_ptn.size = ptn->length;
	atag_ptn.flags = ptn->flags;
	memcpy(*ptr, &atag_ptn, sizeof(struct atag_ptbl_entry));
	*ptr += sizeof(struct atag_ptbl_entry) / sizeof(unsigned);
}

void boot_linux(void *kernel, unsigned *tags, 
		const char *cmdline, unsigned machtype,
		void *ramdisk, unsigned ramdisk_size)
{
	unsigned *ptr = tags;
	unsigned pcount = 0;
	void (*entry)(unsigned,unsigned,unsigned*) = kernel;
	struct ptable *ptable;
	int cmdline_len = 0;
	int have_cmdline = 0;
	int pause_at_bootup = 0;

	/* CORE */
	*ptr++ = 2;
	*ptr++ = 0x54410001;

	if (ramdisk_size)
	{
		*ptr++ = 4;
		*ptr++ = 0x54420005;
		*ptr++ = (unsigned)ramdisk;
		*ptr++ = ramdisk_size;
	}

	ptr = target_atag_mem(ptr);
	if( (ptable = flash_get_ptable()) && (ptable->count != 0) )
	{
      		int i;
      		for(i=0; i < ptable->count; i++)
      		{
      			struct ptentry *ptn;
      			ptn =  ptable_get(ptable, i);
      			if (ptn->type == TYPE_APPS_PARTITION)
      				pcount++;
      		}
      		*ptr++ = 2 + (pcount * (sizeof(struct atag_ptbl_entry) / sizeof(unsigned)));
      		*ptr++ = 0x4d534d70;
      		for (i = 0; i < ptable->count; ++i)
      			ptentry_to_tag(&ptr, ptable_get(ptable, i));
	}

	if (cmdline && cmdline[0])
	{
		cmdline_len = strlen(cmdline);
		have_cmdline = 1;
	}
	if (target_pause_for_battery_charge())
	{
		pause_at_bootup = 1;
		cmdline_len += strlen(battchg_pause);
	}

	if (cmdline_len > 0)
	{
		const char *src;
		char *dst;
		unsigned n;
		/* include terminating 0 and round up to a word multiple */
		n = (cmdline_len + 4) & (~3);
		*ptr++ = (n / 4) + 2;
		*ptr++ = 0x54410009;
		dst = (char *)ptr;
		if (have_cmdline)
		{
			src = cmdline;
			while ((*dst++ = *src++));
		}
		if (pause_at_bootup)
		{
			src = battchg_pause;
			if (have_cmdline) --dst;
			while ((*dst++ = *src++));
		}
		ptr += (n / 4);
	}

	/* END */
	*ptr++ = 0;
	*ptr++ = 0;

	printf( "booting Linux @ %p, ramdisk @ %p (%d)\n", kernel, ramdisk, ramdisk_size);
	if (cmdline)
		printf( "cmdline: %s\n", cmdline);

	enter_critical_section();
	platform_uninit_timer();
	arch_disable_cache(UCACHE);
	arch_disable_mmu();

#if DISPLAY_SPLASH_SCREEN
	display_shutdown();
#endif

	htcleo_boot();
	entry(0, machtype, tags);
}

/*
 * koko : Ported Rick's commits for fixing bad boot
 */
int boot_linux_from_flash(void)
{
	struct boot_img_hdr *hdr = (void*) buf;
	unsigned n;
	struct ptentry *ptn;
	struct ptable *ptable;
	unsigned offset = 0;
	char *cmdline;

	set_usb_status_listener(OFF);

	ptable = flash_get_ptable();
	if (ptable == NULL)
	{
		dprintf(CRITICAL, "   ERROR: Partition table not found\n");
		goto failed;
	}

	/* Fixed Hang after failed boot */
	if (boot_into_recovery)
	{ 
		// Boot from recovery
		boot_into_sboot = 0;
		boot_into_tboot = 0;
		dprintf(ALWAYS,"\n\nBooting to recovery ...\n\n");
		
        	ptn = ptable_find(ptable, "recovery");
        	if (ptn == NULL)
		{
	        	dprintf(CRITICAL, "   ERROR: No recovery partition found\n");
			boot_into_recovery=0;
	        	goto failed;
        	}
	}
	else if (boot_into_sboot)
	{ 
		//Boot from sboot partition
		printf("\n\nBooting from sboot partition ...\n\n");
		ptn = ptable_find(ptable,"sboot");
		if (ptn == NULL)
		{
			dprintf(CRITICAL,"   ERROR: No sboot partition found!\n");
			boot_into_sboot=0;
			goto failed;
		}
	}
	else if (boot_into_tboot)
	{ 
		//Boot from tboot partition
		printf("\n\nBooting from tboot partition ...\n\n");
		ptn = ptable_find(ptable,"tboot");
		if (ptn == NULL)
		{
			dprintf(CRITICAL,"   ERROR: No tboot partition found!\n");
			boot_into_tboot=0;
			goto failed;
		}
	}
	else 
	{ 
		// Standard boot
		printf("\n\nNormal boot ...\n\n");
        	ptn = ptable_find(ptable, "boot");
        	if (ptn == NULL)
		{
	        	dprintf(CRITICAL, "   ERROR: No boot partition found\n");
	        	goto failed;
        	}
	}

	if (flash_read(ptn, offset, buf, page_size))
	{ 
		dprintf(CRITICAL, "   ERROR: Cannot read boot image header\n");
		goto failed;
	}
	offset += page_size;

	if (memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE))
	{
		dprintf(CRITICAL, "   ERROR: Invaled boot image heador\n");
		goto failed;
	}

	if (hdr->page_size != page_size)
	{
		dprintf(CRITICAL, "   ERROR: Invalid boot image pagesize. Device pagesize: %d, Image pagesize: %d\n",page_size,hdr->page_size);
		goto failed;
	}

	n = ROUND_TO_PAGE(hdr->kernel_size, page_mask);
	if (flash_read(ptn, offset, (void *)hdr->kernel_addr, n))
	{
		dprintf(CRITICAL, "   ERROR: Cannot read kernel image\n");
		goto failed;
	}
	offset += n;

	n = ROUND_TO_PAGE(hdr->ramdisk_size, page_mask);
	if (flash_read(ptn, offset, (void *)hdr->ramdisk_addr, n))
	{
		dprintf(CRITICAL, "   ERROR: Cannot read ramdisk image\n");
		goto failed;
	}
	offset += n;

	printf( "kernel  @ %x (%d bytes)\n", hdr->kernel_addr, hdr->kernel_size);
	printf( "ramdisk @ %x (%d bytes)\n", hdr->ramdisk_addr, hdr->ramdisk_size);

	if (hdr->cmdline[0])
		cmdline = (char*) hdr->cmdline;
	else
		cmdline = "";

	strcat(cmdline," clk=");
	strcat(cmdline,PSUEDO_VERSION);
	/* TODO: create/pass atags to kernel */
	printf( "cmdline = '%s'\n", cmdline);

	printf( "Booting Linux ...\n");
	boot_linux((void *)hdr->kernel_addr, (void *)TAGS_ADDR,
		   (const char *) cmdline, board_machtype(),
		   (void *)hdr->ramdisk_addr, hdr->ramdisk_size);
	return 0;

failed:
	{
		printf("\n\n   An irrecoverable error occured!!!\n   Device will reboot to bootloader in 5s.");
		thread_sleep(5000);
		reboot_device(FASTBOOT_MODE);
		return -1;
	}
}

void cmd_boot(const char *arg, void *data, unsigned sz)
{
	unsigned kernel_actual;
	unsigned ramdisk_actual;
	static struct boot_img_hdr hdr;
	char *ptr = ((char*) data);

	if (sz < sizeof(hdr))
	{
		fastboot_fail("invalid bootimage header");
		return;
	}

	memcpy(&hdr, data, sizeof(hdr));

	/* ensure commandline is terminated */
	hdr.cmdline[BOOT_ARGS_SIZE-1] = 0;

	kernel_actual = ROUND_TO_PAGE(hdr.kernel_size, page_mask);
	ramdisk_actual = ROUND_TO_PAGE(hdr.ramdisk_size, page_mask);

	memmove((void*) KERNEL_ADDR, ptr + page_size, hdr.kernel_size);
	memmove((void*) RAMDISK_ADDR, ptr + page_size + kernel_actual, hdr.ramdisk_size);

	fastboot_okay("");
	target_battery_charging_enable(0, 1);
	udc_stop();

	boot_linux((void*) KERNEL_ADDR, (void*) TAGS_ADDR,
		   (const char*) hdr.cmdline, board_machtype(),
		   (void*) RAMDISK_ADDR, hdr.ramdisk_size);
}

void cmd_erase(const char *arg, void *data, unsigned sz)
{
	set_usb_status_listener(OFF);
	struct ptentry *ptn;
	struct ptable *ptable;

	ptable = flash_get_ptable();
	if (ptable == NULL)
	{
		fastboot_fail("partition table doesn't exist");
		return;
	}
	ptn = ptable_find(ptable, arg);
	if (ptn == NULL)
	{
		fastboot_fail("unknown partition name");
		return;
	}
	if (flash_erase(ptn))
	{
		fastboot_fail("failed to erase partition");
		return;
	}

	selector_enable();
	set_usb_status_listener(ON);
	fastboot_okay("");
}

void cmd_flash(const char *arg, void *data, unsigned sz)
{
	redraw_menu();
	set_usb_status_listener(OFF);

	struct ptentry *ptn;
	struct ptable *ptable;
	unsigned extra = 0;

	ptable = flash_get_ptable();
	if (ptable == NULL)
	{
		fastboot_fail("partition table doesn't exist");
		return;
	}
	ptn = ptable_find(ptable, arg);
	if (ptn == NULL) 
	{
		fastboot_fail("unknown partition name");
		return;
	}
	if (!strcmp(ptn->name, "boot") || !strcmp(ptn->name, "recovery") || !strcmp(ptn->name, "sboot") || !strcmp(ptn->name, "tboot")) 
	{
		if (memcmp((void *)data, BOOT_MAGIC, BOOT_MAGIC_SIZE))
		{
			fastboot_fail("image is not a boot image");
			return;
		}
	}
	if (!strcmp(ptn->name, "system") || !strcmp(ptn->name, "userdata"))
	{
		extra = ((page_size >> 9) * 16);
	}
	else
	{
		sz = ROUND_TO_PAGE(sz, page_mask);
	}

	printf( "   writing %d bytes to '%s'...\n", sz, ptn->name);
	if (flash_write(ptn, extra, data, sz))
	{
		fastboot_fail("flash write failure");
		return;
	}
	printf( "\n   partition '%s' updated", ptn->name);

	selector_enable();
	set_usb_status_listener(ON);
	fastboot_okay("");
}

void cmd_continue(const char *arg, void *data, unsigned sz)
{
	fastboot_okay("");
	target_battery_charging_enable(0, 1);
	udc_stop();
	boot_linux_from_flash();
}

void cmd_reboot(const char *arg, void *data, unsigned sz)
{
	redraw_menu();

	fastboot_okay("");
	reboot_device(0);
}

void cmd_reboot_bootloader(const char *arg, void *data, unsigned sz)
{
	redraw_menu();

	fastboot_okay("");
	reboot_device(FASTBOOT_MODE);
}

void cmd_oem_rec_boot(void)
{
	fbcon_resetdisp();

	fastboot_okay("");
	target_battery_charging_enable(0, 1);
	udc_stop();
        boot_into_sboot = 0;
        boot_into_tboot = 0;
        boot_into_recovery = 1;
        boot_linux_from_flash();
}

void cmd_powerdown(const char *arg, void *data, unsigned sz)
{
	redraw_menu();

	fastboot_okay("");
	shutdown();
	thread_exit(0);
}

void send_mem(char* start, int len)
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

void cmd_oem_smesg()
{
	redraw_menu();

	send_mem((char*)0x1fe00018, MIN(0x200000, readl(0x1fe00000)));

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_dmesg()
{
	redraw_menu();

	if(*((unsigned*)0x2FFC0000) == 0x43474244 /* DBGC */  ) //see ram_console_buffer in kernel ram_console.c
	{
		send_mem((char*)0x2FFC000C, *((unsigned*)0x2FFC0008));
	}

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_dumpmem(const char *arg)
{
	redraw_menu();

	char *sStart = strtok((char*)arg, " ");
	char *sLen = strtok(NULL, " ");
	if(sStart==NULL || sLen==NULL)
	{
		fastboot_fail("usage:oem dump start len");
		return;
	}

	send_mem((char*)str2u(sStart), str2u(sLen));

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_set(const char *arg)
{
	redraw_menu();
	set_usb_status_listener(OFF);

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

	selector_enable();
	set_usb_status_listener(ON);
	fastboot_okay("");
}

void cmd_oem_cls()
{
	redraw_menu();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_add(const char *arg)
{
	redraw_menu();
	
	device_add(arg);
	device_list();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_resize(const char *arg)
{
	redraw_menu();
	
	device_resize(arg);
	device_list();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_del(const char *arg)
{
	redraw_menu();
	
	device_del(arg);
	device_list();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_commit()
{
	device_commit();

	redraw_menu();

	printf("\n   Partition changes saved! Device will reboot in 2s.\n");
	fastboot_okay("");
	thread_sleep(2000);
	reboot_device(FASTBOOT_MODE);
}

void cmd_oem_part_read()
{
	redraw_menu();
	
	device_read();
	device_list();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_list()
{
	redraw_menu();

	device_list();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_clear()
{
	redraw_menu();
	
	device_clear();
	device_list();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_create_default()
{
	redraw_menu();
	
	device_clear();
	device_create_default();
	device_list();

	selector_enable();
	fastboot_okay("");
}

void prnt_nand_stat(void)
{
	struct ptable *ptable;
	struct ptentry* ptn;

	if ( flash_info == NULL )
	{
		printf( "   ERROR: flash info unavailable!!!\n" );
		return;
	}
	ptable = flash_get_devinfo();
	if ( ptable == NULL )
	{
		printf( "   ERROR: DEVINFO not found!!!\n" );
		return;
	}
	ptn = ptable_find( ptable, PTN_DEV_INF );
	if ( ptn == NULL )
	{
		printf( "   ERROR: DEVINFO not found!!!\n" );
		return;
	}

	printf("    ____________________________________________________ \n\
		   |                      NAND INFO                     |\n\
		   |____________________________________________________|\n" );
	printf("   | ID: 0x%x MAKER: 0x%2x   DEV: 0x%2x TYPE: %dbit |\n",
		flash_info->id, flash_info->vendor, flash_info->device, (flash_info->type)*8);
	//printf("   | MAC ADDR.: %s IMEI: %s |\n", device_mac_addr, device_imei);
	printf("   |                              IMEI: %s |\n", device_imei);
	printf("   |====================================================|\n");
	printf("   | Flash   block   size: %22i bytes |\n", flash_info->block_size);
	printf("   | Flash   page    size: %22i bytes |\n", flash_info->page_size);
	printf("   | Flash   spare   size: %22i bytes |\n", flash_info->spare_size);
	printf("   | Flash   total   size: %4i block(s)  = %5i MB    |\n",
		flash_info->num_blocks, (int)(flash_info->num_blocks/get_blk_per_mb()));
	printf("   |====================================================|\n");
	printf("   | ROM     (0x%x) size: %4i block(s)  = %5i MB    |\n",
		HTCLEO_ROM_OFFSET, get_usable_flash_size(), (int)(get_usable_flash_size()/get_blk_per_mb()));
	printf("   | DEVINFO (0x%x) size: %4i block(s)  = %5i KB    |\n",
		ptn->start, ptn->length, (flash_info->block_size/1024));
	printf("   | PTABLE  (0x%x) size: %4i block(s)  = %5i MB    |\n",
		get_flash_offset(), get_full_flash_size(), (int)(get_full_flash_size()/get_blk_per_mb()));
	printf("   | ExtROM  (0x%x) size: %4i block(s)  = %5i MB    |\n",
		get_ext_rom_offset(), get_ext_rom_size(), (int)((get_ext_rom_size()/get_blk_per_mb())+1));
	printf("   |____________________________________________________|\n");
}

void cmd_oem_nand_status(void)
{
	redraw_menu();

	prnt_nand_stat();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_format_all()
{
	redraw_menu();
	set_usb_status_listener(OFF);

	struct ptentry *ptn;
	struct ptable *ptable;
	
	printf("   Initializing flash format...\n");
	ptable = flash_get_devinfo();
	if (ptable == NULL) 
	{
		printf( "   ERROR: DEVINFO not found!!!\n");
		return;
	}
	ptn = ptable_find(ptable, "task29");
	if (ptn == NULL) 
	{
		printf( "   ERROR: DEVINFO not found!!!\n");
		return;
	}

	printf("   Formating flash...\n");	
	flash_erase(ptn);	
	printf("\n   Format complete !\n   Reboot device to create default partition table,\n   or create partitions manualy!\n");
	
	selector_enable();
	set_usb_status_listener(ON);
	fastboot_okay("");
}

void cmd_oem_part_format_vpart()
{	
	redraw_menu();
	set_usb_status_listener(OFF);

	struct ptentry *ptn;
	struct ptable *ptable;
	
	printf("   Initializing flash format...\n");
	ptable = flash_get_devinfo();
	if (ptable == NULL)
	{
		printf( "   ERROR: devinfo not found!!!\n");
		return;
	}
	ptn = ptable_find(ptable, "devinf");
	if (ptn == NULL)
	{
		printf( "   ERROR: No devinfo partition!!!\n");
		return;
	}

	printf("   Formating devinfo...\n");
	flash_erase(ptn);
	printf("\n   Format complete !\n   Reboot device to create default partition table,\n   or create partitions manually!\n");

	selector_enable();
	set_usb_status_listener(ON);
	fastboot_okay("");

	return;
}

static int flashlight(void *arg)
{
	volatile unsigned *bank6_in = (unsigned int*)(0xA9000864);
	volatile unsigned *bank6_out = (unsigned int*)(0xA9000814);
	for(;;)
	{
		*bank6_out = *bank6_in ^ 0x200000;
		udelay(496);
		if(keys_get_state_n(0x123)!=0){
			thread_resume(thread_create("detect_usb_cable_status", &usb_status_listener, NULL, DEFAULT_PRIORITY, DEFAULT_STACK_SIZE));
			break;
		}
	}
	thread_exit(0);
}

void cmd_flashlight(void)
{
	thread_resume(thread_create("flashlight", &flashlight, NULL, HIGH_PRIORITY, DEFAULT_STACK_SIZE));
	return;
}

/*
 * koko : Cosmetic changes to the HELP text since we use bigger fonts
 */
void oem_help()
{
	printf("   fastboot oem help\n");
	printf("   -This simple help\n");
	printf("   fastboot oem cls\n");
	printf("   -Clear screen\n");
	printf("   fastboot oem part-add name:size\n");
	printf("   -Create new partition with given name and size\n   -(size in Mb, set 0 for auto-size)\n");
	printf("   -example: fastboot oem part-add system:160\n");
	printf("   fastboot oem part-resize name:size\n");
	printf("   -Resize partition with given name to new size\n   -(size in Mb, set 0 for auto-size)\n");
	printf("   -example: fastboot oem part-resize system:150\n");
	printf("   fastboot oem part-del name\n");
	printf("   -Delete named partition\n");
	printf("   fastboot oem part-create-default\n");
	printf("   -Delete current partition table and create default one\n");
	printf("   fastboot oem part-read\n");
	printf("   -Reload partition layout from NAND\n");
	printf("   fastboot oem part-commit\n");
	printf("   -Save current layout to NAND. All changes to partition\n   -layout are tmp until committed to nand!\n");
	printf("   fastboot oem part-list\n");
	printf("   -Display current partition layout\n");
	printf("   fastboot oem part-clear\n");
	printf("   -Clear current partition layout\n");
	printf("   fastboot oem format-all\n");
	printf("   -WARNING ! THIS IS EQUIVALENT TO TASK29 !\n");
	printf("   -THIS WILL FORMAT COMPLETE NAND (except clk)\n   -It will also display BAD SECTORS (if any)");
}
void cmd_oem_help()
{
	active_menu = &help_menu;
	redraw_menu();
	
	fastboot_okay("");
	oem_help();
}

/*
 * koko : Removed flashlight and test from cmds - added boot recovery
 */
void cmd_oem(const char *arg, void *data, unsigned sz)
{
	while(*arg==' ') arg++;
	if(memcmp(arg, "cls", 3)==0)                           cmd_oem_cls();
	if(memcmp(arg, "set", 3)==0)                           cmd_oem_set(arg+3);
	if(memcmp(arg, "set", 3)==0)                           cmd_oem_set(arg+3);
	if(memcmp(arg, "pwf ", 4)==0)                          cmd_oem_dumpmem(arg+4);
	if(memcmp(arg, "boot-recovery", 13)==0)                cmd_oem_rec_boot();
	if(memcmp(arg, "dmesg", 5)==0)                         cmd_oem_dmesg();
	if(memcmp(arg, "smesg", 5)==0)                         cmd_oem_smesg();
	if(memcmp(arg, "nandstat", 8)==0)                      cmd_oem_nand_status();
	if(memcmp(arg, "poweroff", 8)==0)                      cmd_powerdown(arg+8, data, sz);
	if(memcmp(arg, "part-add ", 9)==0)                     cmd_oem_part_add(arg+9);
	if(memcmp(arg, "part-del ", 9)==0)                     cmd_oem_part_del(arg+9);
	if(memcmp(arg, "part-read", 9)==0)                     cmd_oem_part_read();
	if(memcmp(arg, "part-list", 9)==0)                     cmd_oem_part_list();
	if(memcmp(arg, "part-clear", 10)==0)                   cmd_oem_part_clear();
	if(memcmp(arg, "format-all", 10)==0)                   cmd_oem_part_format_all();
	if(memcmp(arg, "part-commit", 11)==0)                  cmd_oem_part_commit();
	if(memcmp(arg, "part-resize ", 12)==0)                 cmd_oem_part_resize(arg+12);
	if(memcmp(arg, "format-vpart", 12)==0)                 cmd_oem_part_format_vpart();
	if(memcmp(arg, "part-create-default", 19)==0)          cmd_oem_part_create_default();
	if((memcmp(arg,"help",4)==0)||(memcmp(arg,"?",1)==0))  cmd_oem_help();
}

void target_init_fboot(void)
{
	// Initiate Fastboot.
	udc_init(&surf_udc_device);
	fastboot_register("oem", cmd_oem);
	fastboot_register("boot", cmd_boot);
	fastboot_register("flash:", cmd_flash);
	fastboot_register("erase:", cmd_erase);
	fastboot_register("continue", cmd_continue);
	fastboot_register("reboot", cmd_reboot);
	fastboot_register("reboot-bootloader", cmd_reboot_bootloader);
	fastboot_register("powerdown", cmd_powerdown);
	fastboot_publish("version", "1.0");
	fastboot_publish("version-bootloader", "1.0");
	fastboot_publish("version-baseband", "unset");
	fastboot_publish("serialno", "HTC HD2");
	fastboot_publish("secure", "no");
	fastboot_publish("product", TARGET(BOARD));
	fastboot_publish("kernel", "lk");
	fastboot_publish("author", "Shantanu Gupta, Danijel Posilovic, Arif Ali, Cedesmith, Qualcomm Innovation Centre, Travis Geiselbrecht.");
	fastboot_init(target_get_scratch_address(),MEMBASE - SCRATCH_ADDR - 0x00100000);
	udc_start();
	target_battery_charging_enable(1, 0);
}

static int bbtbl(void *arg)
{
	_bad_blocks = bad_block_table(ptable_find(flash_get_devinfo(), "task29"));
	thread_exit(0);
	return 0;
}

bool run_asize=0;
static int boot_info(void *arg)
{
	redraw_menu();
	device_list();

	run_asize=1;
	thread_exit(0);
	return 0;
}

bool run_usbcheck=0;
static int asize_parts(void *arg)
{	// if the process has already been run once before, DON'T run it again
	if(!device_info.size_fixed)
	{
		if( _bad_blocks > 0 )	// Bad blocks detected
		{
			// If there is no small partition with bad blocks, no need to start the whole process
			if(small_part_with_bad_blocks_exists())
			{
        			redraw_menu();
				printf("\n   Initializing auto-resizing process...\n   This process will run ONLY this SINGLE time !");
				thread_sleep(4000);
			      	fbcon_resetdisp();
				if(inverted){active_menu=NULL;draw_clk_header();printf(" \n\n");}
				printf("\n\nIncreasing size of partition(s)...\n\n");
				for ( int i = 0; i < block_tbl.count; i++ )
				{
					// Resize the small partitions (recovery, misc, boot, sboot, cache) that have bad block(s)
					// Add 1MB or 8 blocks per 1 bad block
	      				if( (device_partition_size(block_tbl.blocks[i].partition)==40) || (device_partition_size(block_tbl.blocks[i].partition)==8) )
	            			{
						unsigned addendum=8*num_of_bad_blocks_in_part(block_tbl.blocks[i].partition);
						printf("%s RESIZED: %iMB->%2iMB due to %i included bad block(s)\n",
							block_tbl.blocks[i].partition,
							device_info.partition[device_partition_order(block_tbl.blocks[i].partition)-1].size/8,
							(device_info.partition[device_partition_order(block_tbl.blocks[i].partition)-1].size + addendum)/8,
							num_of_bad_blocks_in_part(block_tbl.blocks[i].partition));
                  				device_resize_ex( block_tbl.blocks[i].partition, ((device_info.partition[device_partition_order(block_tbl.blocks[i].partition)-1].size) + addendum)/8, false );
	      					if(!device_variable_exist())
	      					{
	      						device_info.partition[device_partition_order("userdata")-1].size -= addendum;
	      					}
	      				}
	      				// 'userdata' and 'system' partitions are usually big enough - so don't auto-resize them even if they have bad blocks
				}
                  		
				device_info.size_fixed = 1;
      	      			device_resize_asize();
      	      			device_commit();

				// Auto reboot to load new PTABLE layout
      		      		printf("\nReturning to MAIN MENU in a few seconds...");
				ptable_re_init();
				thread_sleep(5000);
				active_menu = &main_menu;
				redraw_menu();
				selector_enable();
			}
		}
	}
	run_usbcheck=1;
	thread_exit(0);
	return 0;
}

static int update_ver_str(void *arg)
{
	while(!(strlen(spl_version))){update_spl_ver();}

	char expected_byte[] = "3";
	while(radBuffer[0] != expected_byte[0]){update_radio_ver();}

	while(!(strlen(device_imei))){update_device_imei();}

	thread_exit(0);
	return 0;
}

void aboot_init(const struct app_descriptor *app)
{
	if (!device_info.inverted_colors){
		inverted = false;
	}else{
		inverted = true;
	}
	flash_info = flash_get_info();
	page_size = flash_page_size();
	page_mask = page_size - 1;

	/* Check if we should do something other than booting up */
	if (keys_get_state(keys[6]) != 0) //[KEY_HOME] pressed => boot recovery
		boot_into_recovery = 1;
	if (keys_get_state(keys[5]) != 0) //[KEY_BACK] pressed => clk menu
		goto bmenu;
	if (keys_get_state(keys[2]) != 0) //[KEY_SOFT1] pressed => boot sboot
        	boot_into_sboot = 1;

 	if (check_reboot_mode() == RECOVERY_MODE)
 		boot_into_recovery = 1;
 	else if(check_reboot_mode() == FASTBOOT_MODE)
		goto bmenu;
	//else if(check_reboot_mode() == SBOOT_MODE)
	//	boot_into_sboot = 1;
	
	if (boot_into_recovery == 1)
		recovery_init();

	/* Environment setup for continuing, Lets boot if we can */
 	boot_linux_from_flash();

	/* Couldn't Find anything to do (OR) User pressed Back Key. Load Menu */
bmenu:
	thread_resume(thread_create("fill_bad_block_table", &bbtbl, NULL, HIGHEST_PRIORITY, DEFAULT_STACK_SIZE));
	thread_resume(thread_create("update_nand_info", &update_ver_str, NULL, HIGH_PRIORITY, DEFAULT_STACK_SIZE));
	display_init();
	target_init_fboot();
	usb_charger_change_state();
	init_menu();
	if(device_info.show_startup_info){
	thread_resume(thread_create("show_boot_info", &boot_info, NULL, DEFAULT_PRIORITY, DEFAULT_STACK_SIZE));
	}else{
	run_asize=1;
	}
	while(!run_asize){thread_sleep(10);}
	thread_resume(thread_create("auto_size_partitions", &asize_parts, NULL, LOW_PRIORITY, DEFAULT_STACK_SIZE));
	while(!run_usbcheck){thread_sleep(10);}
	thread_resume(thread_create("start_usbstatuslistener", &usb_status_listener, NULL, DEFAULT_PRIORITY, DEFAULT_STACK_SIZE));
	selector_enable();
	thread_resume(thread_create("start_keylistener", &key_listener, 0, DEFAULT_PRIORITY, 4096));
}

APP_START(aboot)
	.init = aboot_init,
APP_END
