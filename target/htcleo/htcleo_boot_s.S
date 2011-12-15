.text		
.code 32

.globl htcleo_boot_s
htcleo_boot_s:
		// save registers
		MOV r9, r0

		//cedesmith:
		// cotulla's code so kernel will not crash. aux control register
		// found more info here: http://www.spinics.net/lists/linux-arm-msm/msg00492.html
		// it looks it is Martijn Stolk's code
		MRC p15, 0, r0, c1, c0, 1
		BIC r0, r0, #0x40					//; (1<<6)  IBE (0 = executes the CP15 Invalidate All and Invalidate by MVA instructions as a NOP instruction, reset value)
		BIC r0, r0, #0x200000				//; (1<<21) undocumented bit
		MCR p15, 0, r0, c1, c0, 1

		//; Disable VFP
		MOV R0, #0
        FMXR  FPEXC, r0

		//; ICIALL to invalidate entire I-Cache
        MCR    p15, 0, r0, c7, c5, 0    //; ICIALLU

        // disable dcache and i cache
		MRC p15, 0, r0, c1, c0, 0
		BIC r0, r0, #(1<<0)					// disable mmu ( already disabled )
		BIC r0, r0, #(1<<2)					// disable data cache
		BIC r0, r0, #(1<<12)				// disable instruction cache
		MCR p15, 0, r0, c1, c0, 0
		ISB

		//; DCIALL to invalidate L2 cache bank (needs to be run 4 times, once per bank)
        //; This must be done early in code (prior to enabling the caches)
        MOV    r0, #0x2
        MCR    p15, 0, r0, c9, c0, 6   //; DCIALL bank D ([15:14] == 2'b00)
        ORR    r0, r0, #0x00004000
        MCR    p15, 0, r0, c9, c0, 6   //; DCIALL bank C ([15:14] == 2'b01)
        ADD    r0, r0, #0x00004000
        MCR    p15, 0, r0, c9, c0, 6   //; DCIALL bank B ([15:14] == 2'b10)
        ADD    r0, r0, #0x00004000
        MCR    p15, 0, r0, c9, c0, 6   //; DCIALL bank A ([15:14] == 2'b11)
		//; DCIALL to invalidate entire D-Cache
		MOV    r0, #0
        MCR    p15, 0, r0, c9, c0, 6    //; DCIALL  r0
		DSB
		ISB

		//; Invalidate the UTLB
        MOV r0, #0
        MCR    p15, 0, r0, c8, c7, 0    //; UTLBIALL
        ISB


		//flashlight to see we get here
		//ldr     r4, =0xa9000864        @ bank6_in (phys)
		//ldr     r5, =0xa9000814        @ bank6_out (phys)
		//orr     r6, r4, #0x200000      @ 22nd bit for flash
		//str     r6, [r5, #0]           @ store in out (enables bright for 500ms, limited by hardware)

		MOV R0, #0
        BLX R9

.ltorg

.global htcleo_flash
htcleo_flash:
	ldr     r4, =0xa9000864
	ldr     r5, =0xa9000814
	orr     r6, r4, #0x200000
	str     r6, [r5, #0]
	bx		lr
.ltorg


// code to put at 0, get opcode from debug.lst
.global pc_reset_vector
.global pc_reset_vector_end
pc_reset_vector:
		mov r0, #0x11800000
		ldr pc, [r0, #4]
		//mov pc, #MEMBASE
		#ldr pc, #MEMBASE
		BX LR
pc_reset_vector_end:
.ltorg
