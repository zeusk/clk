/*
 * Copyright (c) 2012, Shantanu Gupta <shans95g@gmail.com>
 */
 
#ifndef __DEV_GPIO_KEY_H
#define __DEV_GPIO_KEY_H

struct gpio_key {
	unsigned gpio_nr;
	unsigned key_code;
	unsigned polarity;
	/*
	 * Can be used by board to be notified without using extra resources
	 * Used for key backlight, power off etc..
	 */
	void (*notify_fn)(unsigned key_code, unsigned state);
};

struct gpio_keys_pdata {
	struct gpio_key *gpio_key;
	unsigned nr_keys;
	unsigned poll_time;
	unsigned debounce_time;
};

void gpio_keys_init(struct gpio_keys_pdata *kpdata);

#endif /* __DEV_GPIO_KEY_H */