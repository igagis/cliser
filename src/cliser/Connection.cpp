#include <utki/debug.hpp>

#include "Connection.hpp"
#include "ConnectionsThread.hpp"



using namespace cliser;



void Connection::send_ts(const std::shared_ptr<const std::vector<std::uint8_t>>& data){
	std::lock_guard<decltype(this->mutex)> mutexGuard(this->mutex);//make sure that this->clientThread won't be zeroed out by other thread
	if(!this->parentThread){
		//client disconnected, do nothing
//		TRACE(<< "Connection::" << __func__ << "(): client disconnected" << std::endl)
		return;
	}
	ASSERT(this->parentThread)
	
	this->parentThread->pushMessage(std::bind(
			[](ConnectionsThread* thr, std::shared_ptr<Connection>& c, std::shared_ptr<const std::vector<std::uint8_t>>& data){
				thr->handleSendDataMessage(c, std::move(data));
			},
			this->parentThread,
			this->sharedFromThis(this),
			data
		));
}






void Connection::disconnect_ts(){
	std::lock_guard<decltype(this->mutex)> mutexGuard(this->mutex);
	if(!this->parentThread){
		//client disconnected, do nothing
		return;
	}
	ASSERT(this->parentThread)
	
	
	this->parentThread->pushMessage(std::bind(
			[](ConnectionsThread* t, std::shared_ptr<Connection>& c){
				t->handleRemoveConnectionMessage(c);
			},
			this->parentThread,
			this->sharedFromThis(this)
		));

	this->parentThread = 0;
}



const std::vector<std::uint8_t> Connection::getReceivedData_ts(){
	std::lock_guard<decltype(this->mutex)> parentThreadMutextGuard(this->mutex);

	//At the moment of sending the ResumeListeningForReadMessage the receivedData variable should be empty.
	std::vector<std::uint8_t> ret = std::move(this->receivedData);

	//Send the message to parent thread only if
	//there was received data stored, which means
	//that socket is not listened for READ condition.
	if(ret.size() != 0 && this->parentThread){
		//send message to parentHandler thread
		ASS(this->parentThread)->pushMessage(std::bind(
				[](ConnectionsThread* t, std::shared_ptr<Connection>& c){
					t->handleResumeListeningForReadMessage(c);
				},
				this->parentThread,
				this->sharedFromThis(this)
			));
	}

	return std::move(ret);
}


