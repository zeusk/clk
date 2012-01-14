#include <platform/iomap.h>
#include <reg.h>
#include <debug.h>
#include <dev/keys.h>
#include <arch/arm.h>

// cedesmith: we need to stop interrupts or kernel will receive dex interrupt to early and crash
#define VIC_REG(off) (MSM_VIC_BASE + (off))
#define VIC_INT_EN0         VIC_REG(0x0010)
#define VIC_INT_EN1         VIC_REG(0x0014)
#define VIC_INT_CLEAR0      VIC_REG(0x00B0)
#define VIC_INT_CLEAR1      VIC_REG(0x00B4)
#define VIC_INT_MASTEREN    VIC_REG(0x0068)  /* 1: IRQ, 2: FIQ     */

void htcleo_disable_interrupts(void)
{
	//clear current pending interrupts
	writel(0xffffffff, VIC_INT_CLEAR0);
	writel(0xffffffff, VIC_INT_CLEAR1);

	//disable all
	writel(0, VIC_INT_EN0);
	writel(0, VIC_INT_EN1);
	//disable interrupts
	writel(0, VIC_INT_MASTEREN);
}

void htcleo_boot_s(void* kernel,unsigned machtype,void* tags);
void htcleo_boot(void* kernel,unsigned machtype,void* tags)
{
	htcleo_disable_interrupts();
	htcleo_boot_s(kernel, machtype, tags);
}
