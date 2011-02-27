// (c) Ivan Gagis
// e-mail: igagis@gmail.com

// Description:
//

#include <list>

#include <ting/Socket.hpp>
#include <ting/debug.hpp>
#include <ting/math.hpp>
#include <ting/utils.hpp>
#include <ting/Buffer.hpp>

#include "ServerThread.hpp"
#include "Connection.hpp"
#include "ConnectionsThread.hpp"



using namespace cliser;



ConnectionsThread::ConnectionsThread(unsigned maxConnections) :
		waitSet(maxConnections + 1) //+1 for messages queue
{
	M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::TCPClientsHandlerThread(): invoked"<<std::endl)
}



void ConnectionsThread::Run(){
	M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::Run(): new thread started"<<std::endl)

	this->waitSet.Add(&this->queue, ting::Waitable::READ);
	
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
				ASSERT(*p != &this->queue)

				ting::Ref<cliser::Connection> conn(
						reinterpret_cast<cliser::Connection*>(ASS((*p)->GetUserData()))
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
		ting::Ref<Connection> c(ASS(*i));
		
		this->RemoveSocketFromSocketSet(&c->socket);
		c->socket.Close();
		c->ClearHandlingThread();

		//NOTE: Do not send notifications to server main thread because this thread is
		//exiting, therefore it is not an active thread which can be used for adding
		//new clients further.
	}
	this->connections.clear();//clear clients list

	this->waitSet.Remove(&this->queue);

	ASSERT(this->connections.size() == 0)
	M_SRV_CLIENTS_HANDLER_TRACE(<< "TCPClientsHandlerThread::Run(): exiting" << std::endl)
}



void ConnectionsThread::HandleSocketActivity(ting::Ref<Connection>& conn){
	if(conn->socket.CanWrite()){
		TRACE(<< "ConnectionsThread::HandleSocketActivity(): CanWrite" << std::endl)
		if(conn->packetQueue.size() == 0){
			//was waiting for connect

			//clear WRITE waiting flag and set READ waiting flag
			this->waitSet.Change(&conn->socket, ting::Waitable::READ);

			//try writing 0 bytes to clear the write flag and to check if connection was successful
			try{
				ting::StaticBuffer<ting::u8, 0> buf;
				conn->socket.Send(buf);

				ASSERT(!conn->socket.CanRead())
				ASSERT(!conn->socket.CanWrite())

				conn->SetHandlingThread(this);
				this->OnConnected_ts(conn);
				TRACE(<< "ConnectionsThread::HandleSocketActivity(): connection was successful" << std::endl)
			}catch(ting::Socket::Exc& e){
				TRACE(<< "ConnectionsThread::HandleSocketActivity(): connection was unsuccessful: " << e.What() << std::endl)
				this->HandleRemoveConnectionMessage(conn);
				return;
			}

		}else{
			ASSERT(conn->dataSent < conn->packetQueue.front().Size())

			try{
				TRACE(<< "ConnectionsThread::HandleSocketActivity(): Packet data left = " << (conn->packetQueue.front().Size() - conn->dataSent) << std::endl)

				conn->dataSent += conn->socket.Send(conn->packetQueue.front(), conn->dataSent);
				ASSERT(conn->dataSent <= conn->packetQueue.front().Size())

				TRACE(<< "ConnectionsThread::HandleSocketActivity(): Packet data left = " << (conn->packetQueue.front().Size() - conn->dataSent) << std::endl)

				if(conn->dataSent == conn->packetQueue.front().Size()){
					conn->packetQueue.pop_front();
					conn->dataSent = 0;

					TRACE(<< "ConnectionsThread::HandleSocketActivity(): Packet sent!!!!!!!!!!!!!!!!!!!!!" << std::endl)

					this->OnDataSent_ts(conn, conn->packetQueue.size(), false);

					if(conn->packetQueue.size() == 0){
						//clear WRITE waiting flag.

						ting::Waitable::EReadinessFlags flags = ting::Waitable::NOT_READY;
						if(!conn->receivedData){
							flags = ting::Waitable::EReadinessFlags(flags | ting::Waitable::READ);
						}
						this->waitSet.Change(&conn->socket, flags);
					}
				}
			}catch(ting::Socket::Exc &e){
				TRACE(<< "ConnectionsThread::HandleSocketActivity(): exception caught while sending: " << e.What() << std::endl)
				this->HandleRemoveConnectionMessage(conn);
				return;
			}
		}
	}

	if(conn->socket.CanRead()){
//		TRACE(<< "ConnectionsThread::HandleSocketActivity(): CanRead()" << std::endl)

		ting::StaticBuffer<ting::u8, 0x1000> buffer;//8kb

		try{
			unsigned bytesReceived = conn->socket.Recv(buffer);
			if(bytesReceived != 0){
				ting::Buffer<ting::u8> b(buffer.Begin(), bytesReceived);
				if(!this->OnDataReceived_ts(conn, b)){
					TRACE(<< "ConnectionsThread::HandleSocketActivity(): received data not handled!!!!!!!!!!!" << std::endl)
					ting::Mutex::Guard mutexGuard(conn->mutex);
					ASSERT(!conn->receivedData)

					conn->receivedData.Init(b);

					//stop waiting for READ condition
					ting::Waitable::EReadinessFlags flags = ting::Waitable::NOT_READY;
					if(conn->packetQueue.size() > 0){
						flags = ting::Waitable::EReadinessFlags(flags | ting::Waitable::WRITE);
					}
					this->waitSet.Change(&conn->socket, flags);
				}
			}else{
				//connection closed
				this->HandleRemoveConnectionMessage(conn);
				return;
			}
		}catch(ting::Socket::Exc &e){
			TRACE(<< "ConnectionsThread::HandleSocketActivity(): exception caught while reading: " << e.What() << std::endl)
			this->HandleRemoveConnectionMessage(conn);
			return;
		}
	}
}



void ConnectionsThread::HandleAddConnectionMessage(const ting::Ref<Connection>& conn, bool isConnected){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::HandleAddConnectionMessage(): enter" << std::endl)

//    ASSERT(!this->thread->IsFull())

	//set Waitable pointer to connection
	conn->socket.SetUserData(conn.operator->());

	//add socket to waitset
	try{
		this->AddSocketToSocketSet(
				&conn->socket,
				//if not connected, will be waiting for WRITE, because WRITE indicates that connect request has finished.
				isConnected ? ting::Waitable::READ : ting::Waitable::WRITE
			);
	}catch(ting::Exc& e){
		TRACE(<< "ConnectionsThread::HandleAddConnectionMessage(): adding socket to waitset failed: " << e.What() << std::endl)
		this->OnDisconnected_ts(conn);
		return;
	}

	ASSERT(conn->packetQueue.size() == 0)

	this->connections.push_back(conn);

	//notify new client connection
	if(isConnected){
		//set client's handling thread
		conn->SetHandlingThread(this);

		this->OnConnected_ts(conn);
	}else{
		ASSERT(!conn->parentThread)
	}

	ASSERT(!conn->socket.CanRead())
	ASSERT(!conn->socket.CanWrite())

	//ASSERT(this->thread->numPlayers <= this->thread->players.Size())
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::HandleAddConnectionMessage(): exit" << std::endl)
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

				conn->ClearHandlingThread();

				//clear packetQueue
				conn->packetQueue.clear();

				//notify client disconnection
				this->OnDisconnected_ts(conn);

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
		//put data to queue and notify
		conn->packetQueue.push_back(data);
		this->OnDataSent_ts(conn, conn->packetQueue.size(), true);
//		TRACE(<< "SendNetworkDataToClientMessage::Handle(): socket is disconnected, ignoring message" << std::endl)
		return;
	}

	if(conn->packetQueue.size() != 0){
//		TRACE(<< "ConnectionsThread::HandleSendDataMessage(): adding data to send queue" << std::endl)
		conn->packetQueue.push_back(data);
		this->OnDataSent_ts(conn, conn->packetQueue.size(), true);
		return;
	}else{
		try{
			unsigned numBytesSent = conn->socket.Send(data);
			ASSERT(numBytesSent <= data.Size())

			if(numBytesSent != data.Size()){
//				TRACE(<< "ConnectionsThread::HandleSendDataMessage(): adding data to send queue" << std::endl)
				conn->dataSent = numBytesSent;
				conn->packetQueue.push_back(data);

				//Set WRITE wait flag
				this->waitSet.Change(&conn->socket, ting::Waitable::READ_AND_WRITE);

				ASSERT_INFO(conn->packetQueue.size() == 1, conn->packetQueue.size())
				this->OnDataSent_ts(conn, 1, true);
			}else{
//				TRACE(<< "ConnectionsThread::HandleSendDataMessage(): NOT adding data to send queue" << std::endl)
				ASSERT_INFO(conn->packetQueue.size() == 0, conn->packetQueue.size())
				this->OnDataSent_ts(conn, 0, false);
			}
		}catch(ting::Socket::Exc& e){
			conn->Disconnect_ts();
		}
	}
}



void ConnectionsThread::HandleResumeListeningForReadMessage(ting::Ref<Connection>& conn){
	if(conn->socket.IsNotValid())//if connection is closed
		return;

	TRACE(<< "ConnectionsThread::HandleResumeListeningForReadMessage(): resuming data receiving!!!!!!!!!!" << std::endl)

	ASSERT(!conn->receivedData)

	ting::Waitable::EReadinessFlags flags = ting::Waitable::READ;
	if(conn->packetQueue.size() > 0){
		flags = ting::Waitable::EReadinessFlags(flags | ting::Waitable::WRITE);
	}

	this->waitSet.Change(&conn->socket, flags);
}


