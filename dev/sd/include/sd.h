/*
 * WIP (or wishful thinking): Placeholder for qsd8250b basic sd driver
 */

#ifndef __SD_H_
#define __SD_H_

#include <stdint.h>
#include <platform/iomap.h>

/***********************************************************************************
 * TODO: Add ALL missing definitions, etc.
 */

#define SD_MAX_RETRY_COUNT   		10

/***********************************************************************************
 * Commands , responses , structures
 */

/* Command Types:
 * bc   = Broadcast commands
 * bcr  = Broadcast commands with response
 * ac   = Addressed commands - no data transfer on DAT
 * adtc = Addressed data transfer commands - data transfer on DAT
 */

#define CMD_BC				(1 << 0)
#define CMD_BCR				(1 << 1)
#define CMD_AC				(1 << 2)
#define CMD_ADTC			(1 << 3)

/* Common command format:
 * [47]		Start bit:0
 * [46]		Transmission bit:1
 * [45:40]	Command index
 * [39:8]	Argument
 * [7:1]	CRC7
 * [0]		End bit:1
 */

/*        Command        */	    /*INDEX*/	/*RESP*/	/*TYPE*/
#define CMD0_GO_IDLE_STATE		0	/*NONE*/	/* bc */
/* CMD0 Description:
 * Resets all cards to idle state
 *
 * CMD0 Argument:
 * [31:0]	stuff bits
 */

#define CMD1_SEND_OP_COND               1	/* R3 */	/*bcr */
/* CMD1 Description:
 * Checks whether the card supports
 * host VDD profile or not
 *
 * CMD1 format:
 * [31]		busy bit
 * [30:29]	access mode
 * [28:24]	reserved
 * [23:15]	2.7-3.6
 * [14:8]	2.0-2.6
 * [7]		1.7-1.95
 * [6:0]	reserved
 */

#define CMD2_ALL_SEND_CID		2	/* R2 */	/*bcr */
/* CMD2 Description:
 * Asks any card to send the CID
 * numbers
 *
 * CMD2 Argument:
 * [31:0]	stuff bits
 */

#define CMD3_SEND_RELATIVE_ADDR		3	/* R6 */	/*bcr */
/* CMD3 Description:
 * Asks the card to publish a new
 * relative address - RCA
 *
 * CMD3 Argument:
 * [31:0]	stuff bits
 */

#define CMD4_SET_DSR			4	/*NONE*/	/* bc */
/* CMD4 Description:
 * Programs the DSR of all cards
 *
 * CMD4 Argument:
 * [31:16]	DSR
 * [15:0]	stuff bits
 */

#define CMD6_SWITCH_FUNC		6	/* R1 */	/*adtc*/
/* CMD6 Description:
 * Checks switchable function(mode 0)
 * and switch card function(mode 1)
 *
 * CMD6 Argument:
 * [31]		mode
 * [30:24]	reserved bits
 * [23:20]	reserved for function group 6
 * [19:16]	reserved for function group 5
 * [15:12]	function group 4 for current limit
 * [11:8]	function group 3 for drive strength
 * [7:4]	function group 2 for command system
 * [3:0]	function group 1 for access mode
 */

#define CMD7_SELECT_DESELECT_CARD	7	/* R1b*/	/* ac */
/* CMD7 Description:
 * Toggles a card between
 * (stand-by & transfer) OR
 * (programming & disconnect) states
 *
 * CMD7 Argument:
 * [31:16]	RCA
 * [15:0]	stuff bits
 */

#define CMD8_SEND_IF_COND               8	/* R7 */	/*bcr */
/* CMD8 Description:
 * Sends card interface condition
 * host supply voltage,
 * asks card if supports voltage
 *
 * CMD8 Format:
 * [47]		start bit:0
 * [46]		transmission bit:1
 * [45:40]	command index
 * [39:20]	reserved bits
 * [19:16]	VHS
 * [15:8]	check pattern
 * [7:1]	CRC7
 * [0]		end bit:1
 */

#define CMD9_SEND_CSD			9	/* R2 */	/* ac */
/* CMD9 Description:
 * Addressed card sends card
 * specific data - CSD
 *
 * CMD9 Argument:
 * [31:16]	RCA
 * [15:0]	stuff bits
 */

#define CMD10_SEND_CID			10	/* R2 */	/* ac */
/* CMD10 Description:
 * Addressed card sends card
 * identification - CID
 *
 * CMD10 Argument:
 * [31:16]	RCA
 * [15:0]	stuff bits
 */

#define CMD12_STOP_TRANSMISSION		12	/* R1b*/	/* ac */
/* CMD12 Description:
 * Forces the card to stop
 * transmission
 *
 * CMD12 Argument:
 * [31:0]	stuff bits
 */

#define CMD13_SEND_STATUS		13	/* R1 */	/* ac */
/* CMD13 Description:
 * Addressed card sends
 * its status register
 *
 * CMD13 Argument:
 * [31:16]	RCA
 * [15:0]	stuff bits
 */

#define CMD15_GO_INACTIVE_STATE		15	/*NONE*/	/* ac */
/* CMD15 Description:
 * Sends the card into inactive state
 *
 * CMD15 Argument:
 * [31:16]	RCA
 * [15:0]	reserved bits
 */

#define CMD16_SET_BLOCKLEN		16	/* R1 */	/* ac */
/* CMD16 Description:
 * SDSC: sets(read,write,lock)block cmds
 * SDHC: sets(lock)block cmd
 *
 * CMD16 Argument:
 * [31:0]	block length
 */

#define CMD17_READ_SINGLE_BLOCK		17	/* R1 */	/*adtc*/
/* CMD17 Description:
 * Reads a block
 *
 * CMD17 Argument:
 * [31:0]	data address
 */

#define CMD18_READ_MULTIPLE_BLOCK	18	/* R1 */	/*adtc*/
/* CMD18 Description:
 * Continuously reads blocks of data
 * till interrupted by
 * CMD12_STOP_TRANSMISSION
 *
 * CMD18 Argument:
 * [31:0]	data address
 */

#define CMD24_WRITE_BLOCK		24	/* R1 */	/*adtc*/
/* CMD24 Description:
 * Writes a block
 *
 * CMD24 Argument:
 * [31:0]	data address
 */

#define CMD25_WRITE_MULTIPLE_BLOCK	25	/* R1 */	/*adtc*/
/* CMD25 Description:
 * Continuously writes blocks of data
 * till interrupted by
 * CMD12_STOP_TRANSMISSION
 *
 * CMD25 Argument:
 * [31:0]	data address
 */

#define CMD27_PROGRAM_CSD		27	/* R1 */	/*adtc*/
/* CMD27 Description:
 * Programming of the programmable
 * bits of the CSD
 *
 * CMD27 Argument:
 * [31:0]	stuff bits
 */

#define CMD28_SET_WRITE_PROT		28	/* R1b*/	/* ac */
/* CMD28 Description:
 * Sets the write protection bit if the
 * card has write protection features
 *
 * CMD28 Argument:
 * [31:0]	data address
 */

#define CMD29_CLR_WRITE_PROT		29	/* R1b*/	/* ac */
/* CMD29 Description:
 * Clears the write protection bit if the
 * card has write protection features
 *
 * CMD29 Argument:
 * [31:0]	data address
 */

#define CMD30_SEND_WRITE_PROT		30	/* R1 */	/*adtc*/
/* CMD30 Description:
 * Asks for the status of write
 * protection bit if the card has
 * write protection features
 *
 * CMD30 Argument:
 * [31:0]	write protect data address
 */

#define CMD32_ERASE_WR_BLK_START	32	/* R1 */	/* ac */
/* CMD32 Description:
 * Sets the address of the first
 * write block to be erased
 *
 * CMD32 Argument:
 * [31:0]	data address
 */

#define CMD33_ERASE_WR_BLK_END		33	/* R1 */	/* ac */
/* CMD33 Description:
 * Sets the address of the last write
 * block of the range to be erased
 *
 * CMD33 Argument:
 * [31:0]	data address
 */

#define CMD38_ERASE			38	/* R1b*/	/* ac */
/* CMD38 Description:
 * Erases all previously selected
 * write blocks
 *
 * CMD38 Argument:
 * [31:0]	stuff bits
 */

#define ACMD6_SET_BUS_WIDTH		6	/* R1 */	/* ac */
/* ACMD6 Description:
 * Defines the data bus width to be
 * used for data transfer
 *
 * ACMD6 Argument:
 * [31:2]	stuff bits
 * [1:0]	bus width
 */

#define ACMD13_SD_STATUS		6	/* R1 */	/*adtc*/
/* ACMD13 Description:
 * Send the SD Status
 *
 * ACMD13 Argument:
 * [31:0]	stuff bits
 */

#define ACMD22_SEND_NUM_WR_BLOCKS	22	/* R1 */	/*adtc*/
/* ACMD22 Description:
 * Send the number of the written
 * write blocks
 *
 * ACMD22 Argument:
 * [31:0]	stuff bits
 */

#define ACMD23_SET_WR_BLK_ERASE_COUNT	23	/* R1 */	/* ac */
/* ACMD23 Description:
 * Set the number of write blocks
 * to be erased before writing
 *
 * ACMD23 Argument:
 * [31:23]	stuff bits
 * [22:0]	number of blocks
 */

#define ACMD41_SD_APP_OP_COND		41	/* R3 */	/*bcr */
/* ACMD41 Description:
 * Sends host capacity support info(HCS)
 * and asks the card for its
 * operation condition register(OCR)
 *
 * ACMD41 Argument:
 * [31]		reserved bit
 * [30]		HCS(OCR[30])
 * [29]		reserved for eSD
 * [28]		XPC
 * [27:25]	reserved bits
 * [24]		S18R
 * [23:0]	VDD Voltage Window(OCR[23:0])
 */

#define ACMD42_SET_CLR_CARD_DETECT	42	/* R1 */	/* ac */
/* ACMD42 Description:
 * Connect/Disconnect the 50 KOhm
 * pull-up registor on CD/DAT3
 * of the card
 *
 * ACMD42 Argument:
 * [31:1]	stuff bits
 * [0]		set_cd
 */

#define ACMD51_SEND_SCR			51	/* R1 */	/*adtc*/
/* ACMD51 Description:
 * Reads the SD
 * configuration register(SCR)
 *
 * ACMD51 Argument:
 * [31:0]	stuff bits
 */

#define CMD55_APP_CMD			55	/* R1 */	/* ac */
/* CMD55 Description:
 * Indicates to the card that the next
 * command is an application specific
 * command - not a standard command
 *
 * CMD55 Argument:
 * [31:16]	RCA
 * [15:0]	stuff bits
 */

#define CMD56_GEN_CMD			56	/* R1 */	/*adtc*/
/* CMD56 Description:
 * Transfer/get a block of data to/from
 * the card for general purpose commands
 *
 * CMD56 Argument:
 * [31:1]	stuff bits
 * [0]		RD/WR
 */

/* Response types */
#define SD_RESP_NONE			0
#define SD_RESP_R1                  	(1 << 0)
/* [47]		start bit:0
 * [46]		transmission bit:0
 * [45:40]	command index
 * [39:8]	card status
 * [7:1]	CRC7
 * [0]		end bit:1
*/
#define SD_RESP_R1B                 	(1 << 1)
/* 
 * Identical to R1 + optional busy signal on data line
 */
#define SD_RESP_R2                  	(1 << 2)
/* [135]	start bit:0
 * [134]	transmission bit:0
 * [133:128]	reserved bits:111111
 * [127:1]	CID or CSD including CRC7
 * [0]		end bit:1
*/
#define SD_RESP_R3                  	(1 << 3)
/* [47]		start bit:0
 * [46]		transmission bit:0
 * [45:40]	reserved bits:111111
 * [39:8]	OCR
 * [7:1]	reserved bits:111111
 * [0]		end bit:1
*/
#define SD_RESP_R6                  	(1 << 6)
/* [47]		start bit:0
 * [46]		transmission bit:0
 * [45:40]	command index
 * [39:24]	RCA
 * [23:8]	card status bits
 * [7:1]	CRC7
 * [0]		end bit:1
*/
#define SD_RESP_R7                  	(1 << 7)
/* [47]		start bit:0
 * [46]		transmission bit:0
 * [45:40]	command index
 * [39:20]	reserved bits:00000h
 * [19:16]	voltage accepted:{0000b , 0001b , 0010b , 0100b , 1000b , others}
 * [15:8]	echo-back of check pattern
 * [7:1]	CRC7
 * [0]		end bit:1
*/

/* Macros for Common Errors */
#define SD_STATUS_SUCCESS     		0
#define SD_STATUS_LOAD_ERROR  		1
#define SD_STATUS_TIMEOUT     		2
#define SD_ERROR_DEVICE            	3
#define SD_ERROR_UNSUPPORTED       	4
#define SD_ERROR_NO_MEDIA          	5
#define SD_ERROR_INVALID_PARAMETER 	6
#define SD_ERROR_BAD_BUFFER_SIZE   	7

/* Card Status bits (R1 register) */
#define CS_AKE_SEQ_ERROR         	(1 << 3)
#define CS_APP_CMD               	(1 << 5)
#define CS_READY_FOR_DATA          	(1 << 8)
#define CS_CURRENT_STATE_IDLE       	(0 << 9)
#define CS_CURRENT_STATE_READY        	(1 << 9)
#define CS_CURRENT_STATE_IDENT      	(2 << 9)
#define CS_CURRENT_STATE_STBY       	(3 << 9)
#define CS_CURRENT_STATE_TRAN       	(4 << 9)
#define CS_CURRENT_STATE_DATA      	(5 << 9)
#define CS_CURRENT_STATE_RCV        	(6 << 9)
#define CS_CURRENT_STATE_PRG        	(7 << 9)
#define CS_CURRENT_STATE_DIS        	(8 << 9)
#define CS_ERASE_RESET           	(1 << 13)
#define CS_CARD_ECC_DISABLED     	(1 << 14)
#define CS_WP_ERASE_SKIP         	(1 << 15)
#define CS_CSD_OVERWRITE		(1 << 16)
#define CS_ERROR			(1 << 19)
#define CS_CC_ERROR              	(1 << 20)
#define CS_CARD_ECC_FAILED       	(1 << 21)
#define CS_ILLEGAL_COMMAND           	(1 << 22)
#define CS_COM_CRC_ERROR           	(1 << 23)
#define CS_LOCK_UNLOCK_FAILED      	(1 << 24)
#define CS_CARD_IS_LOCKED        	(1 << 25)
#define CS_WP_VIOLATION          	(1 << 26)
#define CS_ERASE_PARAM           	(1 << 27)
#define CS_ERASE_SEQ_ERROR         	(1 << 28)
#define CS_BLOCK_LEN_ERROR         	(1 << 29)
#define CS_ADDRESS_ERROR              	(1 << 30)
#define CS_OUT_OF_RANGE          	(1 << 31)

typedef struct{ /* SD Status bits (data block of 512 bit) - response to ACMD13 */
	uint32_t  DAT_BUS_WIDTH;		// Currently defined data bus width [511:510]
	uint32_t  SECURED_MODE;			// Card in Secured Mode of operation [509]
	uint32_t  RESERVED_5;			// Reserved for security functions [508:502]
	uint32_t  RESERVED_4;			// Reserved [501:496]
	uint32_t  SD_CARD_TYPE;			// Different variations of an SD Card [495:480]
	uint32_t  SIZE_OF_PROTECTED_AREA;	// SDSC: Protected Area = SIZE_OF_PROTECTED_AREA*MULT*BLOCK_LEN [479:448]
						// SDHC: Protected Area = SIZE_OF_PROTECTED_AREA [479:448]
	uint32_t  SPEED_CLASS;			// Class of SD Card [447:440]
	uint32_t  PERFORMANCE_MOVE;		// PM (can be set by 1 MB/sec step) [439:432]
	uint32_t  AU_SIZE;			// AU size [431:428]
	uint32_t  RESERVED_3;			// Reserved [427:424]
	uint32_t  ERASE_SIZE;			// N_erase [423:408]
	uint32_t  ERASE_TIMEOUT;		// T_erase [407:402]
	uint32_t  ERASE_OFFSET;			// T_offset [401:400]
	uint32_t  UHS_SPEED_GRADE; 		// UHS mode speed grade [399:396]
	uint32_t  UHS_AU_SIZE; 			// AU size for UHS card [395:392]
	uint64_t  RESERVED_2;			// Reserved [391:312]
	uint64_t  RESERVED_1;			// Reserved for manufacturer [311:0]
}SD_STATUS;

typedef struct{
	uint32_t  RESERVED_3:7;		// Reserved
	uint32_t  V170_V270:1;		// Reserved for Low Voltage Range
	uint32_t  RESERVED_2:7;		// Reserved
	uint32_t  V270_V280:1;		// 2.70V - 2.80V
	uint32_t  V280_V290:1;		// 2.80V - 2.90V
	uint32_t  V290_V300:1;		// 2.90V - 3.00V
	uint32_t  V300_V310:1;		// 3.00V - 3.10V
	uint32_t  V310_V320:1;		// 3.10V - 3.20V
	uint32_t  V320_V330:1;		// 3.20V - 3.30V
	uint32_t  V330_V340:1;		// 3.30V - 3.40V
	uint32_t  V340_V350:1;		// 3.40V - 3.50V
	uint32_t  V350_V360:1;		// 3.50V - 3.60V
	uint32_t  RESERVED_1:5; 	// Reserved
	uint32_t  CCS:1; 		// Card Capacity Status
	uint32_t  Busy:1;		// Card power up status bit
}SD_OCR;

typedef struct{
	uint8_t   MID;        		// Manufacturer ID [127:120]
	uint16_t  OID;        		// OEM/Application ID [119:104]
	uint64_t  PNM[5];     		// Product name [103:64]
	uint8_t   PRV;        		// Product revision [63:56]
	uint32_t  PSN;        		// Product serial number [55:24]
	uint32_t  RESERVED_1; 		// Reserved [23:20]
	uint32_t  MDT;        		// Manufacturing date [19:8]
	uint8_t   CRC;        		// CRC7 checksum [7:1]
	uint32_t  NOT_USED;   		// 1 [0:0]
}SD_CID;

typedef struct{
	uint32_t  SCR_STRUCTURE:4;  		// SCR structure [63:60]
	uint32_t  SD_SPEC:4;        		// SD Memory Card Spec. Version [59:56]
	uint32_t  DATA_STAT_AFTER_ERASE:1;	// Data status after erases [55:55]
	uint32_t  SD_SECURITY:3;     		// CPRM Security Support [54:52]
	uint32_t  SD_BUS_WIDTHS:4;     		// DAT Bus widths supported [51:48]
	uint32_t  SD_SPEC3:1;        		// Spec. Version 3.00 or higher [47]
	uint32_t  EX_SECURITY:4;        	// Extended Security Support [46:43]
	uint32_t  RESERVED_2:9; 		// Reserved [42:34]
	uint32_t  CMD_SUPPORT:2;  		// Command Support bits [33:32]
	uint32_t  RESERVED_1:32;   		// Reserved for manufacturer [31:0]
}SD_SCR;

typedef struct{
 	uint32_t  CSD_STRUCTURE:2;	// CSD structure [127:126]
	uint32_t  RESERVED_5:6;		// Reserved [125:120]
	uint8_t   TAAC;			// Data read access-time 1 [119:112]
	uint8_t   NSAC;			// Data read access-time 2 in CLK cycles (NSAC*100) [111:104]
	uint8_t   TRAN_SPEED;		// Max. bus clock frequency [103:96]
	uint32_t  CCC:12;		// Card command classes [95:84]
	uint32_t  READ_BL_LEN:4;	// Max. read data block length [83:80]
	uint32_t  READ_BL_PARTIAL:1;	// Partial blocks for read allowed [79:79]
	uint32_t  WRITE_BLK_MISALIGN:1; // Write block misalignment [78:78]
	uint32_t  READ_BLK_MISALIGN:1;	// Read block misalignment [77:77]
	uint32_t  DSR_IMP:1;		// DSR implemented [76:76]
	uint32_t  RESERVED_4:2;		// Reserved [75:74]
	uint32_t  C_SIZE:12;		// Device size [73:62]
	uint32_t  VDD_R_CURR_MIN:3;	// Max. read current @ VDD min [61:59]
	uint32_t  VDD_R_CURR_MAX:3;	// Max. read current @ VDD max [58:56]
	uint32_t  VDD_W_CURR_MIN:3;	// Max. write current @ VDD min [55:53]
	uint32_t  VDD_W_CURR_MAX:3;	// Max. write current @ VDD max [52:50]
	uint32_t  C_SIZE_MULT:3;	// Device size multiplier [49:47]
	uint32_t  ERASE_BLK_EN:1;	// Erase single block enable [46:46]
	uint32_t  SECTOR_SIZE:7;	// Erase sector size [45:39]
	uint32_t  WP_GRP_SIZE:7;	// Write protect group size [38:32]
	uint32_t  WP_GRP_ENABLE:1;	// Write protect group enable [31:31]
	uint32_t  RESERVED_3:2;		// Reserved [30:29]
	uint32_t  R2W_FACTOR:3;		// Write speed factor [28:26]
	uint32_t  WRITE_BL_LEN:4;	// Max. write data block length [25:22]
	uint32_t  WRITE_BL_PARTIAL:1;	// Partial blocks for write allowed [21:21]
	uint32_t  RESERVED_2:5;		// Reserved [20:16]
	uint32_t  FILE_FORMAT_GRP:1;	// File format group [15:15]
	uint32_t  COPY:1;		// Copy flag (OTP) [14:14]
	uint32_t  PERM_WRITE_PROTECT:1; // Permanent write protection [13:13]
	uint32_t  TMP_WRITE_PROTECT:1;	// Temporary write protection [12:12]
	uint32_t  FILE_FORMAT:2;	// File format [11:10]
	uint32_t  RESERVED_1:2;		// Reserved [9:8]
	uint32_t  CRC:7;		// CRC [7:1]
	uint32_t  NOT_USED:1;		// Not used, always 1 [0:0]
}SD_CSD;

typedef struct{
	uint32_t  CSD_STRUCTURE:2; 	// CSD structure [127:126]
	uint32_t  RESERVED_6:6;		// Reserved [125:120]
	uint8_t   TAAC;  		// Data read access-time 1 [119:112]
	uint8_t   NSAC;  		// Data read access-time 2 in CLK cycles (NSAC*100) [111:104]
	uint8_t   TRAN_SPEED;  		// Max. bus clock frequency [103:96]
	uint32_t  CCC:12;		// Card command classes [95:84]
	uint32_t  READ_BL_LEN:4; 	// Max. read data block length [83:80]
	uint32_t  READ_BL_PARTIAL:1; 	// Partial blocks for read allowed [79:79]
	uint32_t  WRITE_BLK_MISALIGN:1; // Write block misalignment [78:78]
	uint32_t  READ_BLK_MISALIGN:1; 	// Read block misalignment [77:77]
	uint32_t  DSR_IMP:1; 		// DSR implemented [76:76]
	uint32_t  RESERVED_5:6;		// Reserved [75:70]
	uint32_t  C_SIZE:22;		// Device size [69:48]
	uint32_t  RESERVED_4:2;		// Reserved [47:47]
	uint32_t  ERASE_BLK_EN:1; 	// Erase single block enable [46:46]
	uint32_t  SECTOR_SIZE:7;	// Erase sector size [45:39]
	uint32_t  WP_GRP_SIZE:7;	// Write protect group size [38:32]
	uint32_t  WP_GRP_ENABLE:1;	// Write protect group enable [31:31]
	uint32_t  RESERVED_3:2;		// Reserved [30:29]
	uint32_t  R2W_FACTOR:3;		// Write speed factor [28:26]
	uint32_t  WRITE_BL_LEN:4;	// Max. write data block length [25:22]
	uint32_t  WRITE_BL_PARTIAL:1;	// Partial blocks for write allowed [21:21]
	uint32_t  RESERVED_2:5; 	// Reserved [20:16]
	uint32_t  FILE_FORMAT_GRP:1;	// File format group [15:15]
	uint32_t  COPY:1;		// Copy flag (OTP) [14:14]
	uint32_t  PERM_WRITE_PROTECT:1; // Permanent write protection [13:13]
	uint32_t  TMP_WRITE_PROTECT:1;	// Temporary write protection [12:12]
	uint32_t  FILE_FORMAT:2;	// File format [11:10]
	uint32_t  RESERVED_1:2; 	// 0 [9:8]
	uint32_t  CRC:7;		// CRC [7:1]
	uint32_t  NOT_USED:1;		// Not used, always 1 [0:0]
}SD_CSD_V2;

typedef enum{
	UNKNOWN_CARD,
	SDSC,               		//SD 1.1 standard card
	SDSC_2,             		//SD 2.0 standard card
	SDHC				//SD 2.0 high capacity card
}SD_TYPE;

typedef struct _SD_CARD_INFO{
	uint16_t  RCA;
	uint32_t  block_size;
	uint32_t  num_blocks;
	uint32_t  clock_frequency;
	SD_TYPE   type;
	SD_OCR	  OCR_data;
	SD_CID	  CID_data;
	SD_CSD	  CSD_data;
}SD_CARD_INFO;

typedef struct _SD_DEVICE{
	uint32_t  (*read) (struct _SD_DEVICE *, uint32_t reg, uint32_t length, void* buffer);
	uint32_t  (*write)(struct _SD_DEVICE *, uint32_t reg, uint32_t length, void* buffer);
}SD_DEVICE;

typedef struct _BLOCK_IO{
	uint32_t  block_size;
	uint64_t  last_block;
	uint32_t  (*read_blocks) (struct _BLOCK_IO *, uint64_t lba, uint32_t buffer_size, void* buffer);
	uint32_t  (*write_blocks)(struct _BLOCK_IO *, uint64_t lba, uint32_t buffer_size, void* buffer);
}BLOCK_IO;

uint32_t 		sd_init();
SD_CARD_INFO		sd_card_info;
SD_DEVICE* 		sd_card;
BLOCK_IO 		sd_block_io;

#endif /* __SD_H_ */
