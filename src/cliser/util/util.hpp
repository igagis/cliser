// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//

#pragma once

#include <ting/Socket.hpp>
#include <ting/Array.hpp>
#include <ting/types.hpp>
#include <ting/Buffer.hpp>

namespace cliser{

struct NetworkReceiverState{
	unsigned numBytesInPacketSizeHolder;
	ting::StaticBuffer<ting::u8, 2> packetSizeHolder;

	unsigned numBytesToReceive;
	ting::Array<ting::u8> receivedData;

	NetworkReceiverState() :
			numBytesInPacketSizeHolder(0),
			numBytesToReceive(0)
	{}

	class PacketListener{
		friend class NetworkReceiverState;
	protected:
		virtual void OnNewDataPacketReceived(ting::Array<ting::u8> d) = 0;
	};

	//returns true if socket disconnected
	bool ReadSocket(ting::TCPSocket* s, PacketListener* rl);
};

}//~namespace
