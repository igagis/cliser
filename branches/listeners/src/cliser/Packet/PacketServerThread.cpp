// (c) Ivan Gagis
// e-mail: igagis@gmail.com

#include "PacketServerThread.hpp"



using namespace cliser;



//override
bool PacketServerThread::OnDataReceived_ts(const ting::Ref<Connection>& conn, const ting::Buffer<ting::u8>& d){
	ting::Ref<PacketConnection> c(
				static_cast<PacketConnection*>(conn.operator->())
			);
	
	//TODO:

	return true;
}
