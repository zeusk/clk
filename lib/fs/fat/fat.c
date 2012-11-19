/*
 * fat.c
 *
 * R/O (V)FAT 12/16/32 filesystem implementation by Marcus Sundberg
 *
 * 2002-07-28 - rjones@nexus-tech.net - ported to ppcboot v1.1.6
 * 2003-03-10 - kharris@nexus-tech.net - ported to uboot
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <target/board_htcleo.h>
#include <lib/fs/fat.h>
#include <debug.h>
#include <compiler.h>
#include <string.h>
#include <stddef.h>
#include <malloc.h>
#include <endian.h>

/*
 * Convert a string to lowercase.
 */
static
void downcase(char *str)
{
	while (*str != '\0') {
		TOLOWER(*str);
		str++;
	}
}

/*
 * Convert a string to uppercase.
 */
static
void uppercase(char *str, int len)
{
	for (int i = 0; i < len; i++) {
		TOUPPER(*str);
		str++;
	}
}

static block_dev_desc_t *cur_dev = NULL;
static unsigned long part_offset = 0;
static int cur_part = 1;
static uint32_t total_sector;

#define DOS_PART_TBL_OFFSET		0x1be
#define DOS_PART_MAGIC_OFFSET	0x1fe
#define DOS_FS_TYPE_OFFSET		0x36

int disk_read(uint32_t startblock, uint32_t getsize, uint8_t * bufptr)
{
	startblock += part_offset;
	
	if (cur_dev == NULL)
		return -1;
		
	if (cur_dev->block_read)
		return cur_dev->block_read(cur_dev->dev, startblock, getsize, (unsigned long *)bufptr);

	return -1;
}

int disk_write(uint32_t startblock, uint32_t getsize, uint8_t *bufptr)
{
	if (cur_dev == NULL)
		return -1;

	if (startblock + getsize > total_sector)
		return -1;

	startblock += part_offset;

	if (cur_dev->block_read)
		return cur_dev->block_write(cur_dev->dev, startblock, getsize, (unsigned long *) bufptr);

	return -1;
}

int fat_register_device(block_dev_desc_t *dev_desc, int part_no)
{
	unsigned char buffer[SECTOR_SIZE] __UNUSED;
	disk_partition_t info __UNUSED;

	if (!dev_desc->block_read)
		return -1;
		
	cur_dev = dev_desc;
	
	/* check if we have a MBR (on floppies we have only a PBR) */
	if (dev_desc->block_read (dev_desc->dev, 0, 1, (ulong *) buffer) != 1) {
		printf ("** Can't read from device %d **\n", dev_desc->dev);
		return -1;
	}
	if (buffer[DOS_PART_MAGIC_OFFSET] != 0x55 || buffer[DOS_PART_MAGIC_OFFSET + 1] != 0xaa) {
		/* no signature found */
		return -1;
	}
#if (defined(CONFIG_CMD_IDE) || \
     defined(CONFIG_CMD_MG_DISK) || \
     defined(CONFIG_CMD_SATA) || \
     defined(CONFIG_CMD_SCSI) || \
     defined(CONFIG_CMD_USB) || \
     defined(CONFIG_MMC) || \
     defined(CONFIG_SYSTEMACE) )
	/* First we assume, there is a MBR */
	if (!get_partition_info (dev_desc, part_no, &info)) {
		part_offset = info.start;
		cur_part = part_no;
	} else if (!strncmp((char *)&buffer[DOS_FS_TYPE_OFFSET], "FAT", 3)) {
		/* ok, we assume we are on a PBR only */
		cur_part = 1;
		part_offset = 0;
	} else {
		printf ("** Partition %d not valid on device %d **\n", part_no, dev_desc->dev);
		return -1;
	}
#else
	if (!strncmp((char *)&buffer[DOS_FS_TYPE_OFFSET],"FAT",3)) {
		/* ok, we assume we are on a PBR only */
		cur_part = 1;
		part_offset = 0;
		info.start = part_offset;
	} else {
		/* FIXME we need to determine the start block of the
		 * partition where the DOS FS resides. This can be done
		 * by using the get_partition_info routine. For this
		 * purpose the libpart must be included.
		 */
		part_offset = 32;
		cur_part = 1;
	}
#endif
	return 0;
}

/*
 * Get the first occurence of a directory delimiter ('/' or '\') in a string.
 * Return index into string if found, -1 otherwise.
 */
static
int dirdelim(char *str)
{
	char *start = str;

	while (*str != '\0') {
		if (ISDIRDELIM(*str))
			return str - start;
			
		str++;
	}
	return -1;
}

/*
 * Extract zero terminated short name from a directory entry.
 */
static
void get_name(dir_entry *dirent, char *s_name)
{
	char *ptr;

	memcpy (s_name, dirent->name, 8);
	s_name[8] = '\0';
	ptr = s_name;
	
	while (*ptr && *ptr != ' ')
		ptr++;
		
	if (dirent->ext[0] && dirent->ext[0] != ' ') {
		*ptr = '.';
		ptr++;
		memcpy (ptr, dirent->ext, 3);
		ptr[3] = '\0';
		while (*ptr && *ptr != ' ')
			ptr++;
	}
	
	*ptr = '\0';
	
	if (*s_name == DELETED_FLAG)
		*s_name = '\0';
	else if (*s_name == aRING)
		*s_name = DELETED_FLAG;
		
	downcase (s_name);
}

/*
 * Get the entry at index 'entry' in a FAT (12/16/32) table.
 * On failure 0x00 is returned.
 */
static
uint32_t get_fatent(fsdata *mydata, uint32_t entry)
{
	uint32_t bufnum;
	uint32_t offset;
	uint32_t ret = 0x00;

	switch (mydata->fatsize) {
	case 32:
		bufnum = entry / FAT32BUFSIZE;
		offset = entry - bufnum * FAT32BUFSIZE;
		break;
	case 16:
		bufnum = entry / FAT16BUFSIZE;
		offset = entry - bufnum * FAT16BUFSIZE;
		break;
	case 12:
		bufnum = entry / FAT12BUFSIZE;
		offset = entry - bufnum * FAT12BUFSIZE;
		break;

	default:
		/* Unsupported FAT size */
		return ret;
	}

	/* Read a new block of FAT entries into the cache. */
	if (bufnum != (unsigned)mydata->fatbufnum) {
		unsigned getsize = FATBUFSIZE/FS_BLOCK_SIZE;
		uint8_t *bufptr = mydata->fatbuf;
		uint32_t fatlength = mydata->fatlength;
		uint32_t startblock = bufnum * FATBUFBLOCKS;

		fatlength *= SECTOR_SIZE;	/* We want it in bytes now */
		startblock += mydata->fat_sect;	/* Offset from start of disk */

		if (getsize > fatlength) getsize = fatlength;
		if (disk_read(startblock, getsize, bufptr) < 0) {
			FAT_DPRINT("Error reading FAT blocks\n");
			return ret;
		}
		mydata->fatbufnum = bufnum;
	}

	/* Get the actual entry from the table */
	switch (mydata->fatsize) {
	case 32:
		ret = FAT2CPU32(((uint32_t*)mydata->fatbuf)[offset]);
		break;
	case 16:
		ret = FAT2CPU16(((uint16_t*)mydata->fatbuf)[offset]);
		break;
	case 12: {
		uint32_t off16 = (offset*3)/4;
		uint16_t val1, val2;

		switch (offset & 0x3) {
		case 0:
			ret = FAT2CPU16(((uint16_t*)mydata->fatbuf)[off16]);
			ret &= 0xfff;
			break;
		case 1:
			val1 = FAT2CPU16(((uint16_t*)mydata->fatbuf)[off16]);
			val1 &= 0xf000;
			val2 = FAT2CPU16(((uint16_t*)mydata->fatbuf)[off16+1]);
			val2 &= 0x00ff;
			ret = (val2 << 4) | (val1 >> 12);
			break;
		case 2:
			val1 = FAT2CPU16(((uint16_t*)mydata->fatbuf)[off16]);
			val1 &= 0xff00;
			val2 = FAT2CPU16(((uint16_t*)mydata->fatbuf)[off16+1]);
			val2 &= 0x000f;
			ret = (val2 << 8) | (val1 >> 8);
			break;
		case 3:
			ret = FAT2CPU16(((uint16_t*)mydata->fatbuf)[off16]);;
			ret = (ret & 0xfff0) >> 4;
			break;
		default:
			break;
		}
	}
	break;
	}
	FAT_DPRINT("ret: %d, offset: %d\n", ret, offset);

	return ret;
}

/*
 * Read at most 'size' bytes from the specified cluster into 'buffer'.
 * Return 0 on success, -1 otherwise.
 */
static
int get_cluster(fsdata *mydata, uint32_t clustnum, uint8_t *buffer, unsigned long size)
{
	int idx = 0;
	uint32_t startsect;

	if (clustnum > 0) {
		startsect = mydata->data_begin + clustnum*mydata->clust_size;
	} else {
		startsect = mydata->rootdir_sect;
	}

	FAT_DPRINT("gc - clustnum: %d, startsect: %d\n", clustnum, startsect);
	if (disk_read(startsect, size/FS_BLOCK_SIZE , buffer) < 0) {
		FAT_DPRINT("Error reading data\n");
		return -1;
	}
	if(size % FS_BLOCK_SIZE) {
		uint8_t tmpbuf[FS_BLOCK_SIZE];
		idx= size/FS_BLOCK_SIZE;
		if (disk_read(startsect + idx, 1, tmpbuf) < 0) {
			FAT_DPRINT("Error reading data\n");
			return -1;
		}
		buffer += idx*FS_BLOCK_SIZE;

		memcpy(buffer, tmpbuf, size % FS_BLOCK_SIZE);
		return 0;
	}

	return 0;
}

/*
 * Read at most 'maxsize' bytes from the file associated with 'dentptr'
 * into 'buffer'.
 * Return the number of bytes read or -1 on fatal errors.
 */
static
long get_contents(fsdata *mydata, dir_entry *dentptr, uint8_t *buffer, unsigned long maxsize)
{
	unsigned long filesize = FAT2CPU32(dentptr->size), gotsize = 0;
	unsigned int bytesperclust = mydata->clust_size * SECTOR_SIZE;
	uint32_t curclust = START(dentptr);
	uint32_t endclust, newclust;
	unsigned long actsize;

	FAT_DPRINT("Filesize: %ld bytes\n", filesize);

	if (maxsize > 0 && filesize > maxsize) filesize = maxsize;

	FAT_DPRINT("Reading: %ld bytes\n", filesize);

	actsize=bytesperclust;
	endclust=curclust;
	do {
		/* search for consecutive clusters */
		while(actsize < filesize) {
			newclust = get_fatent(mydata, endclust);
			if((newclust -1)!=endclust)
				goto getit;
			if (CHECK_CLUST(newclust, mydata->fatsize)) {
				FAT_DPRINT("curclust: 0x%x\n", newclust);
				FAT_DPRINT("Invalid FAT entry\n");
				return gotsize;
			}
			endclust=newclust;
			actsize+= bytesperclust;
		}
		/* actsize >= file size */
		actsize -= bytesperclust;
		/* get remaining clusters */
		if (get_cluster(mydata, curclust, buffer, (int)actsize) != 0) {
			FAT_ERROR("Error reading cluster\n");
			return -1;
		}
		/* get remaining bytes */
		gotsize += (int)actsize;
		filesize -= actsize;
		buffer += actsize;
		actsize= filesize;
		if (get_cluster(mydata, endclust, buffer, (int)actsize) != 0) {
			FAT_ERROR("Error reading cluster\n");
			return -1;
		}
		gotsize+=actsize;
		return gotsize;
		
getit:
		if (get_cluster(mydata, curclust, buffer, (int)actsize) != 0) {
			FAT_ERROR("Error reading cluster\n");
			return -1;
		}
		gotsize += (int)actsize;
		filesize -= actsize;
		buffer += actsize;
		curclust = get_fatent(mydata, endclust);
		if (CHECK_CLUST(curclust, mydata->fatsize)) {
			FAT_DPRINT("curclust: 0x%x\n", curclust);
			FAT_ERROR("Invalid FAT entry\n");
			return gotsize;
		}
		actsize=bytesperclust;
		endclust=curclust;
	} while (1);
}

#ifdef HTCLEO_SUPPORT_VFAT
/*
 * Extract the file name information from 'slotptr' into 'l_name',
 * starting at l_name[*idx].
 * Return 1 if terminator (zero byte) is found, 0 otherwise.
 */
static
int slot2str(dir_slot *slotptr, char *l_name, int *idx)
{
	for (int j = 0; j <= 8; j += 2) {
		l_name[*idx] = slotptr->name0_4[j];
		if (l_name[*idx] == 0x00)
			return 1;
			
		(*idx)++;
	}
	for (int j = 0; j <= 10; j += 2) {
		l_name[*idx] = slotptr->name5_10[j];
		if (l_name[*idx] == 0x00)
			return 1;
			
		(*idx)++;
	}
	for (int j = 0; j <= 2; j += 2) {
		l_name[*idx] = slotptr->name11_12[j];
		if (l_name[*idx] == 0x00)
			return 1;
			
		(*idx)++;
	}

	return 0;
}

/*
 * Extract the full long filename starting at 'retdent' (which is really
 * a slot) into 'l_name'. If successful also copy the real directory entry
 * into 'retdent'
 * Return 0 on success, -1 otherwise.
 */
__attribute__ ((__aligned__(__alignof__(dir_entry))))
uint8_t get_vfatname_block[MAX_CLUSTSIZE];
static
int get_vfatname(fsdata *mydata, int curclust, uint8_t *cluster, dir_entry *retdent, char *l_name)
{
	dir_entry *realdent;
	dir_slot  *slotptr = (dir_slot*)retdent;
	uint8_t	  *nextclust = cluster + mydata->clust_size * SECTOR_SIZE;
	uint8_t	   counter = (slotptr->id & ~LAST_LONG_ENTRY_MASK) & 0xff;
	int idx = 0;

	while ((uint8_t*)slotptr < nextclust) {
		if (counter == 0)
			break;
			
		if (((slotptr->id & ~LAST_LONG_ENTRY_MASK) & 0xff) != counter)
			return -1;
			
		slotptr++;
		counter--;
	}

	if ((uint8_t*)slotptr >= nextclust) {
		dir_slot *slotptr2;
		slotptr--;
		curclust = get_fatent(mydata, curclust);
		if (CHECK_CLUST(curclust, mydata->fatsize)) {
			FAT_DPRINT("curclust: 0x%x\n", curclust);
			FAT_ERROR("Invalid FAT entry\n");
			return -1;
		}
		if (get_cluster(mydata, curclust, get_vfatname_block, mydata->clust_size * SECTOR_SIZE) != 0) {
			FAT_DPRINT("Error: reading directory block\n");
			return -1;
		}
		slotptr2 = (dir_slot*) get_vfatname_block;
		while (slotptr2->id > 0x01) {
			slotptr2++;
		}
		/* Save the real directory entry */
		realdent = (dir_entry*)slotptr2 + 1;
		while ((uint8_t*)slotptr2 >= get_vfatname_block) {
			slot2str(slotptr2, l_name, &idx);
			slotptr2--;
		}
	} else {
		/* Save the real directory entry */
		realdent = (dir_entry*)slotptr;
	}

	do {
		slotptr--;
		if (slot2str(slotptr, l_name, &idx))
			break;
	} while (!(slotptr->id & LAST_LONG_ENTRY_MASK));

	l_name[idx] = '\0';
	if (*l_name == DELETED_FLAG)
		*l_name = '\0';
	else if (*l_name == aRING) *l_name = DELETED_FLAG;
		downcase(l_name);

	/* Return the real directory entry */
	memcpy(retdent, realdent, sizeof(dir_entry));

	return 0;
}

/* Calculate short name checksum */
static
uint8_t mkcksum(const char *str)
{
	int i;
	uint8_t ret = 0;

	for (i = 0; i < 11; i++) {
		ret = (((ret&1)<<7)|((ret&0xfe)>>1)) + str[i];
	}

	return ret;
}
#endif

/*
 * Get the directory entry associated with 'filename' from the directory
 * starting at 'startsect'
 */
__attribute__ ((__aligned__(__alignof__(dir_entry))))
uint8_t get_dentfromdir_block[MAX_CLUSTSIZE];
static
dir_entry *get_dentfromdir(fsdata * mydata, int startsect, char *filename, dir_entry * retdent, int dols)
{
    uint16_t prevcksum = 0xffff;
    uint32_t curclust = START(retdent);
    int files = 0, dirs = 0;

    FAT_DPRINT("get_dentfromdir: %s\n", filename);
    while (1) {
		dir_entry *dentptr;
		unsigned i;

		if (get_cluster (mydata, curclust, get_dentfromdir_block, mydata->clust_size * SECTOR_SIZE) != 0) {
			FAT_DPRINT("Error: reading directory block\n");
			return NULL;
		}
		dentptr = (dir_entry *)get_dentfromdir_block;
		for (i = 0; i < DIRENTSPERCLUST; i++) {
			char s_name[14], l_name[256];

			l_name[0] = '\0';
			if (dentptr->name[0] == DELETED_FLAG) {
				dentptr++;
				continue;
			}
			if ((dentptr->attr & ATTR_VOLUME)) {
#ifdef HTCLEO_SUPPORT_VFAT
				if ((dentptr->attr & ATTR_VFAT) && (dentptr->name[0] & LAST_LONG_ENTRY_MASK)) {
					prevcksum = ((dir_slot *)dentptr)->alias_checksum;
					get_vfatname(mydata, curclust, get_dentfromdir_block, dentptr, l_name);
					if (dols) {
						int isdir = (dentptr->attr & ATTR_DIR);
						char dirc;
						int doit = 0;

						if (isdir) {
							dirs++;
							dirc = '/';
							doit = 1;
						} else {
							dirc = ' ';
							if (l_name[0] != 0) {
								files++;
								doit = 1;
							}
						}
						if (doit) {
							if (dirc == ' ') {
								printf(" %8ld   %s%c\n",(long)FAT2CPU32(dentptr->size), l_name, dirc);
							} else {
								printf("            %s%c\n", l_name, dirc);
							}
						}
						dentptr++;
						continue;
					}
					FAT_DPRINT("vfatname: |%s|\n", l_name);
				} else
#endif
				{
					/* Volume label or VFAT entry */
					dentptr++;
					continue;
				}
			}
			if (dentptr->name[0] == 0) {
				if (dols) {
					printf("\n%d file(s), %d dir(s)\n\n", files, dirs);
				}
				FAT_DPRINT("Dentname == NULL - %d\n", i);
				return NULL;
			}
#ifdef HTCLEO_SUPPORT_VFAT
			if (dols && mkcksum (dentptr->name) == prevcksum) {
				dentptr++;
				continue;
			}
#endif
			get_name(dentptr, s_name);
			if (dols) {
				int isdir = (dentptr->attr & ATTR_DIR);
				char dirc;
				int doit = 0;

				if (isdir) {
					dirs++;
					dirc = '/';
					doit = 1;
				} else {
					dirc = ' ';
					if (s_name[0] != 0) {
						files++;
						doit = 1;
					}
				}
				if (doit) {
					if (dirc == ' ') {
						printf (" %8ld   %s%c\n", (long)FAT2CPU32(dentptr->size), s_name, dirc);
					} else {
						printf ("            %s%c\n", s_name, dirc);
					}
				}
				dentptr++;
				continue;
			}
			if (strcmp(filename, s_name) && strcmp(filename, l_name)) {
				FAT_DPRINT("Mismatch: |%s|%s|\n", s_name, l_name);
				dentptr++;
				continue;
			}
			memcpy(retdent, dentptr, sizeof (dir_entry));

			FAT_DPRINT("DentName: %s", s_name);
			FAT_DPRINT(", start: 0x%x", START(dentptr));
			FAT_DPRINT(", size:  0x%x %s\n", FAT2CPU32(dentptr->size), (dentptr->attr & ATTR_DIR) ? "(DIR)" : "");

			return retdent;
		}
		curclust = get_fatent(mydata, curclust);
		if (CHECK_CLUST(curclust, mydata->fatsize)) {
			FAT_DPRINT("curclust: 0x%x\n", curclust);
			FAT_ERROR("Invalid FAT entry\n");
			return NULL;
		}
    }

    return NULL;
}

/*
 * Read boot sector and volume info from a FAT filesystem
 */
static
int read_bootsectandvi(boot_sector *bs, volume_info *volinfo, int *fatsize)
{
	uint8_t block[FS_BLOCK_SIZE];
	volume_info *vistart;

	if (disk_read(0, 1, block) < 0) {
		FAT_DPRINT("Error: reading block\n");
		return -1;
	}

	memcpy(bs, block, sizeof(boot_sector));
	bs->reserved	= FAT2CPU16(bs->reserved);
	bs->fat_length	= FAT2CPU16(bs->fat_length);
	bs->secs_track	= FAT2CPU16(bs->secs_track);
	bs->heads		= FAT2CPU16(bs->heads);
#if 0 /* UNUSED */
	bs->hidden		= FAT2CPU32(bs->hidden);
#endif
	bs->total_sect	= FAT2CPU32(bs->total_sect);

	/* FAT32 entries */
	if (bs->fat_length == 0) {
		/* Assume FAT32 */
		bs->fat32_length = FAT2CPU32(bs->fat32_length);
		bs->flags	 	 = FAT2CPU16(bs->flags);
		bs->root_cluster = FAT2CPU32(bs->root_cluster);
		bs->info_sector  = FAT2CPU16(bs->info_sector);
		bs->backup_boot  = FAT2CPU16(bs->backup_boot);
		vistart = (volume_info*) (block + sizeof(boot_sector));
		*fatsize = 32;
	} else {
		vistart = (volume_info*) &(bs->fat32_length);
		*fatsize = 0;
	}
	memcpy(volinfo, vistart, sizeof(volume_info));

	if (*fatsize == 32) {
		if (strncmp(FAT32_SIGN, vistart->fs_type, SIGNLEN) == 0) {
			return 0;
		}
	} else {
		if (strncmp(FAT12_SIGN, vistart->fs_type, SIGNLEN) == 0) {
			*fatsize = 12;
			return 0;
		}
		if (strncmp(FAT16_SIGN, vistart->fs_type, SIGNLEN) == 0) {
			*fatsize = 16;
			return 0;
		}
	}

	FAT_DPRINT("Error: broken fs_type sign\n");
	return -1;
}

__attribute__ ((__aligned__(__alignof__(dir_entry))))
uint8_t do_fat_read_block[MAX_CLUSTSIZE];
long
do_fat_read(const char *filename, void *buffer, unsigned long maxsize, int dols)
{
#if CONFIG_NIOS /* NIOS CPU cannot access big automatic arrays */
    static
#endif
    char fnamecopy[2048];
    boot_sector bs;
    volume_info volinfo;
    fsdata datablock;
    fsdata *mydata = &datablock;
    dir_entry *dentptr;
    uint16_t prevcksum = 0xffff;
    char *subname = "";
    int rootdir_size, cursect;
    int idx, isdir = 0;
    int files = 0, dirs = 0;
    long ret = 0;
    int firsttime;

    if (read_bootsectandvi (&bs, &volinfo, &mydata->fatsize)) {
		FAT_DPRINT ("Error: reading boot sector\n");
		return -1;
    }
    if (mydata->fatsize == 32) {
		mydata->fatlength = bs.fat32_length;
    } else {
		mydata->fatlength = bs.fat_length;
    }
    mydata->fat_sect = bs.reserved;
    cursect = mydata->rootdir_sect = mydata->fat_sect + mydata->fatlength * bs.fats;
    mydata->clust_size = bs.cluster_size;
    if (mydata->fatsize == 32) {
		rootdir_size = mydata->clust_size;
		mydata->data_begin = mydata->rootdir_sect - (mydata->clust_size * 2);
    } else {
		rootdir_size = ((bs.dir_entries[1] * (int) 256 + bs.dir_entries[0]) * sizeof (dir_entry)) / SECTOR_SIZE;
		mydata->data_begin = mydata->rootdir_sect + rootdir_size - (mydata->clust_size * 2);
    }
    mydata->fatbufnum = -1;

    FAT_DPRINT("FAT%d, fatlength: %d\n", mydata->fatsize, mydata->fatlength);
    FAT_DPRINT("Rootdir begins at sector: %d, offset: %x, size: %d\n"
				"Data begins at: %d\n",
				mydata->rootdir_sect, mydata->rootdir_sect * SECTOR_SIZE,
				rootdir_size, mydata->data_begin);
    FAT_DPRINT("Cluster size: %d\n", mydata->clust_size);

    /* "cwd" is always the root... */
    while (ISDIRDELIM(*filename))
		filename++;
		
    /* Make a copy of the filename and convert it to lowercase */
    strcpy(fnamecopy, filename);
    downcase(fnamecopy);
	
    if (*fnamecopy == '\0') {
		if (!dols)
			return -1;
			
		dols = LS_ROOT;
    }
	else if ((idx = dirdelim(fnamecopy)) >= 0) {
		isdir = 1;
		fnamecopy[idx] = '\0';
		subname = fnamecopy + idx + 1;
		/* Handle multiple delimiters */
		while (ISDIRDELIM(*subname))
			subname++;
	}
	else if (dols) {
		isdir = 1;
    }

    while (1) {
		if (disk_read(cursect, mydata->clust_size, do_fat_read_block) < 0) {
			FAT_DPRINT("Error: reading rootdir block\n");
			return -1;
		}
		
		dentptr = (dir_entry *) do_fat_read_block;
		
		for (unsigned i = 0; i < DIRENTSPERBLOCK; i++) {
			char s_name[14], l_name[256];
			l_name[0] = '\0';
			if ((dentptr->attr & ATTR_VOLUME)) {
#ifdef HTCLEO_SUPPORT_VFAT
				if ((dentptr->attr & ATTR_VFAT) && (dentptr->name[0] & LAST_LONG_ENTRY_MASK)) {
					prevcksum = ((dir_slot *)dentptr)->alias_checksum;
					get_vfatname(mydata, 0, do_fat_read_block, dentptr, l_name);
					if (dols == LS_ROOT) {
						int isdir = (dentptr->attr & ATTR_DIR);
						char dirc;
						int doit = 0;

						if (isdir) {
							dirs++;
							dirc = '/';
							doit = 1;
						} else {
							dirc = ' ';
							if (l_name[0] != 0) {
								files++;
								doit = 1;
							}
						}
						if (doit) {
							if (dirc == ' ') {
								printf(" %8ld   %s%c\n", (long)FAT2CPU32(dentptr->size), l_name, dirc);
							} else {
								printf("            %s%c\n", l_name, dirc);
							}
						}
						dentptr++;
						continue;
					}
					FAT_DPRINT("Rootvfatname: |%s|\n", l_name);
				} else
#endif
				{
					/* Volume label or VFAT entry */
					dentptr++;
					continue;
				}
			}
			else if (dentptr->name[0] == 0) {
				FAT_DPRINT("RootDentname == NULL - %d\n", i);
				if (dols == LS_ROOT) {
					printf("\n%d file(s), %d dir(s)\n\n", files, dirs);
					return 0;
				}
				return -1;
			}	
#ifdef HTCLEO_SUPPORT_VFAT
			else if (dols == LS_ROOT && mkcksum(dentptr->name) == prevcksum) {
				dentptr++;
				continue;
			}
#endif
			get_name(dentptr, s_name);
			if (dols == LS_ROOT) {
				int isdir = (dentptr->attr & ATTR_DIR);
				char dirc;
				int doit = 0;

				if (isdir) {
					dirc = '/';
					if (s_name[0] != 0) {
						dirs++;
						doit = 1;
					}
				} else {
					dirc = ' ';
					if (s_name[0] != 0) {
						files++;
						doit = 1;
					}
				}
				if (doit) {
					if (dirc == ' ') {
						printf (" %8ld   %s%c\n", (long)FAT2CPU32(dentptr->size), s_name,	dirc);
					} else {
						printf ("            %s%c\n", s_name, dirc);
					}
				}
				dentptr++;
				continue;
			}
			if (strcmp(fnamecopy, s_name) && strcmp(fnamecopy, l_name)) {
				FAT_DPRINT("RootMismatch: |%s|%s|\n", s_name, l_name);
				dentptr++;
				continue;
			}
			if (isdir && !(dentptr->attr & ATTR_DIR))
				return -1;

			FAT_DPRINT("RootName: %s", s_name);
			FAT_DPRINT(", start: 0x%x", START (dentptr));
			FAT_DPRINT(", size:  0x%x %s\n",
			FAT2CPU32(dentptr->size), isdir ? "(DIR)" : "");

			goto rootdir_done;  /* We got a match */
		}
		cursect++;
    }

rootdir_done:
    firsttime = 1;
    while (isdir) {
		int startsect = mydata->data_begin + START(dentptr) * mydata->clust_size;
		dir_entry dent;
		char *nextname = NULL;

		dent = *dentptr;
		dentptr = &dent;

		idx = dirdelim(subname);
		if (idx >= 0) {
			subname[idx] = '\0';
			nextname = subname + idx + 1;
			/* Handle multiple delimiters */
			while (ISDIRDELIM(*nextname))
				nextname++;
				
			if (dols && *nextname == '\0')
				firsttime = 0;
		} else {
			if (dols && firsttime) {
				firsttime = 0;
			} else {
				isdir = 0;
			}
		}

		if (get_dentfromdir(mydata, startsect, subname, dentptr, isdir ? 0 : dols) == NULL) {
			if (dols && !isdir)
				return 0;
				
			return -1;
		}

		if (idx >= 0) {
			if (!(dentptr->attr & ATTR_DIR))
				return -1;
				
	    subname = nextname;
		}
    }
    ret = get_contents(mydata, dentptr, buffer, maxsize);
    FAT_DPRINT("Size: %d, got: %ld\n", FAT2CPU32(dentptr->size), ret);

    return ret;
}

/********************************************************************************************************
 * fat_write.c
 ********************************************************************************************************/
// Set short name in directory entry
static
void set_name(dir_entry *dirent, const char *filename)
{
	char s_name[VFAT_MAXLEN_BYTES];
	char *period;
	int period_location, len, i, ext_num;

	if (filename == NULL)
		return;

	len = strlen(filename);
	if (len == 0)
		return;

	memcpy(s_name, filename, len);
	uppercase(s_name, len);

	period = strchr(s_name, '.');
	if (period == NULL) {
		period_location = len;
		ext_num = 0;
	} else {
		period_location = period - s_name;
		ext_num = len - period_location - 1;
	}

	// Pad spaces when the length of file name is shorter than eight
	if (period_location < 8) {
		memcpy(dirent->name, s_name, period_location);
		for (i = period_location; i < 8; i++)
			dirent->name[i] = ' ';
	} else if (period_location == 8) {
		memcpy(dirent->name, s_name, period_location);
	} else {
		memcpy(dirent->name, s_name, 6);
		dirent->name[6] = '~';
		dirent->name[7] = '1';
	}

	if (ext_num < 3) {
		memcpy(dirent->ext, s_name + period_location + 1, ext_num);
		for (i = ext_num; i < 3; i++)
			dirent->ext[i] = ' ';
	} else
		memcpy(dirent->ext, s_name + period_location + 1, 3);

	printf("name : %s\n", dirent->name);
	printf("ext : %s\n", dirent->ext);
}

// Write fat buffer into block device
static uint8_t num_of_fats;
static
int flush_fat_buffer(fsdata *mydata)
{
	unsigned getsize = FATBUFBLOCKS;
	uint32_t fatlength = mydata->fatlength;
	uint8_t *bufptr = mydata->fatbuf;
	uint32_t startblock = mydata->fatbufnum * FATBUFBLOCKS;

	fatlength *= mydata->sect_size;
	startblock += mydata->fat_sect;

	if (getsize > fatlength)
		getsize = fatlength;

	// Write FAT buf
	if (disk_write(startblock, getsize, bufptr) < 0) {
		printf("error: writing FAT blocks\n");
		return -1;
	}

	if (num_of_fats == 2) {
		// Update corresponding second FAT blocks
		startblock += mydata->fatlength;
		if (disk_write(startblock, getsize, bufptr) < 0) {
			printf("error: writing second FAT blocks\n");
			return -1;
		}
	}

	return 0;
}

// Get the entry at index 'entry' in a FAT (12/16/32) table.
// On failure 0x00 is returned.
// When bufnum is changed, write back the previous fatbuf to the disk.
static
uint32_t get_fatent_value(fsdata *mydata, uint32_t entry)
{
	uint32_t bufnum;
	uint32_t off16, offset;
	uint32_t ret = 0x00;
	uint16_t val1, val2;

	switch (mydata->fatsize) {
	case 32:
		bufnum = entry / FAT32BUFSIZE;
		offset = entry - bufnum * FAT32BUFSIZE;
		break;
	case 16:
		bufnum = entry / FAT16BUFSIZE;
		offset = entry - bufnum * FAT16BUFSIZE;
		break;
	case 12:
		bufnum = entry / FAT12BUFSIZE;
		offset = entry - bufnum * FAT12BUFSIZE;
		break;

	default:
		// Unsupported FAT size
		return ret;
	}

	printf("FAT%d: entry: 0x%04x = %d, offset: 0x%04x = %d\n",
	       mydata->fatsize, entry, entry, offset, offset);

	// Read a new block of FAT entries into the cache.
	if (bufnum != (unsigned)mydata->fatbufnum) {
		unsigned getsize = FATBUFBLOCKS;
		uint8_t *bufptr = mydata->fatbuf;
		uint32_t fatlength = mydata->fatlength;
		uint32_t startblock = bufnum * FATBUFBLOCKS;

		if (getsize > fatlength)
			getsize = fatlength;

		fatlength *= mydata->sect_size;	// We want it in bytes now
		startblock += mydata->fat_sect;	// Offset from start of disk

		// Write back the fatbuf to the disk
		if (mydata->fatbufnum != -1) {
			if (flush_fat_buffer(mydata) < 0)
				return -1;
		}

		if (disk_read(startblock, getsize, bufptr) < 0) {
			printf("Error reading FAT blocks\n");
			return ret;
		}
		mydata->fatbufnum = bufnum;
	}

	// Get the actual entry from the table
	switch (mydata->fatsize) {
	case 32:
		ret = FAT2CPU32(((uint32_t *) mydata->fatbuf)[offset]);
		break;
	case 16:
		ret = FAT2CPU16(((uint16_t *) mydata->fatbuf)[offset]);
		break;
	case 12:
		off16 = (offset * 3) / 4;

		switch (offset & 0x3) {
		case 0:
			ret = FAT2CPU16(((uint16_t *) mydata->fatbuf)[off16]);
			ret &= 0xfff;
			break;
		case 1:
			val1 = FAT2CPU16(((uint16_t *)mydata->fatbuf)[off16]);
			val1 &= 0xf000;
			val2 = FAT2CPU16(((uint16_t *)mydata->fatbuf)[off16 + 1]);
			val2 &= 0x00ff;
			ret = (val2 << 4) | (val1 >> 12);
			break;
		case 2:
			val1 = FAT2CPU16(((uint16_t *)mydata->fatbuf)[off16]);
			val1 &= 0xff00;
			val2 = FAT2CPU16(((uint16_t *)mydata->fatbuf)[off16 + 1]);
			val2 &= 0x000f;
			ret = (val2 << 8) | (val1 >> 8);
			break;
		case 3:
			ret = FAT2CPU16(((uint16_t *)mydata->fatbuf)[off16]);
			ret = (ret & 0xfff0) >> 4;
			break;
		default:
			break;
		}
		break;
	}
	printf("FAT%d: ret: %08x, entry: %08x, offset: %04x\n",
	       mydata->fatsize, ret, entry, offset);

	return ret;
}

#ifdef HTCLEO_SUPPORT_VFAT
// Set the file name information from 'name' into 'slotptr',
static
int str2slot(dir_slot *slotptr, const char *name, int *idx)
{
	int j, end_idx = 0;

	for (j = 0; j <= 8; j += 2) {
		if (name[*idx] == 0x00) {
			slotptr->name0_4[j] = 0;
			slotptr->name0_4[j + 1] = 0;
			end_idx++;
			goto name0_4;
		}
		slotptr->name0_4[j] = name[*idx];
		(*idx)++;
		end_idx++;
	}
	for (j = 0; j <= 10; j += 2) {
		if (name[*idx] == 0x00) {
			slotptr->name5_10[j] = 0;
			slotptr->name5_10[j + 1] = 0;
			end_idx++;
			goto name5_10;
		}
		slotptr->name5_10[j] = name[*idx];
		(*idx)++;
		end_idx++;
	}
	for (j = 0; j <= 2; j += 2) {
		if (name[*idx] == 0x00) {
			slotptr->name11_12[j] = 0;
			slotptr->name11_12[j + 1] = 0;
			end_idx++;
			goto name11_12;
		}
		slotptr->name11_12[j] = name[*idx];
		(*idx)++;
		end_idx++;
	}

	if (name[*idx] == 0x00)
		return 1;

	return 0;
// Not used characters are filled with 0xff 0xff
name0_4:
	for (; end_idx < 5; end_idx++) {
		slotptr->name0_4[end_idx * 2] = 0xff;
		slotptr->name0_4[end_idx * 2 + 1] = 0xff;
	}
	end_idx = 5;

name5_10:
	end_idx -= 5;
	for (; end_idx < 6; end_idx++) {
		slotptr->name5_10[end_idx * 2] = 0xff;
		slotptr->name5_10[end_idx * 2 + 1] = 0xff;
	}
	end_idx = 11;
	
name11_12:
	end_idx -= 11;
	for (; end_idx < 2; end_idx++) {
		slotptr->name11_12[end_idx * 2] = 0xff;
		slotptr->name11_12[end_idx * 2 + 1] = 0xff;
	}

	return 1;
}

// Fill dir_slot entries with appropriate name, id, and attr
// The real directory entry is returned by 'dentptr'
static int is_next_clust(fsdata *mydata, dir_entry *dentptr);
static void flush_dir_table(fsdata *mydata, dir_entry **dentptr);
static
void fill_dir_slot(fsdata *mydata, dir_entry **dentptr, const char *l_name)
{
	dir_slot *slotptr = (dir_slot *)get_vfatname_block;
	uint8_t counter = 0, checksum;
	int idx = 0, ret;
	char s_name[16];

	// Get short file name and checksum value
	strncpy(s_name, (*dentptr)->name, 16);
	checksum = mkcksum(s_name);

	do {
		memset(slotptr, 0x00, sizeof(dir_slot));
		ret = str2slot(slotptr, l_name, &idx);
		slotptr->id = ++counter;
		slotptr->attr = ATTR_VFAT;
		slotptr->alias_checksum = checksum;
		slotptr++;
	} while (ret == 0);

	slotptr--;
	slotptr->id |= LAST_LONG_ENTRY_MASK;

	while (counter >= 1) {
		if (is_next_clust(mydata, *dentptr)) {
			// A new cluster is allocated for directory table
			flush_dir_table(mydata, dentptr);
		}
		memcpy(*dentptr, slotptr, sizeof(dir_slot));
		(*dentptr)++;
		slotptr--;
		counter--;
	}

	if (is_next_clust(mydata, *dentptr)) {
		// A new cluster is allocated for directory table
		flush_dir_table(mydata, dentptr);
	}
}

// Extract the full long filename starting at 'retdent' (which is really
// a slot) into 'l_name'. If successful also copy the real directory entry
// into 'retdent'
// If additional adjacent cluster for directory entries is read into memory,
// then 'get_vfatname_block' is copied into 'get_dentfromdir_block' and
// the location of the real directory entry is returned by 'retdent'
// Return 0 on success, -1 otherwise.
static uint32_t dir_curclust;
static
int get_long_file_name(fsdata *mydata, int curclust, uint8_t *cluster, dir_entry **retdent, char *l_name)
{
	dir_entry *realdent;
	dir_slot *slotptr = (dir_slot *)(*retdent);
	dir_slot *slotptr2 = NULL;
	uint8_t *buflimit = cluster + mydata->sect_size * ((curclust == 0) ?
							PREFETCH_BLOCKS :
							mydata->clust_size);
	uint8_t counter = (slotptr->id & ~LAST_LONG_ENTRY_MASK) & 0xff;
	int idx = 0, cur_position = 0;

	if (counter > VFAT_MAXSEQ) {
		printf("Error: VFAT name is too long\n");
		return -1;
	}

	while ((uint8_t *)slotptr < buflimit) {
		if (counter == 0)
			break;
			
		if (((slotptr->id & ~LAST_LONG_ENTRY_MASK) & 0xff) != counter)
			return -1;
			
		slotptr++;
		counter--;
	}

	if ((uint8_t *)slotptr >= buflimit) {
		if (curclust == 0)
			return -1;
			
		curclust = get_fatent_value(mydata, dir_curclust);
		if (CHECK_CLUST(curclust, mydata->fatsize)) {
			printf("curclust: 0x%x\n", curclust);
			printf("Invalid FAT entry\n");
			return -1;
		}

		dir_curclust = curclust;

		if (get_cluster(mydata, curclust, get_vfatname_block,
				mydata->clust_size * mydata->sect_size) != 0) {
			printf("Error: reading directory block\n");
			return -1;
		}

		slotptr2 = (dir_slot *)get_vfatname_block;
		while (counter > 0) {
			if (((slotptr2->id & ~LAST_LONG_ENTRY_MASK) & 0xff) != counter)
				return -1;
				
			slotptr2++;
			counter--;
		}

		// Save the real directory entry
		realdent = (dir_entry *)slotptr2;
		while ((uint8_t *)slotptr2 > get_vfatname_block) {
			slotptr2--;
			slot2str(slotptr2, l_name, &idx);
		}
	} else {
		// Save the real directory entry
		realdent = (dir_entry *)slotptr;
	}

	do {
		slotptr--;
		if (slot2str(slotptr, l_name, &idx))
			break;
			
	} while (!(slotptr->id & LAST_LONG_ENTRY_MASK));

	l_name[idx] = '\0';
	if (*l_name == DELETED_FLAG)
		*l_name = '\0';
	else if (*l_name == aRING)
		*l_name = DELETED_FLAG;
		
	downcase(l_name);

	// Return the real directory entry
	*retdent = realdent;

	if (slotptr2) {
		memcpy(get_dentfromdir_block, get_vfatname_block, mydata->clust_size * mydata->sect_size);
		cur_position = (uint8_t *)realdent - get_vfatname_block;
		*retdent = (dir_entry *) &get_dentfromdir_block[cur_position];
	}

	return 0;
}

#endif

// Set the entry at index 'entry' in a FAT (16/32) table.
static
int set_fatent_value(fsdata *mydata, uint32_t entry, uint32_t entry_value)
{
	uint32_t bufnum, offset;

	switch (mydata->fatsize) {
	case 32:
		bufnum = entry / FAT32BUFSIZE;
		offset = entry - bufnum * FAT32BUFSIZE;
		break;
	case 16:
		bufnum = entry / FAT16BUFSIZE;
		offset = entry - bufnum * FAT16BUFSIZE;
		break;
	default:
		// Unsupported FAT size
		return -1;
	}

	// Read a new block of FAT entries into the cache.
	if (bufnum != (unsigned)mydata->fatbufnum) {
		unsigned getsize = FATBUFBLOCKS;
		uint8_t *bufptr = mydata->fatbuf;
		uint32_t fatlength = mydata->fatlength;
		uint32_t startblock = bufnum * FATBUFBLOCKS;

		fatlength *= mydata->sect_size;
		startblock += mydata->fat_sect;

		if (getsize > fatlength)
			getsize = fatlength;

		if (mydata->fatbufnum != -1) {
			if (flush_fat_buffer(mydata) < 0)
				return -1;
		}

		if (disk_read(startblock, getsize, bufptr) < 0) {
			printf("Error reading FAT blocks\n");
			return -1;
		}
		mydata->fatbufnum = bufnum;
	}

	// Set the actual entry
	switch (mydata->fatsize) {
	case 32:
		((uint32_t *) mydata->fatbuf)[offset] = LE32(entry_value);
		break;
	case 16:
		((uint16_t *) mydata->fatbuf)[offset] = LE16(entry_value);
		break;
	default:
		return -1;
	}

	return 0;
}

// Determine the entry value at index 'entry' in a FAT (16/32) table
static
uint32_t determine_fatent(fsdata *mydata, uint32_t entry)
{
	uint32_t next_fat, next_entry = entry + 1;

	while (1) {
		next_fat = get_fatent_value(mydata, next_entry);
		if (next_fat == 0) {
			set_fatent_value(mydata, entry, next_entry);
			break;
		}
		next_entry++;
	}
	printf("FAT%d: entry: %08x, entry_value: %04x\n",
	       mydata->fatsize, entry, next_entry);

	return next_entry;
}

// Write at most 'size' bytes from 'buffer' into the specified cluster.
// Return 0 on success, -1 otherwise.
static
int set_cluster(fsdata *mydata, uint32_t clustnum, uint8_t *buffer, unsigned long size)
{
	int idx = 0;
	uint32_t startsect;

	if (clustnum > 0)
		startsect = mydata->data_begin + clustnum * mydata->clust_size;
	else
		startsect = mydata->rootdir_sect;

	printf("clustnum: %d, startsect: %d\n", clustnum, startsect);

	if (disk_write(startsect, size / mydata->sect_size, buffer) < 0) {
		printf("Error writing data\n");
		return -1;
	}

	if (size % mydata->sect_size) {
		uint8_t tmpbuf[mydata->sect_size];
		idx = size / mydata->sect_size;
		buffer += idx * mydata->sect_size;
		memcpy(tmpbuf, buffer, size % mydata->sect_size);

		if (disk_write(startsect + idx, 1, tmpbuf) < 0) {
			printf("Error writing data\n");
			return -1;
		}

		return 0;
	}

	return 0;
}

// Find the first empty cluster
static
int find_empty_cluster(fsdata *mydata)
{
	uint32_t fat_val, entry = 3;

	while (1) {
		fat_val = get_fatent_value(mydata, entry);
		if (fat_val == 0)
			break;
			
		entry++;
	}

	return entry;
}

// Write directory entries in 'get_dentfromdir_block' to block device
static
void flush_dir_table(fsdata *mydata, dir_entry **dentptr)
{
	int dir_newclust = 0;

	if (set_cluster(mydata, dir_curclust, get_dentfromdir_block, mydata->clust_size * mydata->sect_size) != 0) {
		printf("error: wrinting directory entry\n");
		return;
	}
	dir_newclust = find_empty_cluster(mydata);
	set_fatent_value(mydata, dir_curclust, dir_newclust);
	if (mydata->fatsize == 32)
		set_fatent_value(mydata, dir_newclust, 0xffffff8);
	else if (mydata->fatsize == 16)
		set_fatent_value(mydata, dir_newclust, 0xfff8);

	dir_curclust = dir_newclust;

	if (flush_fat_buffer(mydata) < 0)
		return;

	memset(get_dentfromdir_block, 0x00,	mydata->clust_size * mydata->sect_size);

	*dentptr = (dir_entry *) get_dentfromdir_block;
}

// Set empty cluster from 'entry' to the end of a file
static
int clear_fatent(fsdata *mydata, uint32_t entry)
{
	uint32_t fat_val;

	while (1) {
		fat_val = get_fatent_value(mydata, entry);
		if (fat_val != 0)
			set_fatent_value(mydata, entry, 0);
		else
			break;

		if (fat_val == 0xfffffff || fat_val == 0xffff)
			break;

		entry = fat_val;
	}

	// Flush fat buffer
	if (flush_fat_buffer(mydata) < 0)
		return -1;

	return 0;
}

// Write at most 'maxsize' bytes from 'buffer' into
// the file associated with 'dentptr'
// Return the number of bytes read or -1 on fatal errors.
static
int set_contents(fsdata *mydata, dir_entry *dentptr, uint8_t *buffer, unsigned long maxsize)
{
	unsigned long filesize = FAT2CPU32(dentptr->size), gotsize = 0;
	unsigned int bytesperclust = mydata->clust_size * mydata->sect_size;
	uint32_t curclust = START(dentptr);
	uint32_t endclust = 0, newclust = 0;
	unsigned long actsize;

	printf("Filesize: %ld bytes\n", filesize);

	if (maxsize > 0 && filesize > maxsize)
		filesize = maxsize;

	printf("%ld bytes\n", filesize);

	actsize = bytesperclust;
	endclust = curclust;
	do {
		// search for consecutive clusters
		while (actsize < filesize) {
			newclust = determine_fatent(mydata, endclust);

			if ((newclust - 1) != endclust)
				goto getit;

			if (CHECK_CLUST(newclust, mydata->fatsize)) {
				printf("curclust: 0x%x\n", newclust);
				printf("Invalid FAT entry\n");
				return gotsize;
			}
			endclust = newclust;
			actsize += bytesperclust;
		}
		// actsize >= file size
		actsize -= bytesperclust;
		// set remaining clusters
		if (set_cluster(mydata, curclust, buffer, (int)actsize) != 0) {
			printf("error: writing cluster\n");
			return -1;
		}

		// set remaining bytes
		gotsize += (int)actsize;
		filesize -= actsize;
		buffer += actsize;
		actsize = filesize;

		if (set_cluster(mydata, endclust, buffer, (int)actsize) != 0) {
			printf("error: writing cluster\n");
			return -1;
		}
		gotsize += actsize;

		// Mark end of file in FAT
		if (mydata->fatsize == 16)
			newclust = 0xffff;
		else if (mydata->fatsize == 32)
			newclust = 0xfffffff;
			
		set_fatent_value(mydata, endclust, newclust);

		return gotsize;
		
getit:
		if (set_cluster(mydata, curclust, buffer, (int)actsize) != 0) {
			printf("error: writing cluster\n");
			return -1;
		}
		gotsize += (int)actsize;
		filesize -= actsize;
		buffer += actsize;

		if (CHECK_CLUST(curclust, mydata->fatsize)) {
			printf("curclust: 0x%x\n", curclust);
			printf("Invalid FAT entry\n");
			return gotsize;
		}
		actsize = bytesperclust;
		curclust = endclust = newclust;
	} while (1);
}

// Fill dir_entry
static
void fill_dentry(fsdata *mydata, dir_entry *dentptr,	const char *filename, uint32_t start_cluster, uint32_t size, uint8_t attr)
{
	if (mydata->fatsize == 32)
		dentptr->starthi = LE16((start_cluster & 0xffff0000) >> 16);
		
	dentptr->start = LE16(start_cluster & 0xffff);
	dentptr->size = LE32(size);

	dentptr->attr = attr;

	set_name(dentptr, filename);
}

// Check whether adding a file makes the file system to
// exceed the size of the block device
// Return -1 when overflow occurs, otherwise return 0
static
int check_overflow(fsdata *mydata, uint32_t clustnum, unsigned long size)
{
	uint32_t startsect, sect_num;

	if (clustnum > 0) {
		startsect = mydata->data_begin + clustnum * mydata->clust_size;
	} else {
		startsect = mydata->rootdir_sect;
	}

	sect_num = size / mydata->sect_size;
	if (size % mydata->sect_size)
		sect_num++;

	if ((unsigned)(startsect + sect_num) > total_sector)
		return -1;

	return 0;
}

// Check if adding several entries exceed one cluster boundary
static
int is_next_clust(fsdata *mydata, dir_entry *dentptr)
{
	int cur_position;

	cur_position = (uint8_t *)dentptr - get_dentfromdir_block;

	if (cur_position >= mydata->clust_size * mydata->sect_size)
		return 1;
	else
		return 0;
}

// Find a directory entry based on filename or start cluster number
// If the directory entry is not found,
// the new position for writing a directory entry will be returned
static
dir_entry *empty_dentptr;
static
dir_entry *find_directory_entry(fsdata *mydata, int startsect, char *filename, dir_entry *retdent, uint32_t start)
{
	uint16_t prevcksum __UNUSED = 0xffff;
	uint32_t curclust = (startsect - mydata->data_begin) / mydata->clust_size;

	printf("get_dentfromdir: %s\n", filename);

	while (1) {
		dir_entry *dentptr;

		unsigned i;

		if (get_cluster(mydata, curclust, get_dentfromdir_block, mydata->clust_size * mydata->sect_size) != 0) {
			printf("Error: reading directory block\n");
			return NULL;
		}

		dentptr = (dir_entry *)get_dentfromdir_block;

		dir_curclust = curclust;

		for (i = 0; i < DIRENTSPERCLUST; i++) {
			char s_name[14], l_name[VFAT_MAXLEN_BYTES];

			l_name[0] = '\0';
			if (dentptr->name[0] == DELETED_FLAG) {
				dentptr++;
				if (is_next_clust(mydata, dentptr))
					break;
					
				continue;
			}
			if ((dentptr->attr & ATTR_VOLUME)) {
#ifdef HTCLEO_SUPPORT_VFAT
				if ((dentptr->attr & ATTR_VFAT) && (dentptr->name[0] & LAST_LONG_ENTRY_MASK)) {
					prevcksum =	((dir_slot *)dentptr)->alias_checksum;
					get_long_file_name(mydata, curclust, get_dentfromdir_block, &dentptr, l_name);
					printf("vfatname: |%s|\n", l_name);
				} else
#endif
				{
					// Volume label or VFAT entry
					dentptr++;
					if (is_next_clust(mydata, dentptr))
						break;
						
					continue;
				}
			}
			if (dentptr->name[0] == 0) {
				printf("Dentname == NULL - %d\n", i);
				empty_dentptr = dentptr;
				return NULL;
			}

			get_name(dentptr, s_name);

			if (strcmp(filename, s_name) && strcmp(filename, l_name)) {
				printf("Mismatch: |%s|%s|\n", s_name, l_name);
				dentptr++;
				if (is_next_clust(mydata, dentptr))
					break;
					
				continue;
			}

			memcpy(retdent, dentptr, sizeof(dir_entry));

			printf("DentName: %s", s_name);
			printf(", start: 0x%x", START(dentptr));
			printf(", size:  0x%x %s\n", FAT2CPU32(dentptr->size), (dentptr->attr & ATTR_DIR) ? "(DIR)" : "");

			return dentptr;
		}

		curclust = get_fatent_value(mydata, dir_curclust);
		if ((curclust >= 0xffffff8) || (curclust >= 0xfff8)) {
			empty_dentptr = dentptr;
			return NULL;
		}
		if (CHECK_CLUST(curclust, mydata->fatsize)) {
			printf("curclust: 0x%x\n", curclust);
			printf("Invalid FAT entry\n");
			return NULL;
		}
	}

	return NULL;
}

static
int do_fat_write(const char *filename, void *buffer,	unsigned long size)
{
	dir_entry *dentptr, *retdent;
	dir_slot *slotptr __UNUSED;
	uint32_t startsect;
	uint32_t start_cluster;
	boot_sector bs;
	volume_info volinfo;
	fsdata datablock;
	fsdata *mydata = &datablock;
	int cursect;
	int root_cluster __UNUSED, ret = -1, name_len;
	char l_filename[VFAT_MAXLEN_BYTES];
	int write_size = size;

	dir_curclust = 0;

	if (read_bootsectandvi(&bs, &volinfo, &mydata->fatsize)) {
		printf("error: reading boot sector\n");
		return -1;
	}

	total_sector = bs.total_sect;
	//if (total_sector == 0)
	//	total_sector = part_size;

	root_cluster = bs.root_cluster;

	if (mydata->fatsize == 32)
		mydata->fatlength = bs.fat32_length;
	else
		mydata->fatlength = bs.fat_length;

	mydata->fat_sect = bs.reserved;

	cursect = mydata->rootdir_sect = mydata->fat_sect + mydata->fatlength * bs.fats;
	num_of_fats = bs.fats;

	mydata->sect_size = (bs.sector_size[1] << 8) + bs.sector_size[0];
	mydata->clust_size = bs.cluster_size;

	if (mydata->fatsize == 32) {
		mydata->data_begin = mydata->rootdir_sect -	(mydata->clust_size * 2);
	} else {
		int rootdir_size;

		rootdir_size = ((bs.dir_entries[1]  * (int)256 +
						bs.dir_entries[0]) * sizeof(dir_entry)) /
						mydata->sect_size;
		mydata->data_begin = mydata->rootdir_sect +
							 rootdir_size -
							 (mydata->clust_size * 2);
	}

	mydata->fatbufnum = -1;
	mydata->fatbuf = malloc(FATBUFSIZE);
	if (mydata->fatbuf == NULL) {
		printf("Error: allocating memory\n");
		return -1;
	}

	if (disk_read(cursect,
				 (mydata->fatsize == 32) ? (mydata->clust_size) : PREFETCH_BLOCKS,
				  do_fat_read_block) < 0) {
		printf("Error: reading rootdir block\n");
		goto exit;
	}
	dentptr = (dir_entry *) do_fat_read_block;

	name_len = strlen(filename);
	if (name_len >= VFAT_MAXLEN_BYTES)
		name_len = VFAT_MAXLEN_BYTES - 1;

	memcpy(l_filename, filename, name_len);
	l_filename[name_len] = 0; // terminate the string
	downcase(l_filename);

	startsect = mydata->rootdir_sect;
	retdent = find_directory_entry(mydata, startsect, l_filename, dentptr, 0);
	if (retdent) {
		// Update file size and start_cluster in a directory entry
		retdent->size = LE32(size);
		start_cluster = FAT2CPU16(retdent->start);
		if (mydata->fatsize == 32)
			start_cluster |= (FAT2CPU16(retdent->starthi) << 16);

		ret = check_overflow(mydata, start_cluster, size);
		if (ret) {
			printf("Error: %ld overflow\n", size);
			goto exit;
		}

		ret = clear_fatent(mydata, start_cluster);
		if (ret) {
			printf("Error: clearing FAT entries\n");
			goto exit;
		}

		ret = set_contents(mydata, retdent, buffer, size);
		if (ret < 0) {
			printf("Error: writing contents\n");
			goto exit;
		}
		write_size = ret;
		printf("attempt to write 0x%x bytes\n", write_size);

		// Flush fat buffer
		ret = flush_fat_buffer(mydata);
		if (ret) {
			printf("Error: flush fat buffer\n");
			goto exit;
		}

		// Write directory table to device
		ret = set_cluster(mydata, dir_curclust, get_dentfromdir_block, mydata->clust_size * mydata->sect_size);
		if (ret) {
			printf("Error: writing directory entry\n");
			goto exit;
		}
	} else {
		slotptr = (dir_slot *)empty_dentptr;

		// Set short name to set alias checksum field in dir_slot
		set_name(empty_dentptr, filename);
		fill_dir_slot(mydata, &empty_dentptr, filename);

		ret = start_cluster = find_empty_cluster(mydata);
		if (ret < 0) {
			printf("Error: finding empty cluster\n");
			goto exit;
		}

		ret = check_overflow(mydata, start_cluster, size);
		if (ret) {
			printf("Error: %ld overflow\n", size);
			goto exit;
		}

		// Set attribute as archieve for regular file
		fill_dentry(mydata, empty_dentptr, filename, start_cluster, size, 0x20);

		ret = set_contents(mydata, empty_dentptr, buffer, size);
		if (ret < 0) {
			printf("Error: writing contents\n");
			goto exit;
		}
		write_size = ret;
		printf("attempt to write 0x%x bytes\n", write_size);

		// Flush fat buffer
		ret = flush_fat_buffer(mydata);
		if (ret) {
			printf("Error: flush fat buffer\n");
			goto exit;
		}

		// Write directory table to device
		ret = set_cluster(mydata, dir_curclust, get_dentfromdir_block, mydata->clust_size * mydata->sect_size);
		if (ret) {
			printf("Error: writing directory entry\n");
			goto exit;
		}
	}

exit:
	free(mydata->fatbuf);
	return ret < 0 ? ret : write_size;
}

/********************************************************************************************************
 * Exports
 ********************************************************************************************************/
int file_fat_detectfs(void)
{
	boot_sector	bs;
	volume_info	volinfo;
	int		fatsize;
	char	vol_label[12];

	if(cur_dev==NULL) {
		printf("No current device\n");
		return 1;
	}
#if defined(CONFIG_CMD_IDE) || \
    defined(CONFIG_CMD_MG_DISK) || \
    defined(CONFIG_CMD_SATA) || \
    defined(CONFIG_CMD_SCSI) || \
    defined(CONFIG_CMD_USB) || \
    defined(CONFIG_MMC)
	printf("Interface:  ");
	switch(cur_dev->if_type) {
		case IF_TYPE_IDE :	printf("IDE"); break;
		case IF_TYPE_SATA :	printf("SATA"); break;
		case IF_TYPE_SCSI :	printf("SCSI"); break;
		case IF_TYPE_ATAPI :	printf("ATAPI"); break;
		case IF_TYPE_USB :	printf("USB"); break;
		case IF_TYPE_DOC :	printf("DOC"); break;
		case IF_TYPE_MMC :	printf("MMC"); break;
		default :		printf("Unknown");
	}
	printf("\n  Device %d: ",cur_dev->dev);
	dev_print(cur_dev);
#endif
	if(read_bootsectandvi(&bs, &volinfo, &fatsize)) {
		printf("\nNo valid FAT fs found\n");
		return 1;
	}
	memcpy (vol_label, volinfo.volume_label, 11);
	vol_label[11] = '\0';
	volinfo.fs_type[5]='\0';
	printf("Partition %d: Filesystem: %s \"%s\"\n",cur_part,volinfo.fs_type,vol_label);
	return 0;
}

int file_fat_ls(const char *dir)
{
	return do_fat_read(dir, NULL, 0, LS_YES);
}

long file_fat_read(const char *filename, void *buffer, unsigned long maxsize)
{
	printf("reading %s\n",filename);
	return do_fat_read(filename, buffer, maxsize, LS_NO);
}

int file_fat_write(const char *filename, void *buffer, unsigned long maxsize)
{
	printf("writing %s\n", filename);
	return do_fat_write(filename, buffer, maxsize);
}
