/*
 * Copyright (c) 2009 Travis Geiselbrecht
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <debug.h>
#include <list.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>
#include <lib/fs.h>

#define USE_FILELIB_STDIO_COMPAT_NAMES 1

#include <fat_filelib.h>
#if WITH_DEV_SD
#include <sd.h>
#endif

int fs_read(uint32 sector, uint8 *buffer, uint32 sector_count)
{
	// IO device read
#if WITH_DEV_SD
	sd_card->read(sd_card, sector, sector_count*512, buffer);
#endif
	return 1;
}

int fs_write(uint32 sector, uint8 *buffer, uint32 sector_count)
{
	// IO device write
#if WITH_DEV_SD
	sd_card->write(sd_card, sector, sector_count*512, buffer);
#endif
	return 1;
}

void fs_init(void)
{
	// Initialize media
#if WITH_DEV_SD
	sd_init();
#endif
	// Initialize FAT IO library.
	fl_init();

	// Attach the media IO functions to the File IO library
	if (fl_attach_media(fs_read, fs_write) != FAT_INIT_OK)
        {
                printf("\n   ERROR: Media attach failed!\n");
                return; 
        }

	// Print list of dirs @ root of sdcard - test purpose
	fl_listdirectory("/");
}

void fs_stop(void)
{
	// Shutdown the FAT IO library
	fl_shutdown();
}

ssize_t fs_load_file(const char *path, void *ptr)
{
	FL_FILE *file;
	int err = 0; long file_length = 0;
	const char mode[] = {'r'};

	// Open file if it exists
	file = fopen(path, mode);
	if (file == NULL){
		printf("\n   ERROR: Open file failed!");
		return 1;
	}else{
		printf("\n   %s opened successfully!", path);
	}

	// Calc file length
	fseek(file, 0, SEEK_END);
	file_length = ftell(file);
	printf("\n   File length : %u bytes", file_length);
	fseek(file, 0, SEEK_SET);

	// Read the file and close after
	ptr = malloc(sizeof(char) * file_length);
	err = fread(ptr, sizeof(char), file_length, file);
	fclose(file);

	return err;
}
