// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Client class

#pragma once

#include <ting/types.hpp>
#include <ting/Array.hpp>
#include <ting/Buffer.hpp>
#include <ting/Ref.hpp>
#include <ting/Socket.hpp>
#include <ting/Thread.hpp>

#include "../util/util.hpp"



namespace cliser{



//forward declarations
class ConnectionsThread;



class Connection : public ting::RefCounted{
	friend class ConnectionsThread;
	friend class ServerThread;
	friend class ClientThread;

	//This is the network data associated with Connection
	std::list<ting::Array<ting::u8> > packetQueue;
	unsigned dataSent;//number of bytes sent from first packet in the queue
	//~

	ting::TCPSocket socket;
	
	//NOTE: clientThread may be accessed from different threads, therefore, protect it with mutex
	ConnectionsThread *clientThread;
	ting::Mutex mutex;

	inline void SetClientHandlerThread(ConnectionsThread *thr){
		ASSERT(thr)
		ting::Mutex::Guard mutexGuard(this->mutex);
		//Assert that client is not added to some thread already.
		ASSERT_INFO(!this->clientThread, "client's handler thread is already set")
		this->clientThread = thr;
	}

	inline void ClearClientHandlerThread(){
		ting::Mutex::Guard mutexGuard(this->mutex);
		ASSERT(this->clientThread)
		this->clientThread = 0;
	}

protected:

public:
	inline Connection() :
			clientThread(0)
	{}

	virtual ~Connection(){
//		TRACE(<< "~cliser::Client(): invoked" << std::endl)
	}

	void Disconnect_ts();

	void Send_ts(ting::Array<ting::u8> data);
	void SendCopy_ts(const ting::Buffer<ting::u8>& data);


	static inline ting::Ref<cliser::Connection> New(){
		return ting::Ref<cliser::Connection>(new cliser::Connection());
	}
};

}//~namespace
