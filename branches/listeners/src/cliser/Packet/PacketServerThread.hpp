// (c) Ivan Gagis
// e-mail: igagis@gmail.com

#pragma once

#include "../ServerThread.hpp"

#include "PacketConnection.hpp"



namespace cliser{



class PacketServerThread : public ServerThread{
public:

	PacketServerThread(ting::u16 port, unsigned maxClientsPerThread) :
			ServerThread(port, maxClientsPerThread)
	{}

	//TODO:
private:
	//override
	bool OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d);

	virtual void OnPacketReceived_ts(const ting::Ref<PacketConnection>& c, ting::Array<ting::u8>& d) = 0;
};



}//~namespace
