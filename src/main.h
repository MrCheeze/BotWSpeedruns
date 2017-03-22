#ifndef _MAIN_H_
#define _MAIN_H_

#include "common/types.h"
#include "common/game_defs.h"
#include "dynamic_libs/padscore_functions.h"
#include "dynamic_libs/vpad_functions.h"

/* Main */
#ifdef __cplusplus
extern "C" {
#endif

//! C wrapper for our C++ functions
int Menu_Main(void);

//Global Vars
extern const char *HOST_IP;

//For the Input Viewer Thread
extern VPADData g_currentInputDataVPAD;
extern KPADData g_currentInputDataKPAD;

//For Function Hooks.cpp
extern u8 g_langRedirectActive;
extern u8 g_gameRunning;
extern int g_gameID;

extern int g_regionCode;
extern int g_langCode;

extern u8 g_remapsVPAD[128];
extern u8 g_remapsKPAD[128];
extern u8 g_remapsButtonCount;

extern u8 g_remapsAmiibo[512];
extern u8 g_remapsAmiiboCount;

#ifdef __cplusplus
}
#endif

#endif
