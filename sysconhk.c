#include <pspkernel.h>
#include <pspctrl.h>

#include "sysconhk.h"

extern void syscon_ctrl(sceSysconPacket *packet);

void syscon_put_dword(u8 *ptr,u32 value)
{
	ptr[0] = value;
	ptr[1] = value>>8;
	ptr[2] = value>>16;
	ptr[3] = value>>24;
}

u32 syscon_get_dword(u8 *ptr)
{
	return ptr[0] | (ptr[1]<<8) | (ptr[2]<<16)| (ptr[3]<<24);
}

void syscon_make_checksum(void *data)
{
	int i;
	u8 sum;
	u8 *ptr = (u8 *)data;
	int size = ptr[1];
	
	sum = 0;
	for(i=0;i<size;i++)
		sum += *ptr++;
	*ptr = sum^0xff;
}

static void syscon_debug_callback0(sceSysconPacket *packet)
{
	while(1);
}

static void syscon_transmit_callback(sceSysconPacket *packet)
{
}

static void syscon_receive_callback(sceSysconPacket *packet)
{
	switch(packet->tx_cmd)
	{
	case 0x07:
		syscon_ctrl(packet);
		break;
	case 0x08:
		syscon_ctrl(packet);
		break;
	}
}

static void syscon_debug_callback3(sceSysconPacket *packet)
{
	while(1);
}

static sceSysconDebugHandlers syscon_debug_callbacks =
{
	syscon_debug_callback0,
	syscon_transmit_callback,
	syscon_receive_callback,
	syscon_debug_callback3
};

void install_syscon_hook(void)
{
	sceSysconSetDebugHandlers(&syscon_debug_callbacks);
}

void uninstall_syscon_hook(void)
{
	sceSysconSetDebugHandlers(NULL);
}
