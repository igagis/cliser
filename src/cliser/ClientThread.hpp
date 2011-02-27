// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Network Thread class

#pragma once

#include <ting/Array.hpp>
#include <ting/Buffer.hpp>
#include <ting/types.hpp>
#include <ting/debug.hpp>
#include <ting/Thread.hpp>
#include <ting/Socket.hpp>
#include <ting/WaitSet.hpp>

#include "ConnectionsThread.hpp"



//forward declarations
//...



namespace cliser{

class ClientThread : public cliser::ConnectionsThread{
    friend class ConnectToServerMessage;

	
public:
    ClientThread(unsigned maxConnections);
    
    virtual ~ClientThread();

	//send connection request message to the thread
	ting::Ref<cliser::Connection> Connect_ts(const ting::IPAddress& ip);

	virtual ting::Ref<cliser::Connection> CreateConnectionObject() = 0;

private:
	class ConnectToServerMessage : public ting::Message{
		ClientThread* ct;
		ting::IPAddress ip;
		const ting::Ref<cliser::Connection> conn;
	public:
		ConnectToServerMessage(
				ClientThread* ct,
				const ting::IPAddress& ip,
				const ting::Ref<cliser::Connection>& conn
			) :
				ct(ASS(ct)),
				ip(ip),
				conn(conn)
		{}

		//override
		void Handle(){
		//	TRACE(<<"ConnectToServerMessage::Handle(): host=" << reinterpret_cast<void*>(ip.host) << " port=" << (ip.port) << std::endl)
			this->ct->HandleConnectRequest(this->ip, this->conn);
		}
	};

	void HandleConnectRequest(const ting::IPAddress& ip, const ting::Ref<cliser::Connection>& conn);
};



}//~namespace
