.text		
.code 32

.globl target_uninit_s
target_uninit_s:
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

        // Disable mmu
		MRC p15, 0, r0, c1, c0, 0
		BIC r0, r0, #(1<<0)					// disable mmu ( already disabled )
		MCR p15, 0, r0, c1, c0, 0
		ISB

		//; Invalidate the UTLB
        MOV r0, #0
        MCR    p15, 0, r0, c8, c7, 0    //; UTLBIALL
        ISB

		MOV R0, #0
		
		ISB
		DSB
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
		BX LR
pc_reset_vector_end:
.ltorg
