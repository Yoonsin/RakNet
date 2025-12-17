// This is a Demo of the Irrlicht Engine (c) 2005-2008 by N.Gebhardt.
// This file is not documented.

#include <irrlicht.h>
#ifdef _WIN32__
//#define _CRTDBG_MAP_ALLOC
//#include <crtdbg.h>
#endif

#include <stdio.h>

#include "CMainMenu.h"
#include "CDemo.h"

using namespace irr;

#ifdef  __ANDROID__
#else
CDemo* demo = nullptr;
#ifdef _WIN32
//#pragma comment(lib, "Irrlicht.lib")
INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR strCmdLine, INT)
#else
int main(int argc, char* argv[])
#endif
{
	bool fullscreen = false;
	bool music = true;
	bool shadows = false;
	bool additive = false;
	bool vsync = false;
	bool aa = false;
	core::stringw playerName;

	bool isServer = false;
	bool isLogged = true;
	bool isBot = false;
	int logCount = 2;
	int winScore = 3;
	int methodMask = 5;

#ifndef _IRR_WINDOWS_
	video::E_DRIVER_TYPE driverType = video::EDT_OPENGL;
#else
	video::E_DRIVER_TYPE driverType = video::EDT_OPENGL;
#endif
	GamePlatform platform = GamePlatform::Shooter;

#ifdef _WIN32
	/*_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);*/
	CMainMenu menu;
	char* token = strtok(strCmdLine, " ");  // 공백을 기준으로 명령어를 분리
	while (token != nullptr) {
		if (strcmp(token, "-log") == 0) {
			isLogged = true;  // -log 인수가 있으면 isLogged를 true로 설정
			token = strtok(nullptr, " ");  // 다음 인수로 넘어가기
			if (token != nullptr) {
				logCount = atoi(token);  // 다음 인수를 정수로 변환하여 logCount에 저장
			}
		}
		else if (strcmp(token, "-bot") == 0) {
			isBot = true;
		}
		else if (strcmp(token, "-score") == 0) {
			token = strtok(nullptr, " ");  // 다음 인수로 넘어가기
			if (token != nullptr) {
				winScore = atoi(token);  // 다음 인수를 정수로 변환하여 logCount에 저장
			}
		}
		else if (strcmp(token, "-method") == 0) {
			token = strtok(nullptr, " ");  // 다음 인수로 넘어가기
			if (token != nullptr) {
				methodMask = atoi(token);  // 다음 인수를 정수로 변환하여 logCount에 저장
			}
		}
		token = strtok(nullptr, " ");  // 더 이상 없으면 종료
	}

	const char* baseDir = "C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/stats/";

	//#ifndef _DEBUG
	if (menu.run(fullscreen, music, shadows, additive, vsync, aa, driverType, playerName, isServer,logCount,isBot))
		//#endif
	{
		if (isServer) platform = GamePlatform::Server;
		demo = new CDemo(fullscreen, music, shadows, additive, vsync, aa, driverType, playerName, isServer, platform, isLogged, logCount, baseDir, isBot, winScore, methodMask);
		demo->run();
		delete demo;
		demo = nullptr;
	}
#else
	isServer = true;
	vsync = true;
	if (isServer) platform = GamePlatform::Server;

	// 명령행 인수를 처리하여 isLogged 값 설정
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-log") == 0) {
			isLogged = true;  // -log 인수가 있으면 isLogged를 true로 설정
			if (i + 1 < argc) {
				logCount = atoi(argv[i + 1]);  // 다음 인수를 정수로 변환하여 logCount에 저장
				i++;
			}
		}
		else if (strcmp(argv[i], "-bot") == 0) {
			isBot = true;
		}
		else if (strcmp(argv[i], "-score") == 0) {
			if (i + 1 < argc) {
				winScore = atoi(argv[i + 1]);  // 다음 인수를 정수로 변환하여 logCount에 저장
				i++;
			}
		}
		else if (strcmp(argv[i], "-method") == 0) {
			if (i + 1 < argc) {
				methodMask = atoi(argv[i + 1]);  // 다음 인수를 정수로 변환하여 logCount에 저장
				i++;
			}
		}
	}

	demo = new CDemo(fullscreen, music, shadows, additive, vsync, aa, driverType, playerName, isServer, platform, isLogged, logCount, "",isBot, winScore, methodMask);
	demo->run();
	delete demo;
	demo = nullptr;
#endif // _WIN32
	return 0;
}
#endif //  __ANDROID__



