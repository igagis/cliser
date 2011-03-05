// (c) Ivan Gagis
// e-mail: igagis@gmail.com

#pragma once

#include "../Connection.hpp"


namespace cliser{


//TODO: make inheritance private?
class PacketConnection : public Connection{
public:


	//TODO: hide functions from Connection

	static inline ting::Ref<cliser::PacketConnection> New(){
		return ting::Ref<cliser::PacketConnection>(new cliser::PacketConnection());
	}
};

}//~namespace
