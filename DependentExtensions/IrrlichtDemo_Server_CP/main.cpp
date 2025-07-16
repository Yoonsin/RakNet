// This is a Demo of the Irrlicht Engine (c) 2005-2008 by N.Gebhardt.
// This file is not documented.

#include <irrlicht.h>
#ifdef _WIN32__
//#include "WindowsIncludes.h" // Prevent 'fd_set' : 'struct' type redefinition
//#include <windows.h>
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
	bool isLogged = false;
	int logCount = 2;

#ifndef _IRR_WINDOWS_
	video::E_DRIVER_TYPE driverType = video::EDT_OPENGL;
#else
	video::E_DRIVER_TYPE driverType = video::EDT_OPENGL;
#endif

	CDemo::GamePlatform platform = CDemo::GamePlatform::PC;

#ifdef _WIN32
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
		token = strtok(nullptr, " ");  // 더 이상 없으면 종료
	}

	const char* baseDir = "";

	//#ifndef _DEBUG
	if (menu.run(fullscreen, music, shadows, additive, vsync, aa, driverType, playerName, isServer))
		//#endif
	{
		demo = new CDemo(fullscreen, music, shadows, additive, vsync, aa, driverType, playerName, isServer, platform, isLogged, logCount, baseDir);
		demo->run();
		delete demo;
		demo = nullptr;
	}
#else
	isServer = true;
	vsync = true;

	// 명령행 인수를 처리하여 isLogged 값 설정
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-log") == 0) {
			isLogged = true;  // -log 인수가 있으면 isLogged를 true로 설정
			if (i + 1 < argc) {
				logCount = atoi(argv[i + 1]);  // 다음 인수를 정수로 변환하여 logCount에 저장
				i++;
			}
		}
	}

	demo = new CDemo(fullscreen, music, shadows, additive, vsync, aa, driverType, playerName, isServer, platform, isLogged, logCount, "");
	demo->run();
	delete demo;
	demo = nullptr;
#endif // _WIN32

	

	return 0;
}
#endif //  __ANDROID__



