// (c) Ivan Gagis
// e-mail: igagis@gmail.com

#pragma once

#include "../ServerThread.hpp"

#include "PacketConnection.hpp"



namespace cliser{



class PacketServerThread : public ServerThread{
public:

	//TODO:
private:
	//override
	bool OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d);
};



}//~namespace
