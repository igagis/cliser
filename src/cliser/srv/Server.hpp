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
#include "TCPAcceptorThread.hpp"
#include "Connection.hpp"



namespace cliser{

//forward declarations



//==============================================================================
//==============================================================================
//==============================================================================
class Server : public ting::MsgThread{
	friend class ConnectionsThread;

	TCPAcceptorThread acceptorThread;//TODO: listen in server thread, no need for separate acceptor thread
    ThreadsKillerThread threadsKillerThread;

	typedef std::list<ting::Ptr<ConnectionsThread> > T_ThrList;
	typedef T_ThrList::iterator T_ThrIter;
    T_ThrList clientsThreads;

	unsigned maxClientsPerThread;
	
public:
	inline unsigned MaxClientsPerThread()const{
		return this->maxClientsPerThread;
	}

    Server(ting::u16 listeningPort, unsigned maxClientsPerThread) :
			acceptorThread(this, listeningPort),
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

	void HandleNewConnection(ting::TCPSocket socket);

	void DisconnectClient(ting::Ref<Connection>& c);



private:
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
	class ConnectionRemovedMessage : public ting::Message{
		Server *smt;//this mesage should hold reference to the thread this message is sent to
		ConnectionsThread* cht;
	  public:
		ConnectionRemovedMessage(Server* serverMainThread, ConnectionsThread* clientsHandlerThread) :
				smt(serverMainThread),
				cht(clientsHandlerThread)
		{
			ASSERT(this->smt)
			ASSERT(this->cht)
		}

		//override
		void Handle();
	};
};



}//~namespace
