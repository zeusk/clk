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
#include <lib/fs/fat.h>

long fs_read(const char *filename, void *buffer, unsigned long maxsize)
{
#ifdef HTCLEO_SUPPORT_VFAT
	return file_fat_read(filename, buffer, maxsize);
#endif
	return 0;
}

int fs_write(const char *filename, void *buffer, unsigned long maxsize)
{
#ifdef HTCLEO_SUPPORT_VFAT
	return file_fat_write(filename, buffer, maxsize);
#endif
	return 0;
}

void fs_init(void)
{
#ifdef HTCLEO_SUPPORT_VFAT
	fat_register_device(&mmc_dev, 1);
	file_fat_detectfs();
#endif
}
