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

#include "../srv/ConnectionsThread.hpp"

//forward declarations
//...

namespace cliser{

class ClientThread : public cliser::ConnectionsThread{
    friend class ConnectToServerMessage;
	
public:
    ClientThread(unsigned maxConnections);
    
    virtual ~ClientThread();

	//send connection request message to the thread
	void Connect_ts(const ting::IPAddress& ip);

	enum EConnectFailureReason{
		SOME_ERROR
	};

	virtual void OnConnectFailure(EConnectFailureReason failReason) = 0;

	virtual ting::Ref<cliser::Connection> CreateConnectionObject() = 0;

private:
	class ConnectToServerMessage : public ting::Message{
		ClientThread* ct;
		ting::IPAddress ip;
	public:
		ConnectToServerMessage(ClientThread* ct, const ting::IPAddress& ip) :
				ct(ASS(ct)),
				ip(ip)
		{}

		//override
		void Handle(){
		//	TRACE(<<"ConnectToServerMessage::Handle(): host=" << reinterpret_cast<void*>(ip.host) << " port=" << (ip.port) << std::endl)
			this->ct->HandleConnectRequest(this->ip);
		}
	};

	void HandleConnectRequest(const ting::IPAddress& ip);
};



}//~namespace
