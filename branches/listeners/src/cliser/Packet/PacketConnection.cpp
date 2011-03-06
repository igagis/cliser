// (c) Ivan Gagis
// e-mail: igagis@gmail.com

#include "PacketConnection.hpp"



using namespace cliser;



Packet::Packet(ting::u16 size) :
		array(unsigned(size) + 2)
{
	this->size = size;
	this->buf = this->array.Begin() + 2;

	ting::Serialize16(size, this->array.Begin());//write packet size to the buffer
}



Packet& Packet::operator=(const Packet& packet){
	this->array = packet.array;
	this->size = packet.size;
	this->buf = packet.buf;
	const_cast<Packet&>(packet).size = 0;
	const_cast<Packet&>(packet).buf = 0;
	return *this;
}



void PacketConnection::Send_ts(Packet packet){
	this->Connection::Send_ts(packet.array);
}



void PacketConnection::SendCopy_ts(const ting::Buffer<ting::u8>& packet){
	ASSERT(packet.Size() <= ting::u16(-1))
	Packet p(ting::u16(packet.Size()));
	ASSERT(p.Size() <= packet.Size())
	memcpy(p.Begin(), packet.Begin(), p.Size());
	this->Send_ts(p);
}
