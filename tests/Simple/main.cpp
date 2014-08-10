#include <ting/debug.hpp>
#include <ting/ArrayAdaptor.hpp>
#include <ting/mt/MsgThread.hpp>
#include <ting/mt/Mutex.hpp>
#include <ting/net/Lib.hpp>

#include "../../src/cliser/ServerThread.hpp"
#include "../../src/cliser/ClientThread.hpp"



namespace{
const unsigned DMaxConnections = 63;

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
		std::vector<std::uint8_t> buf(0xffff + 1);
		
		ASSERT_INFO_ALWAYS((buf.size() % sizeof(std::uint32_t)) == 0, "buf.Size() = " << buf.size() << " (buf.Size() % sizeof(std::uint32_t)) = " << (buf.size() % sizeof(std::uint32_t)))

		std::uint8_t* p = &*buf.begin();
		for(; p != &*buf.end(); p += sizeof(std::uint32_t)){
			ASSERT_INFO_ALWAYS(p < (&*buf.end() - (sizeof(std::uint32_t) - 1)), "p = " << p << " buf.End() = " << &*buf.end())
			ting::util::Serialize32LE(this->cnt, p);
			++this->cnt;
		}
		ASSERT_ALWAYS(p == &*buf.end())

		this->Send_ts(std::make_shared<std::vector<std::uint8_t>>(std::move(buf)));
	}


	void HandleReceivedData(const ting::ArrayAdaptor<std::uint8_t> d){
		for(const std::uint8_t* p = &*d.begin(); p != &*d.end(); ++p){
			this->rbuf[this->rbufBytes] = *p;
			++this->rbufBytes;

			ASSERT_ALWAYS(this->rbufBytes <= this->rbuf.size())

			if(this->rbufBytes == this->rbuf.size()){
				this->rbufBytes = 0;
				std::uint32_t num = ting::util::Deserialize32LE(this->rbuf.begin());
				ASSERT_INFO_ALWAYS(
						this->rcnt == num,
						"num = " << num << " rcnt = " << this->rcnt 
								<< " rcnt - num = " << (this->rcnt - num)
								<< " rbuf = "
								<< unsigned(rbuf[0]) << ", "
								<< unsigned(rbuf[1]) << ", "
								<< unsigned(rbuf[2]) << ", "
								<< unsigned(rbuf[3])
					)
				++this->rcnt;
			}
		}
	}
};



class Server : private cliser::Listener, public cliser::ServerThread{
public:
	Server() :
			cliser::Listener(),
			cliser::ServerThread(DPort, 2, this, false, 100)
	{}

	~Server()NOEXCEPT{
		ASSERT_INFO_ALWAYS(this->numConnections == 0, "this->numConnections = " << this->numConnections)
	}
private:
	ting::mt::Mutex numConsMut;
	unsigned numConnections = 0;
	
	std::shared_ptr<cliser::Connection> CreateConnectionObject()override{
		return ting::New<Connection>();
	}

	void OnConnected_ts(const std::shared_ptr<cliser::Connection>& c)override{
		std::shared_ptr<Connection> conn = std::static_pointer_cast<Connection>(c);
		ASSERT_ALWAYS(!conn->isConnected)
		conn->isConnected = true;

		{
			ting::mt::Mutex::Guard mutexGuard(this->numConsMut);
			++(this->numConnections);
			ASSERT_INFO_ALWAYS(this->numConnections <= DMaxConnections, "this->numConnections = " << this->numConnections)
		}


		TRACE_ALWAYS(<< "Server: sending data" << std::endl)
		std::static_pointer_cast<Connection>(c)->SendPortion();
	}

	void OnDisconnected_ts(const std::shared_ptr<cliser::Connection>& c)override{
		std::shared_ptr<Connection> conn = std::static_pointer_cast<Connection>(c);

		ASSERT_INFO_ALWAYS(conn->isConnected, "Server: disconnected non-connected connection")
		conn->isConnected = false;

		{
			ting::mt::Mutex::Guard mutexGuard(this->numConsMut);
			--(this->numConnections);
			ASSERT_INFO_ALWAYS(this->numConnections <= DMaxConnections, "this->numConnections = 0x" << std::hex << this->numConnections)
		}
	}

	bool OnDataReceived_ts(const std::shared_ptr<cliser::Connection>& c, const ting::ArrayAdaptor<std::uint8_t> d)override{
		std::shared_ptr<Connection> con = std::static_pointer_cast<Connection>(c);

		con->HandleReceivedData(d);
		TRACE_ALWAYS(<< "Server: data received" << std::endl)
		return true;
	}

	void OnDataSent_ts(const std::shared_ptr<cliser::Connection>& c, unsigned numPacketsInQueue, bool addedToQueue)override{
		if(numPacketsInQueue >= 2){
			return;
		}

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
private:
	ting::mt::Mutex numConsMut;
	unsigned numConnections = 0;
	
	std::shared_ptr<cliser::Connection> CreateConnectionObject()override{
		return ting::New<Connection>();
	}

	void OnConnected_ts(const std::shared_ptr<cliser::Connection>& c)override{
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

	void OnDisconnected_ts(const std::shared_ptr<cliser::Connection>& c)override{
		std::shared_ptr<Connection> conn = std::static_pointer_cast<Connection>(c);
		
		if(conn->isConnected){
			conn->isConnected = false;

			{
				ting::mt::Mutex::Guard mutexGuard(this->numConsMut);
				--(this->numConnections);
				ASSERT_INFO_ALWAYS(this->numConnections <= DMaxConnections, "this->numConnections = " << this->numConnections)
			}
		}else{
			//if we get here then it is a connect request failure
			ASSERT_INFO_ALWAYS(conn->isConnected, "Connect request failure")
		}
	}


	bool OnDataReceived_ts(const std::shared_ptr<cliser::Connection>& c, const ting::ArrayAdaptor<std::uint8_t> d)override{
		TRACE_ALWAYS(<< "Client: data received" << std::endl)
		
		this->PushMessage(
				[c](){
					std::vector<std::uint8_t> d = std::move(c->GetReceivedData_ts());
					if(d.size()){
						std::static_pointer_cast<Connection>(c)->HandleReceivedData(d);
					}else{
						ASSERT_ALWAYS(false)
					}
				}
			);

		return false;
	}

	void OnDataSent_ts(const std::shared_ptr<cliser::Connection>& c, unsigned numPacketsInQueue, bool addedToQueue)override{
		if(numPacketsInQueue >= 2){
			return;
		}
		
		TRACE_ALWAYS(<< "Client: sending data" << std::endl)
		std::static_pointer_cast<Connection>(c)->SendPortion();
	}
};



int main(int argc, char *argv[]){
	TRACE_ALWAYS(<< "Cliser test" << std::endl)

	unsigned msec = 10000;

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
		ting::mt::Thread::Sleep(msec);
	}

	client.PushQuitMessage();
	client.Join();

	server.PushQuitMessage();
	server.Join();

	return 0;
}
