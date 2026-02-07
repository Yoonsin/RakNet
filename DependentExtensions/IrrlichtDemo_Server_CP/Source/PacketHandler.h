#pragma once
#include "MessageIdentifiers.h"

enum GameMessages {
	ID_GAME_MESSAGE_BALL_REQUEST = ID_USER_PACKET_ENUM + 1,
	ID_GAME_MESSAGE_PLAYER_LIFE = ID_USER_PACKET_ENUM + 2,
	ID_GAME_MESSAGE_PLAYER_NAME = ID_USER_PACKET_ENUM + 3,
	ID_GAME_MESSAGE_PLAYER_RESPAWN = ID_USER_PACKET_ENUM + 4,
	ID_GAME_MESSAGE_GAME_MATCH = ID_USER_PACKET_ENUM + 5,
	ID_GAME_MESSAGE_PLAYER_ACK = ID_USER_PACKET_ENUM + 6, //Method 1
	ID_GAME_MESSAGE_PLAYER_ORDER = ID_USER_PACKET_ENUM + 7, //Method 2
	ID_GAME_MESSAGE_HEAVY_PACKET = ID_USER_PACKET_ENUM + 8, //Method 3
	ID_GAME_MESSAGE_STRESS_TEST_PENDING = ID_USER_PACKET_ENUM + 9, 
};

class PacketHandler
{
public:
	static PacketHandler* Instance();
	static void DestroyInstance();

	PacketHandler();
	virtual ~PacketHandler();
	void OnHandlePacket();
	void Update();
	void OnUpdateReplica();
	void MakeRespawnPacket(RakNet::BitStream* outBs);
private:
	static PacketHandler* instance;
	int testPacketCount;
	int testRountCount;
	RakNet::TimeMS testStartTime;
};

