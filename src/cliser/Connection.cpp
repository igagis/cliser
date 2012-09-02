/* The MIT License:

Copyright (c) 2009-2011 Ivan Gagis <igagis@gmail.com>

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

//Homepage: http://code.google.com/p/cliser/

#include <ting/debug.hpp>

#include "Connection.hpp"
#include "ConnectionsThread.hpp"



using namespace cliser;



void Connection::Send_ts(ting::Array<ting::u8> data){
	ting::mt::Mutex::Guard mutexGuard(this->mutex);//make sure that this->clientThread won't be zeroed out by other thread
	if(!this->parentThread){
		//client disconnected, do nothing
//		TRACE(<< "Connection::" << __func__ << "(): client disconnected" << std::endl)
		return;
	}
	ASSERT(this->parentThread)
	
	ting::Ref<Connection> c(this);
	this->parentThread->PushMessage(
			ting::Ptr<ting::mt::Message>(
					new ConnectionsThread::SendDataMessage(this->parentThread, c, data)
				)
		);
}



void Connection::SendCopy_ts(const ting::Buffer<ting::u8>& data){
	ting::Array<ting::u8> buf(data);

	this->Send_ts(buf);
}



void Connection::Disconnect_ts(){
	ting::mt::Mutex::Guard mutexGuard(this->mutex);
	if(!this->parentThread){
		//client disconnected, do nothing
		return;
	}
	ASSERT(this->parentThread)
	ting::Ref<Connection> c(this);
	this->parentThread->PushMessage(
			ting::Ptr<ting::mt::Message>(
					new ConnectionsThread::RemoveConnectionMessage(this->parentThread, c)
				)
		);

	this->parentThread = 0;
}



ting::Array<ting::u8> Connection::GetReceivedData_ts(){
	ting::mt::Mutex::Guard parentThreadMutextGuard(this->mutex);

	//At the moment of sending the ResumeListeningForReadMessage the receivedData variable should be empty.
	ting::Array<ting::u8> ret = this->receivedData;

	//Send the message to parent thread only if
	//there was received data stored, which means
	//that socket is not listened for READ condition.
	if(ret && this->parentThread){
		ting::Ref<Connection> c(this);
		ASSERT(c)

		//send message to parentHandler thread
		ASS(this->parentThread)->PushMessage(
				ting::Ptr<ting::mt::Message>(
						new ConnectionsThread::ResumeListeningForReadMessage(this->parentThread, c)
					)
			);
	}

	return ret;
}


