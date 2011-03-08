// (c) Ivan Gagis
// e-mail: igagis@gmail.com

#pragma once

#include "../Connection.hpp"


namespace cliser{

class Packet : public ting::Buffer<ting::u8>{
	friend class PacketConnection;

	ting::Array<ting::u8> array;
public:
	Packet() :
			ting::Buffer<ting::u8>(0, 0)
	{}

	Packet(ting::u16 size);

	Packet(const Packet& packet){
		this->operator=(packet);
	}

	Packet& operator=(const Packet& packet);
};



class PacketConnection : private Connection, public virtual ting::RefCounted{
	ting::Inited<unsigned, 0> numBytesInPacketSizeHolder;

#ifdef DEBUG
	ting::StaticBuffer<ting::u8, 2> packetSizeHolder;
#else
	ting::u8 packetSizeHolder[2];
#endif

	ting::Inited<unsigned, 0> numBytesToReceive;
	ting::Array<ting::u8> receivedData;//TODO: rename to packet

	ting::Array<ting::u8> unhandledPacket;
public:

	void Send_ts(Packet packet);
	void SendCopy_ts(const ting::Buffer<ting::u8>& packet);

	ting::Array<ting::u8> GetReceivedPacket_ts();

	static inline ting::Ref<cliser::PacketConnection> New(){
		return ting::Ref<cliser::PacketConnection>(new cliser::PacketConnection());
	}

private:
	ting::Array<ting::u8> HandleIncomingData_ts(const ting::Buffer<ting::u8>& data, PacketListener *listener);
};



class PacketListener{
	friend class cliser::PacketServerThread;

	class InternalListener : public cliser::Listener{
		PacketListener* pl;

		//override
		ting::Ref<cliser::Connection> CreateConnectionObject(){
			return ting::Ref<cliser::Connection>(
					this->pl->CreateConnectionObject().operator->()
				);
		}

		//override
		void OnConnected_ts(const ting::Ref<Connection>& c){
			this->pl->OnConnected_ts(ting::Ref<PacketConnection>(
					static_cast<PacketConnection*>(c.operator->())
				));
		}

		//override
		void OnDisconnected_ts(const ting::Ref<Connection>& c){
			this->pl->OnDisconnected_ts(ting::Ref<PacketConnection>(
					static_cast<PacketConnection*>(c.operator->())
				));
		}

		//override
		bool OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d);

		//override
		void OnDataSent_ts(const ting::Ref<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){
			this->pl->OnPacketSent_ts(
					ting::Ref<PacketConnection>(
							static_cast<PacketConnection*>(c.operator->())
						),
					numPacketsInQueue,
					addedToQueue
				);
		}
		
	public:
		InternalListener(PacketListener *pl) :
				pl(ASS(pl))
		{}
	} listener;


	
public:
	PacketListener() :
			listener(this)
	{}

	virtual ting::Ref<PacketConnection> CreateConnectionObject() = 0;

	virtual void OnConnected_ts(const ting::Ref<PacketConnection>& c) = 0;

	virtual void OnDisconnected_ts(const ting::Ref<PacketConnection>& c) = 0;

	virtual void OnPacketReceived_ts(const ting::Ref<PacketConnection>& c, ting::Array<ting::u8>& d) = 0;

	virtual void OnPacketSent_ts(const ting::Ref<PacketConnection>& c, unsigned numPacketsInQueue, bool addedToQueue){}
};



class PacketServerThread : public ServerThread{
public:

	PacketServerThread(
			ting::u16 port,
			unsigned maxClientsPerThread,
			PacketListener* listener
		) :
			ServerThread(
					port,
					maxClientsPerThread,
					&(ASS(listener)->listener)
				)
	{}
};



class PacketClientThread : public ClientThread{
public:

	PacketClientThread(
			unsigned maxClientsPerThread,
			PacketListener* listener
		) :
			ClientThread(
					maxClientsPerThread,
					&(ASS(listener)->listener)
				)
	{}
};



}//~namespace
