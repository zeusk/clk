.text		
.code 32
.globl target_flashlight_s
target_flashlight_s:
	ldr     r4, =0xa9000864        //;@ bank6_in (phys)
        ldr     r5, =0xa9000814        //;@ bank6_out (phys)
        orr    r6, r4, #0x200000      //;@ 22nd bit for flash
        str     r6, [r5, #0]                     //;@ store in out (enables bright for 500ms, limited by hardware)
	bx      lr                                         //;@ return
.ltorg