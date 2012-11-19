/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007 QUALCOMM Incorporated
 * Copyright (c) 2010 Cotulla
 * Copyright (c) 2012 Shantanu Gupta <shans95g@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */
 
#include <array.h>
#include <compiler.h>
#include <reg.h>
#include <pcom.h>
#include <kernel/thread.h>
#include <platform/iomap.h>
#include <target/clock.h>
#include <platform/timer.h>

#define TCX0               19200000
#define GLBL_CLK_ENA       ((uint32_t)MSM_CLK_CTL_BASE)
#define GLBL_CLK_ENA_2     ((uint32_t)MSM_CLK_CTL_BASE + 0x220)
#define PLLn_BASE(n)       (MSM_CLK_CTL_BASE + 0x300 + 32 * (n))
#define PLL_FREQ(l, m, n)  (TCX0 * (l) + TCX0 * (m) / (n))

struct mdns_clock_params
{
	unsigned freq;
	uint32_t calc_freq;
	uint32_t md;
	uint32_t ns;
	uint32_t pll_freq;
	uint32_t clk_id;
};

struct msm_clock_params
{
	unsigned clk_id;
	uint32_t glbl;
	unsigned idx;
	unsigned offset;
	unsigned ns_only;
	char	 *name;
};

static unsigned int pll_get_rate(int n)
{
	unsigned int mode __UNUSED, L, M, N, freq;
	if (n == -1)
		return TCX0;
		
	if (n > 1) {
		return 0;
	} else {
		mode = readl(PLLn_BASE(n) + 0x0);
		L = readl(PLLn_BASE(n) + 0x4);
		M = readl(PLLn_BASE(n) + 0x8);
		N = readl(PLLn_BASE(n) + 0xc);
		freq = PLL_FREQ(L, M, N);
	}
	
	return freq;
}

static int idx2pll(uint32_t idx)
{
	switch(idx)	{
		case 0: /* TCX0 */
			return -1;
		case 1: /* PLL1 */
			return 1;
		case 4: /* PLL0 */
			return 0;
		default:
			return 4;
	}
}

static struct msm_clock_params msm_clock_parameters[] = {
	/* clk_id 					 glbl 					 idx 		 offset 			 ns_only 			 name  			*/
	{ .clk_id = SDC1_CLK,		.glbl = GLBL_CLK_ENA, 	.idx =  7,	.offset = 0x0a4,						.name="SDC1_CLK"	},
	{ .clk_id = SDC2_CLK,		.glbl = GLBL_CLK_ENA, 	.idx =  8, 	.offset = 0x0ac, 						.name="SDC2_CLK"	},
	{ .clk_id = SDC3_CLK,		.glbl = GLBL_CLK_ENA, 	.idx = 27, 	.offset = 0x3d8,						.name="SDC3_CLK"	},
	{ .clk_id = SDC4_CLK,		.glbl = GLBL_CLK_ENA, 	.idx = 28, 	.offset = 0x3e0,						.name="SDC4_CLK"	},
	{ .clk_id = UART1DM_CLK,	.glbl = GLBL_CLK_ENA, 	.idx = 17, 	.offset = 0x124,						.name="UART1DM_CLK"	},
	{ .clk_id = UART2DM_CLK,	.glbl = GLBL_CLK_ENA, 	.idx = 26, 	.offset = 0x12c,						.name="UART2DM_CLK"	},
	{ .clk_id = USB_HS_CLK,		.glbl = GLBL_CLK_ENA_2,	.idx =  7, 	.offset = 0x3e8,	.ns_only = 0xb41, 	.name="USB_HS_CLK"	},
	{ .clk_id = GRP_CLK,		.glbl = GLBL_CLK_ENA, 	.idx =  3, 	.offset = 0x084, 	.ns_only = 0xa80, 	.name="GRP_CLK"		}, 
	{ .clk_id = IMEM_CLK,		.glbl = GLBL_CLK_ENA, 	.idx =  3, 	.offset = 0x084, 	.ns_only = 0xa80, 	.name="IMEM_CLK"	},
	{ .clk_id = VFE_CLK,		.glbl = GLBL_CLK_ENA, 	.idx =  2, 	.offset = 0x040, 						.name="VFE_CLK"		},
	{ .clk_id = MDP_CLK,		.glbl = GLBL_CLK_ENA, 	.idx =  9, 											.name="MDP_CLK"		},
	{ .clk_id = GP_CLK,			.glbl = GLBL_CLK_ENA, 				.offset = 0x058, 	.ns_only = 0x800, 	.name="GP_CLK"		},
	{ .clk_id = PMDH_CLK,		.glbl = GLBL_CLK_ENA_2, .idx =  4, 	.offset = 0x08c, 	.ns_only = 0x00c, 	.name="PMDH_CLK"	},
	{ .clk_id = I2C_CLK,											.offset = 0x064, 	.ns_only = 0xa00, 	.name="I2C_CLK"		},
	{ .clk_id = SPI_CLK,		.glbl = GLBL_CLK_ENA_2, .idx = 13, 	.offset = 0x14c, 	.ns_only = 0xa08, 	.name="SPI_CLK"		}
};

#define MSM_CLOCK_REG(frequency,M,N,D,PRE,a5,SRC,MNE,pll_frequency) { \
	.freq = (frequency), \
	.md = ((0xffff & (M)) << 16) | (0xffff & ~((D) << 1)), \
	.ns = ((0xffff & ~((N) - (M))) << 16) \
	    | ((0xff & (0xa | (MNE))) << 8) \
	    | ((0x7 & (a5)) << 5) \
	    | ((0x3 & (PRE)) << 3) \
	    | (0x7 & (SRC)), \
	.pll_freq = (pll_frequency), \
	.calc_freq = 1000*((pll_frequency/1000)*M/((PRE+1)*N)), \
}

struct mdns_clock_params msm_clock_freq_parameters[] = {
	MSM_CLOCK_REG(  144000,   3, 0x64, 0x32, 3, 3, 0, 1, 19200000),  /* SD, 144kHz */
	MSM_CLOCK_REG(  400000,   1, 0x30, 0x15, 0, 3, 0, 1, 19200000),  /* SD, 400kHz */
	MSM_CLOCK_REG( 7372800,   3, 0x64, 0x32, 0, 2, 4, 1, 245760000), /* 460800*16, will be divided by 4 for 115200 */
	MSM_CLOCK_REG(12000000,   1, 0x20, 0x10, 1, 3, 1, 1, 768000000), /* SD, 12MHz */
	MSM_CLOCK_REG(14745600,   3, 0x32, 0x19, 0, 2, 4, 1, 245760000), /* BT, 921600 (*16)*/
	MSM_CLOCK_REG(19200000,   1, 0x0a, 0x05, 3, 3, 1, 1, 768000000), /* SD, 19.2MHz */
	MSM_CLOCK_REG(24000000,   1, 0x10, 0x08, 1, 3, 1, 1, 768000000), /* SD, 24MHz */
	MSM_CLOCK_REG(24576000,   1, 0x0a, 0x05, 0, 2, 4, 1, 245760000), /* SD, 24,576000MHz */
	MSM_CLOCK_REG(25000000,  14, 0xd7, 0x6b, 1, 3, 1, 1, 768000000), /* SD, 25MHz */
	MSM_CLOCK_REG(32000000,   1, 0x0c, 0x06, 1, 3, 1, 1, 768000000), /* SD, 32MHz */
	MSM_CLOCK_REG(48000000,   1, 0x08, 0x04, 1, 3, 1, 1, 768000000), /* SD, 48MHz */
	MSM_CLOCK_REG(50000000,  25, 0xc0, 0x60, 1, 3, 1, 1, 768000000), /* SD, 50MHz */
	MSM_CLOCK_REG(58982400,   6, 0x19, 0x0c, 0, 2, 4, 1, 245760000), /* BT, 3686400 (*16) */
	MSM_CLOCK_REG(64000000,0x19, 0x60, 0x30, 0, 2, 4, 1, 245760000), /* BT, 4000000 (*16) */
};

static void set_grp_clk( int on )
{
	if ( on != 0 ) {
		//axi_reset
		writel(readl(MSM_CLK_CTL_BASE+0x208) |0x20,          MSM_CLK_CTL_BASE+0x208); //AXI_RESET
		//row_reset
		writel(readl(MSM_CLK_CTL_BASE+0x214) |0x20000,       MSM_CLK_CTL_BASE+0x214); //ROW_RESET
		//vdd_grp gfs_ctl
		writel(                              0x11f,          MSM_CLK_CTL_BASE+0x284); //VDD_GRP_GFS_CTL
		// very rough delay
		mdelay(20);
		//grp NS
		writel(readl(MSM_CLK_CTL_BASE+0x84)  |0x800,         MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		writel(readl(MSM_CLK_CTL_BASE+0x84)  |0x80,          MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		writel(readl(MSM_CLK_CTL_BASE+0x84)  |0x200,         MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		//grp idx
		writel(readl(MSM_CLK_CTL_BASE)       |0x8,           MSM_CLK_CTL_BASE);
		//grp clk ramp
		writel(readl(MSM_CLK_CTL_BASE+0x290) &(~(0x4)),      MSM_CLK_CTL_BASE+0x290); //MSM_RAIL_CLAMP_IO
		//Suppress bit 0 of grp MD (?!?)
		writel(readl(MSM_CLK_CTL_BASE+0x80)  &(~(0x1)),      MSM_CLK_CTL_BASE+0x80);  //PRPH_WEB_NS_REG
		//axi_reset
		writel(readl(MSM_CLK_CTL_BASE+0x208) &(~(0x20)),     MSM_CLK_CTL_BASE+0x208); //AXI_RESET
		//row_reset
		writel(readl(MSM_CLK_CTL_BASE+0x218) &(~(0x20000)),  MSM_CLK_CTL_BASE+0x218); //ROW_RESET
	} else {
		//grp NS
		writel(readl(MSM_CLK_CTL_BASE+0x84)  |0x800,         MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		writel(readl(MSM_CLK_CTL_BASE+0x84)  |0x80,          MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		writel(readl(MSM_CLK_CTL_BASE+0x84)  |0x200,         MSM_CLK_CTL_BASE+0x84); //GRP_NS_REG
		//grp idx
		writel(readl(MSM_CLK_CTL_BASE)       |0x8,           MSM_CLK_CTL_BASE);
		//grp MD
		writel(readl(MSM_CLK_CTL_BASE+0x80)  |0x1,      	 MSM_CLK_CTL_BASE+0x80); //PRPH_WEB_NS_REG
		int i = 0;
		int status = 0;
		while ( status == 0 && i < 100) {
			i++;
			status = readl(MSM_CLK_CTL_BASE+0x84) & 0x1;			
		}
		//axi_reset
		writel(readl(MSM_CLK_CTL_BASE+0x208) |0x20,     	MSM_CLK_CTL_BASE+0x208); //AXI_RESET
		//row_reset
		writel(readl(MSM_CLK_CTL_BASE+0x218) |0x20000,  	MSM_CLK_CTL_BASE+0x218); //ROW_RESET
		//grp NS
		writel(readl(MSM_CLK_CTL_BASE+0x84)  &(~(0x800)),   MSM_CLK_CTL_BASE+0x84);  //GRP_NS_REG
		writel(readl(MSM_CLK_CTL_BASE+0x84)  &(~(0x80)),    MSM_CLK_CTL_BASE+0x84);  //GRP_NS_REG
		writel(readl(MSM_CLK_CTL_BASE+0x84)  &(~(0x200)),   MSM_CLK_CTL_BASE+0x84);  //GRP_NS_REG
		//grp clk ramp
		writel(readl(MSM_CLK_CTL_BASE+0x290) |0x4,      	MSM_CLK_CTL_BASE+0x290); //MSM_RAIL_CLAMP_IO
		writel(                              0x11f,         MSM_CLK_CTL_BASE+0x284); //VDD_GRP_GFS_CTL
		int control = readl(MSM_CLK_CTL_BASE+0x288); 								 //VDD_VDC_GFS_CTL
		if ( control & 0x100 )
			writel(readl(MSM_CLK_CTL_BASE) &(~(0x8)),      	MSM_CLK_CTL_BASE);
	}
}

static inline struct msm_clock_params msm_clk_get_params(uint32_t id)
{
	struct msm_clock_params empty = { .clk_id = 0xdeadbeef, .glbl = 0, .idx = 0, .offset = 0, .ns_only = 0, .name="deafbeef"};
	for (uint32_t i = 0; i < ARRAY_SIZE(msm_clock_parameters); i++) {
		if (id == msm_clock_parameters[i].clk_id) {
			return msm_clock_parameters[i];
		}
	}
	
	return empty;
}

static inline uint32_t msm_clk_enable_bit(uint32_t id)
{
	struct msm_clock_params params;
	params = msm_clk_get_params(id);
	if (!params.idx) return 0;
	
	return 1U << params.idx;
}

static inline uint32_t msm_clk_get_glbl(uint32_t id)
{
	struct msm_clock_params params;
	params = msm_clk_get_params(id);
	if (!params.glbl) return 0;
	
	return params.glbl;
}

static inline unsigned msm_clk_reg_offset(uint32_t id)
{
	struct msm_clock_params params;
	params = msm_clk_get_params(id);
	
	return params.offset;
}

static int set_mdns_host_clock(uint32_t id, unsigned long freq)
{
	int n;
	unsigned offset;
	int retval __UNUSED;
	bool found __UNUSED;
	struct msm_clock_params params;
	uint32_t nsreg;
	found = 0;
	retval = -1;
	
	params = msm_clk_get_params(id);
	offset = params.offset;

	if (!params.offset)
		return -1;

	// Turn off clock-enable bit if supported
	if (params.idx > 0 && params.glbl > 0)
		writel(readl(params.glbl) & ~(1U << params.idx), params.glbl);

	if (params.ns_only > 0)	{
		nsreg = readl(MSM_CLK_CTL_BASE + offset) & 0xfffff000;
		writel( nsreg | params.ns_only, MSM_CLK_CTL_BASE + offset);
		found = 1;
		retval = 0;
	} else {
		for (n = ARRAY_SIZE(msm_clock_freq_parameters)-1; n >= 0; n--) {
			if (freq >= msm_clock_freq_parameters[n].freq) {
				// This clock requires MD and NS regs to set frequency:
				writel(msm_clock_freq_parameters[n].md, MSM_CLK_CTL_BASE + offset - 4);
				writel(msm_clock_freq_parameters[n].ns, MSM_CLK_CTL_BASE + offset);
				retval = 0;
				found = 1;
				break;
			}
		}
	}

	// Turn clock-enable bit back on, if supported
	if (params.idx > 0 && params.glbl > 0)
		writel(readl(params.glbl) | (1U << params.idx), params.glbl);

    return 0;
}

static unsigned long get_mdns_host_clock(uint32_t id)
{
	uint32_t n;
	uint32_t offset;
	uint32_t mdreg;
	uint32_t nsreg;
	unsigned long freq = 0;

	offset = msm_clk_reg_offset(id);
	if (offset == 0)
		return -1;

	mdreg = readl(MSM_CLK_CTL_BASE + offset - 4);
	nsreg = readl(MSM_CLK_CTL_BASE + offset);

	for (n = 0; n < ARRAY_SIZE(msm_clock_freq_parameters); n++) {
		if (msm_clock_freq_parameters[n].md == mdreg &&
			msm_clock_freq_parameters[n].ns == nsreg) {
			freq = msm_clock_freq_parameters[n].freq;
			break;
		}
	}

	return freq;
}

// Cotullaz "new" clock functions
static int cotulla_clk_set_rate(uint32_t id, unsigned long rate)
{
    unsigned clk = -1;
    unsigned speed = 0;
    switch (id) {
		case ICODEC_RX_CLK:
			if (rate > 11289600)     speed = 9;
			else if (rate > 8192000) speed = 8;
			else if (rate > 6144000) speed = 7;
			else if (rate > 5644800) speed = 6;
			else if (rate > 4096000) speed = 5;
			else if (rate > 3072000) speed = 4;
			else if (rate > 2822400) speed = 3;
			else if (rate > 2048000) speed = 2;
			else speed = 1;
			clk = 50;
			break;
		case ICODEC_TX_CLK:
			if (rate > 11289600) 	 speed = 9;
			else if (rate > 8192000) speed = 8;
			else if (rate > 6144000) speed = 7;
			else if (rate > 5644800) speed = 6;
			else if (rate > 4096000) speed = 5;
			else if (rate > 3072000) speed = 4;
			else if (rate > 2822400) speed = 3;
			else if (rate > 2048000) speed = 2;
			else speed = 1;
			clk = 52;
			break;
		case SDAC_MCLK:
			if (rate > 1411200) 	speed = 9;
			else if (rate > 1024000)speed = 8;
			else if (rate > 768000) speed = 7;
			else if (rate > 705600) speed = 6;
			else if (rate > 512000) speed = 5;
			else if (rate > 384000) speed = 4;
			else if (rate > 352800) speed = 3;
			else if (rate > 256000) speed = 2;
			else speed = 1;
			clk = 64;
			break;
		case UART1DM_CLK:
			if (rate > 61440000) 	  speed = 15;
			else if (rate > 58982400) speed = 14;
			else if (rate > 56000000) speed = 13;
			else if (rate > 51200000) speed = 12;
			else if (rate > 48000000) speed = 11;
			else if (rate > 40000000) speed = 10;
			else if (rate > 32000000) speed = 9;
			else if (rate > 24000000) speed = 8;
			else if (rate > 16000000) speed = 7;
			else if (rate > 15360000) speed = 6;
			else if (rate > 14745600) speed = 5;
			else if (rate >  7680000) speed = 4;
			else if (rate >  7372800) speed = 3;
			else if (rate >  3840000) speed = 2;
			else speed = 1;
			clk = 78;
			break;
		case UART2DM_CLK:
			if (rate > 61440000) 	  speed = 15;
			else if (rate > 58982400) speed = 14;
			else if (rate > 56000000) speed = 13;
			else if (rate > 51200000) speed = 12;
			else if (rate > 48000000) speed = 11;
			else if (rate > 40000000) speed = 10;
			else if (rate > 32000000) speed = 9;
			else if (rate > 24000000) speed = 8;
			else if (rate > 16000000) speed = 7;
			else if (rate > 15360000) speed = 6;
			else if (rate > 14745600) speed = 5;
			else if (rate >  7680000) speed = 4;
			else if (rate >  7372800) speed = 3;
			else if (rate >  3840000) speed = 2;
			else speed = 1;
			clk = 80;
			break;
		case ECODEC_CLK:
			if (rate > 2048000) 	speed = 3;
			else if (rate > 128000) speed = 2;
			else speed = 1;
			clk = 42;
			break;
		case VFE_MDC_CLK:
			if (rate == 96000000) 		speed = 37;
			else if (rate == 48000000)	speed = 32;
			else if (rate == 24000000) 	speed = 22;
			else if (rate == 12000000) 	speed = 14;
			else if (rate ==  6000000) 	speed = 6;
			else if (rate ==  3000000) 	speed = 1;
			else return 0;
			clk = 40;
			break;
		case VFE_CLK:
			if (rate == 36000000) 		speed = 1;
			else if (rate == 48000000) 	speed = 2;
			else if (rate == 64000000) 	speed = 3;
			else if (rate == 78000000) 	speed = 4;
			else if (rate == 96000000) 	speed = 5;
			else return 0;
			clk = 41;
			break;
		case SPI_CLK:
			if (rate > 15360000) 		speed = 5;
			else if (rate > 9600000) 	speed = 4;
			else if (rate > 4800000) 	speed = 3;
			else if (rate >  960000) 	speed = 2;
			else speed = 1;
			clk = 95;
			break;
		case SDC1_CLK:
			if (rate > 50000000) 		speed = 14;
			else if (rate > 49152000) 	speed = 13;
			else if (rate > 45000000) 	speed = 12;
			else if (rate > 40000000) 	speed = 11;
			else if (rate > 35000000) 	speed = 10;
			else if (rate > 30000000) 	speed = 9;
			else if (rate > 25000000) 	speed = 8;
			else if (rate > 20000000) 	speed = 7;
			else if (rate > 15000000) 	speed = 6;
			else if (rate > 10000000) 	speed = 5;
			else if (rate > 5000000)  	speed = 4;
			else if (rate > 400000)		speed = 3;
			else if (rate > 144000)		speed = 2;
			else speed = 1;
			clk = 66;
			break;
		case SDC2_CLK:
			if (rate > 50000000) 		speed = 14;
			else if (rate > 49152000) 	speed = 13;
			else if (rate > 45000000) 	speed = 12;
			else if (rate > 40000000) 	speed = 11;
			else if (rate > 35000000) 	speed = 10;
			else if (rate > 30000000) 	speed = 9;
			else if (rate > 25000000) 	speed = 8;
			else if (rate > 20000000) 	speed = 7;
			else if (rate > 15000000) 	speed = 6;
			else if (rate > 10000000) 	speed = 5;
			else if (rate > 5000000)  	speed = 4;
			else if (rate > 400000)		speed = 3;
			else if (rate > 144000)		speed = 2;
			else speed = 1;
			clk = 67;
			break;
		case SDC1_PCLK:
		case SDC2_PCLK:
			return 0;
			break;
		default:
			return -1;  
    }
    msm_proc_comm(PCOM_CLK_REGIME_SEC_SEL_SPEED, &clk, &speed);
	
    return 0;
}

static int cotulla_clk_enable(uint32_t id)
{
    unsigned clk = -1;
    switch (id) {
		case ICODEC_RX_CLK:
			clk = 50;
			break;
		case ICODEC_TX_CLK:
			clk = 52;
			break;
		case ECODEC_CLK:
			clk = 42;
			break;   
		case SDAC_MCLK:
			clk = 64;
			break;
		case IMEM_CLK:
			clk = 55;
			break;
		case GRP_CLK:
			clk = 56;
			break;
		case ADM_CLK:
			clk = 19;
			break;
		case UART1DM_CLK:
			clk = 78;
			break;
		case UART2DM_CLK:
			clk = 80;
			break;
		case VFE_AXI_CLK:
			clk = 24;
			break;
		case VFE_MDC_CLK:
			clk = 40;
			break;
		case VFE_CLK:
			clk = 41;
			break;
		case MDC_CLK:
			clk = 53; // ??
			break;
		case SPI_CLK:
			clk = 95;
			break;
		case MDP_CLK:
			clk = 9;
			break;
		case SDC1_CLK:
			clk = 66;
			break;
		case SDC2_CLK:
			clk = 67;
			break;
		case SDC1_PCLK:
			clk = 17;
			break;
		case SDC2_PCLK:
			clk = 16;
			break;
		default:
			return -1;  
    }
    msm_proc_comm(PCOM_CLK_REGIME_SEC_ENABLE, &clk, 0);
	
    return 0;
}

static int cotulla_clk_disable(uint32_t id)
{
    unsigned clk = -1;
    switch (id) {
		case ICODEC_RX_CLK:
			clk = 50;
			break;
		case ICODEC_TX_CLK:
			clk = 52;
			break;
		case ECODEC_CLK:
			clk = 42;
			break;   
		case SDAC_MCLK:
			clk = 64;
			break;
		case IMEM_CLK:
			clk = 55;
			break;
		case GRP_CLK:
			clk = 56;
			break;
		case ADM_CLK:
			clk = 19;
			break;
		case UART1DM_CLK:
			clk = 78;
			break;
		case UART2DM_CLK:
			clk = 80;
			break;
		case VFE_AXI_CLK:
			clk = 24;
			break;
		case VFE_MDC_CLK:
			clk = 40;
			break;
		case VFE_CLK:
			clk = 41;
			break;
		case MDC_CLK:
			clk = 53; // WTF??
			break;
		case SPI_CLK:
			clk = 95;
			break;
		case MDP_CLK:
			clk = 9;
			break;
		case SDC1_CLK:
			clk = 66;
			break;
		case SDC2_CLK:
			clk = 67;
			break;
		case SDC1_PCLK:
			clk = 17;
			break;
		case SDC2_PCLK:
			clk = 16;
			break;
		default:
			return -1;  
    }
    msm_proc_comm(PCOM_CLK_REGIME_SEC_DISABLE, &clk, 0);
	
    return 0;
}

static long cotulla_clk_get_rate(uint32_t id)
{
    unsigned clk = -1;
    unsigned rate;
    switch (id) {
		case ICODEC_RX_CLK:
			clk = 50;
			break;
		case ICODEC_TX_CLK:
			clk = 52;
			break;
		case ECODEC_CLK:
			clk = 42;
			break;
		case SDAC_MCLK:
			clk = 64;
			break;
		case IMEM_CLK:
			clk = 55;
			break;
		case GRP_CLK:
			clk = 56;
			break;
		case ADM_CLK:
			clk = 19;
			break;
		case UART1DM_CLK:
			clk = 78;
			break;
		case UART2DM_CLK:
			clk = 80;
			break;
		case VFE_AXI_CLK:
			clk = 24;
			break;
		case VFE_MDC_CLK:
			clk = 40;
			break;
		case VFE_CLK:
			clk = 41;
			break;
		case MDC_CLK:
			clk = 53; // ??
			break;
		case SPI_CLK:
			clk = 95;
			break;
		case MDP_CLK:
			clk = 9;
			break;
		case SDC1_CLK:
			clk = 66;
			break;
		case SDC2_CLK:
			clk = 67;
			break;
		case SDC1_PCLK:
			clk = 17;
			break;
		case SDC2_PCLK:
			clk = 16;
			break;
		default:
			return 0;
    }
    msm_proc_comm(PCOM_CLK_REGIME_SEC_MSM_GET_CLK_FREQ_KHZ, &clk, &rate);
	
    return clk*1000;
}

static int cotulla_clk_set_flags(uint32_t id, unsigned long flags)
{    
    if (id == VFE_CLK) {        
        if (flags == (0x00000100 << 1)) {        
            uint32_t f = 0; 
            msm_proc_comm(PCOM_CLK_REGIME_SEC_SEL_VFE_SRC, &f, 0);
            return 0;
        }
        else if (flags == 0x00000100) {
            uint32_t f = 1; 
            msm_proc_comm(PCOM_CLK_REGIME_SEC_SEL_VFE_SRC, &f, 0);
            return 0;
        }
    }
	
    return -1;
}

int clk_enable(unsigned id)
{
	if (id == ACPU_CLK)
		return -1;
		
	struct msm_clock_params params;
	int r;
	
	r = cotulla_clk_enable(id);
	if (r != -1)
		return r;

	params = msm_clk_get_params(id);

	if ( id == IMEM_CLK || id == GRP_CLK ) {
		set_grp_clk( 1 );
		writel(readl(params.glbl) | (1U << params.idx), params.glbl);
		return 0;
	}

	if (params.idx > 0 && params.glbl > 0) {
		writel(readl(params.glbl) | (1U << params.idx), params.glbl);
		return 0;
	} else if (params.ns_only > 0 && params.offset) {
		writel((readl(MSM_CLK_CTL_BASE + params.offset) & 0xfffff000) | params.ns_only, MSM_CLK_CTL_BASE + params.offset);
		return 0;
	}
	
	return 0;
}

void clk_disable(unsigned id)
{
	struct msm_clock_params params;
	params = msm_clk_get_params(id);
	int r;

    r = cotulla_clk_disable(id);
    if (r != -1)
		return;

	if ( id == IMEM_CLK || id == GRP_CLK ) {
		set_grp_clk( 1 );
		writel(readl(params.glbl) & ~(1U << params.idx), params.glbl);
		return;
	}

	if (params.idx > 0 && params.glbl > 0) {
		writel(readl(params.glbl) & ~(1U << params.idx), params.glbl);
	} else if (params.ns_only > 0 && params.offset)	{
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & 0xfffff000, MSM_CLK_CTL_BASE + params.offset);
	}
}
/* 
int clk_enable(unsigned id)
{
	if (id == ACPU_CLK)
		return -1;

	enter_critical_section();
	pc_clk_enable(id);
	exit_critical_section();
	
	return 0;
}

void clk_disable(unsigned id)
{	
	enter_critical_section();
	pc_clk_disable(id);
	exit_critical_section();
}
 */
int clk_set_rate(unsigned id, unsigned freq)
{
	int retval = 0;
    retval = cotulla_clk_set_rate(id, freq);
	if (retval != -1)
		return retval;
		
	retval = set_mdns_host_clock(id, freq);
		
	return retval;
}

unsigned long msm_clk_get_rate(uint32_t id)
{
	unsigned long rate = 0;
	rate = cotulla_clk_get_rate(id);
	if(rate == 0) {
		switch (id) {
			case SDC1_CLK:
			case SDC2_CLK:
			case SDC3_CLK:
			case SDC4_CLK:
			case UART1DM_CLK:
			case UART2DM_CLK:
			case USB_HS_CLK:
			case SDAC_CLK:
			case TV_DAC_CLK:
			case TV_ENC_CLK:
			case USB_OTG_CLK:
				rate = get_mdns_host_clock(id);
				break;
			case SDC1_PCLK:
			case SDC2_PCLK:
			case SDC3_PCLK:
			case SDC4_PCLK:
				rate = 64000000;
				break;
			default:
				rate = 0;
		}
	}
	
	return rate;
}

int msm_clk_set_flags(uint32_t id, unsigned long flags)
{
	int r;
    r = cotulla_clk_set_flags(id, flags);
	if (r != -1)
		return r;
		
	return 0;
}

int msm_clk_is_enabled(uint32_t id)
{
	int is_enabled = 0;
	unsigned bit;
	uint32_t glbl;
	glbl = msm_clk_get_glbl(id);
	bit = msm_clk_enable_bit(id);
	if (bit > 0 && glbl>0) {
		is_enabled = (readl(glbl) & bit) != 0;
	}
	
	if (id==SDC1_PCLK || id==SDC2_PCLK || id==SDC3_PCLK || id==SDC4_PCLK)
		is_enabled = 1;
		
	return is_enabled;
}

int msm_pll_request(unsigned id, unsigned on)
{
	on = !!on;
	
	return msm_proc_comm(PCOM_CLKCTL_RPC_PLL_REQUEST, &id, &on);
}

void msm_clock_init(void)
{
	/* ?struct clk *clk;

	spin_lock_init(&clocks_lock);
	mutex_lock(&clocks_mutex);
	for (clk = msm_clocks; clk && clk->name; clk++) {
		list_add_tail(&clk->list, &clocks);
	}
	mutex_unlock(&clocks_mutex); */
}