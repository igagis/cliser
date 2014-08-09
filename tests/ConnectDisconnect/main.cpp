#include <ting/debug.hpp>
#include <algorithm>
#include <ting/Buffer.hpp>
#include <ting/net/Lib.hpp>

#include "../../src/cliser/ServerThread.hpp"
#include "../../src/cliser/ClientThread.hpp"




namespace{
const unsigned DMaxConnections = 63;

const std::uint32_t DMaxCnt = 16384;
const std::uint16_t DPort = 13666;
const char* DIpAddress = "127.0.0.1";
}



class Connection : public cliser::Connection{
public:

	std::array<std::uint8_t, sizeof(std::uint32_t)> rbuf;
	unsigned rbufBytes = 0;

	std::uint32_t cnt = 0;

	std::uint32_t rcnt = 0;

	bool isConnected = false;

	Connection(){
//		TRACE(<< "Connection::" << __func__ << "(): invoked" << std::endl)
	}

	~Connection()throw(){
//		TRACE(<< "Connection::" << __func__ << "(): invoked" << std::endl)
	}

	void SendPortion(){
		ASSERT_INFO(this->cnt <= DMaxCnt, "this->cnt = " << this->cnt)

		if(this->cnt == DMaxCnt){
			if(this->rcnt == DMaxCnt){
				this->Disconnect_ts();
			}
			return;
		}

		std::vector<std::uint8_t> buf(sizeof(std::uint32_t) * ( (std::min)(std::uint32_t((0xffff + 1) / sizeof(std::uint32_t)), DMaxCnt - this->cnt)) );

		ASSERT(buf.size() > 0)

		ASSERT_INFO((buf.size() % sizeof(std::uint32_t)) == 0, "buf.Size() = " << buf.size() << " (buf.Size() % sizeof(std::uint32_t)) = " << (buf.size() % sizeof(std::uint32_t)))

		for(std::uint8_t* p = &*buf.begin(); p != &*buf.end(); p += sizeof(std::uint32_t)){
			ting::util::Serialize32LE(this->cnt, p);
			++this->cnt;
		}

		this->Send_ts(ting::New<cliser::SharedBuffer>(std::move(buf)));
	}


	void HandleReceivedData(const ting::Buffer<std::uint8_t>& d){
		for(const std::uint8_t* p = d.begin(); p != d.end(); ++p){
			this->rbuf[this->rbufBytes] = *p;
			++this->rbufBytes;

			if(this->rbufBytes == this->rbuf.size()){
				this->rbufBytes = 0;
				std::uint32_t num = ting::util::Deserialize32LE(this->rbuf.begin());
				ASSERT_INFO_ALWAYS(this->rcnt == num, "num = " << num << " rcnt = " << this->rcnt)
				++this->rcnt;
			}
		}

		ASSERT_INFO(this->rcnt <= DMaxCnt, "this->rcnt = " << this->rcnt)

		if(this->rcnt == DMaxCnt){
			if(this->cnt == DMaxCnt){
				this->Disconnect_ts();
			}
		}
	}
};



class Server : private cliser::Listener, public cliser::ServerThread{
public:
	Server() :
			cliser::Listener(),
			cliser::ServerThread(DPort, 2, this, true, 100)
	{}

	~Server()noexcept{
		ASSERT_INFO_ALWAYS(this->numConnections == 0, "this->numConnections = " << this->numConnections)
	}
private:
	ting::mt::Mutex numConsMut;
	unsigned numConnections = 0;
	
	//override
	std::shared_ptr<cliser::Connection> CreateConnectionObject(){
		return ting::New<Connection>();
	}

	//override
	void OnConnected_ts(const std::shared_ptr<cliser::Connection>& c){
		TRACE_ALWAYS(<< "Server::" << __func__ << "(): CONNECTED!!!" << std::endl)

		std::shared_ptr<Connection> conn = std::static_pointer_cast<Connection>(c);
		ASSERT_ALWAYS(!conn->isConnected)
		conn->isConnected = true;

		{
			ting::mt::Mutex::Guard mutexGuard(this->numConsMut);
			++(this->numConnections);
//			ASSERT_INFO_ALWAYS(this->numConnections <= 2 * DMaxConnections, "this->numConnections = " << this->numConnections)
		}
		
		TRACE_ALWAYS(<< "Server: sending data" << std::endl)
		std::static_pointer_cast<Connection>(c)->SendPortion();
	}

	//override
	void OnDisconnected_ts(const std::shared_ptr<cliser::Connection>& c){
		TRACE_ALWAYS(<< "Server::" << __func__ << "(): DISCONNECTED!!!" << std::endl)

		std::shared_ptr<Connection> conn = std::static_pointer_cast<Connection>(c);
		ASSERT_INFO_ALWAYS(conn->isConnected, "Server: disconnected non-connected connection")

		//do not clear the flag to catch second connection of the same Connection object if any
//		conn->isConnected = false;

		{
			ting::mt::Mutex::Guard mutexGuard(this->numConsMut);
			--(this->numConnections);
//			ASSERT_INFO_ALWAYS(this->numConnections <= 2 * DMaxConnections, "this->numConnections = " << this->numConnections)
		}
	}

	class HandleDataMessage{
		std::shared_ptr<Connection> conn;
	public:
		HandleDataMessage(const std::shared_ptr<Connection>& conn) :
				conn(conn)
		{}

		//override
		void Handle(){
			
		}
	};

	//override
	bool OnDataReceived_ts(const std::shared_ptr<cliser::Connection>& c, const ting::Buffer<std::uint8_t>& d){
		TRACE_ALWAYS(<< "Server: data received" << std::endl)
		this->PushMessage(
				[c](){
					std::vector<std::uint8_t> d = c->GetReceivedData_ts();
					if(d.size() != 0){
						std::static_pointer_cast<Connection>(c)->HandleReceivedData(d);
					}
				}
			);

		return false;
	}

	//override
	void OnDataSent_ts(const std::shared_ptr<cliser::Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){
		if(numPacketsInQueue >= 2)
			return;

		TRACE_ALWAYS(<< "Server: sending data" << std::endl)
		std::static_pointer_cast<Connection>(c)->SendPortion();
	}
};



class Client : private cliser::Listener, public cliser::ClientThread{
public:
	Client() :
			cliser::Listener(),
			cliser::ClientThread(DMaxConnections, this) //max connections
	{}

	~Client()throw(){
		ASSERT_INFO_ALWAYS(this->numConnections == 0, "this->numConnections = " << this->numConnections)
	}

	bool quitMessagePosted = 0;
private:
	ting::mt::Mutex numConsMut;
	unsigned numConnections = 0;

	//override
	std::shared_ptr<cliser::Connection> CreateConnectionObject(){
		return ting::New<Connection>();
	}

	//override
	void OnConnected_ts(const std::shared_ptr<cliser::Connection>& c){
		TRACE_ALWAYS(<< "Client::" << __func__ << "(): CONNECTED!!!" << std::endl)

		{
			ting::mt::Mutex::Guard mutexGuard(this->numConsMut);
			++(this->numConnections);
			ASSERT_INFO_ALWAYS(this->numConnections <= DMaxConnections, "this->numConnections = " << this->numConnections)
		}

		std::shared_ptr<Connection> conn = std::static_pointer_cast<Connection>(c);
		ASSERT_ALWAYS(!conn->isConnected)
		conn->isConnected = true;

		TRACE_ALWAYS(<< "Client: sending data" << std::endl)
		conn->SendPortion();
	}

	//override
	void OnDisconnected_ts(const std::shared_ptr<cliser::Connection>& c){
		std::shared_ptr<Connection> conn = std::static_pointer_cast<Connection>(c);
		
		if(conn->isConnected){
			TRACE_ALWAYS(<< "Client::" << __func__ << "(): DISCONNECTED!!!" << std::endl)

			//do not clear the flag to catch second connection of the same Connection object if any
//			conn->isConnected = false;

			{
				ting::mt::Mutex::Guard mutexGuard(this->numConsMut);
				--(this->numConnections);
				ASSERT_INFO_ALWAYS(this->numConnections <= DMaxConnections, "this->numConnections = " << this->numConnections)
			}
		}else{
			if(!this->quitMessagePosted){
				//if we get here then it is a connect request failure
				ASSERT_INFO_ALWAYS(false, "Connection failed")
			}else{
				TRACE_ALWAYS(<< "Client::" << __func__ << "(): connection failed on thread exit" << std::endl)
			}
		}

		this->Connect_ts(ting::net::IPAddress(DIpAddress, DPort));
	}

	//override
	bool OnDataReceived_ts(const std::shared_ptr<cliser::Connection>& c, const ting::Buffer<std::uint8_t>& d){
		std::shared_ptr<Connection> con = std::static_pointer_cast<Connection>(c);

		con->HandleReceivedData(d);
		TRACE_ALWAYS(<< "Client: data received" << std::endl)
		return true;
	}

	//override
	void OnDataSent_ts(const std::shared_ptr<cliser::Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){
		if(numPacketsInQueue >= 2)
			return;
		
		TRACE_ALWAYS(<< "Client: sending data" << std::endl)
		std::static_pointer_cast<Connection>(c)->SendPortion();
	}
};



int main(int argc, char *argv[]){
	TRACE_ALWAYS(<< "Cliser test" << std::endl)

	unsigned msec = 20000;

	if(argc >= 2){
		if(std::string("0") == argv[1]){
			msec = 0;
		}
	}

	ting::net::Lib socketsLib;

	Server server;
	server.Start();

	ting::mt::Thread::Sleep(100);//give server thread some time to start waiting on the socket

	Client client;
	client.Start();

	for(unsigned i = 0; i < client.MaxConnections(); ++i){
		client.Connect_ts(ting::net::IPAddress(DIpAddress, DPort));
	}

	if(msec == 0){
		while(true){
			ting::mt::Thread::Sleep(1000000);
		}
	}else{
		ting::mt::Thread::Sleep(20000);
	}

	//set the flag to indicate that the thread is exiting
	//and thus, if we get a connection failure, then it must be because
	//there was a pending connection request and since the thread is exiting,
	//it will be reported as the connection has failed. So, no need to assert in that case.
	client.quitMessagePosted = true;
	
	client.PushQuitMessage();
	client.Join();

	server.PushQuitMessage();
	server.Join();

	return 0;
}
