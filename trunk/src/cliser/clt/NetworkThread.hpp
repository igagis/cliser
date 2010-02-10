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

#include "../util/util.hpp"

//forward declarations
//...

namespace cliser{

class NetworkThread : public ting::MsgThread{
    friend class ConnectToServerMessage;
	friend class DisconnectFromServerMessage;
    friend class SendNetworkDataToServerMessage;
    
    ting::TCPSocket socket;
    ting::WaitSet waitSet;
    
    cliser::NetworkReceiverState netReceiverState;
	
public:
    NetworkThread() :
            waitSet(2) //1 for socket and 1 for queue
    {
//        TRACE(<<"C_NetworkThread::C_NetworkThread(): enter"<<std::endl)
		this->waitSet.Add(&this->queue, ting::Waitable::READ);
    }
    
    ~NetworkThread(){
//        TRACE(<<"C_NetworkThread::~C_NetworkThread(): invoked"<<std::endl)
		this->waitSet.Remove(&this->queue);
    }

	//send connection request message to the thread
	void Connect_ts(const std::string& host, ting::u16 port);

	//send disconnection request message to the thread
	void Disconnect_ts();

	virtual void OnDataReceived(ting::Array<ting::u8> data) = 0;

	enum EConnectFailReason{
		SUCCESS,
		ALREADY_CONNECTED,
		COULD_NOT_RESOLVE_HOST_IP,
		SOME_ERROR
	};
	
	virtual void OnConnect(EConnectFailReason failReason) = 0;

	virtual void OnDisconnect() = 0;

	void SendData_ts(ting::Array<ting::u8> data);

private:
	//override
    void Run();

    void HandleSocketActivity();

	void HandleDisconnection();
};



class ConnectToServerMessage : public ting::Message{
    NetworkThread* nt;
	std::string host;
	ting::u16 port;
  public:
    ConnectToServerMessage(
			NetworkThread* netwrokThread,
			const std::string& theHost,
			ting::u16 thePort
		) :
            nt(netwrokThread),
			host(theHost),
			port(thePort)
    {
        ASSERT(this->nt)
    }

    //override
    void Handle();
};



class DisconnectFromServerMessage : public ting::Message{
    NetworkThread* nt;

  public:
    DisconnectFromServerMessage(NetworkThread* netwrokThread) :
            nt(netwrokThread)
    {
        ASSERT(this->nt)
    }

    //override
    void Handle();
};



class SendNetworkDataToServerMessage : public ting::Message{
    NetworkThread* nt;
    ting::Array<ting::u8> data;
  public:
    SendNetworkDataToServerMessage(NetworkThread* netwrokThread, ting::Array<ting::u8> &dataToSend) :
            nt(netwrokThread),
            data(dataToSend)
    {
        ASSERT(this->nt)
    }

    //override
    void Handle();
};

}//~namespace
