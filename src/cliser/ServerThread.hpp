/* The MIT License:

Copyright (c) 2009-2011 Ivan Gagis <igagis@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE. */

//Homepage: http://code.google.com/p/cliser/

/**
 * @file ServerThread.hpp
 * @author Ivan Gagis <igagis@gmail.com>
 * @brief Server thread which manages network connections on server side.
 * TODO: write long description
 */
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

		/// @cond
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
				this->thr->PushQuitMessage();//post a quit message to the thread before message is sent to threads killer thread
			}

			//override
			void Handle();
		};
		/// @endcond
	} threadsKillerThread;

	//forward declaration
	class ServerConnectionsThread;

	typedef std::list<ting::Ptr<ServerConnectionsThread> > T_ThrList;
	typedef T_ThrList::iterator T_ThrIter;
	T_ThrList clientsThreads;

	ting::u16 port;

	unsigned maxClientsPerThread;

	cliser::Listener* const listener;

	bool disableNaggle;
	
	ting::u16 queueLength;
	
public:
	inline unsigned MaxClientsPerThread()const{
		return this->maxClientsPerThread;
	}

	ServerThread(
			ting::u16 port,
			unsigned maxClientsPerThread,
			cliser::Listener* listener,
			bool disableNaggle = false,
			ting::u16 acceptQueueLength = 50
		);
	
	virtual ~ServerThread();

	//override
	void Run();

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
	class ServerConnectionsThread : private cliser::Listener, public ConnectionsThread{
		ServerThread* const serverThread;
	public:
		//This data is controlled by ServerThread
		ting::Inited<unsigned, 0> numConnections;
		//~

		ServerConnectionsThread(
				ServerThread* serverThread,
				unsigned maxConnections
			) :
				cliser::Listener(),
				ConnectionsThread(maxConnections, this),
				serverThread(ASS(serverThread))
		{}

		//override
		virtual ting::Ref<cliser::Connection> CreateConnectionObject(){
			//this function will not be ever called.
			ASSERT(false);
			return ting::Ref<cliser::Connection>();
		}

		//override
		void OnConnected_ts(const ting::Ref<Connection>& c){
			ASS(this->serverThread)->listener->OnConnected_ts(c);
		}

		//override
		void OnDisconnected_ts(const ting::Ref<Connection>& c){
			//Disconnection may be because thread is exiting because server main thread
			//is also exiting, in that case no need to notify server main thread.
			//Send notification message to server main thread only if thread is not exiting yet.
			if(!this->quitFlag){
				this->serverThread->PushMessage(
						ting::Ptr<ting::Message>(
								new ServerThread::ConnectionRemovedMessage(this->serverThread, this)
							)
					);
			}

			ASS(this->serverThread)->listener->OnDisconnected_ts(c);
		}

		//override
		bool OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d){
			return ASS(this->serverThread)->listener->OnDataReceived_ts(c, d);
		}

		//override
		void OnDataSent_ts(const ting::Ref<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){
			ASS(this->serverThread)->listener->OnDataSent_ts(c, numPacketsInQueue, addedToQueue);
		}

		inline static ting::Ptr<ServerConnectionsThread> New(
				ServerThread* serverThread,
				unsigned maxConnections
			)
		{
			return ting::Ptr<ServerConnectionsThread>(
					new ServerConnectionsThread(
							serverThread,
							maxConnections
						)
				);
		}
	};
};



}//~namespace
