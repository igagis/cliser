// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Server main Thread class

#include <exception>

#include <ting/debug.hpp>
#include <ting/Thread.hpp>
#include <ting/Socket.hpp>
#include <ting/utils.hpp>

#include "ServerThread.hpp"


using namespace cliser;



//override
void ServerThread::Run(){
//	TRACE(<<"Server::Run(): enter thread"<<std::endl)
	this->threadsKillerThread.Start();
//	TRACE(<<"Server::Run(): threads started"<<std::endl)

	ting::TCPServerSocket sock(this->port);//create and open listening socket

	ting::WaitSet waitSet(2);
	waitSet.Add(&sock, ting::Waitable::READ);
	waitSet.Add(&this->queue, ting::Waitable::READ);

	while(!this->quitFlag){
		waitSet.Wait();

		//TRACE(<<"C_TCPAcceptorThread::Run(): going to get message"<<std::endl)
		if(this->queue.CanRead()){
			while(ting::Ptr<ting::Message> m = this->queue.PeekMsg()){
				m->Handle();
			}
		}

		if(sock.CanRead()){
			ting::TCPSocket newSock;
			try{
				while((newSock = sock.Accept()).IsValid()){
					this->HandleNewConnection(newSock);
				}
			}catch(ting::Socket::Exc& e){
				ASSERT_INFO(false, "sock.Accept() failed")
			}
		}
		//TRACE(<<"C_TCPAcceptorThread::Run(): cycle"<<std::endl)
	}

	waitSet.Remove(&this->queue);
	waitSet.Remove(&sock);

	TRACE(<<"Server::Run(): quiting thread"<<std::endl)
	this->threadsKillerThread.PushQuitMessage();

	//kill all client threads
	for(T_ThrIter i = this->clientsThreads.begin(); i != this->clientsThreads.end(); ++i)
		(*i)->PushQuitMessage();
	for(T_ThrIter i = this->clientsThreads.begin(); i != this->clientsThreads.end(); ++i)
		(*i)->Join();

	this->threadsKillerThread.Join();

	this->clientsThreads.clear();
}



ServerThread::ServerConnectionsThread* ServerThread::GetNotFullThread(){
	//TODO: adjust threads order for faster search
	for(T_ThrIter i = this->clientsThreads.begin(); i != this->clientsThreads.end(); ++i){
		if((*i)->numConnections < this->maxClientsPerThread)
			return (*i).operator->();
	}

	this->clientsThreads.push_back(ServerConnectionsThread::New(
			this,
			this->MaxClientsPerThread())
		);
	this->clientsThreads.back()->Start();//start new thread
	return this->clientsThreads.back().operator->();
}



void ServerThread::HandleNewConnection(ting::TCPSocket socket){
	//LOG(<<"Server::HandleNewConnection(): enter"<<std::endl)
//	TRACE(<< "Server::HandleNewConnection(): enter" << std::endl)

	ASSERT(socket.IsValid())

	ServerConnectionsThread* thr;
	try{
		 thr = this->GetNotFullThread();
	}catch(std::exception& e){
		TRACE_AND_LOG(<< "Server::HandleNewConnection(): GetNotFullThread() failed: " << e.what() << std::endl)
		//failed getting not full thread, possibly maximum threads limit set by system reached
		//ignore connection
		socket.Close();
		return;
	}

	ASSERT(thr)

	ting::Ref<Connection> conn = this->CreateConnectionObject();

	//set client socket
	conn->socket = socket;
	ASSERT(conn->socket.IsValid())

	thr->PushMessage(
			ting::Ptr<ting::Message>(
					new ConnectionsThread::AddConnectionMessage(thr, conn)
				)
		);
	++thr->numConnections;

//	ASSERT( thr->numClients <= C_TCPClientsHandlerThread::maxClientsPerThread )
}



void ServerThread::HandleConnectionRemovedMessage(ServerThread::ServerConnectionsThread* cht){
//    TRACE(<<"C_ClientRemovedFromThreadMessage::Handle(): enter"<<std::endl)

	ASSERT(cht->numConnections > 0)
	--cht->numConnections;

	if(cht->numConnections > 0)
		return;

	//if we get here then numClients is 0, remove the thread then:
	//find it in the threads list and push to ThreadKillerThread

	//TODO:store iterator
	for(ServerThread::T_ThrIter i = this->clientsThreads.begin();
			i != this->clientsThreads.end();
			++i
		)
	{
		if((*i) == cht){
			(*i)->PushQuitMessage();//initiate exiting process in the thread
			//schedule thread for termination
			this->threadsKillerThread.PushMessage(
					ting::Ptr<ting::Message>(
							new ThreadsKillerThread::KillThreadMessage(
									&this->threadsKillerThread,
									ting::Ptr<ting::MsgThread>(static_cast<ting::MsgThread*>((*i).Extract()))
								)
						)
				);
			//remove thread from threads list
			this->clientsThreads.erase(i);
			break;
		}//~if
	}//~for
}


