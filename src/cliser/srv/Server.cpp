// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Server main Thread class


#include <ting/debug.hpp>
#include <ting/Thread.hpp>
#include <ting/Socket.hpp>
#include <ting/utils.hpp>

#include "Server.hpp"

using namespace cliser;


//override
void Server::Run(){
	TRACE(<<"Server::Run(): enter thread"<<std::endl)
	this->threadsKillerThread.Start();
	this->acceptorThread.Start();

	TRACE(<<"Server::Run(): threads started"<<std::endl)

	while(!this->quitFlag){
		this->queue.GetMsg()->Handle();
//		TRACE(<<"Server::Run(): message handled"<<std::endl)
	}

	TRACE(<<"Server::Run(): quiting thread"<<std::endl)
	this->threadsKillerThread.PushQuitMessage();
	this->acceptorThread.PushQuitMessage();

	//kill all client threads
	for(T_ThrIter i = this->clientsThreads.begin(); i != this->clientsThreads.end(); ++i)
		(*i)->PushQuitMessage();
	for(T_ThrIter i = this->clientsThreads.begin(); i != this->clientsThreads.end(); ++i)
		(*i)->Join();

	this->threadsKillerThread.Join();
	this->acceptorThread.Join();

	this->clientsThreads.clear();
}



ConnectionsThread* Server::GetNotFullThread(){
	for(T_ThrIter i = this->clientsThreads.begin(); i != this->clientsThreads.end(); ++i){
		if((*i)->numClients < this->maxClientsPerThread)
			return (*i).operator->();
	}

	this->clientsThreads.push_back(
			ting::Ptr<ConnectionsThread>(
					new ConnectionsThread(this)
				)
		);
	this->clientsThreads.back()->Start();//start new thread
	return this->clientsThreads.back().operator->();
}



void Server::HandleNewConnection(ting::TCPSocket socket){
	//LOG(<<"Server::HandleNewConnection(): enter"<<std::endl)
//	TRACE(<< "Server::HandleNewConnection(): enter" << std::endl)

	ASSERT(socket.IsValid())

	ConnectionsThread* thr;
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

	ting::Ref<Connection> conn = this->CreateClientObject();

	//set client socket
	conn->socket = socket;
	ASSERT(conn->socket.IsValid())

	//set Waitable pointer to connection
	conn->socket.SetUserData(conn.operator->());

	thr->PushMessage(
			ting::Ptr<ting::Message>(
					new ConnectionsThread::AddConnectionMessage(thr, conn)
				)
		);
	++thr->numClients;

//	ASSERT( thr->numClients <= C_TCPClientsHandlerThread::maxClientsPerThread )
}



void Server::ConnectionRemovedMessage::Handle(){
//    TRACE(<<"C_ClientRemovedFromThreadMessage::Handle(): enter"<<std::endl)

	ASSERT(this->cht->numClients > 0)
	--this->cht->numClients;

	if(this->cht->numClients > 0)
		return;

	//if we get here then numClients is 0, remove the thread then:
	//find it in the threads list and push to ThreadKillerThread

	//TODO:store iterator
	for(Server::T_ThrIter i = this->smt->clientsThreads.begin();
			i != this->smt->clientsThreads.end();
			++i
		)
	{
		if((*i) == this->cht){
			(*i)->PushQuitMessage();//initiate exiting process in the thread
			//schedule thread for termination
			this->smt->threadsKillerThread.PushMessage(
					ting::Ptr<ting::Message>(
							new C_KillThreadMessage(
									&this->smt->threadsKillerThread,
									ting::Ptr<ting::MsgThread>(static_cast<ting::MsgThread*>((*i).Extract()))
								)
						)
				);
			//remove thread from threads list
			this->smt->clientsThreads.erase(i);
			break;
		}
	}//~for
}


