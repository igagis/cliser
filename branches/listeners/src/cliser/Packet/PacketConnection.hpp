// (c) Ivan Gagis
// e-mail: igagis@gmail.com

#pragma once

#include "../Connection.hpp"


namespace cliser{

class Packet;



//TODO: make inheritance private?
class PacketConnection : public Connection{
public:

	void Send_ts(Packet packet);
	void SendCopy_ts(const ting::Buffer<ting::u8>& packet);

	//TODO: hide functions from Connection

	static inline ting::Ref<cliser::PacketConnection> New(){
		return ting::Ref<cliser::PacketConnection>(new cliser::PacketConnection());
	}
};



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



}//~namespace
