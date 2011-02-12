// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Client class

#include "Client.hpp"
#include "TCPClientsHandlerThread.hpp"

#include <ting/debug.hpp>
#include <ting/Thread.hpp>

using namespace cliser;


void Client::SendNetworkData_ts(ting::Array<ting::u8> data){
	ting::Mutex::Guard mutexGuard(this->mutex);//make sure that this->clientThread won't be zeroed out by other thread
	if(!this->clientThread){
		//client disconnected, do nothing
		return;
	}
	ASSERT(this->clientThread)
	ting::Ref<Client> c(this);
	this->clientThread->PushMessage(
			ting::Ptr<ting::Message>(
					new SendNetworkDataToClientMessage(this->clientThread, c, data)
				)
		);
}



void Client::SendNetworkDataCopy_ts(const ting::Buffer<ting::u8>& data){
	ting::Array<ting::u8> buf(data.SizeInBytes());
	memcpy(&buf[0], &data[0], buf.SizeInBytes());

	this->SendNetworkData_ts(buf);
}



void Client::Disconnect(){
	ting::Mutex::Guard mutexGuard(this->mutex);
	if(!this->clientThread){
		//client disconnected, do nothing
		return;
	}
	ASSERT(this->clientThread)
	ting::Ref<Client> c(this);
	this->clientThread->PushMessage(
			ting::Ptr<ting::Message>(new RemoveClientFromThreadMessage(this->clientThread, c))
		);
}
