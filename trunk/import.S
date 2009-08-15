	.set noreorder

#include "pspstub.s"

	STUB_START	"sceSyscon_driver",0x00010011,0x00020005
	STUB_FUNC	0x5EE92F3C,sceSysconSetDebugHandlers
	STUB_FUNC	0x18BFBE65,sceSysconCtrlLED
	STUB_END
