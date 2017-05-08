/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include "os_functions.h"
#include "nfp_functions.h"
#include "common/nfp_defs.h"

unsigned int nfp_handle __attribute__((section(".data"))) = 0;

EXPORT_DECL(int, SwitchToAmiiboSettings__Q2_2nn3nfpFRCQ3_2nn3nfp20AmiiboSettingsArgsInPCcUi, const AmiiboSettingsArgs *settings, const char *str, u32 val);
EXPORT_DECL(int, GetNfpRomInfo__Q2_2nn3nfpFPQ3_2nn3nfp7RomInfo, u32* ptr);
EXPORT_DECL(int, GetTagInfo__Q2_2nn3nfpFPQ3_2nn3nfp7TagInfo, NFPTagInfo* taginfo);

void InitAcquireNFP(void)
{
     OSDynLoad_Acquire("nn_nfp.rpl", &nfp_handle);
}

void InitNFPFunctionPointers(void)
{
	unsigned int *funcPointer = 0;

    InitAcquireNFP();

    if(nfp_handle == 0)
	{
        return;
    }

    OS_FIND_EXPORT(nfp_handle, SwitchToAmiiboSettings__Q2_2nn3nfpFRCQ3_2nn3nfp20AmiiboSettingsArgsInPCcUi);
	OS_FIND_EXPORT(nfp_handle, GetNfpRomInfo__Q2_2nn3nfpFPQ3_2nn3nfp7RomInfo);
	OS_FIND_EXPORT(nfp_handle, GetTagInfo__Q2_2nn3nfpFPQ3_2nn3nfp7TagInfo);
}
