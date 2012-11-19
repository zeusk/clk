/*
 * board_htcleo.c
 * Provides support for the HTC HD2 (codename leo)
 *
 * Copyright (C) 2011-2012 Shantanu Gupta <shans95g@gmail.com>
 * Copyright (C) 2010-2011 Alexander Tarasikov <alexander.tarasikov@gmail.com>
 * Copyright (C) 2007-2011 htc-linux.org and XDANDROID project
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <array.h>
#include <bits.h>
#include <board.h>
#include <bootreason.h>
#include <debug.h>
#include <platform.h>
#include <reg.h>
#include <smem.h>
#include <string.h>
#include <malloc.h>
#include <target.h>
#include <arch/mtype.h>
#include <arch/ops.h>
#include <dev/keys.h>
#include <dev/gpio.h>
#include <dev/gpio_keypad.h>
#include <dev/ds2746.h>
#include <dev/udc.h>
#include <dev/fbcon.h>
#include <dev/flash.h>
#include <kernel/thread.h>
#include <lib/ptable.h>
#include <lib/devinfo.h>
#include <platform/gpio.h>
#include <platform/iomap.h>
#include <platform/irqs.h>
#include <platform/interrupts.h>
#include <platform/timer.h>
#include <target/acpuclock.h>
#include <target/board_htcleo.h>
#include <target/clock.h>
#include <target/display.h>
#include <target/gpio_keys.h>
#include <target/hsusb.h>
#include <target/microp.h>
#include <sys/types.h>
#include <msm_i2c.h>
#include <pcom.h>
#include <target/display.h>
#include <mmc.h>
#include <adm.h>
#include <target/reg.h>

/*******************************************************************************
 * i2C
 ******************************************************************************/
static void msm_set_i2c_mux(int mux_to_i2c) {
	if (mux_to_i2c) {
		pcom_gpio_tlmm_config(MSM_GPIO_CFG(GPIO_I2C_CLK, 0, MSM_GPIO_CFG_OUTPUT, MSM_GPIO_CFG_NO_PULL, MSM_GPIO_CFG_8MA), 0);
		pcom_gpio_tlmm_config(MSM_GPIO_CFG(GPIO_I2C_DAT, 0, MSM_GPIO_CFG_OUTPUT, MSM_GPIO_CFG_NO_PULL, MSM_GPIO_CFG_8MA), 0);
	} else {
		pcom_gpio_tlmm_config(MSM_GPIO_CFG(GPIO_I2C_CLK, 1, MSM_GPIO_CFG_INPUT, MSM_GPIO_CFG_NO_PULL, MSM_GPIO_CFG_2MA), 0);
		pcom_gpio_tlmm_config(MSM_GPIO_CFG(GPIO_I2C_DAT, 1, MSM_GPIO_CFG_INPUT, MSM_GPIO_CFG_NO_PULL, MSM_GPIO_CFG_2MA), 0);
	}
}

static struct msm_i2c_pdata i2c_pdata = {
	.i2c_clock = 400000,
	.clk_nr	= I2C_CLK,
	.irq_nr = INT_PWB_I2C,
	.scl_gpio = GPIO_I2C_CLK,
	.sda_gpio = GPIO_I2C_DAT,
	.set_mux_to_i2c = &msm_set_i2c_mux,
	.i2c_base = (void*)MSM_I2C_BASE,
};

void msm_i2c_init(void)
{
	msm_i2c_probe(&i2c_pdata);
}

/*******************************************************************************
 * MicroP
 ******************************************************************************/
int msm_microp_i2c_status = 0;
static struct microp_platform_data microp_pdata = {
	.chip = MICROP_I2C_ADDR,
	.gpio_reset = HTCLEO_GPIO_UP_RESET_N,
};

void msm_microp_i2c_init(void)
{
	microp_i2c_probe(&microp_pdata);
}

/*******************************************************************************
 * Vibrator
 ******************************************************************************/
void htcleo_vibrator(int enable)
{
	gpio_set(HTCLEO_GPIO_VIBRATOR_ON, !!enable);
}

void htcleo_vibrate_once(void)
{
    htcleo_vibrator(1);
	mdelay(100);
	htcleo_vibrator(0);
}
/*******************************************************************************
 * Lights
 ******************************************************************************/

/******************************** Keypad **************************************/
void htcleo_key_bkl_pwr(int enable)
{
	gpio_set(HTCLEO_GPIO_KP_LED, !!enable);
}

/***************************** Notification ***********************************/
void htcleo_led_set_mode(uint8_t mode)
{
/* Mode
 * 0 => All off
 * 1 => Solid green
 * 2 => Solid amber
 */
	uint8_t data[4];
	
	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = 0;
	
	switch(mode) {
		case 0x01:
			htcleo_notif_led_mode = 1;
			data[1] = 0x01;
			break;
		case 0x02:
			htcleo_notif_led_mode = 2;
			data[1] = 0x02;
			break;
		case 0x00:
		default:
			htcleo_notif_led_mode = 0;
			data[1] = 0x00;
	}
	
	microp_i2c_write(MICROP_I2C_WCMD_LED_CTRL, data, 2);
}

// Useful for calling htcleo_led_set_mode in thread_create()
int htcleo_notif_led_set_mode(void *arg)
{
	htcleo_led_set_mode((int)arg);
	
	thread_exit(0);
	return 0;
}

/******************************** Display *************************************/
int htcleo_panel_status = 0;
static void htcleo_panel_bkl_pwr(int enable)
{
	htcleo_panel_status = enable;
	uint8_t data[1];
	data[0] = !!enable;
	microp_i2c_write(MICROP_I2C_WCMD_BL_EN, data, 1);
}

static void htcleo_panel_bkl_level(uint8_t value)
{
	uint8_t data[1];
	data[0] = value << 4;
	microp_i2c_write(MICROP_I2C_WCMD_LCM_BL_MANU_CTL, data, 1);
}

void htcleo_panel_set_brightness(int val)
{
	if (val > 9) val = 9;
	if (val < 1) {
		if (htcleo_panel_status != 0)
			htcleo_panel_bkl_pwr(0);
		return;
	} else {
		if (htcleo_panel_status == 0)
			htcleo_panel_bkl_pwr(1);
		htcleo_panel_bkl_level((uint8_t)val);
		return;
	}
}

/*******************************************************************************
 * LCD
 ******************************************************************************/
struct fbcon_config *lcdc_init(void);
void htcleo_display_init(void)
{
	if (msm_microp_i2c_status) // Set brightness to a desired level
		htcleo_panel_set_brightness((device_info.panel_brightness == 0 ? 5 : device_info.panel_brightness));
	struct fbcon_config *fb_conf = lcdc_init();
	fbcon_setup(fb_conf, device_info.inverted_colors);
}

void lcdc_shutdown(void);
void htcleo_display_shutdown(void)
{
    fill_screen(0x0000);
	fbcon_teardown();
	lcdc_shutdown();
}

/*******************************************************************************
 * FLASHLIGHT
 ******************************************************************************/
bool key_listen;
int htcleo_flashlight(void *arg)
{
	thread_set_priority(HIGHEST_PRIORITY);
	gpio_set(HTCLEO_GPIO_FLASHLIGHT_TORCH, 1);
	volatile unsigned *bank6_in  = (unsigned int*)(0xA9000864);
	volatile unsigned *bank6_out = (unsigned int*)(0xA9000814);
	for(;;)	{
		if(keys_get_state(KEY_BACK)!=0) {
			gpio_set(HTCLEO_GPIO_FLASHLIGHT_TORCH, 0);
			break;
		}
		*bank6_out = *bank6_in ^ 0x200000;
		udelay(500);
	}
#if TIMER_HANDLES_KEY_PRESS_EVENT
	key_listen = 0; // this should also make key_listener thread to exit
#endif
	thread_set_priority(LOWEST_PRIORITY);

	thread_exit(0);
	return 0;
}

/*******************************************************************************
 * Touch Screen
 ******************************************************************************/
int htcleo_ts_probe(void);
void htcleo_ts_init(void) {
	htcleo_ts_probe();
}

/*******************************************************************************
 * Light Sensor
 ******************************************************************************/
int htcleo_lsensor_probe(void);
void htcleo_ls_init(void) {
	htcleo_lsensor_probe();
}

/*******************************************************************************
 * USB
 ******************************************************************************/
static bool htcleo_get_vbus_state(void) {
	return !!readl(MSM_SHARED_BASE + 0xef20c);
}

static bool htcleo_usb_online(void) {
	return htcleo_get_vbus_state(); // !gpio_get(HTCLEO_GPIO_BATTERY_OVER_CHG);
}

static bool htcleo_ac_online(void) {
	return !!((readl(USB_PORTSC) & PORTSC_LS) == PORTSC_LS);
}

static bool htcleo_want_charging(void) {
	uint32_t voltage = ds2746_voltage(DS2746_I2C_SLAVE_ADDR);
	return (voltage < DS2746_HIGH_VOLTAGE/* htcleo_chg_voltage_threshold */);
}

static int chgr_state = -1;
static void htcleo_set_charger(enum psy_charger_state state)
{
	chgr_state = state;
	switch (state) {
		case CHG_USB_LOW:
			gpio_set(HTCLEO_GPIO_BATTERY_CHARGER_ENABLE, 0);
			gpio_config(HTCLEO_GPIO_BATTERY_CHARGER_ENABLE, GPIO_OUTPUT);
			gpio_set(HTCLEO_GPIO_BATTERY_CHARGER_CURRENT, 0);
			gpio_config(HTCLEO_GPIO_BATTERY_CHARGER_CURRENT, GPIO_OUTPUT);
			break;
		case CHG_AC: case CHG_USB_HIGH:
			gpio_set(HTCLEO_GPIO_BATTERY_CHARGER_CURRENT, 1);
			gpio_config(HTCLEO_GPIO_BATTERY_CHARGER_CURRENT, GPIO_OUTPUT);
			gpio_set(HTCLEO_GPIO_BATTERY_CHARGER_ENABLE, 0);
			gpio_config(HTCLEO_GPIO_BATTERY_CHARGER_ENABLE, GPIO_OUTPUT);
			break;
		case CHG_OFF_FULL_BAT: // zzz
		case CHG_OFF:
		default:	
			// 0 enable; 1 disable;
			gpio_set(HTCLEO_GPIO_BATTERY_CHARGER_ENABLE, 1);
			gpio_config(HTCLEO_GPIO_BATTERY_CHARGER_ENABLE, GPIO_OUTPUT);
			gpio_set(HTCLEO_GPIO_BATTERY_CHARGER_CURRENT, 1);
			gpio_config(HTCLEO_GPIO_BATTERY_CHARGER_CURRENT, GPIO_OUTPUT);
			/*gpio_set(HTCLEO_GPIO_POWER_USB, 0);
			gpio_config(HTCLEO_GPIO_POWER_USB, GPIO_OUTPUT);*/
			break;
	}
}

static int htcleo_charger_state(void) {
	return chgr_state;
}

/*
 * koko: ..._chg_voltage_threshold is used along ds2746 driver
 *		 to detect when the battery is FULL.
 *		 Consider setting it to a value lower than DS2746_HIGH_VOLTAGE.
 *		 Avoiding full charge has benefits, and some manufacturers set
 *		 the charge threshold lower on purpose to prolong battery life.
 * ######################################################################
 * #			Typical charge characteristics of lithium-ion			#
 * ######################################################################
 * # Charge V/cell #   Capacity at	 # Charge time # Capacity with full #
 * #			   # cut-off voltage #			   #	 saturation     #
 * ######################################################################
 * #	 3.80	   #		60%		 #	 120 min   #		65%			#
 * #	 3.90	   #		70%		 #	 135 min   #		76%			#
 * #	 4.00	   #		75%		 #	 150 min   #		82%			#
 * #	 4.10	   #		80%		 #	 165 min   #		87%			#
 * #	 4.20	   #		85%		 #	 180 min   #	   100%			#
 * ######################################################################
 */
static uint32_t default_chg_voltage_threshold[] =
{
	3800,
	3900,
	4000,
	4100,
	4200,//DS2746_HIGH_VOLTAGE
};

void htcleo_charger_on(void) {
	uint32_t voltage = ds2746_voltage(DS2746_I2C_SLAVE_ADDR);

	if (voltage < (uint32_t)htcleo_chg_voltage_threshold) {
		// If battery needs charging, set new charger state
		if (htcleo_ac_online()) {
			if (htcleo_charger_state() != CHG_AC ) {
				writel(0x00080000, USB_USBCMD);
				ulpi_write(0x48, 0x04);
				htcleo_set_charger(CHG_AC);
			}
		} else {
			if (htcleo_charger_state() != CHG_USB_LOW ) {
				writel(0x00080001, USB_USBCMD);
				thread_sleep(10);
				htcleo_set_charger(CHG_USB_LOW);
			}
		}
		// Led = solid amber
		if (htcleo_notif_led_mode != 2)
			htcleo_led_set_mode(2);
	} else {
		// Battery is full, set charger state to CHG_OFF_FULL_BAT
		if (htcleo_charger_state() != CHG_OFF_FULL_BAT ) {
			writel(0x00080001, USB_USBCMD);
			thread_sleep(10);
			htcleo_set_charger(CHG_OFF_FULL_BAT);
		}
		// and turn led solid green
		if (htcleo_notif_led_mode != 1)
			htcleo_led_set_mode(1);
	}
}

void htcleo_charger_off(void) {
	// Cable unplugged, set charger state to CHG_OFF
	if (htcleo_charger_state() != CHG_OFF ) {
		writel(0x00080001, USB_USBCMD);
		thread_sleep(10);
		htcleo_set_charger(CHG_OFF);
	}
	// and turn off led
	if (htcleo_notif_led_mode != 0)
		htcleo_led_set_mode(0);
}

// koko: Thanks to Rick_1995 for suggesting the WFI instruction!
static void htcleo_enter_low_power_state(void) {
	/*
	 * Wait for Interrupt is a feature of the ARMv7-A architecture
	 * that puts the processor in a low power state by disabling most of the clocks in the processor
	 * while keeping the processor powered up. This reduces the power drawn to the static leakage current,
	 * leaving a small clock power overhead to enable the processor to wake up from WFI mode.
	 */
	arch_idle(); // Make the processor to enter into WFI mode by executing the WFI instruction
}

int htcleo_suspend(void *arg) {	
	int power_key_pressed = 0;
	time_t start_time, elapsed_time;
	time_t remaining_time = (time_t)arg;
	bool countdown = (remaining_time > 1);
	bool usb_cable_connected = htcleo_usb_online();
	
	if(countdown)
		htcleo_panel_bkl_pwr(0);
		
	while (!power_key_pressed && remaining_time) {
		start_time = current_time();
		power_key_pressed = keys_get_state(KEY_POWER);
		usb_cable_connected = htcleo_usb_online();
		if (usb_cable_connected) {
			htcleo_charger_on();
		} else {
			htcleo_charger_off();
			if (!countdown) {
				// If suspend_time=1, we only wanted to charge the device while turned off.
				// Not really need to enter low power state if the cable is unplugged.
				// Instead exit loop & shutdown.
				break;
			} else {
				htcleo_enter_low_power_state();				
			}
		}
		elapsed_time = current_time() - start_time;
		
		if (countdown)
			// If suspend_time!=1, we wanted to suspend the device for a specific time.
			// Handle the remaining_time so that we exit loop after the correct time.
			remaining_time -= elapsed_time;
	}
	
	// Set charger state to CHG_OFF before
	// proceeding to reboot or shutdown
	htcleo_charger_off();
	
	// Check if we exited the loop because of a Power button press
	// or because timeout exceeded, and reboot the device
	if (power_key_pressed || remaining_time == 0)
		target_reboot(0);
		
	// We reach this point ONLY IF 
	// suspend_time = 0 (meaning we were into suspend mode just for off-mode charging)
	// AND usb_cable_connected = false (the usb cable was disconnected)
	// Only thing to do is to shutdown the device..
	target_shutdown();
	
	return 0;
}

static struct pda_power_supply htcleo_power_supply = {
	.is_ac_online = &htcleo_ac_online,
	.is_usb_online = &htcleo_usb_online,
	.charger_state = &htcleo_charger_state,
	.set_charger_state = &htcleo_set_charger,
	.want_charging = &htcleo_want_charging,
};

static struct msm_hsusb_pdata htcleo_hsusb_pdata = {
	.set_ulpi_state = NULL,
	.power_supply = &htcleo_power_supply,
};

void htcleo_usb_init(void)
{
	msm_hsusb_init(&htcleo_hsusb_pdata);
}

static struct udc_device htcleo_udc_device[] = {
	// Testing stuff...
	{
		.vendor_id = 0x18d1,
		.product_id = 0xD00D,
		.version_id = 0x0100,
		.manufacturer = "HTC Corporation",
		.product = "HD2",
	},
	{
		.vendor_id = 0x18d1,
		.product_id = 0x0D02,
		.version_id = 0x0001,
		.manufacturer = "Google",
		.product = "Android",
	},
	{
		.vendor_id = 0x0bb4,
		.product_id = 0x0C02,
		.version_id = 0x0100,
		.manufacturer = "HTC",
		.product = "Android Phone",
	},
	{
		.vendor_id = 0x05c6,
		.product_id = 0x9026,
		.version_id = 0x0100,
		.manufacturer = "Qualcomm Incorporated",
		.product = "Qualcomm HSUSB Device",
	},
};

void htcleo_udc_init(void)
{
	udc_init(&htcleo_udc_device[device_info.udc]);
}

/*******************************************************************************
 * GPIO Keys
 ******************************************************************************/
struct timer keyp_bkl;
static unsigned keys_pressed[3] = {0, 0, 0};
static enum handler_return keys_bkl_off(struct timer *timer, time_t now, void *arg)
{
	timer_cancel(timer);
	htcleo_key_bkl_pwr(0);
	
	return INT_RESCHEDULE;
}

static void htcleo_handle_key_bkl(unsigned key_code, unsigned state)
{
	if (key_code == KEY_VOLUMEUP || key_code == KEY_VOLUMEDOWN)
		return;
	
	enter_critical_section();
	if (state) {
		if ((keys_pressed[0] == 0) && (keys_pressed[1] == 0) && (keys_pressed[2] == 0))
			htcleo_key_bkl_pwr(1);

			 if (!(keys_pressed[0]))	keys_pressed[0] = key_code;
		else if (!(keys_pressed[1]))	keys_pressed[1] = key_code;
		else if (!(keys_pressed[2])) 	keys_pressed[2] = key_code;
	
		timer_cancel(&keyp_bkl);
	} else {
			 if (key_code == keys_pressed[0]) 	keys_pressed[0] = 0;
		else if (key_code == keys_pressed[1])	keys_pressed[1] = 0;
		else if (key_code == keys_pressed[2])	keys_pressed[2] = 0;

		if ((keys_pressed[0] == 0) && (keys_pressed[1] == 0) && (keys_pressed[2] == 0))
			timer_set_oneshot(&keyp_bkl, 800, keys_bkl_off, NULL);	
	}
	exit_critical_section();
}

static uint32_t key_rowgpio[] = {
	HTCLEO_GPIO_KP_MKOUT0,
	HTCLEO_GPIO_KP_MKOUT1,
	HTCLEO_GPIO_KP_MKOUT2,
};

static uint32_t key_colgpio[] = {
	HTCLEO_GPIO_KP_MPIN0,
	HTCLEO_GPIO_KP_MPIN1,
	HTCLEO_GPIO_KP_MPIN2,
};

#define ASZ(x) ARRAY_SIZE(x)
#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(key_colgpio) + (col))
static uint16_t htcleo_keymap[ASZ(key_colgpio) * ASZ(key_rowgpio)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_VOLUMEUP,	// Volume Up
	[KEYMAP_INDEX(0, 1)] = KEY_VOLUMEDOWN,	// Volume Down
	[KEYMAP_INDEX(1, 0)] = KEY_SOFT1,		// Windows Button
	[KEYMAP_INDEX(1, 1)] = KEY_SEND,		// Dial Button
	[KEYMAP_INDEX(1, 2)] = KEY_CLEAR,		// Hangup Button
	[KEYMAP_INDEX(2, 0)] = KEY_BACK,		// Back Button
	[KEYMAP_INDEX(2, 1)] = KEY_HOME,		// Home Button
};
#undef KEYMAP_INDEX

static struct gpio_keypad_info htcleo_keypad_info = {
	.keymap        	= htcleo_keymap,
	.output_gpios  	= key_rowgpio,
	.input_gpios   	= key_colgpio,
	.noutputs      	= ASZ(key_rowgpio),
	.ninputs      	= ASZ(key_colgpio),
	.settle_time   	= 40,
	.poll_time     	= 20,
	.flags        	= GPIOKPF_DRIVE_INACTIVE,
	.notify_fn     	= &htcleo_handle_key_bkl,
};
#undef ASZ

static struct gpio_key gpio_keys_tbl[] = {
	{
		.polarity 		= 1,
		.key_code 		= KEY_POWER,
		.gpio_nr 		= HTCLEO_GPIO_POWER_KEY,
		.notify_fn 		= &htcleo_handle_key_bkl,
	},
};

static struct gpio_keys_pdata htcleo_gpio_key_pdata = {
	.nr_keys = 1,
	.poll_time = 20,
	.debounce_time = 40,
	.gpio_key = &gpio_keys_tbl[0],
};

void htcleo_keypad_init(void)
{
	keys_init();
	timer_initialize(&keyp_bkl);
	gpio_keys_init(&htcleo_gpio_key_pdata);
	gpio_keypad_init(&htcleo_keypad_info);
}

/*******************************************************************************
 * clocks
 ******************************************************************************/
static int clocks_off[] = {
	MDC_CLK,
	SDC1_CLK,
	SDC2_CLK,
	SDC3_CLK,
	SDC4_CLK,
	USB_HS_CLK,
};

static int clocks_on[] = {
	IMEM_CLK,
	MDP_CLK,
	PMDH_CLK,
};

void msm_prepare_clocks(void)
{
	for (int i = 0; i < (int)ARRAY_SIZE(clocks_off); i++)
		clk_disable(clocks_off[i]);

	for (int i = 0; i < (int)ARRAY_SIZE(clocks_on); i++)
		clk_enable(clocks_on[i]);
}

/*******************************************************************************
 * SD Card
 ******************************************************************************/
int htcleo_mmc_init(void *arg)
{
	mmc_ready = 0;

#if HTCLEO_USE_QSD_SDCC == 1
	memset(&htcleo_mmc, 0, sizeof(mmc_t));
	memset(&htcleo_sdcc, 0, sizeof(sd_parms_t));
	if (SDC_INSTANCE == 1) {
		htcleo_sdcc.instance           	= 	1;
		htcleo_sdcc.base                = 	SDC1_BASE;
		htcleo_sdcc.ns_addr             = 	SDC1_NS_REG;
		htcleo_sdcc.md_addr             = 	SDC1_MD_REG;
		htcleo_sdcc.row_reset_mask      = 	ROW_RESET__SDC1___M;
		htcleo_sdcc.glbl_clk_ena_mask   = 	GLBL_CLK_ENA__SDC1_H_CLK_ENA___M;
		htcleo_sdcc.adm_crci_num        = 	ADM_CRCI_SDC1;
	}
	else if (SDC_INSTANCE == 2) {
		htcleo_sdcc.instance           	= 	2;
		htcleo_sdcc.base                = 	SDC2_BASE;
		htcleo_sdcc.ns_addr             = 	SDC2_NS_REG;
		htcleo_sdcc.md_addr             = 	SDC2_MD_REG;
		htcleo_sdcc.row_reset_mask      = 	ROW_RESET__SDC2___M;
		htcleo_sdcc.glbl_clk_ena_mask   = 	GLBL_CLK_ENA__SDC2_H_CLK_ENA___M;
		htcleo_sdcc.adm_crci_num        = 	ADM_CRCI_SDC2;
	}
	else if (SDC_INSTANCE == 3) {
		htcleo_sdcc.instance           	= 	3;
		htcleo_sdcc.base                = 	SDC3_BASE;
		htcleo_sdcc.ns_addr             = 	SDC3_NS_REG;
		htcleo_sdcc.md_addr             = 	SDC3_MD_REG;
		htcleo_sdcc.row_reset_mask      = 	ROW_RESET__SDC3___M;
		htcleo_sdcc.glbl_clk_ena_mask	= 	GLBL_CLK_ENA__SDC3_H_CLK_ENA___M;
		htcleo_sdcc.adm_crci_num        = 	ADM_CRCI_SDC3;
	}
	else if (SDC_INSTANCE == 4) {
		htcleo_sdcc.instance           	= 	4;
		htcleo_sdcc.base                = 	SDC4_BASE;
		htcleo_sdcc.ns_addr             = 	SDC4_NS_REG;
		htcleo_sdcc.md_addr             = 	SDC4_MD_REG;
		htcleo_sdcc.row_reset_mask      = 	ROW_RESET__SDC4___M;
		htcleo_sdcc.glbl_clk_ena_mask	= 	GLBL_CLK_ENA__SDC4_H_CLK_ENA___M;
		htcleo_sdcc.adm_crci_num        = 	ADM_CRCI_SDC4;
	}
	
	sprintf(htcleo_mmc.name, "SD_Card");
	htcleo_mmc.priv      	= 	&htcleo_sdcc;
	htcleo_mmc.voltages  	= 	SDCC_VOLTAGE_SUPPORTED;
	htcleo_mmc.f_min     	= 	MCLK_400KHz;
	htcleo_mmc.f_max     	= 	MCLK_48MHz;
	htcleo_mmc.host_caps 	= 	MMC_MODE_4BIT |
								MMC_MODE_HS |
								MMC_MODE_HS_52MHz;
	htcleo_mmc.read_bl_len	= 	512;
	htcleo_mmc.write_bl_len	= 	512;
	htcleo_mmc.send_cmd  	= 	sdcc_send_cmd;
	htcleo_mmc.set_ios   	= 	sdcc_set_ios;
	htcleo_mmc.init      	= 	sdcc_init;
	
	//fail
	mmc_register(&htcleo_mmc);
	mmc_init(&htcleo_mmc);
#elif HTCLEO_USE_QSD_MMC == 1
	//fail
	mmc_legacy_init(0);
#endif

	thread_exit(0);
	return 0;
}

void htcleo_mmc_deinit(void)
{
#if HTCLEO_USE_QSD_SDCC == 1
	/* zzz */
#elif HTCLEO_USE_QSD_MMC == 1
	SDCn_deinit(SDC_INSTANCE);
#endif	
}

/*******************************************************************************
 * NAND
 *******************************************************************************/
static struct ptable flash_ptable;
struct ptable flash_devinfo;
void flash_set_devinfo(struct ptable * new_ptable);

extern unsigned blocks_per_mb;
extern unsigned flash_start_blk;
extern unsigned flash_size_blk;
unsigned lk_start_blk;
unsigned nand_num_blocks;

static char buf[4096];

struct PartEntry
{
	uint8_t BootInd;
	uint8_t FirstHead;
	uint8_t FirstSector;
	uint8_t FirstTrack;
	uint8_t FileSystem;
	uint8_t LastHead;
	uint8_t LastSector;
	uint8_t LastTrack;
	uint32_t StartSector;
	uint32_t TotalSectors;
};

int find_start_block()
{
	unsigned page_size = flash_page_size();

	// Get ROMHDR partition
	struct ptentry *ptn = ptable_find( &flash_devinfo, PTN_ROMHDR );
	if ( ptn == NULL ) {
		dprintf( CRITICAL, "ERROR: ROMHDR partition not found!!!" );
		return -1;
	}

	// Set lk start block
	lk_start_blk = ptn->start;

	// Read ROM header
	if ( flash_read( ptn, 0, buf, page_size ) ) {
		dprintf( CRITICAL, "ERROR: can't read ROM header!!!\n" );
		return -1;
	}

	// Validate ROM signature
	if ( buf[0] != 0xE9 || buf[1] != 0xFD || buf[2] != 0xFF || buf[512-2] != 0x55 || buf[512-1] != 0xAA ) {
		dprintf( CRITICAL, "ERROR: invalid ROM signature!!!\n" );
		return -1;
	}

	const char* part_buf = buf + 0x1BE;

	// Validate first partition layout
	struct PartEntry romptr;
	memcpy( &romptr, part_buf, sizeof( struct PartEntry ) );
	if ( romptr.BootInd != 0 || romptr.FileSystem != 0x23 || romptr.StartSector != 0x2 ) {
		dprintf( CRITICAL, "ERROR: invalid ROM boot partition!!!\n" );
		return -1;
	}

	// Set flash start block (block where ROM boot partition ends)
	flash_start_blk = ptn->start + ( romptr.StartSector + romptr.TotalSectors ) / 64;

	// Validate second partition layout (must be empty)
	part_buf += sizeof( struct PartEntry );
	memcpy( &romptr, part_buf, sizeof( struct PartEntry ) );
	if ( romptr.FileSystem != 0 || romptr.StartSector != 0 || romptr.TotalSectors != 0 ) {
		dprintf( CRITICAL, "ERROR: second partition detected!!!\n" );
		return -1;
	}

	// Read second sector
	if ( flash_read( ptn, page_size, buf, page_size ) ) {
		dprintf( CRITICAL, "ERROR: can't read ROM header!!!\n" );
		return -1;
	}

	// Validate MSFLASH50
	if ( memcmp( buf, "MSFLSH50", sizeof( "MSFLSH50" ) - 1 ) ) {
		dprintf( CRITICAL, "ERROR: can't find MSFLASH50 tag!!!\n" );
		return -1;
	}
		
	return 0;
}

void htcleo_flash_info_init(void)
{
	struct flash_info *flash_info;
	
	flash_init();
	flash_info = flash_get_info();

	ASSERT( flash_info );
	ASSERT( flash_info->num_blocks );

	nand_num_blocks = flash_info->num_blocks;
	blocks_per_mb	= ( 1024 * 1024 ) / flash_info->block_size;
}

void htcleo_devinfo_init(void)
{
	ptable_init( &flash_devinfo );

	// ROM header partition
	ptable_add( &flash_devinfo, PTN_ROMHDR, HTCLEO_ROM_OFFSET, 0x2, 0, TYPE_APPS_PARTITION, PERM_WRITEABLE );

	// Find free ROM start block
	if ( find_start_block() == -1 ) {
		// Display error
		if(fbcon_display() == NULL)
			htcleo_display_init();
		dprintf( CRITICAL, "Failed to find free ROM start block,\npowering off in 20s!!!\n" );
		thread_sleep( 20000 );

		// Invalid ROM partition, shutdown device
		target_shutdown();

		return;
	}

	// DEVINFO partition (1block = 128KBytes)
	ptable_add( &flash_devinfo, PTN_DEV_INF, flash_start_blk++, 1, 0, TYPE_APPS_PARTITION, PERM_WRITEABLE );

	// Calculate remaining flash size
	flash_size_blk = nand_num_blocks - flash_start_blk;

	// task29 partition (to format all partitions at once)
	ptable_add( &flash_devinfo, PTN_TASK29, flash_start_blk, flash_size_blk, 0, TYPE_APPS_PARTITION, PERM_WRITEABLE );

	flash_set_devinfo( &flash_devinfo );

	init_device();
}

void htcleo_ptable_init(void)
{
	unsigned start_blk = flash_start_blk;

	ptable_init( &flash_ptable );
	
	// Add LK as 1st partition
	ptable_add( &flash_ptable, device_info.partition[0].name, lk_start_blk, device_info.partition[0].size, 0, TYPE_APPS_PARTITION, PERM_WRITEABLE );

	// Parse the rest partitions
	for ( unsigned i = 1; i < MAX_NUM_PART; i++ ) {
		// Valid partition?
		if ( strlen( device_info.partition[i].name ) == 0 )
			break;

		// Add partition
		ptable_add( &flash_ptable, device_info.partition[i].name, start_blk, device_info.partition[i].size, 0, TYPE_APPS_PARTITION, PERM_WRITEABLE );
		
		// Skip to next free block
		start_blk += device_info.partition[i].size;
	}
	flash_set_ptable( &flash_ptable );
}

/* koko : No reboot needed for changes to take effect if we use this */
static struct ptable flash_newptable;
void htcleo_ptable_re_init(void)
{
	unsigned start_blk = flash_start_blk;
	
	ptable_init( &flash_ptable );
	ptable_init( &flash_newptable );

	// Add LK as 1st partition
	ptable_add( &flash_newptable, device_info.partition[0].name, lk_start_blk, device_info.partition[0].size, 0, TYPE_APPS_PARTITION, PERM_WRITEABLE );

	// Parse the rest partitions
	for ( unsigned i = 1; i < MAX_NUM_PART; i++ ) {
		// Valid partition?
		if ( strlen( device_info.partition[i].name ) == 0 )
			break;
	
		// Add partition
		ptable_add( &flash_newptable, device_info.partition[i].name, start_blk, device_info.partition[i].size, 0, TYPE_APPS_PARTITION, PERM_WRITEABLE );
		
		// Skip to next free block
		start_blk += device_info.partition[i].size;
	}
	flash_ptable = flash_newptable;
}

/*******************************************************************************
 * Low-level stuff
 ******************************************************************************/
// Replacement for 'htcleo_boot'
void htcleo_prepare_for_linux(void)
{
	// Martijn Stolk's code so kernel will not crash. aux control register
	__asm__ volatile("MRC p15, 0, r0, c1, c0, 1\n"
					 "BIC r0, r0, #0x40\n"
					 "BIC r0, r0, #0x200000\n"
					 "MCR p15, 0, r0, c1, c0, 1");

	// Disable VFP
	__asm__ volatile("MOV R0, #0\n"
					 "FMXR FPEXC, r0");

    // disable mmu
	__asm__ volatile("MRC p15, 0, r0, c1, c0, 0\n"
					 "BIC r0, r0, #(1<<0)\n"
					 "MCR p15, 0, r0, c1, c0, 0\n"
					 "ISB");
	
	// Invalidate the UTLB
	__asm__ volatile("MOV r0, #0\n"
					 "MCR p15, 0, r0, c8, c7, 0");

	// Clean and invalidate cache - Ensure pipeline flush
	__asm__ volatile("MOV R0, #0\n"
					 "DSB\n"
					 "ISB");

	__asm__ volatile("BX LR");
}

/*
 * koko: Decide cpu freq at startup.
 *		 ALWAYS call after htcleo_devinfo_init()
 */
void htcleo_acpu_clock_init(void)
{
	/* if off-mode charging then
	 *		boot at 245MHz (or 384MHz) */
	if (device_info.use_inbuilt_charging) {
		if((htcleo_boot_mark != MARK_BUTTON) && (htcleo_boot_mark != MARK_RESET)) {
			htcleo_pause_for_battery_charge = 1;
			msm_acpu_clock_init(0); // 0 => 245MHz, 1 => 384MHz
			return;
		}
	}
	/* if suspend-mode for alarm then
	 *  	boot at 245MHz (or 384MHz) */
	if ((target_check_reboot_mode() & 0xFF000000) == MARK_ALARM_TAG) {
		msm_acpu_clock_init(0); // 0 => 245MHz, 1 => 384MHz
	}
	/* if entering cLK check device_info.cpu_freq:
	 *  if device_info.cpu_freq = 0 then
	 *  	boot at 768MHz (acpu_freq_tbl[11])
	 *  if device_info.cpu_freq = 1 then
	 *  	boot at 998MHz (acpu_freq_tbl[17]) */
	else if (target_check_reboot_mode() == FASTBOOT_MODE) {
		msm_acpu_clock_init(11 + (device_info.cpu_freq * 6));
	}
	/* In any other case
	 * give a little boost to 998MHz */
	else {
		msm_acpu_clock_init(17);
	}
}

/******************************************************************************
 * Export htcleo board
 *****************************************************************************/
static void htcleo_early_init(void) {
	//cedesmith: write reset vector while we can as MPU kicks in after flash_init();
	writel(0xe3a00546, 0);
	writel(0xe590f004, 4);
}

static void htcleo_init(void) {
	htcleo_notif_led_mode = 0;
	htcleo_pause_for_battery_charge = 0;
	htcleo_boot_mark = readl(SPL_BOOT_REASON_ADDR);
	htcleo_reboot_mode = target_check_reboot_mode();
	htcleo_keypad_init();
	htcleo_flash_info_init();
	htcleo_devinfo_init();
	htcleo_chg_voltage_threshold = default_chg_voltage_threshold[device_info.chg_threshold];
	htcleo_acpu_clock_init();
	htcleo_ptable_init();
}

static void htcleo_exit(void) {
	if (mmc_ready != 0)
		htcleo_mmc_deinit();
	htcleo_prepare_for_linux();
}

unsigned *htcleo_atag(unsigned *ptr)
{
	// Nand ATAG
	*ptr++ = 4;
	*ptr++ = 0x4C47414D;
	*ptr++ = 0x004b4c63;
	*ptr++ = 10;

	return ptr;
}

struct board_mach htcleo = {
	.early_init      = (void *)   htcleo_early_init,
	.init            = (void *)   htcleo_init,
	.exit            = (void *)	  htcleo_exit,
	.atag            = (void *)	  htcleo_atag,
	.flashlight      = (void *)	  htcleo_flashlight,
	.mach_type       = MACH_TYPE_HTCLEO,
	.cmdline         = (char *)   "console=null",
	.scratch_address = (void *)   0x12C00000,
	.scratch_size    = (unsigned) 0x15300000,
};

/************************************ END **************************************/
