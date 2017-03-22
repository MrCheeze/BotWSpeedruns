#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

#include <unistd.h>
#include <fcntl.h>
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/fs_functions.h"
#include "dynamic_libs/gx2_functions.h"
#include "dynamic_libs/sys_functions.h"
#include "dynamic_libs/vpad_functions.h"
#include "dynamic_libs/padscore_functions.h"
#include "dynamic_libs/socket_functions.h"
#include "dynamic_libs/nfp_functions.h"
#include "patcher/function_hooks.h"
#include "fs/fs_utils.h"
#include "fs/sd_fat_devoptab.h"
#include "kernel/kernel_functions.h"
#include "system/memory.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "common/common.h"
#include "common/game_defs.h"
#include "pygecko.h"

#define BUFFER_SIZE 40000

char *buffer[BUFFER_SIZE] __attribute__((section(".data")));

static int app_launched = 0;

//Global Extern Vars
const char *HOST_IP = "192.168.2.104";

VPADData g_currentInputDataVPAD;
KPADData g_currentInputDataKPAD;

u8 g_langRedirectActive = 0;
u8 g_gameRunning = 0;
int g_gameID = -1;

int g_regionCode = -1;
int g_langCode = -1;

u8 g_remapsVPAD[128];
u8 g_remapsKPAD[128];
u8 g_remapsButtonCount = 0;

u8 g_remapsAmiibo[512];
u8 g_remapsAmiiboCount = 0;

/* Entry point */
int Menu_Main(void)
{
        //!*******************************************************************
        //!                   Initialize function pointers                   *
        //!*******************************************************************
        //! do OS (for acquire) and sockets first so we got logging
		InitOSFunctionPointers(); //various uses
		InitSocketFunctionPointers(); //for logging
		InitSysFunctionPointers(); //for SYSLaunchMenu
		InitVPadFunctionPointers(); //for restoring VPAD Read
		InitGX2FunctionPointers(); //Graphics e.g. GX2WaitForVsync
		InitFSFunctionPointers(); //to read SD Card
		InitPadScoreFunctionPointers(); //for KPAD
		InitNFPFunctionPointers(); //for Amiibos

		SetupKernelCallback(); //for RestoreInstructions() and printing RPX Name

		//Init twice is needed so logging works properly
        log_init(HOST_IP);
		log_deinit();
		log_init(HOST_IP);

		log_printf("TCPGecko BotW App was launched\n");

        log_printf("Current RPX Name: %s\n", cosAppXmlInfoStruct.rpx_name);
	    log_printf("App Launched Value: %i\n", app_launched);

		//Return to HBL if app is launched a second time
		if (app_launched == 1)
		{
			RestoreInstructions(); //Restore original VPAD Read function and socket lib finish (and others)

			log_printf("Returning to HBL\n");
			log_deinit();
			return EXIT_SUCCESS; //Returns to HBL
		}

		//Load Config file for language and remaps
		void *pFSClient = NULL;
		void *pFSCmd = NULL;

		pFSClient = malloc(FS_CLIENT_SIZE);
		if (!pFSClient)
			return 0;

		pFSCmd = malloc(FS_CMD_BLOCK_SIZE);
		if (!pFSCmd)
			return 0;

		memset(&g_remapsVPAD, 0, 128);
		memset(&g_remapsKPAD, 0, 128);
		memset(&g_remapsAmiibo, 0, 512);

		FSInit();
		FSInitCmdBlock(pFSCmd);

		FSAddClientEx(pFSClient, 0, -1);

		int status = 0;
		int handle = 0;

		char mountSrc[FS_MOUNT_SOURCE_SIZE];
		char mountPath[FS_MAX_MOUNTPATH_SIZE];

		//Mount sdcard
		if ((status = FSGetMountSource(pFSClient, pFSCmd, FS_SOURCETYPE_EXTERNAL, &mountSrc, FS_RET_NO_ERROR)) == FS_STATUS_OK)
		{
			if ((status = FSMount(pFSClient, pFSCmd, &mountSrc, mountPath, FS_MAX_MOUNTPATH_SIZE, FS_RET_UNSUPPORTED_CMD)) == FS_STATUS_OK)
			{
				char path[255];
				FSStat stats;

				strcpy(path, "/vol/external01/wiiu/DBConfig/Config.ini");

				if ((status = FSGetStat(pFSClient, pFSCmd, path, &stats, -1)) == FS_STATUS_OK)
				{
					char* file = (char*)malloc(stats.size + 1);

					if (file)
					{
						file[stats.size] = '\0';
						if ((status = FSOpenFile(pFSClient, pFSCmd, path, "r", &handle, -1)) == FS_STATUS_OK)
						{
							int total_read = 0;
							int ret2 = 0;

							while ((ret2 = FSReadFile(pFSClient, pFSCmd, file + total_read, 1, stats.size - total_read, handle, 0, FS_RET_ALL_ERROR)) > 0)
							{
								total_read += ret2;
							}

							FSCloseFile(pFSClient, pFSCmd, handle, -1);

							const char delimiters[] = "\n";
							char* running = file;
							char* token;

							token = strsep(&running, delimiters);

							g_remapsButtonCount = 0;
							g_remapsAmiiboCount = 0;

							while (token != NULL)
							{
								char* stringPtr = 0;

								if (strstr(token, "region=") != 0)
								{
									stringPtr = strstr(token, "region=");
									stringPtr += 7;

									if (strstr(stringPtr, "JAP") != 0)
									{
										g_regionCode = 0x01;
									}
									else if (strstr(stringPtr, "USA") != 0)
									{
										g_regionCode = 0x02;
									}
									else if (strstr(stringPtr, "EUR") != 0)
									{
										g_regionCode = 0x04;
									}
									else
									{
										g_regionCode = -1;
									}

									log_printf("Region Code: %i", g_regionCode);
								}
								else if (strstr(token, "lang=") != 0)
								{
									stringPtr = strstr(token, "lang=");
									stringPtr += 5;

									if (strstr(stringPtr, "Japanese") != 0)
									{
										g_langCode = 0x00;
									}
									else if (strstr(stringPtr, "English") != 0)
									{
										g_langCode = 0x01;
									}
									else if (strstr(stringPtr, "French") != 0)
									{
										g_langCode = 0x02;
									}
									else if (strstr(stringPtr, "German") != 0)
									{
										g_langCode = 0x03;
									}
									else if (strstr(stringPtr, "Italian") != 0)
									{
										g_langCode = 0x04;
									}
									else if (strstr(stringPtr, "Spanish") != 0)
									{
										g_langCode = 0x05;
									}
									else if (strstr(stringPtr, "Chinese") != 0)
									{
										g_langCode = 0x06;
									}
									else if (strstr(stringPtr, "Korean") != 0)
									{
										g_langCode = 0x07;
									}
									else if (strstr(stringPtr, "Dutch") != 0)
									{
										g_langCode = 0x08;
									}
									else if (strstr(stringPtr, "Portugese") != 0)
									{
										g_langCode = 0x09;
									}
									else if (strstr(stringPtr, "Russian") != 0)
									{
										g_langCode = 0x0A;
									}
									else if (strstr(stringPtr, "Taiwanese") != 0)
									{
										g_langCode = 0x0B;
									}
									else
									{
										g_langCode = -1;
									}

									log_printf("Language Code: %i", g_langCode);
								}
								else if (strstr(token, "remapButton=") != 0)
								{
									stringPtr = strstr(token, "remapButton=");
									stringPtr += 12;

									int pos = g_remapsButtonCount * 8;

									u8* srcPtrVPAD = (u8*)&g_remapsVPAD;
									u8* srcPtrKPAD = (u8*)&g_remapsKPAD;

									char srcButton[3];
									char destButton[3];

									memcpy(&srcButton, stringPtr, 2);
									srcButton[2] = 0;

									memcpy(&destButton, stringPtr + 3, 2);
									destButton[2] = 0;

									log_printf("Src Button: %s, dest Button: %s", srcButton, destButton);

									for (int n = 0; n < 2; n++)
									{
										const char* currPtr = 0;

										if (n == 0)
										{
											currPtr = srcButton;
										}
										else
										{
											currPtr = destButton;
										}

										u32 buttonCodeVPAD = 0;
										u32 buttonCodeKPAD = 0;

										if (!strcmp(currPtr, "BA"))
										{
											buttonCodeVPAD = VPAD_BUTTON_A;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_A;
										}
										else if (!strcmp(currPtr, "BB"))
										{
											buttonCodeVPAD = VPAD_BUTTON_B;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_B;
										}
										else if (!strcmp(currPtr, "BY"))
										{
											buttonCodeVPAD = VPAD_BUTTON_Y;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_Y;
										}
										else if (!strcmp(currPtr, "BX"))
										{
											buttonCodeVPAD = VPAD_BUTTON_X;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_X;
										}
										else if (!strcmp(currPtr, "BP"))
										{
											buttonCodeVPAD = VPAD_BUTTON_PLUS;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_PLUS;
										}
										else if (!strcmp(currPtr, "BM"))
										{
											buttonCodeVPAD = VPAD_BUTTON_MINUS;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_MINUS;
										}
										else if (!strcmp(currPtr, "DU"))
										{
											buttonCodeVPAD = VPAD_BUTTON_UP;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_UP;
										}
										else if (!strcmp(currPtr, "DR"))
										{
											buttonCodeVPAD = VPAD_BUTTON_RIGHT;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_RIGHT;
										}
										else if (!strcmp(currPtr, "DD"))
										{
											buttonCodeVPAD = VPAD_BUTTON_DOWN;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_DOWN;
										}
										else if (!strcmp(currPtr, "DL"))
										{
											buttonCodeVPAD = VPAD_BUTTON_LEFT;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_LEFT;
										}
										else if (!strcmp(currPtr, "L1"))
										{
											buttonCodeVPAD = VPAD_BUTTON_L;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_L;
										}
										else if (!strcmp(currPtr, "L2"))
										{
											buttonCodeVPAD = VPAD_BUTTON_ZL;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_ZL;
										}
										else if (!strcmp(currPtr, "L3"))
										{
											buttonCodeVPAD = VPAD_BUTTON_STICK_L;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_STICK_L;
										}
										else if (!strcmp(currPtr, "R1"))
										{
											buttonCodeVPAD = VPAD_BUTTON_R;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_R;
										}
										else if (!strcmp(currPtr, "R2"))
										{
											buttonCodeVPAD = VPAD_BUTTON_ZR;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_ZR;
										}
										else if (!strcmp(currPtr, "R3"))
										{
											buttonCodeVPAD = VPAD_BUTTON_STICK_R;
											buttonCodeKPAD = WPAD_CLASSIC_BUTTON_STICK_R;
										}

										memcpy(srcPtrVPAD + pos, &buttonCodeVPAD, 4);
										memcpy(srcPtrKPAD + pos, &buttonCodeKPAD, 4);

										pos += 4;
									}

									g_remapsButtonCount += 1;
								}
								else if (strstr(token, "remapAmiibo=") != 0)
								{
									stringPtr = strstr(token, "remapAmiibo=");
									stringPtr += 12;

									int pos = g_remapsAmiiboCount * 16;

									u8* srcPtrAmiibo = (u8*)&g_remapsAmiibo;

									char srcAmiibo[17];
									char destAmiibo[17];

									memcpy(&srcAmiibo, stringPtr, 16);
									srcAmiibo[16] = 0;

									memcpy(&destAmiibo, stringPtr + 17, 16);
									destAmiibo[16] = 0;

									u64 srcCode = strtoull((const char*)srcAmiibo, NULL, 16);
									u64 destCode = strtoull((const char*)destAmiibo, NULL, 16);

									memcpy(srcPtrAmiibo + pos, &srcCode, 8);
									memcpy(srcPtrAmiibo + pos + 8, &destCode, 8);

									log_printf("Src Amiibo: %llX, dest Amiibo: %llX", srcCode, destCode);

									g_remapsAmiiboCount += 1;
								}

								token = strsep(&running, delimiters);
							}
						}
						else
						{
							log_printf("(FSOpenFile) Couldn't open file (%s), error: %d", path, status);
							free(file);
							file = NULL;
						}
					}
					else
					{
						log_print("Failed to allocate space for reading the file\n");
					}
				}

				if (pFSClient != NULL)
				{
					FSDelClient(pFSClient);

					free(pFSClient);
					pFSClient = NULL;
				}
				if (pFSCmd != NULL)
				{
					free(pFSCmd);
					pFSCmd = NULL;
				}
			}
			else
			{
				log_printf("FSMount failed %d\n", status);
			}
		}
		else
		{
			log_printf("FSGetMountSource failed %d\n", status);
		}
		

		log_printf("TCPGecko BotW prepared for launch\n");

		log_deinit();
	
		app_launched = 1;	

		SYSLaunchMenu(); //Launches the Wii U Main Menu

        return EXIT_RELAUNCH_ON_LOAD;
}
