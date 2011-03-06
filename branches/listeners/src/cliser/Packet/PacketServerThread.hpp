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
	ting::Ref<cliser::Connection> CreateConnectionObject(){
		return ting::Ref<cliser::Connection>(
				this->CreatePacketConnectionObject().operator->()
			);
	}

	//override
	void OnConnected_ts(const ting::Ref<Connection>& c){
		this->OnConnectedPacket_ts(
				ting::Ref<PacketConnection>(
						static_cast<PacketConnection*>(c.operator->())
					)
			);
	}

	//override
	void OnDisconnected_ts(const ting::Ref<Connection>& c){
		this->OnDisconnectedPacket_ts(
				ting::Ref<PacketConnection>(
						static_cast<PacketConnection*>(c.operator->())
					)
			);
	}

	//override
	bool OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d);

	//override
	void OnDataSent_ts(const ting::Ref<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){
		this->OnPacketSent_ts(
				ting::Ref<PacketConnection>(
						static_cast<PacketConnection*>(c.operator->())
					),
				numPacketsInQueue,
				addedToQueue
			);
	}



	virtual ting::Ref<cliser::PacketConnection> CreatePacketConnectionObject() = 0;

	virtual void OnConnectedPacket_ts(const ting::Ref<PacketConnection>& c) = 0;

	virtual void OnDisconnectedPacket_ts(const ting::Ref<PacketConnection>& c) = 0;

	virtual void OnPacketReceived_ts(const ting::Ref<PacketConnection>& c, ting::Array<ting::u8>& d) = 0;

	virtual void OnPacketSent_ts(const ting::Ref<PacketConnection>& c, unsigned numPacketsInQueue, bool addedToQueue){}
};



}//~namespace
