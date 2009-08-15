#include <pspctrl.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspsysmem_kernel.h>
#include <pspdisplay_kernel.h>
#include <stdio.h>
#include <pspsysevent.h>
#include <psppower.h>
#include <string.h>
#include <systemctrl.h>
#include "sysconhk.h"

//#define LOG

PSP_MODULE_INFO("Hold", 0x1000, 3, 8);
PSP_MAIN_THREAD_ATTR(0);

#define HOLD_FULLHOLD		0x00000001
#define HOLD_RESTORE		0x00000002
#define HOLD_CHECKBRIGHT	0x00000004
#define HOLD_SETBRIGHT		0x00000008
#define HOLD_SETANALOG		0x00000010

int sceSysconCtrlLED(int SceLED, int state);
void ToggleLEDs(int);
int sceDisplayEnable(void);
int sceDisplayDisable(void);

int ucpu = 60, ubus = 30;

unsigned char power_sw_lock_counter = 0;
unsigned char suspend_allowed = 1;
unsigned char suspend_pushed = 0;
unsigned char hold_allowed = 1;
unsigned char nodisplay = 0;
unsigned char holding = 0;
unsigned char normal = 0;
unsigned char setbright = 0;

PspSysEventHandler events;
SceUID th_sleep, th_tick;
u32 holdflags = 0;
int tb, model;

int (* vshCtrlReadBufferPositive)(SceCtrlData *pad_data, int count);
int OnModuleStart(SceModule2 *mod);
u32 orgaddr;
STMOD_HANDLER previous = NULL;
unsigned char up=0, dn=0, patched=0;

int vshCtrlReadBufferPositive_Patched(SceCtrlData *pad_data, int count)
{
	int ret,i,intc;
	unsigned int side = 0;

	ret = vshCtrlReadBufferPositive(pad_data, count);
	if(ret <= 0)
	{
		return ret;
	}

	intc = pspSdkDisableInterrupts();

	for(i = 0; i < count; i++)
	{
		if (pad_data[i].Buttons & PSP_CTRL_UP)
		{
			if (up > 7)
			{
				pad_data[i].Buttons = pad_data[i].Buttons ^ PSP_CTRL_UP;
				up = 7;
			}
			else
			{
				up++;
			}
		}
		else
		{
			up = 0;
		}
		if (pad_data[i].Buttons & PSP_CTRL_DOWN)
		{
			if (dn > 7)
			{
				pad_data[i].Buttons = pad_data[i].Buttons ^ PSP_CTRL_DOWN;
				dn = 7;
			}
			else
			{
				dn++;
			}
		}
		else
		{
			dn = 0;
		}

		if (up || dn)
		{
			if (pad_data[i].Buttons & PSP_CTRL_LEFT)
			{
				side = side | PSP_CTRL_LEFT;
			}
			if (pad_data[i].Buttons & PSP_CTRL_RIGHT)
			{
				side = side | PSP_CTRL_RIGHT;
			}
			pad_data[i].Buttons = pad_data[i].Buttons ^ side;
		}
	}

	pspSdkEnableInterrupts(intc);

	return(ret);
}

int OnModuleStart(SceModule2 *mod)
{
	if (strcmp(mod->modname, "music_browser_module") == 0)
	{
		if (!patched && (orgaddr = sctrlHENFindFunction("sceVshBridge_Driver", "sceVshBridge", 0xC6395C03)))
		{
			orgaddr = sctrlHENFindFunction("sceVshBridge_Driver", "sceVshBridge", 0xC6395C03);
			vshCtrlReadBufferPositive = (void *)orgaddr;
			sctrlHENPatchSyscall(orgaddr, vshCtrlReadBufferPositive_Patched);

			sceKernelDcacheWritebackAll();
			sceKernelIcacheClearAll();
			patched = 1;
		}
	}
	else if (sceKernelFindModuleByName("music_browser_module") == NULL)
	{
		if (patched)
		{
			sctrlHENPatchSyscall((u32)vshCtrlReadBufferPositive_Patched, (void *)orgaddr);

			sceKernelDcacheWritebackAll();
			sceKernelIcacheClearAll();
			patched = 0;
		}
	}

	if (!previous)
		return 0;

	return previous(mod);
}

#define PSP_SYSEVENT_SUSPEND_QUERY 0x100

int suspend_handler(int ev_id, char* ev_name, void* param, int* result){
	if (ev_id == PSP_SYSEVENT_SUSPEND_QUERY)
	{
		if (power_sw_lock_counter)
		{
			return -1;
		}
		else if (!suspend_allowed)
		{
			suspend_allowed = 1;
			return -1;
		}
		else
		{
			return 0;
		}
	}
	return 0;
}

void ToggleLEDs(int state)
{
	sceSysconCtrlLED(0, state);
	sceSysconCtrlLED(1, state);
	sceSysconCtrlLED(2, state);
}

int tick_thread(SceSize args, void *argp)
{
	while(1)
	{
		if (nodisplay || holding)
		{
			scePowerTick(PSP_POWER_TICK_DISPLAY);
		}
		sceKernelDelayThread(30*1000*1000);
	}
	return 0;
}

int main_thread(SceSize args, void *argp)
{
	int cpu, bus, brightness;
	#ifdef LOG
	char temp[25];
	#endif

	previous = sctrlHENSetStartModuleHandler(OnModuleStart);

	events.size = 0x40;
	events.name = "SuspendEvent";
	events.type_mask = 0x0000FF00;
	events.handler = suspend_handler;

	sceKernelRegisterSysEventHandler(&events);

	th_tick = sceKernelCreateThread("tick_thread", tick_thread, 0x12, 0x500, 0, NULL);

	if (th_tick >= 0)
	{
		sceKernelStartThread(th_tick, 0, NULL);
	}

	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	cpu = scePowerGetCpuClockFrequency();
	bus = scePowerGetBusClockFrequency();
	sceDisplayGetBrightness(&brightness,NULL);
	tb = brightness;

	model = sceKernelGetModel();

	install_syscon_hook();

	while (1)
	{
		if (!holdflags)
		{
			goto noflags;
		}
		if (holdflags & HOLD_SETBRIGHT)
		{
			sceDisplaySetBrightness(99,0);
			holdflags &= ~HOLD_SETBRIGHT;
		}
		if (holdflags & HOLD_CHECKBRIGHT)
		{
			sceKernelDelayThread(100*1000);
			if (holdflags & HOLD_CHECKBRIGHT)
			{
				sceDisplayGetBrightness(&tb,NULL);

				#ifdef LOG
				SceUID fp = sceIoOpen("ms0:/hold.txt", PSP_O_WRONLY|PSP_O_CREAT, 0777);
				sceIoLseek(fp, 0, SEEK_END);
				sprintf(temp, "Brightness: %d%c%c", tb, 13, 10);
				sceIoWrite(fp, temp, strlen(temp));
				sceIoClose(fp);
				#endif

				holdflags &= ~HOLD_CHECKBRIGHT;
			}
		}
		if (holdflags & HOLD_RESTORE)
		{
			scePowerSetClockFrequency(cpu, cpu, bus);
			sceDisplaySetBrightness(brightness,0);
			sceDisplayEnable();
			ToggleLEDs(1);

			#ifdef LOG
			SceUID fp = sceIoOpen("ms0:/hold.txt", PSP_O_WRONLY|PSP_O_CREAT, 0777);
			sceIoLseek(fp, 0, SEEK_END);
			sprintf(temp, "Restore To: %d,%d,%d%c%c", cpu, bus, brightness, 13, 10);
			sceIoWrite(fp, temp, strlen(temp));
			sceIoClose(fp);
			#endif

			hold_allowed = 1;
			holdflags &= ~HOLD_RESTORE;
		}
		if (holdflags & HOLD_FULLHOLD)
		{
			if (hold_allowed)
			{
				cpu = scePowerGetCpuClockFrequency();
				bus = scePowerGetBusClockFrequency();
				sceDisplayGetBrightness(&brightness,NULL);
				ToggleLEDs(0);
				sceDisplayDisable();
				sceDisplaySetBrightness(0,0);
				scePowerSetClockFrequency(ucpu, ucpu, ubus);

				#ifdef LOG
				SceUID fp = sceIoOpen("ms0:/hold.txt", PSP_O_WRONLY|PSP_O_CREAT, 0777);
				sceIoLseek(fp, 0, SEEK_END);
				sprintf(temp, "Hold From: %d,%d,%d%c%c", cpu, bus, brightness, 13, 10);
				sceIoWrite(fp, temp, strlen(temp));
				sceIoClose(fp);
				#endif
				hold_allowed = 0;
			}
			holdflags &= ~HOLD_FULLHOLD;
		}
		if (holdflags & HOLD_SETANALOG)
		{
			sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
			holdflags &= ~HOLD_SETANALOG;
		}
noflags:
		sceKernelDelayThread(100*1000);
	}

	return 0;
}

unsigned char last_normal = 0;
unsigned char volup_extra = 0;
unsigned char voldn_extra = 0;
unsigned char lcd_handled = 0;

static SceCtrlData raw_ctrl = {0};
u32 prevButtons,newButtons;
u8 *rxd;

void syscon_ctrl(sceSysconPacket *packet)
{
	newButtons=0;
	rxd = packet->rx_data;

	raw_ctrl.Lx = 0;
	raw_ctrl.Ly = 0;

	switch(packet->rx_response)
	{
	case 0x08:
		raw_ctrl.Lx = rxd[4];
		raw_ctrl.Ly = rxd[5];
		goto SkipSetAnalog;
	case 0x07:
		holdflags |= HOLD_SETANALOG;
SkipSetAnalog:
		newButtons = syscon_get_dword(packet->rx_data);
		newButtons = ~newButtons;

		prevButtons = raw_ctrl.Buttons;
		raw_ctrl.Buttons = newButtons;

		if ((packet->rx_sts & SYSCON_STS_POWER_SW_ON) && !(power_sw_lock_counter) && nodisplay)
		{
			suspend_pushed = 1;
		}

		if (!(newButtons & SYSCON_CTRL_HOLD))
		{
			if ((((prevButtons^newButtons) & SYSCON_CTRL_LCD) && (newButtons & SYSCON_CTRL_LCD)) || suspend_pushed)
			{
				if (nodisplay)
				{
					if(newButtons & SYSCON_CTRL_LCD)
					{
						lcd_handled = 1;
					}
					holdflags |= HOLD_RESTORE;
					holdflags &= ~(HOLD_FULLHOLD|HOLD_CHECKBRIGHT);
					nodisplay = 0;
					if (suspend_pushed)
					{
						suspend_pushed = 0;
						suspend_allowed = 0;
					}
				}
				else if (!suspend_pushed)
				{
					if (((model == 1) && (tb >= 63) && (tb <= 73)) || ((model >= 2) && (tb >= 79) && (tb <= 89)))
					{
						lcd_handled = 1;
						setbright = 1;
					}
				}
			}
		}

		if(newButtons & SYSCON_CTRL_LCD)
		{
			if (lcd_handled)
			{
				newButtons &= ~SYSCON_CTRL_LCD;
			}
		}
		else
		{
			lcd_handled = 0;
			if (!(newButtons & SYSCON_CTRL_HOLD) && ((prevButtons^newButtons) & SYSCON_CTRL_LCD))
			{
				if (setbright)
				{
					setbright = 0;
					holdflags |= HOLD_SETBRIGHT;
				}
					holdflags |= HOLD_CHECKBRIGHT;
			}
		}

		if ((prevButtons^newButtons) & SYSCON_CTRL_HOLD)
		{
			if(newButtons & SYSCON_CTRL_HOLD)
			{
				if (power_sw_lock_counter && last_normal)
				{
					normal = 1;
				}
				else
				{
					if (raw_ctrl.Ly > 15)
					{
						if (!nodisplay)
						{
							holdflags |= HOLD_FULLHOLD;
							holdflags &= ~(HOLD_RESTORE|HOLD_CHECKBRIGHT);
						}
						else
						{
							nodisplay = 0;
						}
						holding = 1;
					}
					else
					{
						normal = 1;
					}
				}
				last_normal = 0;
			}
			else
			{
				power_sw_lock_counter = 60;

				if (!normal)
				{
					if (raw_ctrl.Ly > 15)
					{
						holdflags |= HOLD_RESTORE;
						holdflags &= ~(HOLD_FULLHOLD|HOLD_CHECKBRIGHT);
					}
					else
					{
						nodisplay = 1;
					}
					holding = 0;
				}
				else
				{
					last_normal = 1;
					normal = 0;
				}
			}
		}

#define HOLD_KEYS (\
SYSCON_CTRL_ALLOW_LT|\
SYSCON_CTRL_ALLOW_RT|\
SYSCON_CTRL_LTRG|\
SYSCON_CTRL_RTRG|\
SYSCON_CTRL_START|\
SYSCON_CTRL_VOL_UP|\
SYSCON_CTRL_VOL_DN\
)

#define HOLD_KEYS_MASK (\
SYSCON_CTRL_ALLOW_UP|\
SYSCON_CTRL_ALLOW_DN|\
SYSCON_CTRL_TRIANGLE|\
SYSCON_CTRL_CIRCLE|\
SYSCON_CTRL_CROSS|\
SYSCON_CTRL_RECTANGLE|\
SYSCON_CTRL_HOME|\
SYSCON_CTRL_LCD|\
SYSCON_CTRL_NOTE\
)

		if (newButtons & SYSCON_CTRL_HOLD)
		{
			if(((newButtons & SYSCON_CTRL_SELECT) && (newButtons & HOLD_KEYS) && !(newButtons & HOLD_KEYS_MASK)) || volup_extra || voldn_extra)
			{
				newButtons &= ~(SYSCON_CTRL_HOLD | SYSCON_CTRL_SELECT);

				if (((prevButtons^newButtons) & SYSCON_CTRL_VOL_UP) && (newButtons & SYSCON_CTRL_VOL_UP))
				{
					volup_extra = 15;
				}
				if (((prevButtons^newButtons) & SYSCON_CTRL_VOL_DN) && (newButtons & SYSCON_CTRL_VOL_DN))
				{
					voldn_extra = 15;
				}

				if (volup_extra)
				{
					volup_extra--;
				}
				if (voldn_extra)
				{
					voldn_extra--;
				}

			}
		}
	}

	syscon_put_dword(packet->rx_data,~newButtons);
	syscon_make_checksum(&packet->rx_sts);

	if (power_sw_lock_counter)
	{
		power_sw_lock_counter--;
	}
}

int module_start(SceSize args, void *argp)
{
	SceUID th;
	th = sceKernelCreateThread("main_thread", main_thread, 0x12, 0x500, 0, NULL);

	if (th >= 0)
	{
		sceKernelStartThread(th, args, argp);
	}

	return 0;
}

int module_stop(SceSize args, void *argp)
{
	uninstall_syscon_hook();

	if (patched)
	{
		sctrlHENPatchSyscall((u32)vshCtrlReadBufferPositive_Patched, (void *)orgaddr);

		sceKernelDcacheWritebackAll();
		sceKernelIcacheClearAll();
	}

	return 0;
}
