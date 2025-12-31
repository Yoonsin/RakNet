#pragma once

enum GameMessages {
	ID_GAME_MESSAGE_BALL_REQUEST = ID_USER_PACKET_ENUM + 1,
	ID_GAME_MESSAGE_PLAYER_LIFE = ID_USER_PACKET_ENUM + 2,
	ID_GAME_MESSAGE_PLAYER_NAME = ID_USER_PACKET_ENUM + 3,
	ID_GAME_MESSAGE_PLAYER_RESPAWN = ID_USER_PACKET_ENUM + 4,
	ID_GAME_MESSAGE_GAME_MATCH = ID_USER_PACKET_ENUM + 5,
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
	void SendRespawnPacket();
private:
	static PacketHandler* instance;
};

