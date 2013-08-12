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



#include <list>

#include <ting/debug.hpp>
#include <ting/math.hpp>
#include <ting/util.hpp>
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
	++this->listener->numTimesAdded;
}



ConnectionsThread::~ConnectionsThread()throw(){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): invoked" << std::endl)
	ASSERT(this->connections.size() == 0)
	ASSERT(this->listener)
	--this->listener->numTimesAdded;
}



void ConnectionsThread::Run(){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): new thread started" << std::endl)

	this->waitSet.Add(this->queue, ting::Waitable::READ);

	ting::Array<ting::Waitable*> triggered(this->waitSet.Size());

	while(!this->quitFlag){
//		TRACE(<< "ConnectionsThread::" << __func__ << "(): waiting..." << std::endl)
		unsigned numTriggered = this->waitSet.Wait(&triggered);
//		TRACE(<< "ConnectionsThread::" << __func__ << "(): triggered" << std::endl)
//		ASSERT(numTriggered > 0)

		ting::Waitable** p = triggered.Begin();
		for(unsigned i = 0; i != numTriggered; ++i, ++p){
			ASSERT(p != triggered.End())
			ASSERT(*p)

			if(*p == &this->queue){
				//Do not handle messages here, first handle all activities from sockets.
				//Check if there are messages and handle them only after all sockets are
				//handled. This is because connection can be removed as a result of handling some message while
				//pointer to the waitable will still be in the buffer of triggered waitables, thus
				//this pointer to waitable will be invalid.
			}else{
				//socket
				ASSERT(*p != &this->queue)

				ASSERT((*p)->GetUserData())

				ting::Ref<cliser::Connection> conn(
						reinterpret_cast<cliser::Connection*>((*p)->GetUserData())
					);
				ASSERT(conn)

				this->HandleSocketActivity(conn);
			}
		}//~for()

		//At this point we have handled all the socket activities and now we can proceed with
		//handling messages if there are any.
		if(this->queue.CanRead()){
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
				if(ting::Ptr<ting::mt::Message> m = this->queue.PeekMsg()){
					M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): message got" << std::endl)
					m->Handle();
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
		ting::Ref<Connection> c(ASS(*i));

		this->RemoveSocketFromSocketSet(c->socket);
		c->socket.Close();
		c->ClearHandlingThread();

		ASS(this->listener)->OnDisconnected_ts(c);
	}
	this->connections.clear();//clear clients list

	this->waitSet.Remove(this->queue);

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
			conn->currentFlags = ting::Waitable::READ;
			this->waitSet.Change(conn->socket, conn->currentFlags);

			//try writing 0 bytes to clear the write flag and to check if connection was successful
			try{
				ting::StaticBuffer<ting::u8, 0> buf;
				conn->socket.Send(buf);

				//under win32 the CanRead() assertion fails sometimes... //TODO: why?
//				ASSERT(!conn->socket.CanRead())//TODO: remove?
				ASSERT(!conn->socket.CanWrite())

				conn->SetHandlingThread(this);
				ASS(this->listener)->OnConnected_ts(conn);
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): connection was successful" << std::endl)
			}catch(ting::net::Exc& e){
				TRACE(<< "ConnectionsThread::" << __func__ << "(): connection was unsuccessful: " << e.What() << std::endl)
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
						conn->currentFlags = ting::Waitable::EReadinessFlags(
								conn->currentFlags & (~ting::Waitable::WRITE)
							);
						this->waitSet.Change(conn->socket, conn->currentFlags);
					}
				}
			}catch(ting::net::Exc &e){
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): exception caught while sending: " << e.What() << std::endl)
				this->HandleRemoveConnectionMessage(conn);
				return;
			}
		}
	}

	if(conn->socket.CanRead()){
//		TRACE(<< "ConnectionsThread::HandleSocketActivity(): CanRead()" << std::endl)

		ting::StaticBuffer<ting::u8, 0x2000> buffer;//8kb

		try{
			unsigned bytesReceived = conn->socket.Recv(buffer);
			ASSERT(!conn->socket.CanRead())
			if(bytesReceived != 0){
				ting::Buffer<ting::u8> b(buffer.Begin(), bytesReceived);
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): bytesReceived = " << bytesReceived << " b.Size() = " << b.Size() << std::endl)
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): b[...] = "
//						<< unsigned(b[0]) << " "
//						<< unsigned(b[1]) << " "
//						<< unsigned(b[2]) << " "
//						<< unsigned(b[3]) << std::endl
//					)

				ASSERT(!conn->receivedData)
				if(!ASS(this->listener)->OnDataReceived_ts(conn, b)){
					ting::mt::Mutex::Guard mutexGuard(conn->mutex);
					
//					TRACE(<< "ConnectionsThread::HandleSocketActivity(): received data not handled!!!!!!!!!!!" << std::endl)
					ASSERT(!conn->receivedData)

					conn->receivedData.Init(b);

					//clear READ waiting flag
					conn->currentFlags = ting::Waitable::EReadinessFlags(
							conn->currentFlags & (~ting::Waitable::READ)
						);
					this->waitSet.Change(conn->socket, conn->currentFlags);
				}
			}else{
				//connection closed
				this->HandleRemoveConnectionMessage(conn);
				return;
			}
		}catch(ting::net::Exc &e){
//			TRACE(<< "ConnectionsThread::" << __func__ << "(): exception caught while reading: " << e.What() << std::endl)
			this->HandleRemoveConnectionMessage(conn);
			return;
		}
	}

	if(conn->socket.ErrorCondition()){
		this->HandleRemoveConnectionMessage(conn);
		return;
	}

	ASSERT(!conn->socket.CanRead())
	ASSERT(!conn->socket.CanWrite())
}



void ConnectionsThread::HandleAddConnectionMessage(const ting::Ref<Connection>& conn, bool isConnected){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): enter" << std::endl)

	ASSERT(conn)

	//set Waitable pointer to connection
	conn->socket.SetUserData(
			reinterpret_cast<void*>(
					static_cast<cliser::Connection*>(conn.operator->())//NOTE: static cast just to make sure we have cliser::Connection*
				)
		);
	ASSERT(reinterpret_cast<cliser::Connection*>(conn->socket.GetUserData()) == conn.operator->())

	
	//add socket to waitset
	
	//if not connected, will be waiting for WRITE, because WRITE indicates that connect request has finished.
	conn->currentFlags = isConnected ? ting::Waitable::READ : ting::Waitable::WRITE;
	try{
		this->AddSocketToSocketSet(
				conn->socket,
				conn->currentFlags
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

	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): exit" << std::endl)
}



//removing client means disconnect as well
void ConnectionsThread::HandleRemoveConnectionMessage(ting::Ref<cliser::Connection>& conn){
	M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): enter" << std::endl)

	ASSERT(conn.IsValid())

	for(ConnectionsThread::T_ConnectionsIter i = this->connections.begin();
			i != this->connections.end();
			++i
		)
	{
		if((*i) == conn){
			ASSERT((*i) == conn)
			ASSERT((*i).operator->() == conn.operator->())

			this->RemoveSocketFromSocketSet(conn->socket);

			conn->socket.Close();//close connection if it is opened

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



void ConnectionsThread::HandleSendDataMessage(ting::Ref<Connection>& conn, ting::Array<ting::u8> data){
//	TRACE(<< "ConnectionsThread::" << __func__ << "(): enter" << std::endl)
	
	if(!conn->socket.IsValid()){
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

				//Set WRITE waiting flag
				conn->currentFlags = ting::Waitable::EReadinessFlags(
						conn->currentFlags | ting::Waitable::WRITE
					);
				this->waitSet.Change(conn->socket, conn->currentFlags);

				ASSERT_INFO(conn->packetQueue.size() == 1, conn->packetQueue.size())
				ASS(this->listener)->OnDataSent_ts(conn, 1, true);
			}else{
//				TRACE(<< "ConnectionsThread::" << __func__ << "(): NOT adding data to send queue" << std::endl)
				ASSERT_INFO(conn->packetQueue.size() == 0, conn->packetQueue.size())
				ASS(this->listener)->OnDataSent_ts(conn, 0, false);
			}
		}catch(ting::net::Exc& e){
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

	//Set READ waiting flag
	conn->currentFlags = ting::Waitable::EReadinessFlags(
			conn->currentFlags | ting::Waitable::READ
		);
	this->waitSet.Change(conn->socket, conn->currentFlags);
}


