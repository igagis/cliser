#include <list>
#include <cstring>

#include <utki/debug.hpp>
#include <utki/Buf.hpp>

#include "ServerThread.hpp"
#include "Connection.hpp"

#include "ConnectionsThread.hpp"



using namespace cliser;



ConnectionsThread::ConnectionsThread(unsigned maxConnections, cliser::Listener* listener) :
		waitSet(maxConnections + 1), //+1 for messages queue
		listener(ASS(listener))
{
	M_SRV_CLIENTS_HANDLER_TRACE(<< "TCPClientsHandlerThread::" << __func__ << "(): invoked" << std::endl)
	++this->listener->numTimesAdded;
}



ConnectionsThread::~ConnectionsThread()throw(){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): invoked" << std::endl)
	ASSERT(this->connections.size() == 0)
	ASSERT(this->listener)
	--this->listener->numTimesAdded;
}



void ConnectionsThread::run(){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): new thread started" << std::endl)

	this->waitSet.add(this->queue, pogodi::Waitable::READ);

	std::vector<pogodi::Waitable*> triggered(this->waitSet.size());

	while(!this->quitFlag){
//		TRACE(<< "ConnectionsThread::" << __func__ << "(): waiting..." << std::endl)
		unsigned numTriggered = this->waitSet.wait(utki::wrapBuf(triggered));
//		TRACE(<< "ConnectionsThread::" << __func__ << "(): triggered" << std::endl)
//		ASSERT(numTriggered > 0)

		auto p = triggered.begin();
		for(unsigned i = 0; i != numTriggered; ++i, ++p){
			ASSERT(p != triggered.end())
			ASSERT(*p)

			if(*p == &this->queue){
				//Do not handle messages here, first handle all activities from sockets.
				//Check if there are messages and handle them only after all sockets are
				//handled. This is because connection can be removed as a result of handling some message while
				//pointer to the Waitable will still be in the buffer of triggered waitables, thus
				//this pointer to Waitable will be invalid.
			}else{
				//socket
				ASSERT(*p != &this->queue)

				ASSERT((*p)->getUserData())

				auto connection = reinterpret_cast<cliser::Connection*>((*p)->getUserData());
				
				std::shared_ptr<cliser::Connection> conn = connection->sharedFromThis(connection);
				ASSERT(conn)

				this->HandleSocketActivity(conn);
			}
		}//~for()

		//At this point we have handled all the socket activities and now we can proceed with
		//handling messages if there are any.
		if(this->queue.canRead()){
			//NOTE: here we will handle only limited number of messages from queue.
			//      This is because as a result of
			//      handling some message it is possible that a new message will be posted
			//      to the queue, which, in turn, when handled posts another one and so on,
			//      thus possibly causing a kind of a deadlock. Hence, handle some
			//      predefined number of messages. I think good guess is 1 message per connection
			//      plus one (in order to handle at least one if there are no connections).
			//      Number of connections may change during messages handling, thus, save
			//      the number in local variable.
			unsigned numMsgsToHandle = this->connections.size() + 1;

			//Paranoic check: if we have so many connections that its number is a
			//maximum what unsigned in can hold, then adding 1 will turn it to zero.
			//Do a paranoic check to handle that case.
			if(numMsgsToHandle == 0){
				numMsgsToHandle = 1;
			}

			for(unsigned i = 0; i < numMsgsToHandle; ++i){
				if(auto m = this->queue.peekMsg()){
					M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): message got" << std::endl)
					m();
				}else{
					break;
				}
			}
		}

//		M_SRV_CLIENTS_HANDLER_TRACE(<<"TCPClientsHandlerThread::Run(): cycle"<<std::endl)
	}//~while(): main loop of the thread

//	TRACE(<< "ConnectionsThread::" << __func__ << "(): disconnect all clients" << std::endl)

	//disconnect all clients, removing sockets from wait set
	for(T_ConnectionsIter i = this->connections.begin(); i != this->connections.end(); ++i){
		std::shared_ptr<Connection> c(ASS(*i));

		this->RemoveSocketFromSocketSet(c->socket);
		c->socket.close();
		c->ClearHandlingThread();

		ASS(this->listener)->OnDisconnected_ts(c);
	}
	this->connections.clear();//clear clients list

	this->waitSet.remove(this->queue);

	ASSERT(this->connections.size() == 0)
//	TRACE(<< "ConnectionsThread::" << __func__ << "(): exiting" << std::endl)
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): exiting" << std::endl)
}



void ConnectionsThread::HandleSocketActivity(std::shared_ptr<Connection>& conn){
	if(conn->socket.canWrite()){
//		TRACE(<< "ConnectionsThread::HandleSocketActivity(): CanWrite" << std::endl)
		if(conn->packetQueue.size() == 0){
			//was waiting for connect

			//clear WRITE waiting flag and set READ waiting flag
			conn->currentFlags = pogodi::Waitable::READ;
			this->waitSet.change(conn->socket, conn->currentFlags);

			//try writing 0 bytes to clear the write flag and to check if connection was successful
			try{
				std::array<std::uint8_t, 0> buf;
				conn->socket.send(utki::wrapBuf(buf));

				//under win32 the canRead() assertion fails sometimes... //TODO: why?
//				ASSERT(!conn->socket.canRead())//TODO: remove?
				ASSERT(!conn->socket.canWrite())

				conn->SetHandlingThread(this);
				ASS(this->listener)->OnConnected_ts(conn);
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): connection was successful" << std::endl)
			}catch(setka::Exc& e){
				TRACE(<< "ConnectionsThread::" << __func__ << "(): connection was unsuccessful: " << e.what() << std::endl)
				this->HandleRemoveConnectionMessage(conn);
				return;
			}

		}else{
			ASSERT(conn->dataSent < conn->packetQueue.front()->size())

			try{
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): Packet data left = " << (conn->packetQueue.front().Size() - conn->dataSent) << std::endl)

				conn->dataSent += conn->socket.send(utki::Buf<const std::uint8_t>(
						&*conn->packetQueue.front()->begin() + conn->dataSent,
						conn->packetQueue.front()->size() - conn->dataSent
					));
				ASSERT(conn->dataSent <= conn->packetQueue.front()->size())

//				TRACE(<< "ConnectionsThread::" << __func__ << "(): Packet data left = " << (conn->packetQueue.front().Size() - conn->dataSent) << std::endl)

				if(conn->dataSent == conn->packetQueue.front()->size()){
					conn->packetQueue.pop_front();
					conn->dataSent = 0;

//					TRACE(<< "ConnectionsThread::" << __func__ << "(): Packet sent!!!!!!!!!!!!!!!!!!!!!" << std::endl)

					ASS(this->listener)->OnDataSent_ts(conn, conn->packetQueue.size(), false);

					if(conn->packetQueue.size() == 0){
						//clear WRITE waiting flag.
						conn->currentFlags = pogodi::Waitable::EReadinessFlags(
								conn->currentFlags & (~pogodi::Waitable::WRITE)
							);
						this->waitSet.change(conn->socket, conn->currentFlags);
					}
				}
			}catch(setka::Exc &e){
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): exception caught while sending: " << e.What() << std::endl)
				this->HandleRemoveConnectionMessage(conn);
				return;
			}
		}
	}

	if(conn->socket.canRead()){
//		TRACE(<< "ConnectionsThread::HandleSocketActivity(): canRead()" << std::endl)

		std::array<std::uint8_t, 0x2000> buffer;//8kb

		try{
			unsigned bytesReceived = conn->socket.recieve(buffer);
			ASSERT(!conn->socket.canRead())
			if(bytesReceived != 0){
				utki::Buf<std::uint8_t> b(&*buffer.begin(), bytesReceived);
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): bytesReceived = " << bytesReceived << " b.Size() = " << b.Size() << std::endl)
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): b[...] = "
//						<< unsigned(b[0]) << " "
//						<< unsigned(b[1]) << " "
//						<< unsigned(b[2]) << " "
//						<< unsigned(b[3]) << std::endl
//					)

				ASSERT(conn->receivedData.size() == 0)
				if(!ASS(this->listener)->OnDataReceived_ts(conn, b)){
					std::lock_guard<decltype(conn->mutex)> mutexGuard(conn->mutex);
					
//					TRACE(<< "ConnectionsThread::HandleSocketActivity(): received data not handled!!!!!!!!!!!" << std::endl)
					ASSERT(conn->receivedData.size() == 0)

					conn->receivedData.resize(b.size());
					memcpy(&*conn->receivedData.begin(), &*b.begin(), b.size());

					//clear READ waiting flag
					conn->currentFlags = pogodi::Waitable::EReadinessFlags(
							conn->currentFlags & (~pogodi::Waitable::READ)
						);
					this->waitSet.change(conn->socket, conn->currentFlags);
				}
			}else{
				//connection closed
				this->HandleRemoveConnectionMessage(conn);
				return;
			}
		}catch(setka::Exc &e){
//			TRACE(<< "ConnectionsThread::" << __func__ << "(): exception caught while reading: " << e.What() << std::endl)
			this->HandleRemoveConnectionMessage(conn);
			return;
		}
	}

	if(conn->socket.errorCondition()){
		this->HandleRemoveConnectionMessage(conn);
		return;
	}

	ASSERT(!conn->socket.canRead())
	ASSERT(!conn->socket.canWrite())
}



void ConnectionsThread::HandleAddConnectionMessage(const std::shared_ptr<Connection>& conn, bool isConnected){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): enter" << std::endl)

	ASSERT(conn)

	//set Waitable pointer to connection
	conn->socket.setUserData(
			reinterpret_cast<void*>(
					static_cast<cliser::Connection*>(conn.operator->())//NOTE: static cast just to make sure we have cliser::Connection*
				)
		);
	ASSERT(reinterpret_cast<cliser::Connection*>(conn->socket.getUserData()) == conn.operator->())

	
	//add socket to waitset
	
	//if not connected, will be waiting for WRITE, because WRITE indicates that connect request has finished.
	conn->currentFlags = isConnected ? pogodi::Waitable::READ : pogodi::Waitable::WRITE;
	try{
		this->AddSocketToSocketSet(
				conn->socket,
				conn->currentFlags
			);
	}catch(utki::Exc& e){
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

	ASSERT(!conn->socket.canRead())
	ASSERT(!conn->socket.canWrite())

	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): exit" << std::endl)
}



//removing client means disconnect as well
void ConnectionsThread::HandleRemoveConnectionMessage(std::shared_ptr<cliser::Connection>& conn){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): enter" << std::endl)

	ASSERT(conn)

	for(ConnectionsThread::T_ConnectionsIter i = this->connections.begin();
			i != this->connections.end();
			++i
		)
	{
		if((*i) == conn){
			ASSERT((*i) == conn)
			ASSERT((*i).operator->() == conn.operator->())

			this->RemoveSocketFromSocketSet(conn->socket);

			conn->socket.close();//close connection if it is opened

			conn->ClearHandlingThread();

			//clear packetQueue
			conn->packetQueue.clear();

			//notify client disconnection
			ASS(this->listener)->OnDisconnected_ts(conn);

			this->connections.erase(i);//remove client from list

			return;
		}
	}

	//NOTE: it is possible that disconnection request message is posted to the threads message queue
	//      and before it is handled the connection is disconnected by peer. Thus, we will not find the
	//      connection in the list of connections here, so, do not ASSERT(false).
	//ASSERT(false)
}



void ConnectionsThread::HandleSendDataMessage(std::shared_ptr<Connection>& conn, std::shared_ptr<const std::vector<std::uint8_t>>&& data){
//	TRACE(<< "ConnectionsThread::" << __func__ << "(): enter" << std::endl)
	
	if(!conn->socket){
//		TRACE(<< "ConnectionsThread::" << __func__ << "(): socket is disconnected, ignoring message" << std::endl)
		return;
	}

	if(conn->packetQueue.size() != 0){
//		TRACE(<< "ConnectionsThread::" << __func__ << "(): adding data to send queue right away" << std::endl)
		conn->packetQueue.push_back(std::move(data));
		ASS(this->listener)->OnDataSent_ts(conn, conn->packetQueue.size(), true);
		return;
	}else{
		try{
			unsigned numBytesSent = conn->socket.send(*data);
			ASSERT(numBytesSent <= data->size())

			if(numBytesSent != data->size()){
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): adding data to send queue" << std::endl)
				conn->dataSent = numBytesSent;
				conn->packetQueue.push_back(std::move(data));

				//Set WRITE waiting flag
				conn->currentFlags = pogodi::Waitable::EReadinessFlags(
						conn->currentFlags | pogodi::Waitable::WRITE
					);
				this->waitSet.change(conn->socket, conn->currentFlags);

				ASSERT_INFO(conn->packetQueue.size() == 1, conn->packetQueue.size())
				ASS(this->listener)->OnDataSent_ts(conn, 1, true);
			}else{
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): NOT adding data to send queue" << std::endl)
				ASSERT_INFO(conn->packetQueue.size() == 0, conn->packetQueue.size())
				ASS(this->listener)->OnDataSent_ts(conn, 0, false);
			}
		}catch(setka::Exc& e){
//			TRACE(<< "ConnectionsThread::" << __func__ << "(): exception caught" << e.What() << std::endl)
			conn->Disconnect_ts();
		}
	}
//	TRACE(<< "ConnectionsThread::" << __func__ << "(): exit" << std::endl)
}



void ConnectionsThread::HandleResumeListeningForReadMessage(std::shared_ptr<Connection>& conn){
	if(!conn->socket){//if connection is closed
		return;
	}

//	TRACE(<< "ConnectionsThread::" << __func__ << "(): resuming data receiving!!!!!!!!!!" << std::endl)

	ASSERT(conn->receivedData.size() == 0)

	//Set READ waiting flag
	conn->currentFlags = pogodi::Waitable::EReadinessFlags(
			conn->currentFlags | pogodi::Waitable::READ
		);
	this->waitSet.change(conn->socket, conn->currentFlags);
}


