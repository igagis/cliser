// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Client class

#include <ting/debug.hpp>
#include <ting/Thread.hpp>

#include "Connection.hpp"
#include "ConnectionsThread.hpp"



using namespace cliser;



void Connection::Send_ts(ting::Array<ting::u8> data){
	ting::Mutex::Guard mutexGuard(this->mutex);//make sure that this->clientThread won't be zeroed out by other thread
	if(!this->parentThread){
		//client disconnected, do nothing
		TRACE(<< "Connection::" << __func__ << "(): client disconnected" << std::endl)
		return;
	}
	ASSERT(this->parentThread)
	
	ting::Ref<Connection> c(this);
	this->parentThread->PushMessage(
			ting::Ptr<ting::Message>(
					new ConnectionsThread::SendDataMessage(this->parentThread, c, data)
				)
		);
}



void Connection::SendCopy_ts(const ting::Buffer<ting::u8>& data){
	ting::Array<ting::u8> buf(data);

	this->Send_ts(buf);
}



void Connection::Disconnect_ts(){
	ting::Mutex::Guard mutexGuard(this->mutex);
	if(!this->parentThread){
		//client disconnected, do nothing
		return;
	}
	ASSERT(this->parentThread)
	ting::Ref<Connection> c(this);
	this->parentThread->PushMessage(
			ting::Ptr<ting::Message>(
					new ConnectionsThread::RemoveConnectionMessage(this->parentThread, c)
				)
		);

	this->parentThread = 0;
}



ting::Array<ting::u8> Connection::GetReceivedData_ts(){
	ting::Mutex::Guard mutexGuard(this->mutex);

	//Send the message to parent thread only if
	//there was received data stored, which means
	//that socket is not listened for READ condition.
	if(this->receivedData && this->parentThread){
		ting::Ref<Connection> c(this);
		ASSERT(c)
		
		//send message to parentHandler thread
		ASS(this->parentThread)->PushMessage(
				ting::Ptr<ting::Message>(
						new ConnectionsThread::ResumeListeningForReadMessage(this->parentThread, c)
					)
			);
	}

	return this->receivedData;
}


