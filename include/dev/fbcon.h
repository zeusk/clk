/*
 * Copyright (c) 2008, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef __DEV_FBCON_H
#undef __DEV_FBCON_H
#endif

#define __DEV_FBCON_H

#ifndef FB_FORMAT_RGB565
#define FB_FORMAT_RGB565 1
#endif
#ifndef FB_FORMAT_RGB888
#define FB_FORMAT_RGB888 0
#endif

struct fbcon_config {
	void		*base;
	unsigned	width;
	unsigned	height;
	unsigned	stride;
	unsigned	bpp;
	unsigned	format;
	void		(*update_start)(void);
	int		(*update_done)(void);
};
struct fbcon_config* fbcon_display(void);

int  fbcon_get_y_cord(void);
int fbcon_get_x_cord(void);
uint16_t fbcon_get_bgcol(void);

bool didyouscroll(void);

void fbcon_setup(struct fbcon_config *cfg);
void fbcon_putc(char c);
void fbcon_clear(void);
void fbcon_resetdisp(void);
void fbcon_flush(void);
void fbcon_push(void);
void fbcon_setfg(unsigned fg);
void fbcon_setbg(unsigned bg);
void fbcon_settg(unsigned tg);
void fbcon_disp_logo(void);
void fbcon_clear_region(int start_y, int end_y);
void fbcon_set_y_cord(int offset);
void fbcon_set_x_cord(int offset);
void fbcon_inits(char *amss_ver, char *Menuid);
void fill_screen(uint16_t COLOR);
void fbcon_set_txt_colors(unsigned fgcolor, unsigned tgcolor);
void fbcon_reset_colors_rgb555(void);
void fbcon_forcetg(bool flag_boolean);
