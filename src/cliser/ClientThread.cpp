#include <utki/Buf.hpp>
#include <setka/Lib.hpp>

#include "ClientThread.hpp"


using namespace cliser;



ClientThread::ClientThread(unsigned maxConnections, cliser::Listener* listener) :
		ConnectionsThread(maxConnections, listener)
{
	ASSERT(setka::Lib::isCreated())
}



ClientThread::~ClientThread()noexcept{
}



std::shared_ptr<cliser::Connection> ClientThread::connect_ts(const setka::IPAddress& ip){
//    TRACE(<< "ClientThread::" << __func__ << "(): enter" << std::endl)

	std::shared_ptr<cliser::Connection> conn = ASS(this->listener)->createConnectionObject();
	
	//send connect request to thread
	this->pushMessage(
			[this, conn, ip](){
				this->handleConnectRequest(ip, conn);
			}
		);
	
	return std::move(conn);
}



void ClientThread::handleConnectRequest(
		const setka::IPAddress& ip,
		const std::shared_ptr<cliser::Connection>& conn
	)
{
//    TRACE(<< "ConnectToServerMessage::" << __func__ << "(): enter" << std::endl)
	ASSERT(conn)
	try{
		ASSERT(!conn->socket)
		conn->socket.open(ip);
	}catch(setka::Exc &e){
//		TRACE(<< "ConnectToServerMessage::" << __func__ << "(): exception caught, e = " << e.What() << ", sending connect failed reply to main thread" << std::endl)
		ASS(this->listener)->onDisconnected_ts(conn);
		return;
	}

	this->handleAddConnectionMessage(conn, false);
}
