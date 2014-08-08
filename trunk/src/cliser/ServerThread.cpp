/* The MIT License:

Copyright (c) 2009-2013 Ivan Gagis <igagis@gmail.com>

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

//Home page: http://code.google.com/p/cliser/



#include <exception>

#include <ting/debug.hpp>
#include <ting/net/TCPServerSocket.hpp>
#include <ting/net/Lib.hpp>
#include <ting/util.hpp>

#include "ServerThread.hpp"


using namespace cliser;



ServerThread::ServerThread(
		std::uint16_t port,
		unsigned maxClientsPerThread,
		cliser::Listener* listener,
		bool disableNaggle,
		std::uint16_t queueLength
	) :
		port(port),
		maxClientsPerThread(maxClientsPerThread),
		listener(listener),
		disableNaggle(disableNaggle),
		queueLength(queueLength)
{
	ASSERT(ting::net::Lib::IsCreated())

	++this->listener->numTimesAdded;
}



ServerThread::~ServerThread()throw(){
	ASSERT(this->clientsThreads.size() == 0)

	--this->listener->numTimesAdded;
}



//override
void ServerThread::Run(){
//	TRACE(<<"Server::Run(): enter thread"<<std::endl)
	this->threadsKillerThread.Start();
//	TRACE(<<"Server::Run(): threads started"<<std::endl)

	//open listening socket
	ting::net::TCPServerSocket sock;
	sock.Open(this->port, this->disableNaggle, this->queueLength);

	ting::WaitSet waitSet(2);
	waitSet.Add(sock, ting::Waitable::READ);
	waitSet.Add(this->queue, ting::Waitable::READ);

	while(!this->quitFlag){
		waitSet.Wait();

		//TRACE(<<"C_TCPAcceptorThread::Run(): going to get message"<<std::endl)
		if(this->queue.CanRead()){
			if(auto m = this->queue.PeekMsg()){
				m();
			}
		}

		if(sock.CanRead()){
			ting::net::TCPSocket newSock;
			try{
				if(newSock = sock.Accept()){
					this->HandleNewConnection(std::move(newSock));
				}
			}catch(ting::net::Exc& e){
				ASSERT_INFO(false, "sock.Accept() failed")
			}
		}
		//TRACE(<<"C_TCPAcceptorThread::Run(): cycle"<<std::endl)
	}

	waitSet.Remove(this->queue);
	waitSet.Remove(sock);

//	TRACE(<< "ServerThread::" << __func__ << "(): quiting thread" << std::endl)

	this->threadsKillerThread.PushQuitMessage();

	//kill all client threads
	for(auto i = this->clientsThreads.begin(); i != this->clientsThreads.end(); ++i){
		(*i)->PushQuitMessage();
	}

	for(auto i = this->clientsThreads.begin(); i != this->clientsThreads.end(); ++i){
//		TRACE(<< "ServerThread::" << __func__ << "(): joining thread" << std::endl)
		(*i)->Join();
	}
//	TRACE(<< "ServerThread::" << __func__ << "(): connections threads joined" << std::endl)

	this->clientsThreads.clear();

//	TRACE(<< "ServerThread::" << __func__ << "(): waiting for killer thread to finish" << std::endl)

	this->threadsKillerThread.Join();

//	TRACE(<< "ServerThread::" << __func__ << "(): exit" << std::endl)
}



ServerThread::ServerConnectionsThread* ServerThread::GetNotFullThread(){
	//TODO: adjust threads order for faster search
	for(auto i = this->clientsThreads.begin(); i != this->clientsThreads.end(); ++i){
		if((*i)->numConnections < this->maxClientsPerThread){
			return (*i).operator->();
		}
	}

	this->clientsThreads.push_back(ServerConnectionsThread::New(
			this,
			this->MaxClientsPerThread()
		));

//	TRACE(<< "ServerThread::" << __func__ << "(): num threads = " << this->clientsThreads.size() << std::endl)

	this->clientsThreads.back()->Start();//start new thread
	return this->clientsThreads.back().operator->();
}



void ServerThread::HandleNewConnection(ting::net::TCPSocket socket){
//	LOG(<< "ServerThread::" << __func__ << "(): enter" << std::endl)
//	TRACE(<< "ServerThread::" << __func__ << "(): enter" << std::endl)

	ASSERT(socket)

	ServerConnectionsThread* thr;
	try{
		 thr = this->GetNotFullThread();
	}catch(std::exception& e){
//		TRACE_AND_LOG(<< "ServerThread::" << __func__ << "(): GetNotFullThread() failed: " << e.what() << std::endl)
		//failed getting not full thread, possibly maximum threads limit set by system reached
		//ignore connection
		socket.Close();
		return;
	}

	ASSERT(thr)

	std::shared_ptr<Connection> conn = ASS(this->listener)->CreateConnectionObject();

	//set client socket
	conn->socket = std::move(socket);
	ASSERT(conn->socket)	
	
	thr->PushMessage(
			[thr, conn](){
				thr->HandleAddConnectionMessage(conn, true);
			}
		);
	++thr->numConnections;
}



template <typename T> struct CopyablePtr{
	std::unique_ptr<T> p;
	
	CopyablePtr(std::unique_ptr<T> ptr) :
			p(std::move(ptr))
	{}
	
	CopyablePtr(const CopyablePtr& o){
		this->p.reset(const_cast<CopyablePtr&>(o).p.release());
	}
};



void ServerThread::HandleConnectionRemovedMessage(ServerThread::ServerConnectionsThread* cht){
//	TRACE(<< "ServerThread::" << __func__ << "(): enter" << std::endl)

	ASSERT(cht->numConnections > 0)
	--cht->numConnections;

	if(cht->numConnections > 0){
		return;
	}

	//if we get here then numClients is 0, remove the thread then:
	//find it in the threads list and push to ThreadKillerThread

	for(auto i = this->clientsThreads.begin(); i != this->clientsThreads.end(); ++i){
		if((*i).get() == cht){
			(*i)->PushQuitMessage();//post a quit message to the thread before message is sent to threads killer thread
			
			CopyablePtr<decltype(i)::value_type::element_type> ptr(std::move(*i));
			
			//schedule thread for termination
			this->threadsKillerThread.PushMessage(
					[ptr](){
						ptr.p->Join();
					}
				);
			//remove thread from threads list
			this->clientsThreads.erase(i);
			return;
		}//~if
	}//~for
	ASSERT(false)
}




void ServerThread::ThreadsKillerThread::Run(){
	ting::WaitSet ws(1);
	
	ws.Add(this->queue, ting::Waitable::READ);
	
	while(!this->quitFlag){
//		TRACE(<< "ThreadsKillerThread::" << __func__ << "(): going to get message" << std::endl)
		ws.Wait();
		while(auto m = this->queue.PeekMsg()){
			m();
		}
//		TRACE(<< "ThreadsKillerThread::" << __func__ << "(): message handled, qf = " << this->quitFlag << std::endl)
	}
	
	ws.Remove(this->queue);
//	TRACE(<< "ThreadsKillerThread::" << __func__ << "(): exit" << std::endl)
}

