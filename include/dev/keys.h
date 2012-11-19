/*
 * Copyright (c) 2009-2010, Google Inc. All rights reserved.
 * Copyright (c) 2011-2012, Shantanu Gupta <shans95g@gmail.com>
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
 *  * Neither the name of Google, Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef __DEV_KEYS_H
#define __DEV_KEYS_H

#include <sys/types.h>

/* these are just the ascii values for the chars */
#define KEY_ENTER	0x000a

#define KEY_0		0x0030
#define KEY_1		0x0031
#define KEY_2		0x0032
#define KEY_3		0x0033
#define KEY_4		0x0034
#define KEY_5		0x0035
#define KEY_6		0x0036
#define KEY_7		0x0037
#define KEY_8		0x0038
#define KEY_9		0x0039

#define KEY_A		0x0061
#define KEY_B		0x0062
#define KEY_C		0x0063
#define KEY_D		0x0064
#define KEY_E		0x0065
#define KEY_F		0x0066
#define KEY_G		0x0067
#define KEY_H		0x0068
#define KEY_I		0x0069
#define KEY_J		0x006a
#define KEY_K		0x006b
#define KEY_L		0x006c
#define KEY_M		0x006d
#define KEY_N		0x006e
#define KEY_O		0x006f
#define KEY_P		0x0070
#define KEY_Q		0x0071
#define KEY_R		0x0072
#define KEY_S		0x0073
#define KEY_T		0x0074
#define KEY_U		0x0075
#define KEY_V		0x0076
#define KEY_W		0x0077
#define KEY_X		0x0078
#define KEY_Y		0x0079
#define KEY_Z		0x007a

#define KEY_ESC		0x0100
#define KEY_F1		0x0101
#define KEY_F2		0x0102
#define KEY_F3		0x0103
#define KEY_F4		0x0104
#define KEY_F5		0x0105
#define KEY_F6		0x0106
#define KEY_F7		0x0107
#define KEY_F8		0x0108
#define KEY_F9		0x0109

#define KEY_LEFT	0x0110
#define KEY_RIGHT	0x0111
#define KEY_UP		0x0112
#define KEY_DOWN	0x0113
#define KEY_CENTER	0x0114

#define KEY_VOLUMEUP	0x0115
#define KEY_VOLUMEDOWN	0x0116
#define KEY_MUTE	0x0117
#define KEY_SOUND	0x0118

#define KEY_SOFT1	0x011a
#define KEY_SOFT2	0x011b
#define KEY_STAR	0x011c
#define KEY_SHARP	0x011d
#define KEY_MAIL	0x011e

#define KEY_SEND	0x0120
#define KEY_CLEAR	0x0121
#define KEY_HOME	0x0122
#define KEY_BACK	0x0123
#define KEY_MENU	0x0124
#define KEY_END		0x0125
#define KEY_POWER	0x0126

#define MAX_KEYS	0x01ff

int keys_get_state(uint16_t code);
int keys_set_state(uint16_t code);

void keys_init(void);
void keys_post_event(uint16_t code, int16_t value);

#endif /* __DEV_KEYS_H */
