/*
Copyright (c) 2010, Code Aurora Forum. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 and
only version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.
*/

#include <debug.h>
#include <reg.h>
#include <malloc.h>
#include <mmc.h>
#include <adm.h>
#include <target/clock.h>
#include <target/reg.h>
#include <platform/timer.h>

#ifdef USE_PROC_COMM
	#include <pcom.h>
	#include <pcom_clients.h>
#else
	#include <platform/gpio.h>
#endif

static unsigned char sdcc_use_dm = 1;
// Structures for use with ADM
static uint32_t sd_adm_cmd_ptr_list[8] __attribute__ ((aligned(8))); //  Must aligned on 8 byte boundary
static uint32_t sd_box_mode_entry[8] __attribute__ ((aligned(8))); //  Must aligned on 8 byte boundary

// clear the status bit.
#define MCI_CLEAR_STATUS(base, mask) do{ writel(mask, base + MCI_CLEAR); } while(readl(base + MCI_STATUS) & mask);

// Any one of these will be set when command processing is complete.
#define MCI_STATUS_CMD_COMPLETE_MASK        (MCI_STATUS__CMD_SENT___M | \
                                             MCI_STATUS__CMD_RESPONSE_END___M | \
                                             MCI_STATUS__CMD_TIMEOUT___M | \
                                             MCI_STATUS__CMD_CRC_FAIL___M)

// Any one of these will be set during data read processing.
#define MCI_STATUS_DATA_READ_COMPLETE_MASK  (MCI_STATUS_BLK_END_MASK | \
                                             MCI_STATUS_DATA_RD_ERR_MASK)

#define MCI_STATUS_DATA_RD_ERR_MASK         (MCI_STATUS__DATA_CRC_FAIL___M | \
                                             MCI_STATUS__DATA_TIMEOUT___M  | \
                                             MCI_STATUS__RX_OVERRUN___M    | \
                                             MCI_STATUS__START_BIT_ERR___M)

#define MCI_STATUS_BLK_END_MASK             (MCI_STATUS__DATA_BLK_END___M |  \
                                             MCI_STATUS__DATAEND___M)

#define MCI_STATUS_READ_DATA_MASK           (MCI_STATUS__RXDATA_AVLBL___M|  \
                                             MCI_STATUS__RXACTIVE___M)

// Writes to MCI port are not effective for 3 ticks of PCLK.
// The min pclk is 144KHz which gives 6.94 us/tick.
// Thus 21us == 3 ticks.
#define PORT_RUSH_TIMEOUT (21)

/* verify if data read was successful and clear up status bits */
int sdcc_read_data_cleanup(mmc_t *mmc)
{
    int err = 0;
    uint32_t base   = ((sd_parms_t*)(mmc->priv))->base;
    uint32_t status = readl(base + MCI_STATUS);

    if(status & MCI_STATUS__RX_OVERRUN___M) {
        err = SDCC_ERR_RX_OVERRUN;
    } else 
	if (status & MCI_STATUS__DATA_TIMEOUT___M) {
        err = SDCC_ERR_DATA_TIMEOUT;
    } else
	if (status & MCI_STATUS__DATA_CRC_FAIL___M) {
        err = SDCC_ERR_DATA_CRC_FAIL;
    } else 
	if (status & MCI_STATUS__START_BIT_ERR___M ) {
        err = SDCC_ERR_DATA_START_BIT;
    } else {
        // Wait for the blk status bits to be set.
        while (!(readl(base + MCI_STATUS) & MCI_STATUS__DATAEND___M));
    }

    // Clear the status bits.
    MCI_CLEAR_STATUS(base, MCI_STATUS_DATA_READ_COMPLETE_MASK);

    return err;
}

/* Read data from controller fifo */
int sdcc_read_data(mmc_t *mmc, mmc_cmd_t *cmd, mmc_data_t *data)
{
    uint32_t status;
    uint16_t byte_count   = 0;
    uint32_t *dest_ptr    = (uint32_t *)(data->dest);
    uint32_t base         = ((sd_parms_t*)(mmc->priv))->base;
    uint32_t adm_crci_num = ((sd_parms_t*)(mmc->priv))->adm_crci_num; //  ADM CRCI number

    if(sdcc_use_dm) {
        uint32_t num_rows;
        uint32_t addr_shft;
        uint32_t rows_per_block;
        uint16_t row_len;

        row_len = SDCC_FIFO_SIZE;
        rows_per_block = (data->blocksize / SDCC_FIFO_SIZE);
        num_rows = rows_per_block * data->blocks;
        uint32_t tx_dst = (uint32_t)data->dest;

        while( num_rows != 0 ) {
            uint32_t tx_size = 0;
            // Check to see if the attempted transfer size is more than 0xFFFF
            // If it is we need to do more than one transfer.
            if( num_rows > 0xFFFF ) {
                tx_size   = 0xFFFF;
                num_rows -= 0xFFFF;
            } else {
                tx_size  = num_rows;
                num_rows = 0;
            }
			
            // Initialize the DM Box mode command entry (single entry)
            sd_box_mode_entry[0] = (ADM_CMD_LIST_LC | (adm_crci_num << 3) | ADM_ADDR_MODE_BOX);
            sd_box_mode_entry[1] = base + MCI_FIFO;                  	// SRC addr
            sd_box_mode_entry[2] = (uint32_t) tx_dst;                	// DST addr
            sd_box_mode_entry[3] = ((row_len << 16) | (row_len << 0));  // SRC/DST row len
            sd_box_mode_entry[4] = ((tx_size << 16) | (tx_size << 0)); 	// SRC/DST num rows
            sd_box_mode_entry[5] = ((0 << 16) | (SDCC_FIFO_SIZE << 0)); // SRC/DST offset
            
			// Initialize the DM Command Pointer List (single entry)
            addr_shft = ((uint32_t)(&sd_box_mode_entry[0])) >> 3;
            sd_adm_cmd_ptr_list[0] = (ADM_CMD_PTR_LP | ADM_CMD_PTR_CMD_LIST | addr_shft);

            // Start ADM transfer, this transfer waits until it finishes
            // before returing
            if (adm_start_transfer(ADM_AARM_SD_CHN, sd_adm_cmd_ptr_list) != 0) {
               return SDCC_ERR_DATA_ADM_ERR;
            }
            // Add the amount we have transfered to the destination
            tx_dst += (tx_size*row_len);
        }
    } else {
        int read_len = (data->blocks) * (data->blocksize);

        while((status = readl(base + MCI_STATUS)) & MCI_STATUS_READ_DATA_MASK) {
            // rx data available bit is not cleared immidiately.
            // read only the requested amount of data and wait for
            // the bit to be cleared.
            if(byte_count < read_len) {
                if(status & MCI_STATUS__RXDATA_AVLBL___M) {
                    *dest_ptr = readl(base + MCI_FIFO);
                    dest_ptr++;
                    byte_count += 4;
                }
            }
        }
    }
	
    return sdcc_read_data_cleanup(mmc);
}

/* Set SD MCLK speed */
static int sdcc_mclk_set(int instance, enum SD_MCLK_speed speed)
{
#ifdef USE_PROC_COMM
    pcom_set_sdcard_clk(instance, speed);
    pcom_enable_sdcard_clk(instance);
#else
    if (instance == 1) {
		clk_set_rate(SDC1_CLK, MCLK_400KHz);
	} else
	if (instance == 2) {
		clk_set_rate(SDC2_CLK, MCLK_400KHz);
	} else
	if (instance == 3) {
		clk_set_rate(SDC3_CLK, MCLK_400KHz);
	} else
	if (instance == 4) {
		clk_set_rate(SDC4_CLK, MCLK_400KHz);
	}
#endif

   return 0;
}

/* Set bus width and bus clock speed */
void sdcc_set_ios(mmc_t *mmc)
{
    uint32_t clk_reg;
    uint32_t base     = ((sd_parms_t*)(mmc->priv))->base;
    uint32_t instance = ((sd_parms_t*)(mmc->priv))->instance;

    if(mmc->bus_width == 1) {
        clk_reg = readl(base + MCI_CLK) & ~MCI_CLK__WIDEBUS___M;
        writel((clk_reg | (BUS_WIDTH_1 << MCI_CLK__WIDEBUS___S)), base + MCI_CLK);
    } else
	if(mmc->bus_width == 4) {
        clk_reg = readl(base + MCI_CLK) & ~MCI_CLK__WIDEBUS___M;
        writel((clk_reg | (BUS_WIDTH_4 << MCI_CLK__WIDEBUS___S)), base + MCI_CLK);
    } else
	if(mmc->bus_width == 8) {
        clk_reg = readl(base + MCI_CLK) & ~MCI_CLK__WIDEBUS___M;
        writel((clk_reg | (BUS_WIDTH_8 << MCI_CLK__WIDEBUS___S)), base + MCI_CLK);
    }
    if(mmc->clock <= 25000000) {
        uint32_t temp32;

        sdcc_mclk_set(instance, mmc->clock);

        // Latch data on falling edge
        temp32 = readl(base + MCI_CLK) & ~MCI_CLK__SELECT_IN___M;
        temp32 |= (MCI_CLK__SELECT_IN__ON_THE_FALLING_EDGE_OF_MCICLOCK << MCI_CLK__SELECT_IN___S);
        writel(temp32, base + MCI_CLK);
    } else {
        uint32_t temp32;

        sdcc_mclk_set(instance, mmc->clock);

        // Card is in high speed mode, use feedback clock.
        temp32 = readl(base + MCI_CLK);
        temp32 &= ~(MCI_CLK__SELECT_IN___M);
        temp32 |= (MCI_CLK__SELECT_IN__USING_FEEDBACK_CLOCK << MCI_CLK__SELECT_IN___S);
        writel(temp32, base + MCI_CLK);
    }
}

/* Program the data control registers */
void sdcc_start_data(mmc_t *mmc, mmc_cmd_t *cmd, mmc_data_t *data)
{
    uint32_t data_ctrl;
    uint32_t base = ((sd_parms_t*)(mmc->priv))->base;
    uint32_t xfer_size = data->blocksize * data->blocks;;

    data_ctrl = MCI_DATA_CTL__ENABLE___M | (data->blocksize << MCI_DATA_CTL__BLOCKSIZE___S);

    if ( (xfer_size < SDCC_FIFO_SIZE) || (xfer_size % SDCC_FIFO_SIZE) ) {
        // can't use data mover.
        sdcc_use_dm = 0;
    } else {
        sdcc_use_dm = 1;
        data_ctrl |= MCI_DATA_CTL__DM_ENABLE___M;
    }

    if(data->flags == MMC_DATA_READ) {
        data_ctrl |= MCI_DATA_CTL__DIRECTION___M;
    }

    // Set timeout and data length
    writel(RD_DATA_TIMEOUT, base + MCI_DATA_TIMER);
    writel(xfer_size, base + MCI_DATA_LENGTH);

    // Delay for previous param to be applied.
    udelay(PORT_RUSH_TIMEOUT);
    writel(data_ctrl, base + MCI_DATA_CTL);
    // Delay before adm
    udelay(PORT_RUSH_TIMEOUT);
}

/* Set proper bit for the cmd register value */
static void sdcc_get_cmd_reg_value(mmc_cmd_t *cmd, mmc_data_t *data, uint16_t *creg)
{
    *creg = cmd->cmdidx | MCI_CMD__ENABLE___M;

    if(cmd->resp_type & MMC_RSP_PRESENT) {
        *creg |= MCI_CMD__RESPONSE___M;

        if(cmd->resp_type & MMC_RSP_136)
            *creg |= MCI_CMD__LONGRSP___M;
    }
    if(cmd->resp_type & MMC_RSP_BUSY) {
        *creg |= MCI_CMD__PROG_ENA___M;
    }

    // Set the DAT_CMD bit for data commands
    if( (cmd->cmdidx == CMD17) ||
        (cmd->cmdidx == CMD18) ||
        (cmd->cmdidx == CMD24) ||
        (cmd->cmdidx == CMD25) ||
        (cmd->cmdidx == CMD53) ) {
        // We don't need to set this bit for switch function (6) and
        // read scr (51) commands in SD case.
        *creg |= MCI_CMD__DAT_CMD___M;
    }

    if(cmd->resp_type & MMC_RSP_OPCODE) {
        // Don't know what is this for. Seems to work fine
        // without doing anything for this response type.
    }
}

/* Program the cmd registers */
void sdcc_start_command(mmc_t *mmc, mmc_cmd_t *cmd, mmc_data_t *data)
{
    uint16_t creg;
    uint32_t base = ((sd_parms_t*)(mmc->priv))->base;

    sdcc_get_cmd_reg_value(cmd, data, &creg);

    writel(cmd->cmdarg, base + MCI_ARGUMENT);
    udelay(PORT_RUSH_TIMEOUT);
    writel(creg, base + MCI_CMD);
    udelay(PORT_RUSH_TIMEOUT);
}

/* Send command as well as send/receive data */
int sdcc_send_cmd(mmc_t *mmc, mmc_cmd_t *cmd, mmc_data_t *data)
{
    int err = 0;
    uint32_t status;
    uint32_t base = ((sd_parms_t*)(mmc->priv))->base;

    if((status = readl(base + MCI_STATUS))) {
        // Some implementation error.
        // must always start with all status bits cleared.
        //printf("%s: Invalid status on entry: status = 0x%x, cmd = %d\n", __FUNCTION__, status, cmd->cmdidx);

        return SDCC_ERR_INVALID_STATUS;
    }

    if(data) {
        if(data->flags == MMC_DATA_WRITE)
            return SDCC_ERR_DATA_WRITE; // write not yet implemented.

        sdcc_start_data(mmc, cmd, data);
    }
    sdcc_start_command(mmc, cmd, data);

    // Wait for cmd completion (any response included).
    while( !((status = readl(base + MCI_STATUS)) & MCI_STATUS_CMD_COMPLETE_MASK) );

    // Read response registers.
    cmd->response[0] = readl(base + MCI_RESPn(0));
    cmd->response[1] = readl(base + MCI_RESPn(1));
    cmd->response[2] = readl(base + MCI_RESPn(2));
    cmd->response[3] = readl(base + MCI_RESPn(3));

    if(status & MCI_STATUS__CMD_CRC_FAIL___M) {
        if(cmd->resp_type & MMC_RSP_CRC) {
            // failure.
            err = SDCC_ERR_CRC_FAIL;
        }
        // clear status bit.
        MCI_CLEAR_STATUS(base, MCI_STATUS__CMD_CRC_FAIL___M);
    } else
	if(status & MCI_STATUS__CMD_TIMEOUT___M) {
        // failure.
        err = SDCC_ERR_TIMEOUT;

        // clear timeout status bit.
        MCI_CLEAR_STATUS(base, MCI_STATUS__CMD_TIMEOUT___M);
    } else 
	if(status & MCI_STATUS__CMD_SENT___M) {
        // clear CMD_SENT status bit.
        MCI_CLEAR_STATUS(base, MCI_STATUS__CMD_SENT___M);
    } else 
	if(status & MCI_STATUS__CMD_RESPONSE_END___M) {
        // clear CMD_RESP_END status bit.
        MCI_CLEAR_STATUS(base, MCI_STATUS__CMD_RESPONSE_END___M);
    }

    if(cmd->resp_type & MMC_RSP_BUSY) {
        // Must wait for PROG_DONE status to be set before returning.
        while( !(readl(base + MCI_STATUS) & MCI_STATUS__PROG_DONE___M) );

        // now clear that bit.
        MCI_CLEAR_STATUS(base, MCI_CLEAR__PROG_DONE_CLR___M);
    }

    // if this was one of the data read/write cmds, handle the data.
    if(data) {
        if(err) {
            // there was some error while sending the cmd. cancel the data operation.
            writel(0x0, base + MCI_DATA_CTL);
            udelay(PORT_RUSH_TIMEOUT);
        } else {
            if(data->flags == MMC_DATA_READ) {
                err = sdcc_read_data(mmc, cmd, data);
            } else {
                // write data is not implemented.
                // placeholder for future update.
                while(1);
            }
        }
    }

    if(err) {
        //status = readl(base + MCI_STATUS);
        //printf("%s: cmd = %d, err = %d status = 0x%x, response = 0x%x\n", __FUNCTION__, cmd->cmdidx, err, status, cmd->response[0]);

        //if(data)
            //printf("%s: blocksize = %d blocks = %d\n", __FUNCTION__, data->blocksize, data->blocks);
			
        return err;
    }

    // read status bits to verify any condition that was not handled.
    // SDIO_INTR bit is set on D0 line status and is valid only in
    // case of SDIO interface. So it can be safely ignored.
    if(readl(base + MCI_STATUS) & MCI_STATUS__SDIO_INTR___M) {
        MCI_CLEAR_STATUS(base, MCI_CLEAR__SDIO_INTR_CLR___M);
    }

    if((status = readl(base + MCI_STATUS))) {
        // Some implementation error.
        // must always exit with all status bits cleared.
        //printf("%s: Invalid status on exit: status = 0x%x, cmd = %d\n", __FUNCTION__, status, cmd->cmdidx);

        return SDCC_ERR_INVALID_STATUS;
    }

    return err;
}

/* Initialize sd controller block */
void sdcc_controller_init(sd_parms_t *sd)
{
    uint32_t mci_clk;

    // Disable all interrupts sources
    writel(0x0, sd->base + MCI_INT_MASK0);
    writel(0x0, sd->base + MCI_INT_MASK1);

    // Clear all status bits
    writel(0x07FFFFFF, sd->base + MCI_CLEAR);
    udelay(PORT_RUSH_TIMEOUT);

    // Power control to the card, enable MCICLK with power save mode
    // disabled, otherwise the initialization clock cycles will be
    // shut off and the card will not initialize.
    writel(MCI_POWER__CONTROL__POWERON, sd->base + MCI_POWER);

    mci_clk = MCI_CLK__FLOW_ENA___M
			|(MCI_CLK__SELECT_IN__ON_THE_FALLING_EDGE_OF_MCICLOCK << MCI_CLK__SELECT_IN___S)
			| MCI_CLK__ENABLE___M;

    writel(mci_clk, sd->base + MCI_CLK);
}

/* Called during each scan of mmc devices */
int sdcc_init(mmc_t *mmc)
{
    sd_parms_t *sd = (sd_parms_t*)(mmc->priv);

#ifdef USE_PROC_COMM
	// Switch on sd card power. The voltage regulator used is board specific
	pcom_sdcard_power(1); //enable
	
    // Enable clock
    pcom_enable_sdcard_pclk(sd->instance);

    // Set the interface clock
    pcom_set_sdcard_clk(sd->instance, MCLK_400KHz);
    pcom_enable_sdcard_clk(sd->instance);
#else
    // Set the interface clock
	if (sd->instance == 1) {
		clk_set_rate(SDC1_CLK, MCLK_400KHz);
	} else
	if (sd->instance == 2) {
		clk_set_rate(SDC2_CLK, MCLK_400KHz);
	} else
	if (sd->instance == 3) {
		clk_set_rate(SDC3_CLK, MCLK_400KHz);
	} else
	if (sd->instance == 4) {
		clk_set_rate(SDC4_CLK, MCLK_400KHz);
	}
#endif
	// GPIO config
	pcom_sdcard_gpio_config(sd->instance);
	
    // Initialize controller
    sdcc_controller_init(sd);
	mmc_ready = 1;
	
    return 0;
}
