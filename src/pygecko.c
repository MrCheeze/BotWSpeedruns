#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include "common/common.h"
#include "common/thread_defs.h"
#include "main.h"
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/socket_functions.h"
#include "dynamic_libs/gx2_functions.h"
#include "dynamic_libs/vpad_functions.h"
#include "dynamic_libs/sys_functions.h"
#include "dynamic_libs/padscore_functions.h"
#include "dynamic_libs/fs_functions.h"
#include "dynamic_libs/nfp_functions.h"
#include "kernel/syscalls.h"
#include "utils/logger.h"
#include "patcher/function_hooks.h"


#define CHECK_ERROR(cond) if (cond) { bss->line = __LINE__; goto error; }
#define errno (*__gh_errno_ptr())
#define MSG_DONTWAIT 32
#define EWOULDBLOCK 6


static int recvwait(struct pygecko_bss_t *bss, int sock, void *buffer, int len) {
	int ret;
	while (len > 0) {
		ret = recv(sock, buffer, len, 0);
		CHECK_ERROR(ret < 0);
		len -= ret;
		buffer += ret;
	}
	return 0;
error:
	bss->error = ret;
	return ret;
}

static int recvbyte(struct pygecko_bss_t *bss, int sock) {
	unsigned char buffer[1];
	int ret;

	ret = recvwait(bss, sock, buffer, 1);
	if (ret < 0) return ret;
	return buffer[0];
}

static int checkbyte(struct pygecko_bss_t *bss, int sock) {
	unsigned char buffer[1];
	int ret;

	ret = recv(sock, buffer, 1, MSG_DONTWAIT);
	if (ret < 0) return ret;
	if (ret == 0) return ret; //-1
	return buffer[0];
}

static int sendwait(struct pygecko_bss_t *bss, int sock, const void *buffer, int len) {
	int ret;
	while (len > 0) {
		ret = send(sock, buffer, len, 0);
		CHECK_ERROR(ret < 0);
		len -= ret;
		buffer += ret;
	}
	return 0;
error:
	bss->error = ret;
	return ret;
}

static int sendbyte(struct pygecko_bss_t *bss, int sock, unsigned char byte) {
	unsigned char buffer[1];

	buffer[0] = byte;
	return sendwait(bss, sock, buffer, 1);
}

//Send Inputs Function
static int SendInputs(struct pygecko_bss_t *bss, int clientfd, int sendDirections)
{
	int ret = 0;

	unsigned char Directions1[1];
	unsigned char Directions2[1];

	Directions1[0] = 0;
	Directions2[0] = 0;

	unsigned char Buttons1[1];
	unsigned char Buttons2[1];
	unsigned char Buttons3[1];

	Buttons1[0] = 0;
	Buttons2[0] = 0;
	Buttons3[0] = 0;

	//Use Pro Controller if it is in use
	if (g_currentInputDataKPAD.classic.btns_d != 0 || g_currentInputDataKPAD.classic.btns_h != 0 || g_currentInputDataKPAD.classic.btns_r != 0 ||
		g_currentInputDataKPAD.classic.lstick_x > 0.05f || g_currentInputDataKPAD.classic.lstick_y > 0.05f ||
		g_currentInputDataKPAD.classic.rstick_x > 0.05f || g_currentInputDataKPAD.classic.rstick_y > 0.05f ||
		g_currentInputDataKPAD.classic.lstick_x < -0.05f || g_currentInputDataKPAD.classic.lstick_y < -0.05f ||
		g_currentInputDataKPAD.classic.rstick_x < -0.05f || g_currentInputDataKPAD.classic.rstick_y < -0.05f)
	{
		//Sticks
		ret = sendwait(bss, clientfd, &g_currentInputDataKPAD.classic.lstick_x, 4);
		CHECK_ERROR(ret < 0);

		ret = sendwait(bss, clientfd, &g_currentInputDataKPAD.classic.lstick_y, 4);
		CHECK_ERROR(ret < 0);

		ret = sendwait(bss, clientfd, &g_currentInputDataKPAD.classic.rstick_x, 4);
		CHECK_ERROR(ret < 0);

		ret = sendwait(bss, clientfd, &g_currentInputDataKPAD.classic.rstick_y, 4);
		CHECK_ERROR(ret < 0);

		//Directions Left Stick
		if (g_currentInputDataKPAD.classic.lstick_y > 0.05f)
			Directions1[0] += 1;
		else if (g_currentInputDataKPAD.classic.lstick_y < -0.05f)
			Directions1[0] += 4;

		if (g_currentInputDataKPAD.classic.lstick_x > 0.05f)
			Directions1[0] += 2;
		else if (g_currentInputDataKPAD.classic.lstick_x < -0.05f)
			Directions1[0] += 8;

		//Directions Right Stick
		if (g_currentInputDataKPAD.classic.rstick_y > 0.05f)
			Directions2[0] += 1;
		else if (g_currentInputDataKPAD.classic.rstick_y < -0.05f)
			Directions2[0] += 4;

		if (g_currentInputDataKPAD.classic.rstick_x > 0.05f)
			Directions2[0] += 2;
		else if (g_currentInputDataKPAD.classic.rstick_x < -0.05f)
			Directions2[0] += 8;

		if (sendDirections == 1) //Data Viewer Apps Only
		{
			ret = sendwait(bss, clientfd, Directions1, 1);
			CHECK_ERROR(ret < 0);

			ret = sendwait(bss, clientfd, Directions2, 1);
			CHECK_ERROR(ret < 0);
		}

		//Button 1
		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_A) == WPAD_CLASSIC_BUTTON_A) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_A) == WPAD_CLASSIC_BUTTON_A)
			Buttons1[0] += 128;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_B) == WPAD_CLASSIC_BUTTON_B) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_B) == WPAD_CLASSIC_BUTTON_B)
			Buttons1[0] += 64;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_X) == WPAD_CLASSIC_BUTTON_X) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_X) == WPAD_CLASSIC_BUTTON_X)
			Buttons1[0] += 32;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_Y) == WPAD_CLASSIC_BUTTON_Y) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_Y) == WPAD_CLASSIC_BUTTON_Y)
			Buttons1[0] += 16;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_LEFT) == WPAD_CLASSIC_BUTTON_LEFT) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_LEFT) == WPAD_CLASSIC_BUTTON_LEFT)
			Buttons1[0] += 8;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_RIGHT) == WPAD_CLASSIC_BUTTON_RIGHT) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_RIGHT) == WPAD_CLASSIC_BUTTON_RIGHT)
			Buttons1[0] += 4;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_UP) == WPAD_CLASSIC_BUTTON_UP) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_UP) == WPAD_CLASSIC_BUTTON_UP)
			Buttons1[0] += 2;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_DOWN) == WPAD_CLASSIC_BUTTON_DOWN) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_DOWN) == WPAD_CLASSIC_BUTTON_DOWN)
			Buttons1[0] += 1;


		//Button 2
		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_ZL) == WPAD_CLASSIC_BUTTON_ZL) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_ZL) == WPAD_CLASSIC_BUTTON_ZL)
			Buttons2[0] += 128;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_ZR) == WPAD_CLASSIC_BUTTON_ZR) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_ZR) == WPAD_CLASSIC_BUTTON_ZR)
			Buttons2[0] += 64;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_L) == WPAD_CLASSIC_BUTTON_L) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_L) == WPAD_CLASSIC_BUTTON_L)
			Buttons2[0] += 32;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_R) == WPAD_CLASSIC_BUTTON_R) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_R) == WPAD_CLASSIC_BUTTON_R)
			Buttons2[0] += 16;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_PLUS) == WPAD_CLASSIC_BUTTON_PLUS) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_PLUS) == WPAD_CLASSIC_BUTTON_PLUS)
			Buttons2[0] += 8;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_MINUS) == WPAD_CLASSIC_BUTTON_MINUS) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_MINUS) == WPAD_CLASSIC_BUTTON_MINUS)
			Buttons2[0] += 4;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_STICK_L) == WPAD_CLASSIC_BUTTON_STICK_L) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_STICK_L) == WPAD_CLASSIC_BUTTON_STICK_L)
			Buttons2[0] += 2;

		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_STICK_R) == WPAD_CLASSIC_BUTTON_STICK_R) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_STICK_R) == WPAD_CLASSIC_BUTTON_STICK_R)
			Buttons2[0] += 1;


		//Button 3
		if (((g_currentInputDataKPAD.classic.btns_d & WPAD_CLASSIC_BUTTON_HOME) == WPAD_CLASSIC_BUTTON_HOME) || (g_currentInputDataKPAD.classic.btns_h & WPAD_CLASSIC_BUTTON_HOME) == WPAD_CLASSIC_BUTTON_HOME)
			Buttons3[0] += 2;
	}
	else //Fall back to Gamepad by default
	{
		//Sticks
		ret = sendwait(bss, clientfd, &g_currentInputDataVPAD.lstick.x, 4);
		CHECK_ERROR(ret < 0);

		ret = sendwait(bss, clientfd, &g_currentInputDataVPAD.lstick.y, 4);
		CHECK_ERROR(ret < 0);

		ret = sendwait(bss, clientfd, &g_currentInputDataVPAD.rstick.x, 4);
		CHECK_ERROR(ret < 0);

		ret = sendwait(bss, clientfd, &g_currentInputDataVPAD.rstick.y, 4);
		CHECK_ERROR(ret < 0);

		//Directions Left Stick
		if (g_currentInputDataVPAD.lstick.y > 0.05f)
			Directions1[0] += 1;
		else if (g_currentInputDataVPAD.lstick.y < -0.05f)
			Directions1[0] += 4;

		if (g_currentInputDataVPAD.lstick.x > 0.05f)
			Directions1[0] += 2;
		else if (g_currentInputDataVPAD.lstick.x < -0.05f)
			Directions1[0] += 8;

		//Directions Right Stick
		if (g_currentInputDataVPAD.rstick.y > 0.05f)
			Directions2[0] += 1;
		else if (g_currentInputDataVPAD.rstick.y < -0.05f)
			Directions2[0] += 4;

		if (g_currentInputDataVPAD.rstick.x > 0.05f)
			Directions2[0] += 2;
		else if (g_currentInputDataVPAD.rstick.x < -0.05f)
			Directions2[0] += 8;

		if (sendDirections == 1) //Data Viewer Apps Only
		{
			ret = sendwait(bss, clientfd, Directions1, 1);
			CHECK_ERROR(ret < 0);

			ret = sendwait(bss, clientfd, Directions2, 1);
			CHECK_ERROR(ret < 0);
		}

		//Button 1
		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_A) == VPAD_BUTTON_A) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_A) == VPAD_BUTTON_A)
			Buttons1[0] += 128;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_B) == VPAD_BUTTON_B) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_B) == VPAD_BUTTON_B)
			Buttons1[0] += 64;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_X) == VPAD_BUTTON_X) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_X) == VPAD_BUTTON_X)
			Buttons1[0] += 32;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_Y) == VPAD_BUTTON_Y) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_Y) == VPAD_BUTTON_Y)
			Buttons1[0] += 16;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_LEFT) == VPAD_BUTTON_LEFT) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_LEFT) == VPAD_BUTTON_LEFT)
			Buttons1[0] += 8;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_RIGHT) == VPAD_BUTTON_RIGHT) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_RIGHT) == VPAD_BUTTON_RIGHT)
			Buttons1[0] += 4;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_UP) == VPAD_BUTTON_UP) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_UP) == VPAD_BUTTON_UP)
			Buttons1[0] += 2;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_DOWN) == VPAD_BUTTON_DOWN) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_DOWN) == VPAD_BUTTON_DOWN)
			Buttons1[0] += 1;


		//Button 2
		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_ZL) == VPAD_BUTTON_ZL) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_ZL) == VPAD_BUTTON_ZL)
			Buttons2[0] += 128;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_ZR) == VPAD_BUTTON_ZR) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_ZR) == VPAD_BUTTON_ZR)
			Buttons2[0] += 64;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_L) == VPAD_BUTTON_L) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_L) == VPAD_BUTTON_L)
			Buttons2[0] += 32;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_R) == VPAD_BUTTON_R) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_R) == VPAD_BUTTON_R)
			Buttons2[0] += 16;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_PLUS) == VPAD_BUTTON_PLUS) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_PLUS) == VPAD_BUTTON_PLUS)
			Buttons2[0] += 8;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_MINUS) == VPAD_BUTTON_MINUS) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_MINUS) == VPAD_BUTTON_MINUS)
			Buttons2[0] += 4;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_STICK_L) == VPAD_BUTTON_STICK_L) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_STICK_L) == VPAD_BUTTON_STICK_L)
			Buttons2[0] += 2;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_STICK_R) == VPAD_BUTTON_STICK_R) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_STICK_R) == VPAD_BUTTON_STICK_R)
			Buttons2[0] += 1;


		//Button 3
		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_TV) == VPAD_BUTTON_TV) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_TV) == VPAD_BUTTON_TV)
			Buttons3[0] += 1;

		if (((g_currentInputDataVPAD.btns_d & VPAD_BUTTON_HOME) == VPAD_BUTTON_HOME) || (g_currentInputDataVPAD.btns_h & VPAD_BUTTON_HOME) == VPAD_BUTTON_HOME)
			Buttons3[0] += 2;

		if (g_currentInputDataVPAD.tpdata.touched == 1 || g_currentInputDataVPAD.tpdata1.touched == 1 || g_currentInputDataVPAD.tpdata2.touched == 1)
			Buttons3[0] += 4;
	}

	ret = sendwait(bss, clientfd, Buttons1, 1);
	CHECK_ERROR(ret < 0);

	ret = sendwait(bss, clientfd, Buttons2, 1);
	CHECK_ERROR(ret < 0);

	ret = sendwait(bss, clientfd, Buttons3, 1);
	CHECK_ERROR(ret < 0);

	return 0;

error:
	bss->error = ret;
	return -1;
}

//Handles the connection to the Input Viewer App (NintendoSpy)
static int run_inputViewer(struct pygecko_bss_t *bss, int clientfd)
{
	int ret = 0;

	unsigned char Cmd[1];
	Cmd[0] = 0x01;

	while (1)
	{
		GX2WaitForVsync();

		//Send Cmd Bit
		ret = sendwait(bss, clientfd, Cmd, 1);
		CHECK_ERROR(ret < 0);

		ret = SendInputs(bss, clientfd, 0);
		CHECK_ERROR(ret < 0);
	}

error:
	bss->error = ret;
	return ret;
}

//Input Display Thread
static int start_inputViewer(int argc, void *argv)
{
	int sockfd = -1, clientfd = -1, ret = 0, len;
	struct sockaddr_in addr;
	struct pygecko_bss_t *bss = argv;

	while (1)
	{
		addr.sin_family = AF_INET;
		addr.sin_port = 7335; //Input Display uses Port 7335
		addr.sin_addr.s_addr = 0;

		sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  //Open socket
		CHECK_ERROR(sockfd == -1);

		ret = bind(sockfd, (void *)&addr, 16);
		CHECK_ERROR(ret < 0);

		ret = listen(sockfd, 20);
		CHECK_ERROR(ret < 0);

		while (1)
		{
			len = 16;
			clientfd = accept(sockfd, (void *)&addr, &len);
			CHECK_ERROR(clientfd == -1);

			ret = run_inputViewer(bss, clientfd); //This function returns once the client disconnects or an error occurs
			CHECK_ERROR(ret < 0);

			socketclose(clientfd);
			clientfd = -1;
		}

		socketclose(sockfd);
		sockfd = -1;
	error:
		if (clientfd != -1)
			socketclose(clientfd);
		if (sockfd != -1)
			socketclose(sockfd);
		bss->error = ret;

	}
	return 0;
}

static int CCThread(int argc, void *argv) 
{
	//Need to wait a bit so the game has fully started, otherwise it can be unstable and crash the console when trying to connect to a socket
	usleep(7000000);

	InitOSFunctionPointers(); //Cafe OS functions e.g. OSGetTitleID
	InitSocketFunctionPointers(); //Sockets
	InitGX2FunctionPointers(); //Graphics e.g. GX2WaitForVsync
	InitSysFunctionPointers(); //for SYSLaunchMenu
	InitNFPFunctionPointers(); //for Amiibos
	InitPadScoreFunctionPointers();

	GX2WaitForVsync();

	//Init twice is needed so logging works properly
	log_init(HOST_IP);
	log_deinit();
	log_init(HOST_IP);

	log_printf("Game has launched, starting Threads...\n");

	//Input Display Thread
	struct pygecko_bss_t *bss;

	bss = memalign(0x40, sizeof(struct pygecko_bss_t));
	if (bss == 0)
		return 0;
	memset(bss, 0, sizeof(struct pygecko_bss_t));

	if (OSCreateThread(&bss->thread, start_inputViewer, 1, bss, (u32)bss->stack + sizeof(bss->stack), sizeof(bss->stack), 2, OS_THREAD_ATTRIB_AFFINITY_CPU0) == 1)
	{
		OSResumeThread(&bss->thread);
	}
	else
	{
		log_printf("Starting Input Display thread failed!\n");
		free(bss);
	}

	//Handles home button usage (after closing the menu set running flag)
	u8 frameCounter = 0;

	while (1)
	{
		GX2WaitForVsync();

		if (g_gameRunning == 0)
		{
			frameCounter++;

			if (frameCounter == 30)
			{
				frameCounter = 0;
				g_gameRunning = 1;
				g_langRedirectActive = 1;
			}
		}
	}


	return 0;
}

//Gets called when a "game" boots. Creates a helper thread so execution can return to the booting game as quickly as possible
void start_pygecko(void)
{
	unsigned int stack = (unsigned int)memalign(0x40, 0x1000); //Allocates 4096 bytes as own RAM for the new thread and aligns them in 64 byte blocks, returns pointer to the memory

	OSThread *thread = (OSThread *)stack;

	//OSCreateThread Reference: http://bombch.us/CSXL ; Attribute Reference: http://bombch.us/CSXP	
	//Note: Core/CPU0 is reserved for the system and not used by games, CPU1 and 2 are for game tasks

	if (OSCreateThread(thread, CCThread, 1, NULL, stack + sizeof(stack), sizeof(stack), 2, OS_THREAD_ATTRIB_AFFINITY_CPU0) == 1) //Run on System Core
	{
		OSResumeThread(thread); //Thread sleeps by default, so make sure we resume it
	}
	else
	{
		free(thread); //Clear thread memory if something goes wrong
	}
}