// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Network Thread class

#include <ting/Thread.hpp>
#include <ting/math.hpp>
#include <ting/Socket.hpp>
#include <ting/utils.hpp>
#include <ting/Ptr.hpp>
#include <ting/Buffer.hpp>

#include "ClientThread.hpp"

using namespace ting;
using namespace cliser;



ClientThread::ClientThread(unsigned maxConnections) :
		ConnectionsThread(maxConnections)
{
	ASSERT(ting::SocketLib::IsCreated())
}



ClientThread::~ClientThread(){
}



void ClientThread::Connect_ts(const ting::IPAddress& ip){
	//send connect request to thread
	this->PushMessage(
			ting::Ptr<ting::Message>(
					new ConnectToServerMessage(
							this,
							ip
						)
				)
		);
}



void ClientThread::HandleConnectRequest(const ting::IPAddress& ip){
	try{
		ting::Ref<Connection> conn = this->CreateConnectionObject();
		conn->socket.Open(ip);

		this->HandleAddConnectionMessage(conn);
	}catch(ting::Socket::Exc &e){
		TRACE(<< "ConnectToServerMessage::Handle(): exception caught, e = " << e.What() << ", sending connect failed reply to main thread" << std::endl)
		this->OnConnectFailure(ClientThread::SOME_ERROR);
		return;
	}
}
