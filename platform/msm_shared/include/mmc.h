/*
 * mmc.h 
 * SD/MMC definitions
 *
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora Forum nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 * 
 * Alternatively, provided that this notice is retained in full, this software
 * may be relicensed by the recipient under the terms of the GNU General Public
 * License version 2 ("GPL") and only version 2, in which case the provisions of
 * the GPL apply INSTEAD OF those given above.  If the recipient relicenses the
 * software under the GPL, then the identification text in the MODULE_LICENSE
 * macro must be changed to reflect "GPLv2" instead of "Dual BSD/GPL".  Once a 
 * recipient changes the license terms to the GPL, subsequent recipients shall
 * not relicense under alternate licensing terms, including the BSD or dual
 * BSD/GPL terms.  In addition, the following license statement immediately
 * below and between the words START and END shall also then apply when this
 * software is relicensed under the GPL:
 * 
 * START
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 * 
 * END
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __MMC_H
#define __MMC_H

#include <list.h>
#include <lib/part.h>

// Choose the SD controller to use. SDC1, 2, 3, or 4.
#define SDC_INSTANCE  2

#define CONFIG_MMC
#define CONFIG_DOS_PARTITION
#define CONFIG_GENERIC_MMC_MULTI_BLOCK_READ

#define USE_PROC_COMM
#define USE_DM
#define USE_HIGH_SPEED_MODE
#define USE_4_BIT_BUS_MODE

#define BLOCK_SIZE      512
#define SDCC_FIFO_SIZE  64
#define ROWS_PER_BLOCK  (BLOCK_SIZE / SDCC_FIFO_SIZE)

#define BUS_WIDTH_1     0
#define BUS_WIDTH_4     2
#define BUS_WIDTH_8     3

#define R1_ILLEGAL_COMMAND			(1 << 22)
#define R1_APP_CMD					(1 << 5)

#define MMC_RSP_PRESENT (1 << 0)
#define MMC_RSP_136     (1 << 1) /* 136 bit response */
#define MMC_RSP_CRC     (1 << 2) /* expect valid crc */
#define MMC_RSP_BUSY    (1 << 3) /* card may send busy */
#define MMC_RSP_OPCODE  (1 << 4) /* response contains opcode */
#define MMC_RSP_NONE    (0)
#define MMC_RSP_R1      (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R1b		(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE|MMC_RSP_BUSY)
#define MMC_RSP_R2      (MMC_RSP_PRESENT|MMC_RSP_136|MMC_RSP_CRC)
#define MMC_RSP_R3      (MMC_RSP_PRESENT)
#define MMC_RSP_R4      (MMC_RSP_PRESENT)
#define MMC_RSP_R5      (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R6      (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R7      (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)

#define MMC_DATA_READ		1
#define MMC_DATA_WRITE		2

// HCLK_FREQ is only used to set timeout values.  This is not critical,
// as higher HCLK_FREQ will simply create a larger timeout count.
#define HCLK_FREQ         20e6
#define WR_DATA_TIMEOUT   (int)(HCLK_FREQ * 250e-3)
#define RD_DATA_TIMEOUT   (int)(HCLK_FREQ * 100e-3)

#define CMD0    0x00    // GO_IDLE_STATE
#define CMD1    0x01    // SEND_OP_COND
#define CMD2    0x02    // ALL_SEND_CID
#define CMD3    0x03    // SET_RELATIVE_ADDR
#define CMD4    0x04    // SET_DSR
#define CMD6    0x06    // SWITCH_FUNC
#define CMD7    0x07    // SELECT/DESELECT_CARD
#define CMD8    0x08    // SEND_IF_COND
#define CMD9    0x09    // SEND_CSD
#define CMD10   0x0A    // SEND_CID
#define CMD11   0x0B    // READ_DAT_UNTIL_STOP
#define CMD12   0x0C    // STOP_TRANSMISSION
#define CMD13   0x0D    // SEND_STATUS
#define CMD15   0x0F    // GO_INACTIVE_STATE
#define CMD16   0x10    // SET_BLOCKLEN
#define CMD17   0x11    // READ_SINGLE_BLOCK
#define CMD18   0x12    // READ_MULTIPLE_BLOCK
#define CMD20   0x14    // WRITE_DAT_UNTIL_STOP
#define CMD24   0x18    // WRITE_BLOCK
#define CMD25   0x19    // WRITE_MULTIPLE_BLOCK
#define CMD26   0x1A    // PROGRAM_CID
#define CMD27   0x1B    // PROGRAM_CSD
#define CMD28   0x1C    // SET_WRITE_PROT
#define CMD29   0x1D    // CLR_WRITE_PROT
#define CMD30   0x1E    // SEND_WRITE_PROT
#define CMD32   0x20    // TAG_SECTOR_START
#define CMD33   0x21    // TAG_SECTOR_END
#define CMD34   0x22    // UNTAG_SECTOR
#define CMD35   0x23    // TAG_ERASE_GROUP_START
#define CMD36   0x24    // TAG_ERASE_GROUP_END
#define CMD37   0x25    // UNTAG_ERASE_GROUP
#define CMD38   0x26    // ERASE
#define CMD42   0x2A    // LOCK_UNLOCK
#define CMD55   0x37    // APP_CMD
#define CMD56   0x38    // GEN_CMD
#define ACMD6   0x06    // SET_BUS_WIDTH
#define ACMD13  0x0D    // SD_STATUS
#define ACMD22  0x16    // SET_NUM_WR_BLOCKS
#define ACMD23  0x17    // SET_WR_BLK_ERASE_COUNT
#define ACMD41  0x29    // SD_APP_OP_COND
#define ACMD42  0x2A    // SET_CLR_CARD_DETECT
#define ACMD51  0x33    // SEND_SCR
#define CMD5    0x05    // IO_SEND_OP_COND
#define CMD52   0x34    // IO_RW_DIRECT
#define CMD53   0x35    // IO_RW_EXTENDED

// SD controller register offsets
#define MCI_POWER               (0x000)
#define MCI_CLK                 (0x004)
#define MCI_ARGUMENT            (0x008)
#define MCI_CMD                 (0x00C)
#define MCI_RESP_CMD            (0x010)
#define MCI_RESPn(n)            (0x014+4*n)
#define MCI_RESP0               (0x014)
#define MCI_RESP1               (0x018)
#define MCI_RESP2               (0x01C)
#define MCI_RESP3               (0x020)
#define MCI_DATA_TIMER          (0x024)
#define MCI_DATA_LENGTH         (0x028)
#define MCI_DATA_CTL            (0x02C)
#define MCI_DATA_COUNT          (0x030)
#define MCI_STATUS              (0x034)
#define MCI_CLEAR               (0x038)
#define MCI_INT_MASKn(n)        (0x03C+4*n)
#define MCI_INT_MASK0           (0x03C)
#define MCI_INT_MASK1           (0x040)
#define MCI_FIFO_COUNT          (0x044)
#define MCI_CCS_TIMER           (0x058)
#define MCI_FIFO                (0x080)
#define MCI_TESTBUS_CONFIG      (0x0CC)
#define MCI_TEST_CTL            (0x0D0)
#define MCI_TEST_INPUT          (0x0D4)
#define MCI_TEST_OUT            (0x0D8)
#define MCI_PERPH_ID0           (0x0E0)
#define MCI_PERPH_ID1           (0x0E4)
#define MCI_PERPH_ID2           (0x0E8)
#define MCI_PERPH_ID3           (0x0EC)
#define MCI_PCELL_ID0           (0x0F0)
#define MCI_PCELL_ID1           (0x0F4)
#define MCI_PCELL_ID2           (0x0F8)
#define MCI_PCELL_ID3           (0x0FC)

// SD MCLK definitions
#ifdef USE_PROC_COMM
/* the desired duty cycle is 50%,
 * using proc_comm with 45Mhz possibly giving too low duty cycle,
 * breaking it.
 */
enum SD_MCLK_speed
{	MCLK_144KHz = 144000,
	MCLK_400KHz = 400000,
	MCLK_25MHz = 25000000,
	MCLK_40MHz = 40000000,
	MCLK_48MHz = 49152000, /* true 48Mhz not supported, use next highest */
	MCLK_49MHz = 49152000,
	MCLK_50MHz = 50000000
};
#else /*USE_PROC_COMM defined*/
enum SD_MCLK_speed
{
	MCLK_400KHz = 400000,
	MCLK_16MHz  = 16000000,
	MCLK_17MHz  = 17000000,
	MCLK_20MHz  = 20200000,
	MCLK_24MHz  = 24000000,
	MCLK_25MHz  = 25000000,
	MCLK_48MHz  = 48000000,
	MCLK_50MHz  = 50000000
};
#endif /*USE_PROC_COMM */

// For MCLK 400KHz derived from TCXO (19.2MHz) (M=1, N=48, D=24, P=1)
#define MCLK_MD_400KHZ    0x000100CF
#define MCLK_NS_400KHZ    0x00D00B40   
// For MCLK 25MHz derived from PLL1 768MHz (M=14, N=215, D=107.5, P=2), dual edge mode
#define MCLK_MD_25MHZ     0x000E0028
#define MCLK_NS_25MHZ     0x00360B49
// For MCLK 48MHz derived from PLL1 768MHz (M=1, N=8, D=4, P=2)
#define MCLK_MD_48MHZ     0x000100F7
#define MCLK_NS_48MHZ     0x00F80B49
// For MCLK 50MHz derived from PLL1 768MHz (M=25, N=192, D=96, P=2)
#define MCLK_MD_50MHZ     0x0019003F
#define MCLK_NS_50MHZ     0x00580B49

#define MMC_MODE_HS			0x001
#define MMC_MODE_HS_52MHz	0x010
#define MMC_MODE_4BIT		0x100
#define MMC_MODE_8BIT		0x200

// 2.7-3.6
#define SDCC_VOLTAGE_SUPPORTED 0x00FF8000;
// 2.3-2.4 and  2.7-3.6
//#define SDCC_VOLTAGE_SUPPORTED 0x00FF8080;

// ADM Command Pointer List Entry definitions
#define ADM_CMD_PTR_LP          0x80000000    // Last pointer
#define ADM_CMD_PTR_CMD_LIST    (0 << 29)     // Command List
// ADM Command List definitions (First Word)
#define ADM_CMD_LIST_LC         0x80000000    // Last command
#define ADM_ADDR_MODE_BOX       (3 << 0)      // Box address mode

#define	Byte_swap32(value)  ( ((value >>24) & 0x000000ff) \
                            | ((value >> 8) & 0x0000ff00) \
			                | ((value << 8) & 0x00ff0000) \
			                | ((value <<24) & 0xff000000) \
							)

#define SD_VERSION_SD		0x20000
#define SD_VERSION_2		(SD_VERSION_SD | 0x20)
#define SD_VERSION_1_0		(SD_VERSION_SD | 0x10)
#define SD_VERSION_1_10		(SD_VERSION_SD | 0x1a)

#define MMC_VERSION_MMC		0x10000
#define MMC_VERSION_UNKNOWN	(MMC_VERSION_MMC)
#define MMC_VERSION_1_2		(MMC_VERSION_MMC | 0x12)
#define MMC_VERSION_1_4		(MMC_VERSION_MMC | 0x14)
#define MMC_VERSION_2_2		(MMC_VERSION_MMC | 0x22)
#define MMC_VERSION_3		(MMC_VERSION_MMC | 0x30)
#define MMC_VERSION_4		(MMC_VERSION_MMC | 0x40)

#define IS_SD(x) 			(x->version & SD_VERSION_SD)

#define SD_DATA_4BIT		0x00040000

#define MMC_DATA_READ		1
#define MMC_DATA_WRITE		2

#define SDCC_ERR_GENERIC        	-1
#define SDCC_ERR_CRC_FAIL       	-2
#define SDCC_ERR_DATA_WRITE     	-3
#define SDCC_ERR_BLOCK_NUM      	-4
#define SDCC_ERR_RX_OVERRUN     	-5
#define SDCC_ERR_DATA_TIMEOUT   	-6
#define SDCC_ERR_DATA_CRC_FAIL  	-7
#define SDCC_ERR_DATA_ADM_ERR   	-8
#define SDCC_ERR_DATA_START_BIT 	-9
#define SDCC_ERR_INVALID_STATUS 	-10
#define SDCC_ERR_TIMEOUT        	-19
#define SDCC_ERR_NOT_IMPLEMENTED   	-111
#define NO_CARD_ERR					-16 /* No SD/MMC card inserted */
#define UNUSABLE_ERR				-17 /* Unusable Card */
#define COMM_ERR					-18 /* Communications Error */
#define TIMEOUT						-19

#define MMC_CMD_GO_IDLE_STATE			0
#define MMC_CMD_SEND_OP_COND			1
#define MMC_CMD_ALL_SEND_CID			2
#define MMC_CMD_SET_RELATIVE_ADDR		3
#define MMC_CMD_SET_DSR					4
#define MMC_CMD_SWITCH					6
#define MMC_CMD_SELECT_CARD				7
#define MMC_CMD_SEND_EXT_CSD			8
#define MMC_CMD_SEND_CSD				9
#define MMC_CMD_SEND_CID				10
#define MMC_CMD_STOP_TRANSMISSION		12
#define MMC_CMD_SEND_STATUS				13
#define MMC_CMD_SET_BLOCKLEN			16
#define MMC_CMD_READ_SINGLE_BLOCK		17
#define MMC_CMD_READ_MULTIPLE_BLOCK		18
#define MMC_CMD_WRITE_SINGLE_BLOCK		24
#define MMC_CMD_WRITE_MULTIPLE_BLOCK	25
#define MMC_CMD_APP_CMD					55

#define MMC_HS_TIMING				0x00000100
#define MMC_HS_52MHZ				0x2

#define OCR_BUSY					0x80000000
#define OCR_HCS						0x40000000

#define MMC_VDD_165_195				0x00000080	/* VDD voltage 1.65 - 1.95 */
#define MMC_VDD_20_21				0x00000100	/* VDD voltage 2.0 ~ 2.1 */
#define MMC_VDD_21_22				0x00000200	/* VDD voltage 2.1 ~ 2.2 */
#define MMC_VDD_22_23				0x00000400	/* VDD voltage 2.2 ~ 2.3 */
#define MMC_VDD_23_24				0x00000800	/* VDD voltage 2.3 ~ 2.4 */
#define MMC_VDD_24_25				0x00001000	/* VDD voltage 2.4 ~ 2.5 */
#define MMC_VDD_25_26				0x00002000	/* VDD voltage 2.5 ~ 2.6 */
#define MMC_VDD_26_27				0x00004000	/* VDD voltage 2.6 ~ 2.7 */
#define MMC_VDD_27_28				0x00008000	/* VDD voltage 2.7 ~ 2.8 */
#define MMC_VDD_28_29				0x00010000	/* VDD voltage 2.8 ~ 2.9 */
#define MMC_VDD_29_30				0x00020000	/* VDD voltage 2.9 ~ 3.0 */
#define MMC_VDD_30_31				0x00040000	/* VDD voltage 3.0 ~ 3.1 */
#define MMC_VDD_31_32				0x00080000	/* VDD voltage 3.1 ~ 3.2 */
#define MMC_VDD_32_33				0x00100000	/* VDD voltage 3.2 ~ 3.3 */
#define MMC_VDD_33_34				0x00200000	/* VDD voltage 3.3 ~ 3.4 */
#define MMC_VDD_34_35				0x00400000	/* VDD voltage 3.4 ~ 3.5 */
#define MMC_VDD_35_36				0x00800000	/* VDD voltage 3.5 ~ 3.6 */

#define MMC_SWITCH_MODE_CMD_SET		0x00 /* Change the command set */
#define MMC_SWITCH_MODE_SET_BITS	0x01 /* Set bits in EXT_CSD byte addressed by index which are 1 in value field */
#define MMC_SWITCH_MODE_CLEAR_BITS	0x02 /* Clear bits in EXT_CSD byte addressed by index, which are 1 in value field */
#define MMC_SWITCH_MODE_WRITE_BYTE	0x03 /* Set target byte to value */

// EXT_CSD fields
#define EXT_CSD_BUS_WIDTH			183		/* R/W */
#define EXT_CSD_HS_TIMING			185		/* R/W */
#define EXT_CSD_CARD_TYPE			196		/* RO */
#define EXT_CSD_REV					192		/* RO */
#define EXT_CSD_SEC_CNT				212		/* RO, 4 bytes */
#define EXT_CSD_CMD_SET_NORMAL		(1<<0)
#define EXT_CSD_CMD_SET_SECURE		(1<<1)
#define EXT_CSD_CMD_SET_CPSECURE	(1<<2)
#define EXT_CSD_CARD_TYPE_26		(1<<0)	/* Card can run at 26MHz */
#define EXT_CSD_CARD_TYPE_52		(1<<1)	/* Card can run at 52MHz */
#define EXT_CSD_BUS_WIDTH_1			0		/* Card is in 1 bit mode */
#define EXT_CSD_BUS_WIDTH_4			1		/* Card is in 4 bit mode */
#define EXT_CSD_BUS_WIDTH_8			2		/* Card is in 8 bit mode */

#define SD_CMD_SEND_RELATIVE_ADDR	3
#define SD_CMD_SWITCH_FUNC			6
#define SD_CMD_SEND_IF_COND			8

#define SD_CMD_APP_SET_BUS_WIDTH	6
#define SD_CMD_APP_SEND_OP_COND		41
#define SD_CMD_APP_SEND_SCR			51

/* SCR definitions in different words */
#define SD_HIGHSPEED_BUSY			0x00020000
#define SD_HIGHSPEED_SUPPORTED		0x00020000
#define SD_SWITCH_CHECK				0
#define SD_SWITCH_SWITCH			1

typedef struct mmc_cmd {
	ushort cmdidx;
	uint resp_type;
	uint cmdarg;
	uint response[4];
	uint flags;
}mmc_cmd_t;

typedef struct mmc_data {
	char *dest;
	const char *src; /* src buffers don't get written to */
	uint flags;
	uint blocks;
	uint blocksize;
}mmc_data_t;

typedef struct mmc {
	struct list_node link;
	char name[32];
	void *priv;
	uint voltages;
	uint version;
	uint f_min;
	uint f_max;
	int high_capacity;
	uint bus_width;
	uint clock;
	uint card_caps;
	uint host_caps;
	uint ocr;
	uint scr[2];
	uint csd[4];
	uint cid[4];
	ushort rca;
	uint tran_speed;
	uint read_bl_len;
	uint write_bl_len;
	uint64_t capacity;
	block_dev_desc_t block_dev;
	int (*send_cmd)(struct mmc *mmc, mmc_cmd_t *cmd, mmc_data_t *data);
	void (*set_ios)(struct mmc *mmc);
	int (*init)(struct mmc *mmc);
}mmc_t;

typedef struct sd_parms {
   uint32_t instance;                  // which instance of the SD controller
   uint32_t base;                      // SD controller base address
   uint32_t ns_addr;                   // Clock controller NS reg address
   uint32_t md_addr;                   // Clock controller MD reg address
   uint32_t ns_initial;                // Clock controller NS reg initial value
   uint32_t md_initial;                // Clock controller MD reg initial value
   uint32_t row_reset_mask;            // Bit in the ROW reset register
   uint32_t glbl_clk_ena_mask;         // Bit in the global clock enable
   uint32_t glbl_clk_ena_initial;      // Initial value of the global clock enable bit                                
   uint32_t adm_crci_num;              // ADM CRCI number
   uint32_t adm_ch8_rslt_conf_initial; // Initial value of HI0_CH8_RSLT_CONF_SD3                                  
}sd_parms_t;

mmc_t htcleo_mmc;
sd_parms_t htcleo_sdcc;

int  mmc_ready; // Will be set to 1 if sdcard is ready

int  sdcc_init(mmc_t *mmc);
void sdcc_set_ios(mmc_t *mmc);
int  sdcc_send_cmd(mmc_t *mmc, mmc_cmd_t *cmd, mmc_data_t *data);

int   mmc_init(mmc_t *mmc);
int   mmc_register(mmc_t *mmc);
ulong mmc_bread(int dev_num, ulong blknr, ulong blkcnt, void *dst);

void SDCn_deinit(uint32_t instance);
int  mmc_legacy_init(int verbose);
void sdcard_gpio_config(int instance);

#endif /* __MMC_H */