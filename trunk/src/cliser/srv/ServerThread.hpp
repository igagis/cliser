// (c) Ivan Gagis
// e-mail: igagis@gmail.com

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
class ServerThread : public ting::MsgThread{
//	friend class ServerConnectionsThread;

	ThreadsKillerThread threadsKillerThread;

	//forward declaration
	class ServerConnectionsThread;

	typedef std::list<ting::Ptr<ServerConnectionsThread> > T_ThrList;
	typedef T_ThrList::iterator T_ThrIter;
	T_ThrList clientsThreads;

	ting::u16 port;

	unsigned maxClientsPerThread;

public:
	inline unsigned MaxClientsPerThread()const{
		return this->maxClientsPerThread;
	}

	ServerThread(ting::u16 port, unsigned maxClientsPerThread) :
			port(port),
			maxClientsPerThread(maxClientsPerThread)
	{
		ASSERT(ting::SocketLib::IsCreated())
	}

	~ServerThread(){
		ASSERT(this->clientsThreads.size() == 0)
	}

	//override
	void Run();

	virtual ting::Ref<cliser::Connection> CreateClientObject() = 0;

	virtual void OnClientConnected_ts(const ting::Ref<Connection>& c) = 0;

	virtual void OnClientDisconnected_ts(const ting::Ref<Connection>& c) = 0;

	virtual void OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d) = 0;

	virtual void OnDataSent_ts(const ting::Ref<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){}

private:
	ServerConnectionsThread* GetNotFullThread();

	void DisconnectClient(const ting::Ref<Connection>& c);



private:
	void HandleNewConnection(ting::TCPSocket socket);



	//This message is sent to server main thread when the client has been disconnected,
	//and the connection was closed. The player was removed from its handler thread.
	class ConnectionRemovedMessage : public ting::Message{
		ServerThread *thread;//this mesage should hold reference to the thread this message is sent to
		ServerThread::ServerConnectionsThread* cht;
	  public:
		ConnectionRemovedMessage(ServerThread* serverMainThread, ServerThread::ServerConnectionsThread* clientsHandlerThread) :
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

	void HandleConnectionRemovedMessage(ServerThread::ServerConnectionsThread* cht);




private:
	class ServerConnectionsThread : public ConnectionsThread{
		ServerThread* const smt;
		
	public:
		//This data is controlled by Server Main Thread
		unsigned numConnections;
		//~

		ServerConnectionsThread(ServerThread *serverThread, unsigned maxConnections) :
				ConnectionsThread(maxConnections),
				smt(ASS(serverThread)),
				numConnections(0)
		{}

		inline static ting::Ptr<ServerConnectionsThread> New(ServerThread *serverThread, unsigned maxConnections){
			return ting::Ptr<ServerConnectionsThread>(
					new ServerConnectionsThread(serverThread,maxConnections)
				);
		}

		//override
		virtual void OnClientConnected_ts(const ting::Ref<Connection>& c){
			ASS(this->smt)->OnClientConnected_ts(c);
		}

		//override
		virtual void OnClientDisconnected_ts(const ting::Ref<Connection>& c){
			ASSERT(this->smt)
			//send notification message to server main thread
			this->smt->PushMessage(
					ting::Ptr<ting::Message>(new ServerThread::ConnectionRemovedMessage(this->smt, this))
				);

			this->smt->OnClientDisconnected_ts(c);
		}

		//override
		virtual void OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d){
			ASS(this->smt)->OnDataReceived_ts(c, d);
		}

		//override
		virtual void OnDataSent_ts(const ting::Ref<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){
			ASS(this->smt)->OnDataSent_ts(c, numPacketsInQueue, addedToQueue);
		}
	};
};



}//~namespace
