// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//

#include <list>

#include <ting/Socket.hpp>
#include <ting/debug.hpp>
#include <ting/math.hpp>
#include <ting/utils.hpp>

#include "Server.hpp"
#include "Client.hpp"
#include "TCPClientsHandlerThread.hpp"
#include "../util/util.hpp"


using namespace cliser;



TCPClientsHandlerThread::TCPClientsHandlerThread(Server *serverMainThread) :
		smt(serverMainThread),
		waitSet(serverMainThread->MaxClientsPerThread() + 1), //+1 for messages queue
		numClients(0)
{
	M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::TCPClientsHandlerThread(): invoked"<<std::endl)
	ASSERT(this->smt)

	this->waitSet.Add(&this->queue, ting::Waitable::READ);
}



void TCPClientsHandlerThread::Run(){
	M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::Run(): new thread started"<<std::endl)

	while(!this->quitFlag){
		//NOTE: quque is added to wait set, so, wait infinitely
		if(this->waitSet.Wait() > 0){
			if(!this->queue.CanRead()){
				M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::Run(): active sockets found"<<std::endl)
				this->HandleSocketActivities();
			}
		}

		while(ting::Ptr<ting::Message> m = this->queue.PeekMsg()){
			M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::Run(): message got"<<std::endl)
			m->Handle();
		}
		
//        M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::Run(): cycle"<<std::endl)
	}//~while(): main loop of the thread

	//disconnect all clients
	for(T_ClientsIter i = this->clients.begin(); i != this->clients.end(); ++i){
		ting::Ref<Client> c(*i);
		
		this->RemoveSocketFromSocketSet(&c->socket);
		c->socket.Close();
		c->ClearClientHandlerThread();

		//NOTE: Do not send notifications to server main thread because this thread is
		//exiting, therefore it is not an active thread which can be used for adding
		//new clients further.

		//notify client disconnection
		this->smt->OnClientDisconnected(c);
	}
	this->clients.clear();//clear clients list

	ASSERT(this->clients.size() == 0)
	M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::Run(): exiting"<<std::endl)
}



bool TCPClientsHandlerThread::HandleClientSocketActivity(ting::Ref<Client>& c){
	class ResListener : public cliser::NetworkReceiverState::PacketListener{
		Server* smt;
		ting::Ref<Client> c;

		//override
		void OnNewDataPacketReceived(ting::Array<ting::byte> d){
			this->smt->OnDataReceivedFromClient(this->c, d);
		}

	public:
		ResListener(Server* p, ting::Ref<Client> client) :
				smt(p),
				c(client)
		{}
		virtual ~ResListener(){}
	} rl(this->smt, c);

	return c->netReceiverState.ReadSocket(&c->socket, &rl);
}



void TCPClientsHandlerThread::HandleSocketActivities(){
	M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::HandleSocketActivities(): enter, size = "<< this->clients.size()<<std::endl)

	for(T_ClientsIter i = this->clients.begin(); i != this->clients.end();){
		M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::HandleSocketActivities(): cycle"<<std::endl)
		ASSERT((*i)->socket.IsValid())
		if((*i)->socket.CanRead()){//if socket has activity
			if(this->HandleClientSocketActivity(*i)){//if client disconnected, remove it from list
//				M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::HandleSocketActivities(): removing client"<<std::endl)
				ting::Ref<Client> c(*i);
				
				//remove client from list
				i = this->clients.erase(i);

				//handle client disconnection
				this->RemoveSocketFromSocketSet(&c->socket);
				c->socket.Close();
				c->ClearClientHandlerThread();

				//send notification to server main thread
				this->smt->PushMessage(
						ting::Ptr<ting::Message>(new ClientRemovedFromThreadMessage(this->smt, this))
					);

				//notify client disconnection
				this->smt->OnClientDisconnected(c);

//				M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::HandleSocketActivities(): client removed, size = "<<this->clients.size()<<" isEnd = "<<(i == this->clients.end()) << " isBegin = "<<(i == this->clients.begin())<<std::endl)
				continue;//because iterator is already pointing to next client, no need for ++i
			}//~if disconnected
		}//~if(CanRead)
		++i;
	}//~for(client)
};







//
// M       M  EEEEEE   SSSS    SSSS    AAAA    GGGG   EEEEEE   SSSS
// MM     MM  E       S       S       A    A  G    G  E       S
// M M   M M  E       S       S       A    A  G       E       S
// M  M M  M  EEEEEE   SSSS    SSSS   A    A  G  GG   EEEEEE   SSSS
// M   M   M  E            S       S  AAAAAA  G    G  E            S
// M       M  E            S       S  A    A  G    G  E            S
// M       M  EEEEEE   SSSS    SSSS   A    A   GGGG   EEEEEE   SSSS
//
void AddClientToThreadMessage::Handle(){
	M_SRV_CLIENTS_HANDLER_TRACE(<<"C_AddClientToThreadMessage::Handle(): enter"<<std::endl)

//    ASSERT(!this->thread->IsFull())

	//set client socket
	this->client->socket = this->socket;
	ASSERT(this->client->socket.IsValid())

	//set client's handler thread
	this->client->SetClientHandlerThread(this->thread);

	//add socket to waitset
	this->thread->AddSocketToSocketSet(&this->client->socket);

	this->thread->clients.push_back(this->client);

	//notify new client connection
	this->thread->smt->OnClientConnected(this->client);

	//ASSERT(this->thread->numPlayers <= this->thread->players.Size())
	M_SRV_CLIENTS_HANDLER_TRACE(<<"C_AddClientToThreadMessage::Handle(): exit"<<std::endl)
}



//removing client means disconnect as well
void RemoveClientFromThreadMessage::Handle(){
	M_SRV_CLIENTS_HANDLER_TRACE(<<"C_RemoveClientFromThreadMessage::Handle(): enter"<<std::endl)

	ASSERT(this->client.IsValid())

	if(!this->client->clientThread){
		//client already removed, probably removal request was posted twice.
		//This is normal situation.
		M_SRV_CLIENTS_HANDLER_TRACE(
				<< "C_RemoveClientFromThreadMessage::Handle(): already removed, do nothing"
				<< std::endl
			)
		return;
	}

	ASSERT(this->client->clientThread == this->thread)//make sure we are removing client from the right thread

	try{
		for(TCPClientsHandlerThread::T_ClientsIter i = this->thread->clients.begin();
				i != this->thread->clients.end();
				++i
			)
		{
			if((*i) == this->client){
				this->thread->clients.erase(i);//remove client from list

				this->thread->RemoveSocketFromSocketSet(&(*i)->socket);
				this->client->socket.Close();//close connection if it is opened
				this->client->ClearClientHandlerThread();

				//send notification message to server main thread
				this->thread->smt->PushMessage(
						ting::Ptr<ting::Message>(new ClientRemovedFromThreadMessage(this->thread->smt, this->thread))
					);

				//notify client disconnection
				this->thread->smt->OnClientDisconnected(this->client);

				break;
			}
		}

		ASSERT(this->thread->clients.size() <= this->thread->smt->MaxClientsPerThread())
	}catch(...){
		ASSERT_INFO(false, "C_RemoveClientFromThreadMessage::Handle(): removing client failed, should send RemoveClientFailed message to main server thread, it is unimplemented yet")
	}
}



//override
void SendNetworkDataToClientMessage::Handle(){
//	TRACE(<<"SendNetworkDataToClientMessage::Handle(): enter"<<std::endl)
	if(!this->client->socket.IsValid()){
		TRACE(<< "SendNetworkDataToClientMessage::Handle(): socket is disconnected, ignoring message" << std::endl)
		return;
	}

	ASSERT(this->data.SizeInBytes() != 0 && this->data.SizeInBytes() <= (0xffff) )

	//send packet size
	ting::byte packetSize[2];
	ting::ToNetworkFormat16(this->data.SizeInBytes(), packetSize);

//	TRACE(<< "SendNetworkDataToClientMessage::Handle(): sending " << this->data.SizeInBytes() << " bytes" << std::endl)

	try{
		//send packet size
		this->client->socket.SendAll(packetSize, sizeof(packetSize));

		//send data
		this->client->socket.SendAll(this->data.Buf(), this->data.SizeInBytes());
	}catch(ting::Socket::Exc& e){
		this->client->Disconnect();
	}
}


