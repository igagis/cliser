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



ConnectionsThread::ConnectionsThread(unsigned maxConnections, cliser::Listener* listener) :
		waitSet(maxConnections + 1), //+1 for messages queue
		listener(ASS(listener))
{
	M_SRV_CLIENTS_HANDLER_TRACE(<< "TCPClientsHandlerThread::" << __func__ << "(): invoked" << std::endl)
	DEBUG_CODE(++this->listener->numTimesAdded;)
}



ConnectionsThread::~ConnectionsThread(){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): invoked" << std::endl)
	ASSERT(this->connections.size() == 0)
	ASSERT(this->listener)
	DEBUG_CODE(--this->listener->numTimesAdded;)
}



void ConnectionsThread::Run(){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): new thread started" << std::endl)

	this->waitSet.Add(&this->queue, ting::Waitable::READ);

	ting::Array<ting::Waitable*> triggered(this->waitSet.Size());

	while(!this->quitFlag){
//		TRACE(<< "ConnectionsThread::" << __func__ << "(): waiting..." << std::endl)
		unsigned numTriggered = this->waitSet.Wait(&triggered);
//		TRACE(<< "ConnectionsThread::" << __func__ << "(): triggered" << std::endl)
		ASSERT(numTriggered > 0)

		ting::Waitable** p = triggered.Begin();
		for(unsigned i = 0; i != numTriggered; ++i, ++p){
			ASSERT(p != triggered.End())
			ASSERT(*p)

			if(*p == &this->queue){
				//queue
				ASSERT(this->queue.CanRead())
				while(ting::Ptr<ting::Message> m = this->queue.PeekMsg()){
					M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): message got" << std::endl)
					m->Handle();
				}
			}else{
				//socket
				ASSERT(*p != &this->queue)

				ting::Ref<cliser::Connection> conn(
						reinterpret_cast<cliser::Connection*>(ASS((*p)->GetUserData()))
					);
				ASSERT(conn)

				this->HandleSocketActivity(conn);
			}
		}//~for()

//		M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::Run(): cycle"<<std::endl)
	}//~while(): main loop of the thread

//	TRACE(<< "ConnectionsThread::" << __func__ << "(): disconnect all clients" << std::endl)

	//disconnect all clients, removing sockets from wait set
	for(T_ConnectionsIter i = this->connections.begin(); i != this->connections.end(); ++i){
		ting::Ref<Connection> c(ASS(*i));

		this->RemoveSocketFromSocketSet(&c->socket);
		c->socket.Close();
		c->ClearHandlingThread();

		ASS(this->listener)->OnDisconnected_ts(c);
	}
	this->connections.clear();//clear clients list

	this->waitSet.Remove(&this->queue);

	ASSERT(this->connections.size() == 0)
//	TRACE(<< "ConnectionsThread::" << __func__ << "(): exiting" << std::endl)
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): exiting" << std::endl)
}



void ConnectionsThread::HandleSocketActivity(ting::Ref<Connection>& conn){
	if(conn->socket.CanWrite()){
//		TRACE(<< "ConnectionsThread::HandleSocketActivity(): CanWrite" << std::endl)
		if(conn->packetQueue.size() == 0){
			//was waiting for connect

			//clear WRITE waiting flag and set READ waiting flag
			this->waitSet.Change(&conn->socket, ting::Waitable::READ);

			//try writing 0 bytes to clear the write flag and to check if connection was successful
			try{
				ting::StaticBuffer<ting::u8, 0> buf;
				conn->socket.Send(buf);

				//under win32 the CanRead() assertion fails sometimes... //TODO: why?
//				ASSERT(!conn->socket.CanRead())
//				ASSERT(!conn->socket.CanWrite())

				conn->SetHandlingThread(this);
				ASS(this->listener)->OnConnected_ts(conn);
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): connection was successful" << std::endl)
			}catch(ting::Socket::Exc& e){
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): connection was unsuccessful: " << e.What() << std::endl)
				this->HandleRemoveConnectionMessage(conn);
				return;
			}

		}else{
			ASSERT(conn->dataSent < conn->packetQueue.front().Size())

			try{
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): Packet data left = " << (conn->packetQueue.front().Size() - conn->dataSent) << std::endl)

				conn->dataSent += conn->socket.Send(conn->packetQueue.front(), conn->dataSent);
				ASSERT(conn->dataSent <= conn->packetQueue.front().Size())

//				TRACE(<< "ConnectionsThread::" << __func__ << "(): Packet data left = " << (conn->packetQueue.front().Size() - conn->dataSent) << std::endl)

				if(conn->dataSent == conn->packetQueue.front().Size()){
					conn->packetQueue.pop_front();
					conn->dataSent = 0;

//					TRACE(<< "ConnectionsThread::" << __func__ << "(): Packet sent!!!!!!!!!!!!!!!!!!!!!" << std::endl)

					ASS(this->listener)->OnDataSent_ts(conn, conn->packetQueue.size(), false);

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
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): exception caught while sending: " << e.What() << std::endl)
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
				ting::Mutex::Guard mutexGuard(conn->receivedDataMutex);
				if(!ASS(this->listener)->OnDataReceived_ts(conn, b)){
//					TRACE(<< "ConnectionsThread::HandleSocketActivity(): received data not handled!!!!!!!!!!!" << std::endl)
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
//			TRACE(<< "ConnectionsThread::" << __func__ << "(): exception caught while reading: " << e.What() << std::endl)
			this->HandleRemoveConnectionMessage(conn);
			return;
		}
	}
}



void ConnectionsThread::HandleAddConnectionMessage(const ting::Ref<Connection>& conn, bool isConnected){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): enter" << std::endl)
//    TRACE (<< "ConnectionsThread::HandleAddConnectionMessage(): enter" << std::endl)

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
//		TRACE(<< "ConnectionsThread::" << __func__ << "(): adding socket to waitset failed: " << e.What() << std::endl)
		ASS(this->listener)->OnDisconnected_ts(conn);
		return;
	}

	ASSERT(conn->packetQueue.size() == 0)

	this->connections.push_back(conn);

	//notify new client connection
	if(isConnected){
		//set client's handling thread
		conn->SetHandlingThread(this);

		ASS(this->listener)->OnConnected_ts(conn);
	}else{
		ASSERT(!conn->parentThread)
	}

	ASSERT(!conn->socket.CanRead())
	ASSERT(!conn->socket.CanWrite())

	//ASSERT(this->thread->numPlayers <= this->thread->players.Size())
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): exit" << std::endl)
}



//removing client means disconnect as well
void ConnectionsThread::HandleRemoveConnectionMessage(ting::Ref<Connection>& conn){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): enter" << std::endl)

	ASSERT(conn.IsValid())

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
			ASS(this->listener)->OnDisconnected_ts(conn);

			return;
		}
	}

	//NOTE: it is possible that disconnection request message is posted to the threads message queue
	//      and before it is handled the connection is disconnected by peer. Thus, we will not find the
	//      connection in the list of connections here, so, do not ASSERT(false).
	//ASSERT(false)
}



void ConnectionsThread::HandleSendDataMessage(ting::Ref<Connection>& conn, ting::Array<ting::u8> data){
//	TRACE(<< "ConnectionsThread::" << __func__ << "(): enter" << std::endl)
	
	//It is possible that data is send to a connection which is not yet estabilished.
	//This may happen when on client side afterrequesting the Connect_ts()
	//client tries to send the data immediately. But since connect is an
	//asynchronous operation socket may be invalid before connect operation is completed.
	if(!conn->socket.IsValid()){
		//put data to queue and notify
		conn->packetQueue.push_back(data);
		ASS(this->listener)->OnDataSent_ts(conn, conn->packetQueue.size(), true);
//		TRACE(<< "ConnectionsThread::" << __func__ << "(): socket is disconnected, ignoring message" << std::endl)
		return;
	}

	if(conn->packetQueue.size() != 0){
//		TRACE(<< "ConnectionsThread::" << __func__ << "(): adding data to send queue right away" << std::endl)
		conn->packetQueue.push_back(data);
		ASS(this->listener)->OnDataSent_ts(conn, conn->packetQueue.size(), true);
		return;
	}else{
		try{
			unsigned numBytesSent = conn->socket.Send(data);
			ASSERT(numBytesSent <= data.Size())

			if(numBytesSent != data.Size()){
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): adding data to send queue" << std::endl)
				conn->dataSent = numBytesSent;
				conn->packetQueue.push_back(data);

				//Set WRITE wait flag
				this->waitSet.Change(&conn->socket, ting::Waitable::READ_AND_WRITE);

				ASSERT_INFO(conn->packetQueue.size() == 1, conn->packetQueue.size())
				ASS(this->listener)->OnDataSent_ts(conn, 1, true);
			}else{
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): NOT adding data to send queue" << std::endl)
				ASSERT_INFO(conn->packetQueue.size() == 0, conn->packetQueue.size())
				ASS(this->listener)->OnDataSent_ts(conn, 0, false);
			}
		}catch(ting::Socket::Exc& e){
//			TRACE(<< "ConnectionsThread::" << __func__ << "(): exception caught" << e.What() << std::endl)
			conn->Disconnect_ts();
		}
	}
//	TRACE(<< "ConnectionsThread::" << __func__ << "(): exit" << std::endl)
}



void ConnectionsThread::HandleResumeListeningForReadMessage(ting::Ref<Connection>& conn){
	if(conn->socket.IsNotValid())//if connection is closed
		return;

//	TRACE(<< "ConnectionsThread::" << __func__ << "(): resuming data receiving!!!!!!!!!!" << std::endl)

	ASSERT(!conn->receivedData)

	ting::Waitable::EReadinessFlags flags = ting::Waitable::READ;
	if(conn->packetQueue.size() > 0){
		flags = ting::Waitable::EReadinessFlags(flags | ting::Waitable::WRITE);
	}

	this->waitSet.Change(&conn->socket, flags);
}


