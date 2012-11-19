#include <debug.h>
#include <err.h>
#include <stdlib.h>
#include <dev/fbcon.h>
#include <font8x16.h>
#include <string.h>
#include <kernel/thread.h>

#if SHOW_LOGO_SPLASH_H
#include <splash.h>
#endif
/* Cartesian coordinate system */
struct pos {
	int x;
	int y;
};

/* fbconfig, retrieved using fbcon_display() in device instants */
struct fbcon_config *config = NULL;

unsigned			*__fb_font;
static uint16_t		BGCOLOR;
static uint16_t		FGCOLOR;
static uint16_t		TGCOLOR;
static uint16_t     F1COLOR;
static uint16_t     T1COLOR;
static struct pos	cur_pos;
static struct pos	max_pos;
static bool			scrolled;
static bool			forcedtg;

void fbcon_forcetg(bool flag_boolean)
{
	forcedtg = flag_boolean;
}

static void ijustscrolled(void){
	scrolled = true;
}

static void cleanedyourcrap(void){
	scrolled = false;
}

bool didyouscroll(void){
	return scrolled;
}

void fill_screen(uint16_t COLOR)
{
	enter_critical_section();
	memset(config->base, COLOR, (((config->width) * (config->height)) * (config->bpp /8)));
	exit_critical_section();
	
	return;
}

void fbcon_clear_region(int start_y, int end_y, unsigned bg){
	//enter_critical_section();
	unsigned area_size = (((end_y - start_y) * FONT_HEIGHT) * config->width) * ((config->bpp) / 8);
	unsigned start_offset = ((start_y * FONT_HEIGHT) * config->width) * ((config->bpp) / 8);
	memset(config->base + start_offset, bg, area_size);
	//exit_critical_section();
}

int fbcon_get_y_cord(void){
    return cur_pos.y;
}

void fbcon_set_y_cord(int offset){
    cur_pos.y = offset;
}

int fbcon_get_x_cord(void){
    return cur_pos.x;
}

void fbcon_set_x_cord(int offset){
    cur_pos.x = offset;
}

/* koko : Ported Rick_1995's commit for larger font */
uint16_t *pixor;

static unsigned reverse_fnt_byte(unsigned x)
{
	unsigned y = 0;
	for (uint8_t i = 0; i < 9; ++i) {
		y <<= 1;
		y |= (x & 1);
		x >>= 1;
	}
	return y;
}
	
static void fbcon_drawglyph_helper(uint16_t *pixels, uint16_t paint, unsigned stride,
				   unsigned data, bool dtg)
{
	for (unsigned y = 0; y < (FONT_PPCHAR/FONT_HEIGHT); y++) {
		data = reverse_fnt_byte(data);
		for (unsigned x = 0; x < FONT_WIDTH; x++) {
			if (data & 1) *pixor = paint;
			else if(dtg) *pixor = TGCOLOR;
			data >>= 1;
			pixor++;
		}
		pixor += stride;
	}
	return;
}

static void fbcon_drawglyph(uint16_t *pixels, uint16_t paint, unsigned stride,
			    unsigned *glyph)
{
	stride -= FONT_WIDTH;
	bool dtg = false;
	pixor = pixels;
	if((BGCOLOR != TGCOLOR)||(forcedtg))
		dtg=true;
	
	for(unsigned i = 0; i < FONT_PPCHAR; i++) {
		fbcon_drawglyph_helper(pixor, paint, stride, glyph[i], dtg);
	}

	return;
}

void fbcon_flush(void)
{
	/* Send update command and hold pointer till really done */
	if (config->update_start)
		config->update_start();
	if (config->update_done)
		while (!config->update_done());
}

void fbcon_push(void)
{
	/* Send update command and return immediately */
	if (config->update_start)
		config->update_start();
		
	return (void)config->update_done;
}

static void fbcon_scroll_up(void)
{
	ijustscrolled();
#if SHOW_LOGO_SPLASH_H
	unsigned buffer_size = (config->width * (config->height - SPLASH_IMAGE_HEIGHT)) * (config->bpp / 8);
#else
	unsigned buffer_size = config->width * config->height * (config->bpp / 8);
#endif
	unsigned line_size = (config->width * FONT_HEIGHT) * (config->bpp / 8);
	// copy framebuffer from base + 1 line
	memmove(config->base, config->base + line_size, buffer_size-line_size);
	// clear last framebuffer line
	memset(config->base+buffer_size-line_size,BGCOLOR,line_size);
	// Flush holds the control till the Display is REALLY updated, now we update the display data and move on instead of blocking the pointer there and save some time as this function is HIGHLY time critical.
	fbcon_push();
}

void fbcon_clear(void)
{
 	//enter_critical_section();
#if SHOW_LOGO_SPLASH_H
	/* koko : Set the LCD to black till the end of the header */
	unsigned header_size = ( 6 * FONT_HEIGHT * config->width * (config->bpp/8) );
	memset(config->base, 0x0000, header_size);
	/* Set the LCD to BGCOLOR from Menu till the part where logo is displayed */
	unsigned image_base = ( ( (config->height - SPLASH_IMAGE_HEIGHT - (6 * FONT_HEIGHT)) * config->width)
							+ ( (config->width - SPLASH_IMAGE_WIDTH - FONT_WIDTH) / 2 )	);
	memset(config->base + header_size, BGCOLOR, image_base * (config->bpp/8));
#else
	unsigned image_base = ( (config->height * config->width) + ((config->width - FONT_WIDTH)/2)	);
	memset(config->base, BGCOLOR, image_base * (config->bpp/8));
#endif
	//fbcon_flush();
	cur_pos.x = cur_pos.y = 0;
	//exit_critical_section();
}

static void fbcon_set_colors(unsigned bg, unsigned fg)
{
	BGCOLOR = bg;
	F1COLOR = FGCOLOR;
	FGCOLOR = fg;
}

void fbcon_setfg(unsigned fg)
{
	F1COLOR = FGCOLOR;
	FGCOLOR = fg;
}

void fbcon_settg(unsigned tg)
{
	T1COLOR = TGCOLOR;
	TGCOLOR = tg;
}

void fbcon_set_txt_colors(unsigned fgcolor, unsigned tgcolor){
	F1COLOR = FGCOLOR;
	T1COLOR = TGCOLOR;
	
	FGCOLOR = fgcolor;
	TGCOLOR = tgcolor;
}

uint16_t fbcon_get_bgcol(void)
{
	return BGCOLOR;
}

void fbcon_reset_colors_rgb555(void)
{
	uint16_t bg;
	uint16_t fg;
	bg = (inverted ? RGB565_WHITE : RGB565_BLACK);
	fg = (inverted ? RGB565_BLACK : RGB565_WHITE);
	fbcon_set_colors(bg, fg);
	T1COLOR = (TGCOLOR = bg);
	F1COLOR = fg;
}

void fbcon_putc(char c)
{
	uint16_t *pixels;

	if (!config)
		return;
		
	if((unsigned char)c > 127)
		return;
	if((unsigned char)c < 32) {
		if(c == '\n')
			goto newline;
		else if (c == '\r')
			cur_pos.x = 0;
		return;
	}

	pixels = config->base;
	pixels += cur_pos.y * FONT_HEIGHT * config->width;
	pixels += cur_pos.x * FONT_WIDTH;
	
	fbcon_drawglyph(pixels,
					FGCOLOR, config->stride,
					__fb_font + (c - 32) * FONT_PPCHAR);
					
	cur_pos.x++;
	if (cur_pos.x < max_pos.x)
		return;

newline:
	cur_pos.y++;
	cur_pos.x = 0;
	if(cur_pos.y >= max_pos.y) {
		cur_pos.y = max_pos.y - 1;
		fbcon_scroll_up();
	}
#if LCD_REQUIRE_FLUSH
	else fbcon_flush();
#endif
}

void fbcon_setup(struct fbcon_config *_config, int inv)
{
	inverted = inv;
	__fb_font = font8x16;

	uint32_t bg, fg;

	ASSERT(_config);
	config = _config;
	
	switch (config->format) {
		case FB_FORMAT_RGB565:
			fg = (inverted ? RGB565_BLACK : RGB565_WHITE);
			bg = (inverted ? RGB565_WHITE : RGB565_BLACK);
			break;
		case FB_FORMAT_RGB888:
			fg = (inverted ? RGB888_BLACK : RGB888_WHITE);
			bg = (inverted ? RGB888_WHITE : RGB888_BLACK);
			break;
		default:
			dprintf(CRITICAL, "unknown framebuffer pixel format\n");
			ASSERT(0);
			break;
	}
	T1COLOR = (TGCOLOR = bg);
	F1COLOR = fg;
	fbcon_set_colors(bg, fg);
	
	cur_pos.x = 0;
	cur_pos.y = 0;
	max_pos.x = config->width / (FONT_WIDTH);
#if SHOW_LOGO_SPLASH_H
	max_pos.y = (config->height - SPLASH_IMAGE_HEIGHT) / FONT_HEIGHT;
#else
	max_pos.y = config->height / FONT_HEIGHT;
#endif
	
	cleanedyourcrap();
#if SHOW_LOGO_SPLASH_H
	fbcon_disp_logo();
#endif
	fbcon_clear();
}

struct fbcon_config* fbcon_display(void)
{
	return config;
}

void fbcon_resetdisp(void)
{
	fbcon_clear();
	cur_pos.x = 0;
	cur_pos.y = 0;
	cleanedyourcrap();
}

void fbcon_teardown(void)
{
	enter_critical_section();
	config = NULL;
	exit_critical_section();
}

#if SHOW_LOGO_SPLASH_H
void fbcon_disp_logo(void)
{
	unsigned i = 0;
	unsigned bytes_per_bpp = ((config->bpp) / 8);
	unsigned image_base = ((((config->height)-SPLASH_IMAGE_HEIGHT) * (config->width)) + ((config->width/2)-(SPLASH_IMAGE_WIDTH/2)));
	
	//Set the LCD to BGCOLOR from the image base to image size
	memset(config->base + ((((config->height)-(SPLASH_IMAGE_HEIGHT))*(config->width))*bytes_per_bpp), BGCOLOR, (((SPLASH_IMAGE_HEIGHT) * config->width)*bytes_per_bpp));

	if (bytes_per_bpp == 3)
	{
		for (i = 0; i < SPLASH_IMAGE_HEIGHT; i++)
		{
			memcpy (config->base + ((image_base + (i * (config->width))) * bytes_per_bpp),
				imageBuffer_rgb888 + (i * SPLASH_IMAGE_WIDTH * bytes_per_bpp),
				SPLASH_IMAGE_WIDTH * bytes_per_bpp);
		}
	} 
	if (bytes_per_bpp == 2)
	{
		for (i = 0; i < SPLASH_IMAGE_HEIGHT; i++)
		{
			memcpy (config->base + ((image_base + (i * (config->width))) * bytes_per_bpp),
				imageBuffer + (i * SPLASH_IMAGE_WIDTH * bytes_per_bpp),
				SPLASH_IMAGE_WIDTH * bytes_per_bpp);
		}
	}
    fbcon_flush();
}
#endif