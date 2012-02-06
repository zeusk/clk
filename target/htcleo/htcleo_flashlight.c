#include <platform/iomap.h>
#include <reg.h>
#include <debug.h>
#include <dev/keys.h>
#include <arch/arm.h>

void target_flashlight_s(void);
unsigned target_flashlight(void)
{
	target_flashlight_s();
	return ((unsigned) 500);
}