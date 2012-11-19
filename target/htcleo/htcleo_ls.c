/* 
* koko: Porting light sensor support for HTC LEO to LK
* 		Base code copied from Cotulla's board-htcleo-ls.c
*
* Copyright (C) 2010 Cotulla
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
#include <debug.h>
#include <kernel/mutex.h>
#include <kernel/timer.h>
#include <kernel/thread.h>
#include <target/microp.h>
#include <platform/interrupts.h>
#include <platform/irqs.h>
#include <platform/gpio.h>
#include <platform/timer.h>
#include <msm_i2c.h>
#include <target/board_htcleo.h>

#define LSENSOR_POLL_PROMESHUTOK   1000

struct mutex api_lock;
static struct timer lsensor_poll_timer;

static uint16_t lsensor_adc_table[10] = 
{
	0, 5, 20, 70, 150, 240, 330, 425, 515, 590
};

static struct lsensor_data 
{
	uint32_t old_level;
	int enabled;
	int opened;
} the_data;

// range: adc: 0..1023, value: 0..9
static void map_adc_to_level(uint32_t adc, uint32_t *value)
{
	int i;

	if (adc > 1024) {
		*value = 5; // set some good value at error
		return;
	}

	for (i = 9; i >= 0; i--) {
		if (adc >= lsensor_adc_table[i]) {
			*value = i;
			return;
		}
	}
	*value = 5;  // set some good value at error
}

int lightsensor_read_value(uint32_t *val)
{
	int ret;
	uint8_t data[2];

	if (!val)
		return -1;

	ret = microp_i2c_read(MICROP_I2C_RCMD_LSENSOR, data, 2);
	if (ret < 0) {
		return -1;
	}

	*val = data[1] | (data[0] << 8);
	
	return 0;
}

static enum handler_return lightsensor_poll_function(struct timer *timer, time_t now, void *arg)
{
	struct lsensor_data* p = &the_data;
	uint32_t adc = 0, level = 0;

	if (!p->enabled)
		goto error;
      
	lightsensor_read_value(&adc);
	map_adc_to_level(adc, &level);
	if (level != the_data.old_level) {
		the_data.old_level = level;
		htcleo_panel_set_brightness(level == 0 ? 1 : level);
	}
	timer_set_oneshot(timer, 500, lightsensor_poll_function, NULL);

error:
	return INT_RESCHEDULE;
}


static int lightsensor_enable(void)
{
	struct lsensor_data* p = &the_data;
	int rc = -1;

	if (p->enabled)
		return 0;
	
	rc = capella_cm3602_power(LS_PWR_ON, 1);
	if (rc < 0)
		return -1;
	
	the_data.old_level = -1;
	p->enabled = 1;

	return 0;
}

static int lightsensor_disable(void)
{
	struct lsensor_data* p = &the_data;
	int rc = -1;

	if (!p->enabled)
		return 0;
	
	rc = capella_cm3602_power(LS_PWR_ON, 0);
	if (rc < 0)
		return -1;

	p->enabled = 0;
	timer_cancel(&lsensor_poll_timer);
	
	return 0;
}


static int lightsensor_open(void *arg)
{
	int rc = 0;
	
	//mutex_acquire(&api_lock);
	
	if (the_data.opened)
		rc = -1;

	the_data.opened = 1;
	
	//mutex_release(&api_lock);
	
	return rc;
}

static int lightsensor_release(void *arg)
{
	//mutex_acquire(&api_lock);
	
	the_data.opened = 0;
	
	//mutex_release(&api_lock);
	
	return 0;
}

int htcleo_lsensor_probe(void)
{       
	int ret = -1;    
	
	the_data.old_level = -1;
	the_data.enabled=0;
	the_data.opened=0;
	
	//mutex_init(&api_lock);
	
	ret = lightsensor_enable();
	if (ret) {
		printf("lightsensor_enable failed\n");
		return -1;
	}

	enter_critical_section();
	timer_initialize(&lsensor_poll_timer);
	timer_set_oneshot(&lsensor_poll_timer, 0, lightsensor_poll_function, NULL);
	exit_critical_section();
	
	return ret;
}
