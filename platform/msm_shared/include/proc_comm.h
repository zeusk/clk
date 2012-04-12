#ifndef _PROC_COMM_H_
#define _PROC_COMM_H_

#include <reg.h>

#define ACPU_CLK           0  /* Applications processor clock */
#define ADM_CLK            1  /* Applications data mover clock */
#define ADSP_CLK           2  /* ADSP clock */
#define EBI1_CLK           3  /* External bus interface 1 clock */
#define EBI2_CLK           4  /* External bus interface 2 clock */
#define ECODEC_CLK         5  /* External CODEC clock */
#define EMDH_CLK           6  /* External MDDI host clock */
#define GP_CLK             7  /* General purpose clock */
#define GRP_CLK            8  /* Graphics clock */
#define I2C_CLK            9  /* I2C clock */
#define ICODEC_RX_CLK     10  /* Internal CODEX RX clock */
#define ICODEC_TX_CLK     11  /* Internal CODEX TX clock */
#define IMEM_CLK          12  /* Internal graphics memory clock */
#define MDC_CLK           13  /* MDDI client clock */
#define MDP_CLK           14  /* Mobile display processor clock */
#define PBUS_CLK          15  /* Peripheral bus clock */
#define PCM_CLK           16  /* PCM clock */
#define PMDH_CLK          17  /* Primary MDDI host clock */
#define SDAC_CLK          18  /* Stereo DAC clock */
#define SDC1_CLK          19  /* Secure Digital Card clocks */
#define SDC1_PCLK         20
#define SDC2_CLK          21
#define SDC2_PCLK         22
#define SDC3_CLK          23 
#define SDC3_PCLK         24 
#define SDC4_CLK          25
#define SDC4_PCLK         26
#define TSIF_CLK          27  /* Transport Stream Interface clocks */
#define TSIF_REF_CLK      28
#define TV_DAC_CLK        29  /* TV clocks */
#define TV_ENC_CLK        30
#define UART1_CLK         31  /* UART clocks */
#define UART2_CLK         32
#define UART3_CLK         33
#define UART1DM_CLK       34
#define UART2DM_CLK       35
#define USB_HS_CLK        36  /* High speed USB core clock */
#define USB_HS_PCLK       37  /* High speed USB pbus clock */
#define USB_OTG_CLK       38  /* Full speed USB clock */
#define VDC_CLK           39  /* Video controller clock */
#define VFE_CLK           40  /* Camera / Video Front End clock */
#define VFE_MDC_CLK       41  /* VFE MDDI client clock */

/* qsd8k adds... */
#define MDP_LCDC_PCLK_CLK     42
#define MDP_LCDC_PAD_PCLK_CLK 43
#define MDP_VSYNC_CLK         44

#define P_USB_HS_CORE_CLK     53  /* High speed USB 1 core clock */
/* msm7x30 adds... */
#define PMDH_P_CLK            82
#define MDP_P_CLK             86

enum {
	PCOM_CMD_IDLE = 0x0,
	PCOM_CMD_DONE,
	PCOM_RESET_APPS,
	PCOM_RESET_CHIP,
	PCOM_CONFIG_NAND_MPU,
	PCOM_CONFIG_USB_CLKS,
	PCOM_GET_POWER_ON_STATUS,
	PCOM_GET_WAKE_UP_STATUS,
	PCOM_GET_BATT_LEVEL,
	PCOM_CHG_IS_CHARGING,
	PCOM_POWER_DOWN,
	PCOM_USB_PIN_CONFIG,
	PCOM_USB_PIN_SEL,
	PCOM_SET_RTC_ALARM,
	PCOM_NV_READ,
	PCOM_NV_WRITE,
	PCOM_GET_UUID_HIGH,
	PCOM_GET_UUID_LOW,
	PCOM_GET_HW_ENTROPY,
	PCOM_RPC_GPIO_TLMM_CONFIG_REMOTE,
	PCOM_CLKCTL_RPC_ENABLE,
	PCOM_CLKCTL_RPC_DISABLE,
	PCOM_CLKCTL_RPC_RESET,
	PCOM_CLKCTL_RPC_SET_FLAGS,
	PCOM_CLKCTL_RPC_SET_RATE,
	PCOM_CLKCTL_RPC_MIN_RATE,
	PCOM_CLKCTL_RPC_MAX_RATE,
	PCOM_CLKCTL_RPC_RATE,
	PCOM_CLKCTL_RPC_PLL_REQUEST,
	PCOM_CLKCTL_RPC_ENABLED,
	PCOM_VREG_SWITCH,
	PCOM_VREG_SET_LEVEL,
	PCOM_GPIO_TLMM_CONFIG_GROUP,
	PCOM_GPIO_TLMM_UNCONFIG_GROUP,
	PCOM_NV_READ_HIGH_BITS,
	PCOM_NV_WRITE_HIGH_BITS,
	PCOM_RPC_GPIO_TLMM_CONFIG_EX = 0x25,
	PCOM_RESERVED_101 = 0x65,
	PCOM_MSM_HSUSB_PHY_RESET,
	PCOM_GET_BATT_MV_LEVEL,
	PCOM_CHG_USB_IS_PC_CONNECTED,
	PCOM_CHG_USB_IS_CHARGER_CONNECTED,
	PCOM_CHG_USB_IS_DISCONNECTED,
	PCOM_CHG_USB_I_AVAILABLE,
	PCOM_NUM_CMDS,
};

enum {
	 PCOM_INVALID_STATUS = 0x0,
	 PCOM_READY,
	 PCOM_CMD_RUNNING,
	 PCOM_CMD_SUCCESS,
	 PCOM_CMD_FAIL,
};

#ifndef PLATFORM_MSM7X30
#define MSM_A2M_INT(n) (MSM_CSR_BASE + 0x400 + (n) * 4)
#endif

#define APP_COMMAND (MSM_SHARED_BASE + 0x00)
#define APP_STATUS  (MSM_SHARED_BASE + 0x04)
#define APP_DATA1   (MSM_SHARED_BASE + 0x08)
#define APP_DATA2   (MSM_SHARED_BASE + 0x0C)

#define MDM_COMMAND (MSM_SHARED_BASE + 0x10)
#define MDM_STATUS  (MSM_SHARED_BASE + 0x14)
#define MDM_DATA1   (MSM_SHARED_BASE + 0x18)
#define MDM_DATA2   (MSM_SHARED_BASE + 0x1C)

int msm_proc_comm(unsigned cmd, unsigned *data1, unsigned *data2);
int clock_enable(unsigned id);
int clock_disable(unsigned id);
int clock_set_rate(unsigned id, unsigned rate);
int clock_get_rate(unsigned id);

void usb_clock_init();
void lcdc_clock_init(unsigned rate);
void mdp_clock_init (unsigned rate);
void uart3_clock_init(void);
void uart2_clock_init(void);
void mddi_clock_init(unsigned num, unsigned rate);

void reboot(unsigned reboot_reason);
void shutdown(void);

int charger_usb_is_pc_connected(void);
int charger_usb_is_charger_connected(void);
int charger_usb_disconnected(void);
int charger_usb_i(unsigned current);

int mmc_clock_enable_disable(unsigned id, unsigned enable);
int mmc_clock_set_rate(unsigned id, unsigned rate);
int mmc_clock_get_rate(unsigned id);

int gpio_tlmm_config(unsigned config, unsigned disable);
int vreg_set_level(unsigned id, unsigned mv);
int vreg_enable(unsigned id);
int vreg_disable(unsigned id);

#endif /*__PROC_COMM_H_ */

