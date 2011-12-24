/*
 * Copyright (c) 2011, Shantanu/zeusk (shans95g@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
 
#if ((!(defined(DEV_WINCE))) || (DEV_WINCE))
	#error "This module is made for devices where native bootloader does not support linux-android bootstrapping."
#endif

#include <arch/ops.h>
#include <arch/arm.h>

#include <debug.h>

#include <dev/flash.h>
#include <dev/keys.h>
#include <dev/udc.h>

#include <kernel/thread.h>

#include <lib/dev_info.h>
#include <lib/ptable.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char buffer[4096]; /* Maximum supported flash_blck_sz */

int read_dev_inf(struct device_inf *out)
{	
	struct ptable* ptable = flash_get_init_ptable();
	if ( ptable == NULL ) 
	{
		dprintf( CRITICAL, "ERROR: init_ptable pointer returned NULL.\n" );
		return 1;
	}

	struct ptentry* ptn = ptable_find( ptable, PTN_DEV_INF );
	if ( ptn == NULL )
	{
		dprintf( CRITICAL, "ERROR: init_ptable not found.\n" );
		return 2;
	}

	if ( flash_read( ptn, 0, buffer, sizeof( *out ) ) )
	{
		dprintf( CRITICAL, "ERROR: init_ptable could not be read.\n");
		return 3;
	}

	memcpy( out, buffer, sizeof( *out ) );
	return 0;
}
int write_dev_inf(const struct device_inf *in)
{
	struct ptable* ptable = flash_get_init_ptable();
	if ( ptable == NULL ) 
	{
		dprintf( CRITICAL, "ERROR: init_ptable pointer returned NULL.\n" );
		return 1;
	}

	struct ptentry* ptn = ptable_find( ptable, PTN_DEV_INF );
	if ( ptn == NULL )
	{
		dprintf( CRITICAL, "ERROR: init_ptable not found.\n" );
		return 2;
	}

	unsigned pagesize = flash_page_size();
	if ( pagesize == 0 )
	{
		printf( CRITICAL, "ERROR: flash driver returned invalid page_sz.\n" );
		return 3;
	}

	if ( sizeof( *in ) > pagesize )
	{
		printf( CRITICAL, "ERROR: device_inf cannot be aligned.\n" );
		return 4;
	}

	memset( (void*) SCRATCH_ADDR, 0, pagesize );
	memcpy( (void*) SCRATCH_ADDR, (void*) in, sizeof( *in ) );

	if ( flash_write( ptn, 0, (void*) SCRATCH_ADDR, pagesize ) )
	{
		printf( CRITICAL, "ERROR: device_inf could not be written.\n" );
		return 5;
	}

	return 0;
}

