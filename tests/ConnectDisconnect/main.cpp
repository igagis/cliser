#include <utki/debug.hpp>
#include <utki/Buf.hpp>
#include <setka/Setka.hpp>

#include <algorithm>

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
				this->disconnect_ts();
			}
			return;
		}

		std::vector<std::uint8_t> buf(sizeof(std::uint32_t) * ( (std::min)(std::uint32_t((0xffff + 1) / sizeof(std::uint32_t)), DMaxCnt - this->cnt)) );

		ASSERT(buf.size() > 0)

		ASSERT_INFO((buf.size() % sizeof(std::uint32_t)) == 0, "buf.Size() = " << buf.size() << " (buf.Size() % sizeof(std::uint32_t)) = " << (buf.size() % sizeof(std::uint32_t)))

		for(std::uint8_t* p = &*buf.begin(); p != &*buf.end(); p += sizeof(std::uint32_t)){
			utki::serialize32LE(this->cnt, p);
			++this->cnt;
		}

		this->send_ts(std::make_shared<std::vector<std::uint8_t>>(std::move(buf)));
	}


	void HandleReceivedData(const utki::Buf<std::uint8_t> d){
		for(const std::uint8_t* p = d.begin(); p != d.end(); ++p){
			this->rbuf[this->rbufBytes] = *p;
			++this->rbufBytes;

			if(this->rbufBytes == this->rbuf.size()){
				this->rbufBytes = 0;
				std::uint32_t num = utki::deserialize32LE(this->rbuf.begin());
				ASSERT_INFO_ALWAYS(this->rcnt == num, "num = " << num << " rcnt = " << this->rcnt)
				++this->rcnt;
			}
		}

		ASSERT_INFO(this->rcnt <= DMaxCnt, "this->rcnt = " << this->rcnt)

		if(this->rcnt == DMaxCnt){
			if(this->cnt == DMaxCnt){
				this->disconnect_ts();
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
	std::mutex numConsMut;
	unsigned numConnections = 0;
	
	//override
	std::shared_ptr<cliser::Connection> createConnectionObject(){
		return utki::makeShared<Connection>();
	}

	//override
	void onConnected_ts(const std::shared_ptr<cliser::Connection>& c){
		TRACE_ALWAYS(<< "Server::" << __func__ << "(): CONNECTED!!!" << std::endl)

		std::shared_ptr<Connection> conn = std::static_pointer_cast<Connection>(c);
		ASSERT_ALWAYS(!conn->isConnected)
		conn->isConnected = true;

		{
			std::lock_guard<decltype(this->numConsMut)> mutexGuard(this->numConsMut);
			++(this->numConnections);
//			ASSERT_INFO_ALWAYS(this->numConnections <= 2 * DMaxConnections, "this->numConnections = " << this->numConnections)
		}
		
		TRACE_ALWAYS(<< "Server: sending data" << std::endl)
		std::static_pointer_cast<Connection>(c)->SendPortion();
	}

	//override
	void onDisconnected_ts(const std::shared_ptr<cliser::Connection>& c)override{
		TRACE_ALWAYS(<< "Server::" << __func__ << "(): DISCONNECTED!!!" << std::endl)

		std::shared_ptr<Connection> conn = std::static_pointer_cast<Connection>(c);
		ASSERT_INFO_ALWAYS(conn->isConnected, "Server: disconnected non-connected connection")

		//do not clear the flag to catch second connection of the same Connection object if any
//		conn->isConnected = false;

		{
			std::lock_guard<decltype(this->numConsMut)> mutexGuard(this->numConsMut);
			--(this->numConnections);
//			ASSERT_INFO_ALWAYS(this->numConnections <= 2 * DMaxConnections, "this->numConnections = " << this->numConnections)
		}
	}

	//override
	bool onDataReceived_ts(const std::shared_ptr<cliser::Connection>& c, const utki::Buf<std::uint8_t> d)override{
		TRACE_ALWAYS(<< "Server: data received" << std::endl)
		this->pushMessage(
				[c](){
					std::vector<std::uint8_t> d = c->getReceivedData_ts();
					if(d.size() != 0){
						std::static_pointer_cast<Connection>(c)->HandleReceivedData(utki::wrapBuf(d));
					}
				}
			);

		return false;
	}

	//override
	void onDataSent_ts(const std::shared_ptr<cliser::Connection>& c, unsigned numPacketsInQueue, bool addedToQueue)override{
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
	std::mutex numConsMut;
	unsigned numConnections = 0;

	//override
	std::shared_ptr<cliser::Connection> createConnectionObject()override{
		return utki::makeShared<Connection>();
	}

	//override
	void onConnected_ts(const std::shared_ptr<cliser::Connection>& c)override{
		TRACE_ALWAYS(<< "Client::" << __func__ << "(): CONNECTED!!!" << std::endl)

		{
			std::lock_guard<decltype(this->numConsMut)> mutexGuard(this->numConsMut);
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
	void onDisconnected_ts(const std::shared_ptr<cliser::Connection>& c)override{
		std::shared_ptr<Connection> conn = std::static_pointer_cast<Connection>(c);
		
		if(conn->isConnected){
			TRACE_ALWAYS(<< "Client::" << __func__ << "(): DISCONNECTED!!!" << std::endl)

			//do not clear the flag to catch second connection of the same Connection object if any
//			conn->isConnected = false;

			{
				std::lock_guard<decltype(this->numConsMut)> mutexGuard(this->numConsMut);
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

		this->connect_ts(setka::IPAddress(DIpAddress, DPort));
	}

	//override
	bool onDataReceived_ts(const std::shared_ptr<cliser::Connection>& c, const utki::Buf<std::uint8_t> d)override{
		std::shared_ptr<Connection> con = std::static_pointer_cast<Connection>(c);

		con->HandleReceivedData(d);
		TRACE_ALWAYS(<< "Client: data received" << std::endl)
		return true;
	}

	//override
	void onDataSent_ts(const std::shared_ptr<cliser::Connection>& c, unsigned numPacketsInQueue, bool addedToQueue)override{
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

	setka::Setka socketsLib;

	Server server;
	server.start();

	nitki::Thread::sleep(100);//give server thread some time to start waiting on the socket

	Client client;
	client.start();

	for(unsigned i = 0; i < client.maxConnections(); ++i){
		client.connect_ts(setka::IPAddress(DIpAddress, DPort));
	}

	if(msec == 0){
		while(true){
			nitki::Thread::sleep(1000000);
		}
	}else{
		nitki::Thread::sleep(20000);
	}

	//set the flag to indicate that the thread is exiting
	//and thus, if we get a connection failure, then it must be because
	//there was a pending connection request and since the thread is exiting,
	//it will be reported as the connection has failed. So, no need to assert in that case.
	client.quitMessagePosted = true;
	
	client.pushQuitMessage();
	client.join();

	server.pushQuitMessage();
	server.join();

	return 0;
}
