// (c) Ivan Gagis
// e-mail: igagis@gmail.com

#pragma once

#include <list>

#include <ting/debug.hpp>
#include <ting/types.hpp>
#include <ting/Array.hpp>
#include <ting/Thread.hpp>
#include <ting/Socket.hpp>

#include "ConnectionsThread.hpp"
#include "Connection.hpp"



namespace cliser{



class ServerThread : public ting::MsgThread{
	
	class ThreadsKillerThread : public ting::MsgThread{
	public:
		ThreadsKillerThread(){};

		//override
		void Run();

		class KillThreadMessage : public ting::Message{
			ThreadsKillerThread *thread;//to whom this message will be sent
			ting::Ptr<ting::MsgThread> thr;//thread to kill
		  public:
			KillThreadMessage(ThreadsKillerThread *threadKillerThread, ting::Ptr<ting::MsgThread> thread) :
					thread(threadKillerThread),
					thr(thread)
			{
				ASSERT(this->thread)
				ASSERT(this->thr.IsValid())
				this->thr->PushQuitMessage();//post a quit message to the thread before message is sent to threads kiler thread
			};

			//override
			void Handle();
		};
	} threadsKillerThread;

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

	ServerThread(ting::u16 port, unsigned maxClientsPerThread);
	
	virtual ~ServerThread();

	//override
	void Run();

private:
	virtual ting::Ref<cliser::Connection> CreateConnectionObject() = 0;

	virtual void OnConnected_ts(const ting::Ref<Connection>& c) = 0;

	virtual void OnDisconnected_ts(const ting::Ref<Connection>& c) = 0;

	virtual bool OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d) = 0;

	virtual void OnDataSent_ts(const ting::Ref<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){}

private:
	ServerConnectionsThread* GetNotFullThread();



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

	private:
		//override
		virtual void OnConnected_ts(const ting::Ref<Connection>& c){
			ASS(this->smt)->OnConnected_ts(c);
		}

		//override
		virtual void OnDisconnected_ts(const ting::Ref<Connection>& c){
			ASSERT(this->smt)

			//Disconnection may be because thread is exiting because server main thread
			//is also exiting, in that case no need tonotify server main thread.
			//Send notification message to server main thread only if thread is not exiting yet.
			if(!this->quitFlag){
				this->smt->PushMessage(
						ting::Ptr<ting::Message>(new ServerThread::ConnectionRemovedMessage(this->smt, this))
					);
			}

			this->smt->OnDisconnected_ts(c);
		}

		//override
		virtual bool OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d){
			return ASS(this->smt)->OnDataReceived_ts(c, d);
		}

		//override
		virtual void OnDataSent_ts(const ting::Ref<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){
			ASS(this->smt)->OnDataSent_ts(c, numPacketsInQueue, addedToQueue);
		}
	};
};



}//~namespace
