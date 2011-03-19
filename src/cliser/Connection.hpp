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

/**
 * @file Connection.hpp
 * @author Ivan Gagis <igagis@gmail.com>
 * @brief Connection base class.
 * Connection base class. User is supposed to derive his own connection
 * object from it.
 */

#pragma once

#include <ting/types.hpp>
#include <ting/Array.hpp>
#include <ting/Buffer.hpp>
#include <ting/Ref.hpp>
#include <ting/Socket.hpp>
#include <ting/Thread.hpp>



namespace cliser{



//forward declarations
class ConnectionsThread;



class Connection : public virtual ting::RefCounted{
	friend class ConnectionsThread;
	friend class ServerThread;
	friend class ClientThread;

	//This is the network data associated with Connection
	std::list<ting::Array<ting::u8> > packetQueue;
	unsigned dataSent;//number of bytes sent from first packet in the queue
	//~

	ting::TCPSocket socket;
	
	//NOTE: clientThread may be accessed from different threads, therefore, protect it with mutex
	ConnectionsThread *parentThread;
	ting::Mutex parentThreadMutex;

	ting::Array<ting::u8> receivedData;//Should be protected with mutex

	inline void SetHandlingThread(ConnectionsThread *thr){
		ASSERT(thr)
		ting::Mutex::Guard mutexGuard(this->parentThreadMutex);
		//Assert that client is not added to some thread already.
		ASSERT_INFO(!this->parentThread, "client's handler thread is already set")
		this->parentThread = thr;
	}

	inline void ClearHandlingThread(){
		ting::Mutex::Guard mutexGuard(this->parentThreadMutex);
		this->parentThread = 0;
	}

protected:
	ting::Mutex receivedDataMutex;

	inline Connection() :
			parentThread(0)
	{}

public:
	virtual ~Connection(){
//		TRACE(<< "Connection::" << __func__ << "(): invoked" << std::endl)
	}

	void Disconnect_ts();

	void Send_ts(ting::Array<ting::u8> data);
	void SendCopy_ts(const ting::Buffer<ting::u8>& data);

	ting::Array<ting::u8> GetReceivedData_ts();
};



class Listener{

	//NOTE: this numTimesAdded variable is only needed to check that
	//      the listener object has not been deleted before its owner
	//      (the object which does notifications through this listener).
	friend class cliser::ConnectionsThread;
	friend class ServerThread;
	ting::Inited<unsigned, 0> numTimesAdded;

public:
	
	virtual ting::Ref<cliser::Connection> CreateConnectionObject() = 0;

	virtual void OnConnected_ts(const ting::Ref<Connection>& c) = 0;

	virtual void OnDisconnected_ts(const ting::Ref<Connection>& c) = 0;

	virtual bool OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d) = 0;

	virtual void OnDataSent_ts(const ting::Ref<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){}

	virtual ~Listener(){
		ASSERT(this->numTimesAdded == 0)
	}
};



}//~namespace
