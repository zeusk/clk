/*
 * WIP (or wishful thinking): Placeholder for qsd8250b basic sd driver
 */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <reg.h>
#include <sd.h>
#include <proc_comm.h>
#include <platform/timer.h>

static void set_clk_freq(uint32_t new_clkd)
{
	//TODO:
	// 	Set new clock frequency.
}

static void decode_and_save_cid_data(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
	//TODO:
	//	Read CMD10 response and populate CID_data structure.
}

static uint32_t sd_send_cmd(uint32_t cmd, uint32_t cmd_int_en, uint32_t arg )
{
	uint32_t status = SD_STATUS_SUCCESS;

	//TODO:
	// 	Check if command line is in use and poll until it is available.
	// 	Provide the block size.
	// 	Clear status register.
	// 	Set command argument register.
	// 	Enable interrupt events to occur.
	// 	Send command.
	// 	Check for the command status.
	// 	Read status of command response.
	// 	Check if command is completed.

	return status;
}

/* Card Initialization and Identification Flow (SD Mode)
 * CMD0 => CMD8 => ACMD41 => CMD2 => CMD3
 */
static uint32_t sd_card_identification()
{
	uint32_t status = SD_STATUS_SUCCESS;

	//TODO:
	//	Enable interrupts.
	//	Init controller.
	//	Change clock frequency to 400KHz to fit protocol
	//	Send CMD0 command.
	//	Send CMD8 command. If CMD8 is supported => Supports high capacity.
	//	Send ACMD41 command.
	//	Check CCS (Card capacity status) bit. SD 2.0 standard card will response with CCS 0, SD high capacity card will respond with CCS 1.
	//	Send CMD2 command.
	//	Parse CID register data.
	//	Send CMD3 command.
	//	Read RCA.
	//	Change SD Bus setting.
	//	Set the clock frequency to 50MHz.

	return status;
}

static void sd_block_information(uint32_t *block_size, uint32_t *num_blocks)
{
	//TODO:
	//	Check card type and populate correct structure registers.
	// 	Populate block_size.***Even if block_size is 1K, the transfer size is 512 bytes.
	// 	Calculate Total number of blocks.
}

static void calculate_sd_card_CLKD(uint32_t *clock_frequency)
{
	//TODO:
	//	Calculate Transfer rate unit (Bits 2:0 of TRAN_SPEED).
	//	Calculate Time value (Bits 6:3 of TRAN_SPEED).
	// 	Calculate Clock Divider value:[*clock_frequency = ((REFERENCE_CLK / ((transfer_rate_unit*time_value)/10)) + 1)].
}

static void sd_card_configuration_data()
{
	//TODO:
	//	Calculate block_size and Total number of blocks in the detected card.
	//	Calculate Card clock divider value.
}

static uint32_t sd_card_specific_data()
{
	uint32_t status = SD_STATUS_SUCCESS;

	//TODO:
	//	Send CMD9 to retrieve CSD.
	//	Populate 128-bit CSD register data.
	//	Calculate total number of blocks and max_data transfer rate supported by the detected card.

	return status;
}

uint32_t perform_sd_card_configuration()
{
	uint32_t status = SD_STATUS_SUCCESS;

	//TODO:
	//	Send CMD7.
	//	Send CMD16 to set the block length.
	//	Change SD clock frequency to what detected card can support.

	return status;
}

uint32_t detect_sd_card()
{
	uint32_t status = SD_STATUS_SUCCESS;

	//TODO:
	// 	Initialize SD host controller clocks.
	// 	Voltage capabilities initialization. Activate VS18 and VS30.
	// 	Enable internal clock.
	// 	Set the clock frequency to 50MHz.
	// 	Enable SD bus power.
	// 	Card identification
	// 	Get CSD (Card specific data) for the detected card.
	// 	Configure the card in data transfer mode.
	//	Patch BLOCK_IO structure.

	return status;
}

uint32_t sd_read_block_data(BLOCK_IO *bio, void *buffer)
{
	//TODO: 
	// 	Read block worth of data.
	
	return 1;
}

uint32_t sd_write_block_data(BLOCK_IO *bio, void *buffer)
{
	//TODO: 
	//	Write block worth of data.

	return 1;
}

uint32_t sd_read(BLOCK_IO *bio, uint32_t lba, void* buffer, uint32_t buffer_size)
{
	uint32_t status = SD_STATUS_SUCCESS;

	//TODO: 
      	//	Transfer a block worth of data.
      	//	Single block read - CMD17.
      	//	Set command argument based on the card access mode (Byte mode or Block mode).
      	//	Send command.
      	//	Read data.
      	//	Check for the Transfer completion.

	return status;
}

uint32_t sd_write(BLOCK_IO *bio, uint32_t lba, void* buffer, uint32_t buffer_size)
{
	uint32_t status = SD_STATUS_SUCCESS;

	//TODO: 
      	//	Transfer a block worth of data.
      	//	Single block write - CMD24.
      	//	Set command argument based on the card access mode (Byte mode or Block mode).
      	//	Send command.
      	//	Write data.
      	//	Check for the Transfer completion.
	
	return status;
}

uint32_t sd_read_blocks(BLOCK_IO *bio, uint64_t lba, uint32_t buffer_size, void *buffer)
{
	return sd_read(bio, (uint32_t) lba, buffer, buffer_size);
}

uint32_t sd_write_blocks(BLOCK_IO *bio, uint64_t lba, uint32_t buffer_size, void *buffer)
{
	return sd_write(bio, (uint32_t) lba, buffer, buffer_size);
}

BLOCK_IO sd_block_io = {
	512,
	0,
	sd_read_blocks,
	sd_write_blocks
};

uint32_t sd_device_read(SD_DEVICE *sdc, uint32_t reg, uint32_t length, void* buffer)
{
	return sd_read(&sd_block_io, reg, buffer, length);
}

uint32_t sd_device_write(SD_DEVICE *sdc, uint32_t reg, uint32_t length, void* buffer)
{
	return sd_write(&sd_block_io, reg, buffer, length);
}

uint32_t sd_init()
{
	memset((struct _SD_CARD_INFO *)&sd_card_info, 0, sizeof(struct _SD_CARD_INFO));
	sd_card = malloc(sizeof(SD_DEVICE));
	sd_card->read = sd_device_read;
	sd_card->write = sd_device_write;

	return SD_STATUS_SUCCESS;
}
