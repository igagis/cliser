// (c) Ivan Gagis
// e-mail: igagis@gmail.com

#include <ting/Thread.hpp>
#include <ting/math.hpp>
#include <ting/Socket.hpp>
#include <ting/utils.hpp>
#include <ting/Ptr.hpp>
#include <ting/Buffer.hpp>

#include "ClientThread.hpp"


using namespace cliser;



ClientThread::ClientThread(unsigned maxConnections, cliser::Listener* listener) :
		ConnectionsThread(maxConnections, listener)
{
	ASSERT(ting::SocketLib::IsCreated())
}



ClientThread::~ClientThread(){
}



ting::Ref<cliser::Connection> ClientThread::Connect_ts(const ting::IPAddress& ip){
//    TRACE(<< "ClientThread::" << __func__ << "(): enter" << std::endl)

	ting::Ref<cliser::Connection> conn = ASS(this->listener)->CreateConnectionObject();
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
//    TRACE(<< "ConnectToServerMessage::" << __func__ << "(): enter" << std::endl)
	ASSERT(conn)
	try{
		ASSERT(conn->socket.IsNotValid())
		conn->socket.Open(ip);
	}catch(ting::Socket::Exc &e){
//		TRACE(<< "ConnectToServerMessage::" << __func__ << "(): exception caught, e = " << e.What() << ", sending connect failed reply to main thread" << std::endl)
		ASS(this->listener)->OnDisconnected_ts(conn);
		return;
	}

	this->HandleAddConnectionMessage(conn, false);
}
