// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Server main Thread class

#pragma once

#include <list>

#include <ting/debug.hpp>
#include <ting/types.hpp>
#include <ting/Array.hpp>
#include <ting/Thread.hpp>
#include <ting/Socket.hpp>

#include "TCPClientsHandlerThread.hpp"
#include "ThreadsKillerThread.hpp"
#include "TCPAcceptorThread.hpp"
#include "Client.hpp"


namespace cliser{

//forward declarations
class Server;
class NewConnectionAcceptedMessage;
class ClientRemovedFromThreadMessage;

//==============================================================================
//==============================================================================
//==============================================================================
class Server : public ting::Thread{
    friend class ClientRemovedFromThreadMessage;
	friend class NewConnectionAcceptedMessage;

	TCPAcceptorThread acceptorThread;
    ThreadsKillerThread threadsKillerThread;

	typedef std::list<ting::Ptr<TCPClientsHandlerThread> > T_ThrList;
	typedef T_ThrList::iterator T_ThrIter;
    T_ThrList clientsThreads;

	unsigned maxClientsPerThread;
	
public:
	inline unsigned MaxClientsPerThread()const{
		return this->maxClientsPerThread;
	}

    Server(ting::u16 listeningPort, unsigned maxClientPerOneThread) :
			acceptorThread(this, listeningPort),
			maxClientsPerThread(maxClientPerOneThread)
	{}

	~Server(){
		ASSERT(this->clientsThreads.size() == 0)
	}

	inline void MainLoop(){
		this->Run();
	}

	virtual ting::Ref<cliser::Client> CreateClientObject(){
		return ting::Ref<cliser::Client>(new cliser::Client());
	}

	virtual void OnClientConnected(ting::Ref<Client>& c) = 0;

	virtual void OnClientDisconnected(ting::Ref<Client>& c) = 0;

	virtual void OnDataReceivedFromClient(ting::Ref<Client>& c, ting::Array<ting::u8> d) = 0;

private:
	//override
    void Run();
	
    TCPClientsHandlerThread* GetNotFullThread();

	void HandleNewConnection(ting::TCPSocket socket);

	void DisconnectClient(ting::Ref<Client>& c);
};

//==============================================================================
//==============================================================================
//==============================================================================



class NewConnectionAcceptedMessage : public ting::Message{
    Server *smt;//this mesage should hold reference to the thread this message is sent to

    ting::TCPSocket socket;

public:
    NewConnectionAcceptedMessage(Server* serverMainThread, ting::TCPSocket sock) :
            smt(serverMainThread),
            socket(sock)
    {
        ASSERT(this->smt)
        ASSERT(this->socket.IsValid())
    }

    //override
    void Handle(){
//		TRACE(<<"C_NewConnectionAcceptedMessage::Handle(): invoked"<<std::endl)
		this->smt->HandleNewConnection(this->socket);
	}
};



//This message is sent to server main thread when the client has been disconnected,
//and the connection was closed. The player was removed from its handler thread.
class ClientRemovedFromThreadMessage : public ting::Message{
    Server *smt;//this mesage should hold reference to the thread this message is sent to
	TCPClientsHandlerThread* cht;
  public:
    ClientRemovedFromThreadMessage(Server* serverMainThread, TCPClientsHandlerThread* clientsHandlerThread) :
            smt(serverMainThread),
            cht(clientsHandlerThread)
    {
        ASSERT(this->smt)
        ASSERT(this->cht)
    }
    
    //override
    void Handle();
};



}//~namespace
