unsigned* target_atag_mem(unsigned* ptr)
{

#if 0
	//MEM TAG
	*ptr++ = 4;
	*ptr++ = 0x54410002;
	//*ptr++ = 0x1e400000; //mem size from haret
	//*ptr++ = 0x1E7C0000; //mem size from kernel config
	*ptr++ = 0x1CFC0000; //mem size from kernel config with bravo dsp
	*ptr++ = 0x11800000; //mem base
#endif

	//add atag to notify nand boot
	*ptr++ = 4;
	*ptr++ = 0x4C47414D; 	// NAND atag (MAGL :))
	*ptr++ = 0x004b4c63; 	// cLK signature
	*ptr++ = 13;			// cLK version number

	return ptr;
}
