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
	if(!this->clientThread){
		//client disconnected, do nothing
		TRACE(<< "Connection::Send_ts(): client disconnected" << std::endl)
		return;
	}
	ASSERT(this->clientThread)
	
	ting::Ref<Connection> c(this);
	this->clientThread->PushMessage(
			ting::Ptr<ting::Message>(
					new ConnectionsThread::SendDataMessage(this->clientThread, c, data)
				)
		);
}



void Connection::SendCopy_ts(const ting::Buffer<ting::u8>& data){
	ting::Array<ting::u8> buf(data);

	this->Send_ts(buf);
}



void Connection::Disconnect_ts(){
	ting::Mutex::Guard mutexGuard(this->mutex);
	if(!this->clientThread){
		//client disconnected, do nothing
		return;
	}
	ASSERT(this->clientThread)
	ting::Ref<Connection> c(this);
	this->clientThread->PushMessage(
			ting::Ptr<ting::Message>(
					new ConnectionsThread::RemoveConnectionMessage(this->clientThread, c)
				)
		);

	this->clientThread = 0;
}
