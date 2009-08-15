#ifndef SYSCON_H
#define SYSCON_H

#include <pspkerneltypes.h>

/** @defgroup Syscon Interface to the sceSyscon_driver library.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
	CMD 07,08 : CTRL data

	all bits are active LOW

	+03-06:buttons (cmd 07/08)
	+07   :Analog  (cmd 08)
	+08   :Analog  (cmd 08)
*/
#define SYSCON_CTRL_ALLOW_UP  0x00000001
#define SYSCON_CTRL_ALLOW_RT  0x00000002
#define SYSCON_CTRL_ALLOW_DN  0x00000004
#define SYSCON_CTRL_ALLOW_LT  0x00000008
#define SYSCON_CTRL_TRIANGLE  0x00000010
#define SYSCON_CTRL_CIRCLE    0x00000020
#define SYSCON_CTRL_CROSS     0x00000040
#define SYSCON_CTRL_RECTANGLE 0x00000080
#define SYSCON_CTRL_SELECT    0x00000100
#define SYSCON_CTRL_LTRG      0x00000200
#define SYSCON_CTRL_RTRG      0x00000400
#define SYSCON_CTRL_START     0x00000800
#define SYSCON_CTRL_HOME      0x00001000
#define SYSCON_CTRL_HOLD      0x00002000
#define SYSCON_CTRL_WLAN      0x00004000
#define SYSCON_CTRL_HR_EJ     0x00008000
#define SYSCON_CTRL_VOL_UP    0x00010000
#define SYSCON_CTRL_VOL_DN    0x00020000
#define SYSCON_CTRL_LCD       0x00040000
#define SYSCON_CTRL_NOTE      0x00080000
#define SYSCON_CTRL_UMD_EJCT  0x00100000
#define SYSCON_CTRL_UNKNOWN   0x00200000 /* is not-service mode ? */

/*
  +0x1C generic status

  bit 0 : AC power input
  bit 1 : sceSysconSetWlanPowerCallback
  bit 2 : sceSysconSetHRPowerCallback
  bit 3 : sceSysconSetAlarmCallback
  bit 4 : power switch on
  bit 5 : sceSysconSetLowBatteryCallback
  bit 6 : ?
  bit 7 : ?
*/
#define SYSCON_STS_AC_ACTIVE   0x01
#define SYSCON_STS_WLAN_POW    0x02
#define SYSCON_STS_HR_POW      0x04
#define SYSCON_STS_ALARM       0x08
#define SYSCON_STS_POWER_SW_ON 0x10
#define SYSCON_STS_LOW_BATTERY 0x20

/*
	receive +02 : response code

	00-7f : response of COMMAND 00-7f
	80-8f : done / error code
*/

// 1.Receive 1st packet of twice command , send same command again
// 2.CHECK SUM error
#define SYSCON_RES_80 0x80

// SYSCON BUSY , send same command again
#define SYSCON_RES_81 0x81

// Done 2nd packet of twice command
#define SYSCON_RES_OK 0x82

// parameter size error
#define SYSCON_RES_83 0x83

// twice send command error
#define SYSCON_RES_86 0x86

/*
	pspSysconCtrlPower device val
*/
#define SYSCON_DEV_ON  (1<<23)
#define SYSCON_DEV_LCD 0x00080000

/*
*/
#define SYSCON_PACKET_STATUS_BUSY 0x01
#define SYSCON_PACKET_STATUS_RECEIVE_NOT_FINISH 0x02
#define SYSCON_PACKET_STATUS_ERROR_4 0x10
#define SYSCON_PACKET_STATUS_ERROR_5 0x20
#define SYSCON_PACKET_STATUS_ERROR_6 0x40

/*
	parameter of SYSCON access
*/
typedef struct sceSysconPacket
{
	u8	unk00[4];		// +0x00 ?(0x00,0x00,0x00,0x00)
	u8	unk04[2];		// +0x04 ?(arg2)
	u8	status;			// +0x06
	u8	unk07;			// +0x07 ?(0x00)
	u8	unk08[4];		// +0x08 ?(0xff,0xff,0xff,0xff)
// transmit data
	u8	tx_cmd;			// +0x0C command code
	u8	tx_len;			// +0x0D number of transmit bytes
	u8	tx_data[14];	// +0x0E transmit parameters
// receive data
	u8	rx_sts;			// +0x1C generic status
	u8	rx_len;			// +0x1D receive length
	u8	rx_response;	// +0x1E response code(tx_cmd or status code)
	u8	rx_data[9];		// +0x1F receive parameters
// ?
	u32	unk28;			// +0x28
// user callback (when finish an access?)
	void (*callback)(struct sceSysconPacket *,u32);	// +0x2c
	u32	callback_r28;	// +0x30
	u32	callback_arg2;	// +0x34 arg2 of callback (arg4 of sceSycconCmdExec)

	u8	unk38[0x0d];	// +0x38
	u8	old_sts;		// +0x45 old     rx_sts
	u8	cur_sts;		// +0x46 current rx_sts
	u8	unk47[0x21];	// +0x47
// data size == 0x60 ??
} sceSysconPacket;

/*
	parameter of SYSCON debug handlers
*/
typedef struct sceSysconDebugHandlers
{
	void (*handler1)(sceSysconPacket *packet);	// +00 Unknown parameter,Do not used in syscon.prx
	void (*before_tx)(sceSysconPacket *packet);	// +04 calback entry when before transmit packet
	void (*after_rx)(sceSysconPacket *packet);	// +08 calback entry when after receive packet
	void (*handler4)(sceSysconPacket *packet);	// +0C This doesn't exist probably.
} sceSysconDebugHandlers;

/****************************************************************************
	SYSCON driver API
****************************************************************************/

void sceSysconSetDebugHandlers(sceSysconDebugHandlers *handlers);
int sceSysconCmdExec(sceSysconPacket *,u32 unknown);
int sceSysconCmdExecAsync(sceSysconPacket *,u32 unk1,u32 unk2,u32 unk3);

/****************************************************************************
	support routine
****************************************************************************/
void syscon_ctrl(sceSysconPacket *packet);

void syscon_make_checksum(void *data);
void syscon_put_dword(u8 *ptr,u32 value);
u32 syscon_get_dword(u8 *ptr);

/****************************************************************************
	debug routine
****************************************************************************/
void syscon_dump_data(const char *name,sceSysconPacket *packet);

/****************************************************************************
	syscon debug handler hook / release
****************************************************************************/
void install_syscon_hook(void);
void uninstall_syscon_hook(void);

#ifdef __cplusplus
}
#endif

#endif
