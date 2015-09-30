#include <exception>

#include <utki/debug.hpp>
#include <setka/TCPServerSocket.hpp>
#include <setka/Lib.hpp>

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
	ASSERT(setka::Lib::isCreated())

	++this->listener->numTimesAdded;
}



ServerThread::~ServerThread()throw(){
	ASSERT(this->clientsThreads.size() == 0)

	--this->listener->numTimesAdded;
}



void ServerThread::run(){
//	TRACE(<<"Server::Run(): enter thread"<<std::endl)
	this->threadsKillerThread.start();
//	TRACE(<<"Server::Run(): threads started"<<std::endl)

	//open listening socket
	setka::TCPServerSocket sock;
	sock.open(this->port, this->disableNaggle, this->queueLength);

	pogodi::WaitSet waitSet(2);
	waitSet.add(sock, pogodi::Waitable::READ);
	waitSet.add(this->queue, pogodi::Waitable::READ);

	while(!this->quitFlag){
		waitSet.wait();

		//TRACE(<<"C_TCPAcceptorThread::Run(): going to get message"<<std::endl)
		if(this->queue.canRead()){
			if(auto m = this->queue.peekMsg()){
				m();
			}
		}

		if(sock.canRead()){
			setka::TCPSocket newSock;
			try{
				if((newSock = sock.accept())){
					this->HandleNewConnection(std::move(newSock));
				}
			}catch(setka::Exc& e){
				ASSERT_INFO(false, "sock.Accept() failed")
			}
		}
		//TRACE(<<"C_TCPAcceptorThread::Run(): cycle"<<std::endl)
	}

	waitSet.remove(this->queue);
	waitSet.remove(sock);

//	TRACE(<< "ServerThread::" << __func__ << "(): quiting thread" << std::endl)

	this->threadsKillerThread.pushQuitMessage();

	//kill all client threads
	for(auto i = this->clientsThreads.begin(); i != this->clientsThreads.end(); ++i){
		(*i)->pushQuitMessage();
	}

	for(auto i = this->clientsThreads.begin(); i != this->clientsThreads.end(); ++i){
//		TRACE(<< "ServerThread::" << __func__ << "(): joining thread" << std::endl)
		(*i)->join();
	}
//	TRACE(<< "ServerThread::" << __func__ << "(): connections threads joined" << std::endl)

	this->clientsThreads.clear();

//	TRACE(<< "ServerThread::" << __func__ << "(): waiting for killer thread to finish" << std::endl)

	this->threadsKillerThread.join();

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

	this->clientsThreads.back()->start();//start new thread
	return this->clientsThreads.back().operator->();
}



void ServerThread::HandleNewConnection(setka::TCPSocket socket){
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
		socket.close();
		return;
	}

	ASSERT(thr)

	std::shared_ptr<Connection> conn = ASS(this->listener)->CreateConnectionObject();

	//set client socket
	conn->socket = std::move(socket);
	ASSERT(conn->socket)	
	
	thr->pushMessage(
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
			(*i)->pushQuitMessage();//post a quit message to the thread before message is sent to threads killer thread
			
			CopyablePtr<decltype(i)::value_type::element_type> ptr(std::move(*i));
			
			//schedule thread for termination
			this->threadsKillerThread.pushMessage(
					[ptr](){
						ptr.p->join();
					}
				);
			//remove thread from threads list
			this->clientsThreads.erase(i);
			return;
		}//~if
	}//~for
	ASSERT(false)
}




void ServerThread::ThreadsKillerThread::run(){
	pogodi::WaitSet ws(1);
	
	ws.add(this->queue, pogodi::Waitable::READ);
	
	while(!this->quitFlag){
//		TRACE(<< "ThreadsKillerThread::" << __func__ << "(): going to get message" << std::endl)
		ws.wait();
		while(auto m = this->queue.peekMsg()){
			m();
		}
//		TRACE(<< "ThreadsKillerThread::" << __func__ << "(): message handled, qf = " << this->quitFlag << std::endl)
	}
	
	ws.remove(this->queue);
//	TRACE(<< "ThreadsKillerThread::" << __func__ << "(): exit" << std::endl)
}

