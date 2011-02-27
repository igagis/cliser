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



ting::Ref<cliser::Connection> ClientThread::Connect_ts(const ting::IPAddress& ip){
	ting::Ref<cliser::Connection> conn = this->CreateConnectionObject();
	//send connect request to thread
	this->PushMessage(
			ting::Ptr<ting::Message>(
					new ConnectToServerMessage(
							this,
							ip,
							conn
						)
				)
		);
	return conn;
}



void ClientThread::HandleConnectRequest(
		const ting::IPAddress& ip,
		const ting::Ref<cliser::Connection>& conn
	)
{
	ASSERT(conn)
	try{
		ASSERT(conn->socket.IsNotValid())
		conn->socket.Open(ip);
	}catch(ting::Socket::Exc &e){
		TRACE(<< "ConnectToServerMessage::Handle(): exception caught, e = " << e.What() << ", sending connect failed reply to main thread" << std::endl)
		this->OnDisconnected_ts(conn);
		return;
	}

	this->HandleAddConnectionMessage(conn, false);
}
