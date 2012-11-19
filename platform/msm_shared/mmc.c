/*
 * Copyright 2008, Freescale Semiconductor, Inc
 * Andy Fleming
 *
 * Based vaguely on the Linux code
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
#include <compiler.h>
#include <debug.h>
#include <endian.h>
#include <malloc.h>
#include <reg.h>
#include <string.h>
#include <adm.h>
#include <mmc.h>
#include <platform/timer.h>
#include <pcom_clients.h>
#include <target/reg.h>

static struct list_node mmc_devices;
static int cur_dev_num = -1;

int mmc_send_cmd(mmc_t *mmc, mmc_cmd_t *cmd, mmc_data_t *data)
{
	return mmc->send_cmd(mmc, cmd, data);
}

int mmc_set_blocklen(mmc_t *mmc, int len)
{
	mmc_cmd_t cmd;
	cmd.cmdidx = MMC_CMD_SET_BLOCKLEN;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = len;
	cmd.flags = 0;

	return mmc_send_cmd(mmc, &cmd, NULL);
}

mmc_t *find_mmc_device(int dev_num)
{
	mmc_t *m;

	list_for_every_entry(&mmc_devices, m, struct mmc, link) {
		if (m->block_dev.dev == dev_num)
			return m;
	}

	printf("MMC Device %d not found\n", dev_num);

	return NULL;
}

static
ulong _mmc_bwrite(int dev_num, ulong start, ulong blkcnt, const void *src)
{
	int err;
	mmc_t *mmc = /*&htcleo_mmc;*/find_mmc_device(dev_num);
	if (!mmc)
		return -1;

	int blklen = mmc->write_bl_len;

	err = mmc_set_blocklen(mmc, mmc->write_bl_len);
	if (err) {
		printf("set write bl len failed\n\r");
		return err;
	}

	mmc_cmd_t cmd;
	if (blkcnt > 1) {
		cmd.cmdidx = MMC_CMD_WRITE_MULTIPLE_BLOCK;
	} else {
		cmd.cmdidx = MMC_CMD_WRITE_SINGLE_BLOCK;
	}
	if (mmc->high_capacity) {
		cmd.cmdarg = start;
	} else {
		cmd.cmdarg = start * blklen;
	}
	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;

	mmc_data_t data;
	data.src = src;
	data.blocks = blkcnt;
	data.blocksize = blklen;
	data.flags = MMC_DATA_WRITE;

	err = mmc_send_cmd(mmc, &cmd, &data);
	if (err) {
		printf("mmc write failed\n\r");
		return err;
	}

	if (blkcnt > 1) {
		int stoperr __UNUSED = 0;
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		cmd.flags = 0;
		stoperr = mmc_send_cmd(mmc, &cmd, NULL);
	}

	return blkcnt;
}

int mmc_read_block(mmc_t *mmc, void *dst, uint blocknum)
{
	mmc_cmd_t cmd;
	cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;
	if (mmc->high_capacity) {
		cmd.cmdarg = blocknum;
	} else {
		cmd.cmdarg = blocknum * mmc->read_bl_len;
	}
	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;

	mmc_data_t data;
	data.dest = dst;
	data.blocks = 1;
	data.blocksize = mmc->read_bl_len;
	data.flags = MMC_DATA_READ;

	return mmc_send_cmd(mmc, &cmd, &data);
}

static inline uint64_t lldiv(uint64_t dividend, uint32_t divisor)
{
	uint64_t __res = dividend;
	
	if (dividend >> 32 == 0) {
		__res = (uint32_t)(dividend) % divisor;
		dividend = (uint32_t)(dividend) / divisor;
	} else {
		uint64_t __res = dividend;
		uint64_t b = divisor;
		uint64_t d = 1;
		uint32_t high = __res >> 32;
		uint64_t res = 0;
		
		if (high >= divisor) {
			high /= divisor;
			res = (uint64_t)(high) << 32;
			__res -= (uint64_t)(high * divisor) << 32;
		}

		while ((int64_t)b > 0 && b < __res) {
			b = b+b;
			d = d+d;
		}

		do {
			if (__res >= b) {
				__res -= b;
				res += d;
			}
			b >>= 1;
			d >>= 1;
		} while (d);
		
		__res = res;
	}
	
	return (uint64_t)__res;
}

int _mmc_read(mmc_t *mmc, uint64_t src, uchar *dst, int size)
{
	char *buffer;
	int i;
	int blklen = mmc->read_bl_len;
	int startblock = lldiv(src, mmc->read_bl_len);
	int endblock = lldiv(src + size - 1, mmc->read_bl_len);
	int err = 0;

	// Make a buffer big enough to hold all the blocks we might read
	buffer = malloc(blklen);

	if (!buffer) {
		printf("Could not allocate buffer for MMC read!\n");
		return -1;
	}

	// We always do full block reads from the card
	err = mmc_set_blocklen(mmc, mmc->read_bl_len);

	if (err)
		return err;

	for (i = startblock; i <= endblock; i++) {
		int segment_size;
		int offset;

		err = mmc_read_block(mmc, buffer, i);

		if (err)
			goto free_buffer;

		// The first block may not be aligned, so we
		// copy from the desired point in the block
		offset = (src & (blklen - 1));
		segment_size = (blklen - offset <  size ? blklen - offset : size);

		memcpy(dst, buffer + offset, segment_size);

		dst += segment_size;
		src += segment_size;
		size -= segment_size;
	}

free_buffer:
	free(buffer);

	return err;
}

static
ulong _mmc_bread(int dev_num, ulong start, ulong blkcnt, void *dst)
{
	int err;
	mmc_t *mmc = /*&htcleo_mmc;*/find_mmc_device(dev_num);
	if (!mmc)
		return 0;
	
	int blklen = mmc->read_bl_len;
	
	// We always do full block reads from the card
	err = mmc_set_blocklen(mmc, blklen);
	if (err) {
		printf("set read bl len failed err = %d\n\r", err);
		return 0;
	}
#ifdef CONFIG_GENERIC_MMC_MULTI_BLOCK_READ
	mmc_cmd_t cmd;
	if (blkcnt > 1) {
		cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
	} else {
		cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;
	}
	if (mmc->high_capacity) {
		cmd.cmdarg = start;
	} else {
		cmd.cmdarg = start * blklen;
	}
	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;

	mmc_data_t data;
	data.dest = dst;
	data.blocks = blkcnt;
	data.blocksize = blklen;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd(mmc, &cmd, &data);
	if (err) {
		printf("mmc read failed err= %d\n\r", err);
		return 0;
	}
	
	if (blkcnt > 1) {
		int stoperr = 0;
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1;
		cmd.flags = 0;

		stoperr = mmc_send_cmd(mmc, &cmd, NULL);
		if (stoperr) {
			printf("mmc stop read failed err = %d\n\r",stoperr);
			return 0;
		}
	}
#else
	for (unsigned i = start; i < start + blkcnt; i++, dst += mmc->read_bl_len) {
		err = mmc_read_block(mmc, dst, i);
		if (err) {
			printf("block read failed: %d\n", err);
			return i - start;
		}
	}
#endif
	return blkcnt;
}

int mmc_go_idle(struct mmc* mmc)
{
	mmc_cmd_t cmd;
	int err;

	udelay(1000);

	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	udelay(2000);

	return 0;
}

int sd_send_op_cond(mmc_t *mmc)
{
	int timeout = 1000;
	int err;
	mmc_cmd_t cmd;

	do {
		cmd.cmdidx = MMC_CMD_APP_CMD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 0;
		cmd.flags = 0;

		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;

		cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = mmc->voltages;

		if (mmc->version == SD_VERSION_2)
			cmd.cmdarg |= OCR_HCS;

		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;

		udelay(1000);
	} while ((!(cmd.response[0] & OCR_BUSY)) && timeout--);

	if (timeout <= 0)
		return UNUSABLE_ERR;

	if (mmc->version != SD_VERSION_2)
		mmc->version = SD_VERSION_1_0;

	mmc->ocr 			= cmd.response[0];
	mmc->high_capacity	= ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca 			= 0;

	return 0;
}

int mmc_send_op_cond(mmc_t *mmc)
{
	int timeout = 1000;
	mmc_cmd_t cmd;
	int err;

	// Some cards seem to need this
	mmc_go_idle(mmc);

	do {
		cmd.cmdidx = MMC_CMD_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = OCR_HCS | mmc->voltages;
		cmd.flags = 0;

		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;

		udelay(1000);
	} while (!(cmd.response[0] & OCR_BUSY) && timeout--);

	if (timeout <= 0)
		return UNUSABLE_ERR;

	mmc->version 		= MMC_VERSION_UNKNOWN;
	mmc->ocr 			= cmd.response[0];
	mmc->high_capacity 	= ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca 			= 0;

	return 0;
}


int mmc_send_ext_csd(mmc_t *mmc, char *ext_csd)
{
	mmc_cmd_t cmd;
	mmc_data_t data;
	int err;

	// Get the Card Status Register
	cmd.cmdidx = MMC_CMD_SEND_EXT_CSD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;
	cmd.flags = 0;

	data.dest = ext_csd;
	data.blocks = 1;
	data.blocksize = 512;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd(mmc, &cmd, &data);

	return err;
}


int mmc_switch(mmc_t *mmc, uint8_t set, uint8_t index, uint8_t value)
{
	mmc_cmd_t cmd;
	cmd.cmdidx 		= 	MMC_CMD_SWITCH;
	cmd.resp_type 	= 	MMC_RSP_R1b;
	cmd.cmdarg 		= 	(MMC_SWITCH_MODE_WRITE_BYTE << 24)
						|(index << 16)
						|(value << 8);
	cmd.flags 		= 	0;

	return mmc_send_cmd(mmc, &cmd, NULL);
}

int mmc_change_freq(mmc_t *mmc)
{
	char ext_csd[512];
	char cardtype;
	int err;

	mmc->card_caps = 0;

	// Only version 4 supports high-speed
	if (mmc->version < MMC_VERSION_4)
		return 0;

	// Version 4 and above MMC cards will always support the 8bit
	// Its the Host controller who decides between the 4bit and 8bit
	if(mmc->host_caps & MMC_MODE_8BIT)
		mmc->card_caps |= MMC_MODE_8BIT;
	else
		mmc->card_caps |= MMC_MODE_4BIT;

	err = mmc_send_ext_csd(mmc, ext_csd);
	if (err)
		return err;

	if (ext_csd[212] || ext_csd[213] || ext_csd[214] || ext_csd[215])
		mmc->high_capacity = 1;

	cardtype = ext_csd[196] & 0xf;

	err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 1);
	if (err)
		return err;

	// Now check to see that it worked
	err = mmc_send_ext_csd(mmc, ext_csd);
	if (err)
		return err;

	// No high-speed support
	if (!ext_csd[185])
		return 0;

	// High Speed is set, there are two types: 52MHz and 26MHz
	if (cardtype & MMC_HS_52MHZ)
		mmc->card_caps |= MMC_MODE_HS_52MHz | MMC_MODE_HS;
	else
		mmc->card_caps |= MMC_MODE_HS;

	return 0;
}

int sd_switch(mmc_t *mmc, int mode, int group, uint8_t value, uint8_t *resp)
{
	mmc_cmd_t cmd;
	cmd.cmdidx 		= SD_CMD_SWITCH_FUNC; // Switch the frequency
	cmd.resp_type 	= MMC_RSP_R1;
	cmd.cmdarg 		= (mode << 31) | 0xffffff;
	cmd.cmdarg 		&= ~(0xf << (group * 4));
	cmd.cmdarg 		|= value << (group * 4);
	cmd.flags 		= 0;

	mmc_data_t data;
	data.dest 		= (char *)resp;
	data.blocksize 	= 64;
	data.blocks 	= 1;
	data.flags 		= MMC_DATA_READ;

	return mmc_send_cmd(mmc, &cmd, &data);
}


int sd_change_freq(mmc_t *mmc)
{
	int err;
	mmc_cmd_t cmd;
	uint scr[2];
	uint switch_status[16];
	mmc_data_t data;
	int timeout;

	mmc->card_caps = 0;

	// Read the SCR to find out if this card supports higher speeds
	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	cmd.cmdidx = SD_CMD_APP_SEND_SCR;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;
	cmd.flags = 0;

	timeout = 3;

retry_scr:
	data.dest = (char *)&scr;
	data.blocksize = 8;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd(mmc, &cmd, &data);

	if (err) {
		if (timeout--)
			goto retry_scr;

		return err;
	}

	mmc->scr[0] = BE32(scr[0]);
	mmc->scr[1] = BE32(scr[1]);

	switch ((mmc->scr[0] >> 24) & 0xf) {
		case 0:
			mmc->version = SD_VERSION_1_0;
			break;
		case 1:
			mmc->version = SD_VERSION_1_10;
			break;
		case 2:
			mmc->version = SD_VERSION_2;
			break;
		default:
			mmc->version = SD_VERSION_1_0;
			break;
	}

	// Version 1.0 doesn't support switching
	if (mmc->version == SD_VERSION_1_0)
		return 0;

	timeout = 4;
	while (timeout--) {
		err = sd_switch(mmc, SD_SWITCH_CHECK, 0, 1,
				(uint8_t *)&switch_status);

		if (err)
			return err;

		// The high-speed function is busy.  Try again
		if (!(BE32(switch_status[7]) & SD_HIGHSPEED_BUSY))
			break;
	}

	if (mmc->scr[0] & SD_DATA_4BIT)
		mmc->card_caps |= MMC_MODE_4BIT;

	// If high-speed isn't supported, we return
	if (!(BE32(switch_status[3]) & SD_HIGHSPEED_SUPPORTED))
		return 0;

	// If host does not support high-speed, we return
	if(!(mmc->host_caps & MMC_MODE_HS))
		return 0;

	err = sd_switch(mmc, SD_SWITCH_SWITCH, 0, 1, (uint8_t *)&switch_status);

	if (err)
		return err;

	if ((BE32(switch_status[4]) & 0x0f000000) == 0x01000000)
		mmc->card_caps |= MMC_MODE_HS;

	return 0;
}

/* frequency bases
 * divided by 10 to be nice to platforms without floating point */
int fbase[] = {
	10000,
	100000,
	1000000,
	10000000,
};

/* Multiplier values for TRAN_SPEED.  Multiplied by 10 to be nice
 * to platforms without floating point.
 */
int multipliers[] = {
	0,	// reserved
	10,
	12,
	13,
	15,
	20,
	25,
	30,
	35,
	40,
	45,
	50,
	55,
	60,
	70,
	80,
};

void mmc_set_ios(mmc_t *mmc)
{
	mmc->set_ios(mmc);
}

void mmc_set_clock(mmc_t *mmc, uint clock)
{
	if (clock > mmc->f_max)
		clock = mmc->f_max;

	if (clock < mmc->f_min)
		clock = mmc->f_min;

	mmc->clock = clock;

	mmc_set_ios(mmc);
}

void mmc_set_bus_width(mmc_t *mmc, uint width)
{
	mmc->bus_width = width;

	mmc_set_ios(mmc);
}

int mmc_startup(mmc_t *mmc)
{
	int err;
	uint mult, freq;
	uint64_t cmult, csize;
	mmc_cmd_t cmd;
	char ext_csd[512];
	uint64_t sec_count;

	// Put the Card in Identify Mode
	cmd.cmdidx = MMC_CMD_ALL_SEND_CID;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	memcpy(mmc->cid, cmd.response, 16);

	// For MMC cards, set the Relative Address.
	// For SD cards, get the Relative Address.
	// This also puts the cards into Standby State
	cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
	cmd.cmdarg = mmc->rca << 16;
	cmd.resp_type = MMC_RSP_R6;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	if (IS_SD(mmc))
		mmc->rca = (cmd.response[0] >> 16) & 0xffff;

	// Get the Card-Specific Data
	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	mmc->csd[0] = cmd.response[0];
	mmc->csd[1] = cmd.response[1];
	mmc->csd[2] = cmd.response[2];
	mmc->csd[3] = cmd.response[3];

	if (mmc->version == MMC_VERSION_UNKNOWN) {
		int version = (cmd.response[0] >> 26) & 0xf;

		switch (version) {
			case 0:
				mmc->version = MMC_VERSION_1_2;
				break;
			case 1:
				mmc->version = MMC_VERSION_1_4;
				break;
			case 2:
				mmc->version = MMC_VERSION_2_2;
				break;
			case 3:
				mmc->version = MMC_VERSION_3;
				break;
			case 4:
				mmc->version = MMC_VERSION_4;
				break;
			default:
				mmc->version = MMC_VERSION_1_2;
				break;
		}
	}

	// divide frequency by 10, since the mults are 10x bigger
	freq = fbase[(cmd.response[0] & 0x7)];
	mult = multipliers[((cmd.response[0] >> 3) & 0xf)];

	mmc->tran_speed = freq * mult;
	mmc->read_bl_len = 1 << ((cmd.response[1] >> 16) & 0xf);

	if (IS_SD(mmc))
		mmc->write_bl_len = mmc->read_bl_len;
	else
		mmc->write_bl_len = 1 << ((cmd.response[3] >> 22) & 0xf);

	// Select the card, and put it into Transfer Mode
	cmd.cmdidx = MMC_CMD_SELECT_CARD;
	cmd.resp_type = MMC_RSP_R1b;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;
	
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	if(IS_SD(mmc)) {
		if (mmc->high_capacity) {
			csize = (mmc->csd[1] & 0x3f) << 16 | (mmc->csd[2] & 0xffff0000) >> 16;
			cmult = 8;
		} else {
			csize = (mmc->csd[1] & 0x3ff) << 2 | (mmc->csd[2] & 0xc0000000) >> 30;
			cmult = (mmc->csd[2] & 0x00038000) >> 15;
		}
		mmc->capacity = (csize + 1) << (cmult + 2);
		mmc->capacity *= mmc->read_bl_len;
	} else {
		csize = (mmc->csd[1] & 0x3ff) << 2 | (mmc->csd[2] & 0xc0000000) >> 30;

		if (csize == 0xFFF) {
			// Card capacity is > 2GB. Use SEC_COUNT
			// to find capacity.
			err = mmc_send_ext_csd(mmc, ext_csd);
			if (err)
				return err;

			sec_count = (ext_csd[215] << 24) |
						(ext_csd[214] << 16) |
						(ext_csd[213] << 8)  |
						(ext_csd[212]);
						
			mmc->capacity = sec_count * 512;
		} else {
			// Card capacity is <= 2GB.
			// Use csize and cmult to find capacity.
			cmult = (mmc->csd[2] & 0x00038000) >> 15;
			mmc->capacity = (csize + 1) << (cmult + 2);
			mmc->capacity *= mmc->read_bl_len;
		}
	}

	if (mmc->read_bl_len > 512)
		mmc->read_bl_len = 512;

	if (mmc->write_bl_len > 512)
		mmc->write_bl_len = 512;

	if (IS_SD(mmc))
		err = sd_change_freq(mmc);
	else
		err = mmc_change_freq(mmc);

	if (err)
		return err;

	// Restrict card's capabilities by what the host can do
	mmc->card_caps &= mmc->host_caps;

	if (IS_SD(mmc)) {
		if (mmc->card_caps & MMC_MODE_4BIT) {
			cmd.cmdidx = MMC_CMD_APP_CMD;
			cmd.resp_type = MMC_RSP_R1;
			cmd.cmdarg = mmc->rca << 16;
			cmd.flags = 0;

			err = mmc_send_cmd(mmc, &cmd, NULL);
			if (err)
				return err;

			cmd.cmdidx = SD_CMD_APP_SET_BUS_WIDTH;
			cmd.resp_type = MMC_RSP_R1;
			cmd.cmdarg = 2;
			cmd.flags = 0;
			
			err = mmc_send_cmd(mmc, &cmd, NULL);
			if (err)
				return err;

			mmc_set_bus_width(mmc, 4);
		}

		if (mmc->card_caps & MMC_MODE_HS)
			mmc_set_clock(mmc, 50000000);
		else
			mmc_set_clock(mmc, 25000000);
	} else {
		if (mmc->card_caps & MMC_MODE_4BIT) {
			// Set the card to use 4 bit
			err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_WIDTH_4);
			if (err)
				return err;

			mmc_set_bus_width(mmc, 4);
		} else if (mmc->card_caps & MMC_MODE_8BIT) {
			// Set the card to use 8 bit
			err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_WIDTH_8);
			if (err)
				return err;

			mmc_set_bus_width(mmc, 8);
		}

		if (mmc->card_caps & MMC_MODE_HS) {
			if (mmc->card_caps & MMC_MODE_HS_52MHz)
				mmc_set_clock(mmc, 52000000);
			else
				mmc_set_clock(mmc, 26000000);
		} else {
			mmc_set_clock(mmc, 20000000);
		}
	}

	// fill in device description
	mmc->block_dev.lun = 0;
	mmc->block_dev.type = 0;
	mmc->block_dev.blksz = mmc->read_bl_len;
	mmc->block_dev.lba = lldiv(mmc->capacity, mmc->read_bl_len);
	sprintf(mmc->block_dev.vendor, "Man %06x Snr %08x", mmc->cid[0] >> 8,
			(mmc->cid[2] << 8) | (mmc->cid[3] >> 24));
	sprintf(mmc->block_dev.product, "%c%c%c%c%c", mmc->cid[0] & 0xff,
			(mmc->cid[1] >> 24), (mmc->cid[1] >> 16) & 0xff,
			(mmc->cid[1] >> 8) & 0xff, mmc->cid[1] & 0xff);
	sprintf(mmc->block_dev.revision, "%d.%d", mmc->cid[2] >> 28,
			(mmc->cid[2] >> 24) & 0xf);
	mmc->block_dev.part_type = PART_TYPE_DOS;
	mmc_ready = 1;
	
	return 0;
}

int mmc_send_if_cond(mmc_t *mmc)
{
	int err;
	mmc_cmd_t cmd;

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	// We set the bit if the host supports voltages between 2.7 and 3.6 V
	cmd.cmdarg = ((mmc->voltages & 0xff8000) != 0) << 8 | 0xaa;
	cmd.resp_type = MMC_RSP_R7;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	if ((cmd.response[0] & 0xff) != 0xaa)
		return UNUSABLE_ERR;
	else
		mmc->version = SD_VERSION_2;

	return 0;
}

int mmc_register(mmc_t *mmc)
{	
	// Setup the universal parts of the block interface just once
	mmc->block_dev.if_type = IF_TYPE_MMC;
	mmc->block_dev.dev = cur_dev_num++;
	mmc->block_dev.removable = 1;
	mmc->block_dev.block_read = _mmc_bread;
	mmc->block_dev.block_write = _mmc_bwrite;
	
	list_initialize(&mmc_devices);
	list_add_head(&mmc_devices, &mmc->link);

	return 0;
}

block_dev_desc_t *_mmc_get_dev(int dev)
{
	mmc_t *mmc = /*&htcleo_mmc;*/find_mmc_device(dev);

	return mmc ? &mmc->block_dev : NULL;
}

int mmc_init(mmc_t *mmc)
{
	int err;

	err = mmc->init(mmc);
	if (err)
		return err;

	mmc_set_bus_width(mmc, 1);
	mmc_set_clock(mmc, 1);

	// Reset the Card
	err = mmc_go_idle(mmc);
	if (err)
		return err;

	// Test for SD version 2
	err = mmc_send_if_cond(mmc);
	
	// Now try to get the SD card's operating condition
	err = sd_send_op_cond(mmc);

	// If the command timed out, we check for an MMC card
	if (err == TIMEOUT) {
		err = mmc_send_op_cond(mmc);
		if (err) {
			printf("Card did not respond to voltage select!\n");
			return UNUSABLE_ERR;
		}
	}

	return mmc_startup(mmc);
}
