/****************************************************************************
 * Copyright (C) 2016 Maschell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <algorithm>
#include <list>
#include <stdarg.h>
#include <gctypes.h>
#include "function_hooks.h"
#include "dynamic_libs/aoc_functions.h"
#include "dynamic_libs/ax_functions.h"
#include "dynamic_libs/fs_functions.h"
#include "dynamic_libs/gx2_functions.h"
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/padscore_functions.h"
#include "dynamic_libs/socket_functions.h"
#include "dynamic_libs/sys_functions.h"
#include "dynamic_libs/vpad_functions.h"
#include "dynamic_libs/acp_functions.h"
#include "dynamic_libs/syshid_functions.h"
#include "dynamic_libs/nfp_functions.h"
#include "kernel/kernel_functions.h"
#include "utils/logger.h"
#include "common/common.h"
#include "common/fs_defs.h"
#include "common/game_defs.h"
#include "common/nfp_defs.h"
#include "main.h"
#include "pygecko.h"
#include "main.h"
#include "fs/fs_utils.h"


#define LIB_CODE_RW_BASE_OFFSET                         0xC1000000
#define CODE_RW_BASE_OFFSET                             0x00000000
#define DEBUG_LOG_DYN                                   0

#define USE_EXTRA_LOG_FUNCTIONS   0

#define DECL(res, name, ...) \
        res (* real_ ## name)(__VA_ARGS__) __attribute__((section(".data"))); \
        res my_ ## name(__VA_ARGS__)

#define MAKE_MAGIC(x, lib,functionType) { (unsigned int) my_ ## x, (unsigned int) &real_ ## x, lib, # x,0,0,functionType,0}


static void ClearGlobalVars()
{
	memset(&g_currentInputDataVPAD, 0, sizeof(VPADData));
	memset(&g_currentInputDataKPAD, 0, sizeof(KPADData));

	g_gameRunning = 0;
	g_langRedirectActive = 0;
	g_gameID = -1;
}

//Gets called on process exit
DECL(void, _Exit, void)
{
	//Cleanup
	ClearGlobalVars();

	real__Exit();
}

//Re-direct socket lib finish into nothing so games can't kill the socket library and our connection by accident
DECL(int, socket_lib_finish, void)
{
	return 0;
}

//Realize when the home button menu is about to come up since threads need to freeze asap
DECL(int, OSIsHomeButtonMenuEnabled, void)
{
	int ret = real_OSIsHomeButtonMenuEnabled();

	g_gameRunning = 0;
	g_langRedirectActive = 0;

	return ret;
}

//Change System Language
DECL(int, UCReadSysConfig, u32 handle, u32 count, UCSysConfig *settings)
{
	int result = real_UCReadSysConfig(handle, count, settings);

	if (g_langRedirectActive)
	{
		if (g_gameID != -1)
		{
			if (!strcmp(settings->name, "cafe.language")) 
			{
				if (g_langCode != -1)
				{
					u32* language = (u32*)settings->data;

					*language = (u32)g_langCode;
					DCFlushRange(language, 4);
				}
			}
		}
	}

	return result;
}

//Change System Region
DECL(int, MCP_GetSysProdSettings, u32 handle, MCPSysProdSettings *settings)
{
	int result = real_MCP_GetSysProdSettings(handle, settings);

	if (g_langRedirectActive)
	{
		if (g_gameID != -1)
		{
			if (g_regionCode != -1)
			{
				settings->gameRegion = (u8)g_regionCode;
				settings->platformRegion = (u8)g_regionCode;
			}
		}
	}

	return result;
}

//Amiibo Remaps
DECL(int, GetNfpRomInfo__Q2_2nn3nfpFPQ3_2nn3nfp7RomInfo, NFPRomInfo *romInfo)
{
	int result = real_GetNfpRomInfo__Q2_2nn3nfpFPQ3_2nn3nfp7RomInfo(romInfo);

	if (g_gameRunning)
	{
		//log_printf("Amiibo ID: %X, variant: %i, type: %i, model nr: %X, series: %X", romInfo->gameID, romInfo->characterVariant, romInfo->figureType, romInfo->modelNumber, romInfo->series);

		u8* srcPtr = (u8*)&g_remapsAmiibo;
		u64* amiiboCode = (u64*)romInfo;

		for (int i = 0; i < g_remapsAmiiboCount; i++)
		{
			int pos = i * 16;

			u64* srcCode = (u64*)(srcPtr + pos);
			u64* destCode = (u64*)(srcPtr + pos + 8);

			if (*amiiboCode == *destCode)
			{
				*amiiboCode = *srcCode;
				break;
			}
		}
	}

	return result;
}

//Amiibo serial number modification
DECL(int, GetTagInfo__Q2_2nn3nfpFPQ3_2nn3nfp7TagInfo, NFPTagInfo* taginfo)
{
	int result = real_GetTagInfo__Q2_2nn3nfpFPQ3_2nn3nfp7TagInfo(taginfo);

	if (g_gameRunning)
	{
		g_serialCounter++;
		*(u32*)taginfo += g_serialCounter;
	}

	return result;
}

//Gets called whenever the system polls the WiiU Gamepad
DECL(int, VPADRead, int chan, VPADData *buffer, u32 buffer_size, s32 *error)
{
	int result = 0;

	if (g_gameRunning)
	{
		result = real_VPADRead(chan, &g_currentInputDataVPAD, 1, error); //Read the actual inputs from the real function
	}
	else
	{
		result = real_VPADRead(chan, buffer, buffer_size, error);
	}

	if (chan == 0) //Only read inputs from Controller Port 0 for now
	{
		//Button Remaps
		if (g_gameRunning)
		{
			u32 tempPressed = g_currentInputDataVPAD.btns_d;
			u32 tempHold = g_currentInputDataVPAD.btns_h;
			u32 tempReleased = g_currentInputDataVPAD.btns_r;

			for (int i = 0; i < g_remapsButtonCount; i++)
			{
				int pos = i * 8;
				u8* srcPtr = (u8*)&g_remapsVPAD;

				u32* srcCode = (u32*)(srcPtr + pos);
				u32* destCode = (u32*)(srcPtr + pos + 4);

				//Pressed
				if ((tempPressed & *destCode) == *destCode)
				{
					if ((tempPressed & *srcCode) != *srcCode)
					{
						g_currentInputDataVPAD.btns_d += *srcCode;
					}
				}
				else
				{
					if ((tempPressed & *srcCode) == *srcCode)
					{
						g_currentInputDataVPAD.btns_d -= *srcCode;
					}
				}

				//Held
				if ((tempHold & *destCode) == *destCode)
				{
					if ((tempHold & *srcCode) != *srcCode)
					{
						g_currentInputDataVPAD.btns_h += *srcCode;
					}
				}
				else
				{
					if ((tempHold & *srcCode) == *srcCode)
					{
						g_currentInputDataVPAD.btns_h -= *srcCode;
					}
				}

				//Released
				if ((tempReleased & *destCode) == *destCode)
				{
					if ((tempReleased & *srcCode) != *srcCode)
					{
						g_currentInputDataVPAD.btns_r += *srcCode;
					}
				}
				else
				{
					if ((tempReleased & *srcCode) == *srcCode)
					{
						g_currentInputDataVPAD.btns_r -= *srcCode;
					}
				}
			}

			*buffer = g_currentInputDataVPAD;
		}

		g_currentInputDataVPAD = *buffer;
	}
	else
	{
		if (g_gameRunning)
		{
			*buffer = g_currentInputDataVPAD;
		}
	}

	return result;
}

//Pro Controller Remapping
DECL(s32, KPADReadEx, s32 chan, KPADData *buffer, u32 count, s32 *error)
{
	s32 result = 0;
	u8 readController = 1;

	if (g_gameRunning)
	{
		result = real_KPADReadEx(chan, &g_currentInputDataKPAD, 1, error);
	}
	else
	{
		result = real_KPADReadEx(chan, buffer, count, error);
	}

	if (error)
	{
		if (*error == -2) //Controller not connected
		{
			readController = 0;
		}
	}

	if (readController) //Only read inputs from Controllers that are connected
	{
		//Button Remaps
		if (g_gameRunning)
		{
			u32 tempPressed = g_currentInputDataKPAD.classic.btns_d;
			u32 tempHold = g_currentInputDataKPAD.classic.btns_h;
			u32 tempReleased = g_currentInputDataKPAD.classic.btns_r;

			for (int i = 0; i < g_remapsButtonCount; i++)
			{
				int pos = i * 8;
				u8* srcPtr = (u8*)&g_remapsKPAD;

				u32* srcCode = (u32*)(srcPtr + pos);
				u32* destCode = (u32*)(srcPtr + pos + 4);
	
				//Pressed
				if ((tempPressed & *destCode) == *destCode)
				{
					if ((tempPressed & *srcCode) != *srcCode)
					{
						g_currentInputDataKPAD.classic.btns_d += *srcCode;
					}
				}
				else
				{
					if ((tempPressed & *srcCode) == *srcCode)
					{
						g_currentInputDataKPAD.classic.btns_d -= *srcCode;
					}
				}

				//Held
				if ((tempHold & *destCode) == *destCode)
				{
					if ((tempHold & *srcCode) != *srcCode)
					{
						g_currentInputDataKPAD.classic.btns_h += *srcCode;
					}
				}
				else
				{
					if ((tempHold & *srcCode) == *srcCode)
					{
						g_currentInputDataKPAD.classic.btns_h -= *srcCode;
					}
				}

				//Released
				if ((tempReleased & *destCode) == *destCode)
				{
					if ((tempReleased & *srcCode) != *srcCode)
					{
						g_currentInputDataKPAD.classic.btns_r += *srcCode;
					}
				}
				else
				{
					if ((tempReleased & *srcCode) == *srcCode)
					{
						g_currentInputDataKPAD.classic.btns_r -= *srcCode;
					}
				}
			}

			*buffer = g_currentInputDataKPAD;
		}

		g_currentInputDataKPAD = *buffer;
	}

	return result;
}


/* *****************************************************************************
 * Creates function pointer array
 * ****************************************************************************/

static struct hooks_magic_t
{
	const unsigned int replaceAddr;
	const unsigned int replaceCall;
	const unsigned int library;
	const char functionName[128];
	unsigned int realAddr;
	unsigned int restoreInstruction;
	unsigned char functionType;
	unsigned char alreadyPatched;
}
method_hooks[] =
{
	MAKE_MAGIC(_Exit,											LIB_CORE_INIT,STATIC_FUNCTION),
	MAKE_MAGIC(socket_lib_finish,								LIB_NSYSNET,STATIC_FUNCTION),
	MAKE_MAGIC(OSIsHomeButtonMenuEnabled,						LIB_CORE_INIT,STATIC_FUNCTION),
	MAKE_MAGIC(UCReadSysConfig,									LIB_CORE_INIT,STATIC_FUNCTION),
	MAKE_MAGIC(MCP_GetSysProdSettings,							LIB_CORE_INIT,STATIC_FUNCTION),
	MAKE_MAGIC(GetNfpRomInfo__Q2_2nn3nfpFPQ3_2nn3nfp7RomInfo,	LIB_NN_NFP,DYNAMIC_FUNCTION),
	MAKE_MAGIC(GetTagInfo__Q2_2nn3nfpFPQ3_2nn3nfp7TagInfo,		LIB_NN_NFP,DYNAMIC_FUNCTION),
	MAKE_MAGIC(VPADRead,										LIB_VPAD,STATIC_FUNCTION),
	MAKE_MAGIC(KPADReadEx,										LIB_PADSCORE,DYNAMIC_FUNCTION),
};

//! buffer to store our 7 instructions needed for our replacements
//! the code will be placed in the address of that buffer - CODE_RW_BASE_OFFSET
//! avoid this buffer to be placed in BSS and reset on start up
volatile unsigned int dynamic_method_calls[sizeof(method_hooks) / sizeof(struct hooks_magic_t) * 7] __attribute__((section(".data")));

/*
*Patches a function that is loaded at the start of each application. Its not required to restore, at least when they are really dynamic.
* "normal" functions should be patch with the normal patcher. Current Code by Maschell with the help of dimok.
*/
void PatchMethodHooks(void)
{
    /* Patch branches to it.  */
    volatile unsigned int *space = &dynamic_method_calls[0];

    int method_hooks_count = sizeof(method_hooks) / sizeof(struct hooks_magic_t);

    u32 skip_instr = 1;
    u32 my_instr_len = 6;
    u32 instr_len = my_instr_len + skip_instr;
    u32 flush_len = 4*instr_len;
    for(int i = 0; i < method_hooks_count; i++)
    {
        log_printf("Patching %s ...",method_hooks[i].functionName);
        if(method_hooks[i].functionType == STATIC_FUNCTION && method_hooks[i].alreadyPatched == 1){
            if(isDynamicFunction((u32)OSEffectiveToPhysical((void*)method_hooks[i].realAddr))){
                log_printf(" The function %s is a dynamic function. Please fix that <3 ... ", method_hooks[i].functionName);
                method_hooks[i].functionType = DYNAMIC_FUNCTION;
            }else{
                log_printf(" skipped. Its already patched\n", method_hooks[i].functionName);
                space += instr_len;
                continue;
            }
        }

        u32 physical = 0;
        unsigned int repl_addr = (unsigned int)method_hooks[i].replaceAddr;
        unsigned int call_addr = (unsigned int)method_hooks[i].replaceCall;

        unsigned int real_addr = GetAddressOfFunction(method_hooks[i].functionName,method_hooks[i].library);

        if(!real_addr){
            log_printf("Error. OSDynLoad_FindExport failed for %s\n", method_hooks[i].functionName);
            space += instr_len;
            continue;
        }

        if(DEBUG_LOG_DYN)
			log_printf("%s is located at %08X!\n", method_hooks[i].functionName,real_addr);

        physical = (u32)OSEffectiveToPhysical((void*)real_addr);
        if(!physical){
             log_printf("Error. Something is wrong with the physical address\n");
             space += instr_len;
             continue;
        }

        if(DEBUG_LOG_DYN)log_printf("%s physical is located at %08X!\n", method_hooks[i].functionName,physical);

        bat_table_t my_dbat_table;
        if(DEBUG_LOG_DYN)log_printf("Setting up DBAT\n");
        KernelSetDBATsForDynamicFuction(&my_dbat_table,physical);

        //log_printf("Setting call_addr to %08X\n",(unsigned int)(space) - CODE_RW_BASE_OFFSET);
        *(volatile unsigned int *)(call_addr) = (unsigned int)(space) - CODE_RW_BASE_OFFSET;

        // copy instructions from real function.
        u32 offset_ptr = 0;
        for(offset_ptr = 0;offset_ptr<skip_instr*4;offset_ptr +=4){
             if(DEBUG_LOG_DYN)log_printf("(real_)%08X = %08X\n",space,*(volatile unsigned int*)(physical+offset_ptr));
            *space = *(volatile unsigned int*)(physical+offset_ptr);
            space++;
        }

        //Only works if skip_instr == 1
        if(skip_instr == 1){
            // fill the restore instruction section
            method_hooks[i].realAddr = real_addr;
            method_hooks[i].restoreInstruction = *(volatile unsigned int*)(physical);
        }else{
            log_printf("Error. Can't save %s for restoring!\n", method_hooks[i].functionName);
        }

        //adding jump to real function
        /*
            90 61 ff e0     stw     r3,-32(r1)
            3c 60 12 34     lis     r3,4660
            60 63 56 78     ori     r3,r3,22136
            7c 69 03 a6     mtctr   r3
            80 61 ff e0     lwz     r3,-32(r1)
            4e 80 04 20     bctr*/
        *space = 0x9061FFE0;
        space++;
        *space = 0x3C600000 | (((real_addr + (skip_instr * 4)) >> 16) & 0x0000FFFF); // lis r3, real_addr@h
        space++;
        *space = 0x60630000 |  ((real_addr + (skip_instr * 4)) & 0x0000ffff); // ori r3, r3, real_addr@l
        space++;
        *space = 0x7C6903A6; // mtctr   r3
        space++;
        *space = 0x8061FFE0; // lwz     r3,-32(r1)
        space++;
        *space = 0x4E800420; // bctr
        space++;
        DCFlushRange((void*)(space - instr_len), flush_len);
        ICInvalidateRange((unsigned char*)(space - instr_len), flush_len);

        //setting jump back
        unsigned int replace_instr = 0x48000002 | (repl_addr & 0x03fffffc);
        *(volatile unsigned int *)(physical) = replace_instr;
        ICInvalidateRange((void*)(real_addr), 4);

        //restore my dbat stuff
        KernelRestoreDBATs(&my_dbat_table);

        method_hooks[i].alreadyPatched = 1;

        log_printf("done!\n");
    }
    log_print("Done with patching all functions!\n");
}

/* ****************************************************************** */
/*                  RESTORE ORIGINAL INSTRUCTIONS                     */
/* ****************************************************************** */
void RestoreInstructions(void)
{
    bat_table_t table;
    log_printf("Restore functions!\n");
    int method_hooks_count = sizeof(method_hooks) / sizeof(struct hooks_magic_t);
    for(int i = 0; i < method_hooks_count; i++)
    {
        log_printf("Restoring %s ...",method_hooks[i].functionName);
        if(method_hooks[i].restoreInstruction == 0 || method_hooks[i].realAddr == 0){
            log_printf("Error. I dont have the information for the restore =( skip\n");
            continue;
        }

        unsigned int real_addr = GetAddressOfFunction(method_hooks[i].functionName,method_hooks[i].library);

        if(!real_addr){
            //log_printf("Error. OSDynLoad_FindExport failed for %s\n", method_hooks[i].functionName);
            continue;
        }

        u32 physical = (u32)OSEffectiveToPhysical((void*)real_addr);
        if(!physical){
            log_printf("Error. Something is wrong with the physical address\n");
            continue;
        }

        if(isDynamicFunction(physical)){
             log_printf("Error. Its a dynamic function. We don't need to restore it! %s\n",method_hooks[i].functionName);
        }else{
            KernelSetDBATs(&table);

            *(volatile unsigned int *)(LIB_CODE_RW_BASE_OFFSET + method_hooks[i].realAddr) = method_hooks[i].restoreInstruction;
            DCFlushRange((void*)(LIB_CODE_RW_BASE_OFFSET + method_hooks[i].realAddr), 4);
            ICInvalidateRange((void*)method_hooks[i].realAddr, 4);
            log_printf(" done\n");
            KernelRestoreDBATs(&table);
        }
        method_hooks[i].alreadyPatched = 0; // In case a
    }
    KernelRestoreInstructions();
    log_print("Done with restoring all functions!\n");
}

int isDynamicFunction(unsigned int physicalAddress){
    if((physicalAddress & 0x80000000) == 0x80000000){
        return 1;
    }    return 0;
}

unsigned int GetAddressOfFunction(const char * functionName,unsigned int library){
    unsigned int real_addr = 0;

	unsigned int rpl_handle = 0;
	if (library == LIB_CORE_INIT) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_CORE_INIT\n", functionName);
		if (coreinit_handle == 0) { log_print("LIB_CORE_INIT not aquired\n"); return 0; }
		rpl_handle = coreinit_handle;
	}
	else if (library == LIB_NSYSNET) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_NSYSNET\n", functionName);
		if (nsysnet_handle == 0) { log_print("LIB_NSYSNET not aquired\n"); return 0; }
		rpl_handle = nsysnet_handle;
	}
	else if (library == LIB_GX2) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_GX2\n", functionName);
		if (gx2_handle == 0) { log_print("LIB_GX2 not aquired\n"); return 0; }
		rpl_handle = gx2_handle;
	}
	else if (library == LIB_AOC) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_AOC\n", functionName);
		if (aoc_handle == 0) { log_print("LIB_AOC not aquired\n"); return 0; }
		rpl_handle = aoc_handle;
	}
	else if (library == LIB_AX) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_AX\n", functionName);
		if (sound_handle == 0) { log_print("LIB_AX not aquired\n"); return 0; }
		rpl_handle = sound_handle;
	}
	else if (library == LIB_FS) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_FS\n", functionName);
		if (coreinit_handle == 0) { log_print("LIB_FS not aquired\n"); return 0; }
		rpl_handle = coreinit_handle;
	}
	else if (library == LIB_OS) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_OS\n", functionName);
		if (coreinit_handle == 0) { log_print("LIB_OS not aquired\n"); return 0; }
		rpl_handle = coreinit_handle;
	}
	else if (library == LIB_PADSCORE) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_PADSCORE\n", functionName);
		if (padscore_handle == 0) { log_print("LIB_PADSCORE not aquired\n"); return 0; }
		rpl_handle = padscore_handle;
	}
	else if (library == LIB_SOCKET) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_SOCKET\n", functionName);
		if (nsysnet_handle == 0) { log_print("LIB_SOCKET not aquired\n"); return 0; }
		rpl_handle = nsysnet_handle;
	}
	else if (library == LIB_SYS) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_SYS\n", functionName);
		if (sysapp_handle == 0) { log_print("LIB_SYS not aquired\n"); return 0; }
		rpl_handle = sysapp_handle;
	}
	else if (library == LIB_VPAD) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_VPAD\n", functionName);
		if (vpad_handle == 0) { log_print("LIB_VPAD not aquired\n"); return 0; }
		rpl_handle = vpad_handle;
	}
	else if (library == LIB_NN_ACP) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_NN_ACP\n", functionName);
		if (acp_handle == 0) { log_print("LIB_NN_ACP not aquired\n"); return 0; }
		rpl_handle = acp_handle;
	}
	else if (library == LIB_SYSHID) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_SYSHID\n", functionName);
		if (syshid_handle == 0) { log_print("LIB_SYSHID not aquired\n"); return 0; }
		rpl_handle = syshid_handle;
	}
	else if (library == LIB_VPADBASE) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_VPADBASE\n", functionName);
		if (vpadbase_handle == 0) { log_print("LIB_VPADBASE not aquired\n"); return 0; }
		rpl_handle = vpadbase_handle;
	}
	else if (library == LIB_NN_NFP) {
		if (DEBUG_LOG_DYN)log_printf("FindExport of %s! From LIB_NN_NFP\n", functionName);
		if (nfp_handle == 0) { log_print("LIB_NN_NFP not aquired\n"); return 0; }
		rpl_handle = nfp_handle;
	}

    if(!rpl_handle){
        log_printf("Failed to find the RPL handle for %s\n", functionName);
        return 0;
    }

    OSDynLoad_FindExport(rpl_handle, 0, functionName, &real_addr);

    if(!real_addr){
        log_printf("OSDynLoad_FindExport failed for %s\n", functionName);
        return 0;
    }

    if((u32)(*(volatile unsigned int*)(real_addr) & 0xFF000000) == 0x48000000){
        real_addr += (u32)(*(volatile unsigned int*)(real_addr) & 0x0000FFFF);
        if((u32)(*(volatile unsigned int*)(real_addr) & 0xFF000000) == 0x48000000){
            return 0;
        }
    }

    return real_addr;
}

