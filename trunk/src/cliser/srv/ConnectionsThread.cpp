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
#include <ting/Buffer.hpp>

#include "Server.hpp"
#include "Connection.hpp"
#include "ConnectionsThread.hpp"
#include "../util/util.hpp"



using namespace cliser;



ConnectionsThread::ConnectionsThread(unsigned maxConnections) :
		waitSet(maxConnections + 1) //+1 for messages queue
{
	M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::TCPClientsHandlerThread(): invoked"<<std::endl)

	this->waitSet.Add(&this->queue, ting::Waitable::READ);
}



void ConnectionsThread::Run(){
	M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::Run(): new thread started"<<std::endl)

	ting::Array<ting::Waitable*> triggered(this->waitSet.Size());

	while(!this->quitFlag){
		//NOTE: queue is added to wait set, so, wait infinitely
		unsigned numTriggered = this->waitSet.Wait(&triggered);
		ASSERT(numTriggered > 0)

		if(this->queue.CanRead()){
			while(ting::Ptr<ting::Message> m = this->queue.PeekMsg()){
				M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::Run(): message got"<<std::endl)
				m->Handle();
			}
			continue;
		}else{
			//Handle socket activities

			ting::Waitable** p = triggered.Begin();
			for(unsigned i = 0; i != numTriggered; ++i, ++p){
				ASSERT(p != triggered.End())
				ASSERT(*p)

				ting::Ref<cliser::Connection> conn(
						reinterpret_cast<cliser::Connection*>((*p)->GetUserData())
					);
				ASSERT(conn)

				this->HandleSocketActivity(conn);
			}//~for()

			M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::Run(): active sockets found"<<std::endl)
			//this->HandleSocketActivities();
		}
//        M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::Run(): cycle"<<std::endl)
	}//~while(): main loop of the thread

	//disconnect all clients, removing sockets from wait set
	for(T_ConnectionsIter i = this->connections.begin(); i != this->connections.end(); ++i){
		ting::Ref<Connection> c(*i);
		
		this->RemoveSocketFromSocketSet(&c->socket);
		c->socket.Close();
		c->ClearClientHandlerThread();

		//NOTE: Do not send notifications to server main thread because this thread is
		//exiting, therefore it is not an active thread which can be used for adding
		//new clients further.
	}
	this->connections.clear();//clear clients list

	ASSERT(this->connections.size() == 0)
	M_SRV_CLIENTS_HANDLER_TRACE(<< "TCPClientsHandlerThread::Run(): exiting" << std::endl)
}



void ConnectionsThread::HandleSocketActivity(const ting::Ref<Connection>& conn){
	if(conn->socket.CanRead()){
		ting::StaticBuffer<ting::u8, 0xfff> buffer;//4kb

		unsigned bytesReceived = conn->socket.Recv(buffer);
		if(bytesReceived != 0){
			ting::Buffer<ting::u8> b(buffer.Begin(), bytesReceived);
			this->OnDataReceived_ts(conn, b);
		}else{
			//connection closed
			conn->Disconnect_ts();
			return;
		}
	}

	if(conn->socket.CanWrite()){
		ASSERT(conn->packetQueue.size() != 0)

		ASSERT(conn->dataSent < conn->packetQueue.front().Size())

		try{
			conn->dataSent += conn->socket.Send(conn->packetQueue.front(), conn->dataSent);
			ASSERT(conn->dataSent <= conn->packetQueue.front().Size())

			if(conn->dataSent == conn->packetQueue.front().Size()){
				conn->packetQueue.pop_front();
				conn->dataSent = 0;

				this->OnDataSent_ts(conn);

				if(conn->packetQueue.size() == 0){
					//clear WRITE waiting flag, only READ wait flag remains.
					this->waitSet.Change(&conn->socket, ting::Waitable::READ);
				}
			}
		}catch(ting::Socket::Exc &e){
			conn->Disconnect_ts();
		}
	}
}



//
// M       M  EEEEEE   SSSS    SSSS    AAAA    GGGG   EEEEEE   SSSS
// MM     MM  E       S       S       A    A  G    G  E       S
// M M   M M  E       S       S       A    A  G       E       S
// M  M M  M  EEEEEE   SSSS    SSSS   A    A  G  GG   EEEEEE   SSSS
// M   M   M  E            S       S  AAAAAA  G    G  E            S
// M       M  E            S       S  A    A  G    G  E            S
// M       M  EEEEEE   SSSS    SSSS   A    A   GGGG   EEEEEE   SSSS
//
void ConnectionsThread::HandleAddConnectionMessage(ting::Ref<Connection>& conn){
	M_SRV_CLIENTS_HANDLER_TRACE(<<"C_AddClientToThreadMessage::Handle(): enter"<<std::endl)

//    ASSERT(!this->thread->IsFull())
	
	//set client's handler thread
	conn->SetClientHandlerThread(this);

	//add socket to waitset
	this->AddSocketToSocketSet(&conn->socket);

	this->connections.push_back(conn);

	//notify new client connection
	this->OnClientConnected_ts(conn);

	//ASSERT(this->thread->numPlayers <= this->thread->players.Size())
	M_SRV_CLIENTS_HANDLER_TRACE(<< "C_AddClientToThreadMessage::Handle(): exit" << std::endl)
}



//removing client means disconnect as well
void ConnectionsThread::HandleRemoveConnectionMessage(ting::Ref<Connection>& conn){
	M_SRV_CLIENTS_HANDLER_TRACE(<<"C_RemoveClientFromThreadMessage::Handle(): enter"<<std::endl)

	ASSERT(conn.IsValid())

	try{
		for(ConnectionsThread::T_ConnectionsIter i = this->connections.begin();
				i != this->connections.end();
				++i
			)
		{
			if((*i) == conn){
				this->connections.erase(i);//remove client from list

				this->RemoveSocketFromSocketSet(&(*i)->socket);
				
				conn->socket.Close();//close connection if it is opened

				ASSERT(conn->clientThread == 0)

				//clear packetQueue
				conn->packetQueue.clear();

				//notify client disconnection
				this->OnClientDisconnected_ts(conn);

				break;
			}
		}
	}catch(...){
		ASSERT_INFO(false, "C_RemoveClientFromThreadMessage::Handle(): removing client failed, should send RemoveClientFailed message to main server thread, it is unimplemented yet")
	}
}



void ConnectionsThread::HandleSendDataMessage(ting::Ref<Connection>& conn, ting::Array<ting::u8> data){
//	TRACE(<<"SendNetworkDataToClientMessage::Handle(): enter"<<std::endl)
	if(!conn->socket.IsValid()){
		TRACE(<< "SendNetworkDataToClientMessage::Handle(): socket is disconnected, ignoring message" << std::endl)
		return;
	}

	if(conn->packetQueue.size() != 0){
		conn->packetQueue.push_back(data);
		return;
	}else{
		try{
			unsigned numBytesSent = conn->socket.Send(data);
			ASSERT(numBytesSent <= data.Size())

			if(numBytesSent != data.Size()){
				conn->dataSent = numBytesSent;
				conn->packetQueue.push_back(data);

				//Set WRITE wait flag
				this->waitSet.Change(&conn->socket, ting::Waitable::READ_AND_WRITE);
			}
		}catch(ting::Socket::Exc& e){
			conn->Disconnect_ts();
		}
	}
}


