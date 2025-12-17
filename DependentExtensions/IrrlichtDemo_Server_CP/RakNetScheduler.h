#pragma once

#include "RakNetStuff.h"
#include "NetworkIDManager.h"
#include "CDemo.h"
#include "RakNetTime.h"
#include "GetTime.h"
#include "SocketLayer.h"
#include "RakNetStatistics.h"
#include "StatisticsHistory.h"
#include "PacketLogger.h"
#include <stdio.h>
#include <time.h>
#include <chrono>
#include <irrlicht.h>

struct DelayedPacket {
	RakNet::BitStream* bs;      // 패킷 데이터
	PacketPriority priority;
	PacketReliability reliability;
	char orderingChannel;
	RakNet::AddressOrGUID systemIdentifier;
	RakNet::TimeMS releaseTime; // 언제 보낼지 (현재시간 + 추가딜레이)
};

class PacketScheduler {
	std::deque<DelayedPacket> sendQueue;
	RakNet::RakPeerInterface* rakPeer;

public:
	// 1. 패킷 전송 요청 (즉시 보내지 않고 큐에 넣음)
	void SendUnified(RakNet::BitStream* bs, PacketPriority p, PacketReliability r,
		char chan, RakNet::AddressOrGUID target, int addedDelayMS)
	{
		DelayedPacket packet;
		// BitStream은 복사가 필요할 수 있으므로 주의 (여기선 개념만 설명)
		packet.bs = new RakNet::BitStream();
		packet.bs->Write(bs);

		packet.priority = p;
		packet.reliability = r;
		packet.orderingChannel = chan;
		packet.systemIdentifier = target;


		// 현재 시간 + 목표 딜레이 = 보낼 시간
		packet.releaseTime = RakNet::GetTimeMS() + addedDelayMS;
		sendQueue.push_back(packet);
	}

	// 2. 매 프레임 호출 (시간 된 패킷 발송)
	void Update() {
		RakNet::TimeMS currentTime = RakNet::GetTimeMS();

		// 큐 앞부분부터 확인 (시간 순서대로 들어갔다고 가정)
		auto it = sendQueue.begin();
		while (it != sendQueue.end()) {
			if (currentTime >= it->releaseTime) {
				// 시간이 됐으면 진짜 RakNet으로 전송
				rakPeer->Send(it->bs, it->priority, it->reliability,
					it->orderingChannel, it->systemIdentifier, false);

				delete it->bs; // 메모리 해제
				it = sendQueue.erase(it); // 큐에서 제거
			}
			else {
				// 큐는 시간순 정렬이 보장된다면 여기서 break 해도 됨
				// 정렬이 안 되어 있다면 모든 요소를 순회해야 함
				++it;
			}
		}
	}
};
