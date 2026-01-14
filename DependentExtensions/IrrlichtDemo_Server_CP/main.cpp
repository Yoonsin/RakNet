// This is a Demo of the Irrlicht Engine (c) 2005-2008 by N.Gebhardt.
// This file is not documented.

#include <irrlicht.h>
#ifdef _WIN32__
//#define _CRTDBG_MAP_ALLOC
//#include <crtdbg.h>
#endif

#include <stdio.h>

#include "CMainMenu.h"
#include "CInGame.h"

using namespace irr;

#ifdef  __ANDROID__
#else
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
	bool vsync = true;
	bool aa = false;
	core::stringw playerName;

	bool isServer = false;
	bool isLocalServer = false;
	bool isLogged = true;
	bool isBot = true;
	int clientCount = 2;
	int winScore = 3;
	int methodMask = METHOD_1;
	int methodLogMask = METHOD_1;
	int scenario = 1;

#ifndef _IRR_WINDOWS_
	video::E_DRIVER_TYPE driverType = video::EDT_OPENGL;
#else
	video::E_DRIVER_TYPE driverType = video::EDT_OPENGL;
#endif
	GamePlatform platform = GamePlatform::Shooter;

#ifdef _WIN32
	/*_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);*/
	CMainMenu menu;
	char* token = strtok(strCmdLine, " ");  // ������ �������� ���ɾ �и�
	while (token != nullptr) {
		if (strcmp(token, "-log") == 0) {
			isLogged = true;  // -log �μ��� ������ isLogged�� true�� ����
		
		}
		else if (strcmp(token, "clientCnt") == 0) {
			token = strtok(nullptr, " ");  // ���� �μ��� �Ѿ��
			if (token != nullptr) {
				clientCount = atoi(token);  // ���� �μ��� ������ ��ȯ�Ͽ� clientCount�� ����
			}
		}
		else if (strcmp(token, "-bot") == 0) {
			isBot = true;
		}
		else if (strcmp(token, "-score") == 0) {
			token = strtok(nullptr, " ");  // ���� �μ��� �Ѿ��
			if (token != nullptr) {
				winScore = atoi(token);  
			}
		}
		else if (strcmp(token, "-method") == 0) {
			token = strtok(nullptr, " ");  // ���� �μ��� �Ѿ��
			if (token != nullptr) {
				methodMask = atoi(token); 
			}
		}
		else if (strcmp(token, "-methodLog") == 0) {
			token = strtok(nullptr, " ");  // ���� �μ��� �Ѿ��
			if (token != nullptr) {
				methodLogMask = atoi(token);
			}
		}
		else if (strcmp(token, "-scenario") == 0) {
			token = strtok(nullptr, " ");  // ���� �μ��� �Ѿ��
			if (token != nullptr) {
				scenario = atoi(token);
			}
		}
		token = strtok(nullptr, " ");  // �� �̻� ������ ����
	}

	const char* baseDir = "C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/stats";

	//#ifndef _DEBUG
	if (menu.run(fullscreen, music, shadows, additive, vsync, aa, driverType, playerName, isServer,clientCount,isBot, isLocalServer))
		//#endif
	{
		if (isServer) platform = GamePlatform::Server;
		new CInGame(fullscreen, music, shadows, additive, vsync, aa, driverType, playerName, isServer, platform, isLogged, clientCount, baseDir, isBot, winScore, methodMask, methodLogMask, scenario, isLocalServer);
		CInGame::Instance()->Run();
		CInGame::DestroyInstance();
	}
#else
	isServer = true;
	vsync = true;
	if (isServer) platform = GamePlatform::Server;

	// ������ �μ��� ó���Ͽ� isLogged �� ����
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-log") == 0) {
			isLogged = true;  // -log �μ��� ������ isLogged�� true�� ����
		}
		else if (strcmp(argv[i], "-clientCnt") == 0) {
			if (i + 1 < argc) {
				clientCount = atoi(argv[i + 1]);  // ���� �μ��� ������ ��ȯ�Ͽ� clientCount
				i++;
			}
		}
		else if (strcmp(argv[i], "-bot") == 0) {
			isBot = true;
		}
		else if (strcmp(argv[i], "-score") == 0) {
			if (i + 1 < argc) {
				winScore = atoi(argv[i + 1]);  
				i++;
			}
		}
		else if (strcmp(argv[i], "-method") == 0) {
			if (i + 1 < argc) {
				methodMask = atoi(argv[i + 1]);  
				i++;
			}
		}
		else if (strcmp(argv[i], "-methodLog") == 0) {
			if (i + 1 < argc) {
				methodLogMask = atoi(argv[i + 1]);
				i++;
			}
		}
		else if (strcmp(argv[i], "-scenario") == 0) {
			if (i + 1 < argc) {
				scenario = atoi(argv[i + 1]);
				i++;
			}
		}
	}

	const char* baseDir = "/home/parts/stats";

	new CInGame(fullscreen, music, shadows, additive, vsync, aa, driverType, playerName, isServer, platform, isLogged, clientCount, baseDir,isBot, winScore, methodMask, methodLogMask, scenario, isLocalServer);
	CInGame::Instance()->Run();
	CInGame::DestroyInstance();
#endif // _WIN32
	return 0;
}
#endif //  __ANDROID__



