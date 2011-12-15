#include <dev/keys.h>
#include <dev/gpio_keypad.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

static unsigned int htcleo_row_gpios[] = { 33, 32, 31 };
static unsigned int htcleo_col_gpios[] = { 42, 41, 40 };

#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(htcleo_col_gpios) + (col))

static const unsigned short htcleo_keymap[ARRAY_SIZE(htcleo_col_gpios) * ARRAY_SIZE(htcleo_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_VOLUMEUP,	// Volume Up
	[KEYMAP_INDEX(0, 1)] = KEY_VOLUMEDOWN,	// Volume Down
	[KEYMAP_INDEX(1, 0)] = KEY_SOFT1,		// Windows Button
	[KEYMAP_INDEX(1, 1)] = KEY_SEND,		// Dial Button
	[KEYMAP_INDEX(1, 2)] = KEY_CLEAR,		// Hangup Button
	[KEYMAP_INDEX(2, 0)] = KEY_BACK,		// Back Button
	[KEYMAP_INDEX(2, 1)] = KEY_HOME,		// Home Button
};

static struct gpio_keypad_info htcleo_keypad_info = {
	.keymap		= htcleo_keymap,
	.output_gpios	= htcleo_row_gpios,
	.input_gpios	= htcleo_col_gpios,
	.noutputs	= ARRAY_SIZE(htcleo_row_gpios),
	.ninputs	= ARRAY_SIZE(htcleo_col_gpios),
	.settle_time	= 40 /* msec */,
	.poll_time	= 20 /* msec */,
	.flags		= GPIOKPF_DRIVE_INACTIVE,
};

void keypad_init(void)
{
	gpio_keypad_init(&htcleo_keypad_info);
}
