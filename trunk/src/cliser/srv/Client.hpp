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
class TCPClientsHandlerThread;
class Server;

class Client : public ting::RefCounted{
	friend class TCPClientsHandlerThread;
	friend class SendNetworkDataToClientMessage;
	friend class AddClientToThreadMessage;
	friend class RemoveClientFromThreadMessage;

	//This is the network data associated with Client.
	//This data is controlled by clients's TCPClientsHandlerThread thread
	//after the client is added to the theread.
	cliser::NetworkReceiverState netReceiverState;
	//~

	ting::TCPSocket socket;//socket which corresponds to the client

	//NOTE: clientThread may be accessed from different threads, therefore, protect it with mutex
	TCPClientsHandlerThread *clientThread;
	ting::Mutex mutex;

	inline void SetClientHandlerThread(TCPClientsHandlerThread *thr){
		ASSERT(thr)
		ting::Mutex::Guard mutexGueard(this->mutex);
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
	inline Client() :
			clientThread(0)
	{}

	virtual ~Client(){
//		TRACE(<< "~cliser::Client(): invoked" << std::endl)
	}

	void Disconnect();

	void SendNetworkData_ts(ting::Array<ting::byte> data);
	void SendNetworkDataCopy_ts(const ting::Array<ting::byte>& data);
};

}//~namespace
