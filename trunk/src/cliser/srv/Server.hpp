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

#include "ConnectionsThread.hpp"
#include "ThreadsKillerThread.hpp"
#include "Connection.hpp"



namespace cliser{

//forward declarations



//==============================================================================
//==============================================================================
//==============================================================================
class Server : public ting::MsgThread{
	friend class ConnectionsThread;

    ThreadsKillerThread threadsKillerThread;

	typedef std::list<ting::Ptr<ConnectionsThread> > T_ThrList;
	typedef T_ThrList::iterator T_ThrIter;
    T_ThrList clientsThreads;

	ting::u16 port;

	unsigned maxClientsPerThread;
	
public:
	inline unsigned MaxClientsPerThread()const{
		return this->maxClientsPerThread;
	}

    Server(ting::u16 port, unsigned maxClientsPerThread) :
			port(port),
			maxClientsPerThread(maxClientsPerThread)
	{}

	~Server(){
		ASSERT(this->clientsThreads.size() == 0)
	}

	//override
    void Run();

	virtual ting::Ref<cliser::Connection> CreateClientObject(){
		return cliser::Connection::New();
	}

	virtual void OnClientConnected_ts(ting::Ref<Connection>& c) = 0;

	virtual void OnClientDisconnected_ts(ting::Ref<Connection>& c) = 0;

	virtual void OnDataReceived_ts(ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d) = 0;

	virtual void OnDataSent_ts(ting::Ref<Connection>& c){}

private:	
    ConnectionsThread* GetNotFullThread();

	void DisconnectClient(ting::Ref<Connection>& c);



private:
	void HandleNewConnection(ting::TCPSocket socket);



	//This message is sent to server main thread when the client has been disconnected,
	//and the connection was closed. The player was removed from its handler thread.
	class ConnectionRemovedMessage : public ting::Message{
		Server *thread;//this mesage should hold reference to the thread this message is sent to
		ConnectionsThread* cht;
	  public:
		ConnectionRemovedMessage(Server* serverMainThread, ConnectionsThread* clientsHandlerThread) :
				thread(serverMainThread),
				cht(clientsHandlerThread)
		{
			ASSERT(this->thread)
			ASSERT(this->cht)
		}

		//override
		void Handle(){
			ASS(this->thread)->HandleConnectionRemovedMessage(this->cht);
		}
	};

	void HandleConnectionRemovedMessage(ConnectionsThread* cht);
};



}//~namespace
