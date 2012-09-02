#include <ting/debug.hpp>
#include <ting/Buffer.hpp>
#include <ting/mt/MsgThread.hpp>
#include <ting/mt/Mutex.hpp>
#include <ting/net/Lib.hpp>

#include "../src/cliser/ServerThread.hpp"
#include "../src/cliser/ClientThread.hpp"



namespace{
const unsigned DMaxConnections = 63;

const ting::u16 DPort = 13666;
const char* DIpAddress = "127.0.0.1";
}



class Connection : public cliser::Connection{
public:

	ting::StaticBuffer<ting::u8, sizeof(ting::u32)> rbuf;
	ting::Inited<unsigned, 0> rbufBytes;

	ting::Inited<ting::u32, 0> cnt;

	ting::Inited<ting::u32, 0> rcnt;

	ting::Inited<bool, false> isConnected;

	Connection(){
//		TRACE(<< "Connection::" << __func__ << "(): invoked" << std::endl)
	}

	~Connection()throw(){
//		TRACE(<< "Connection::" << __func__ << "(): invoked" << std::endl)
	}

	void SendPortion(){
		ting::Array<ting::u8> buf(0xffff + 1);
		STATIC_ASSERT(sizeof(ting::u32) == 4)
		ASSERT_INFO_ALWAYS((buf.Size() % sizeof(ting::u32)) == 0, "buf.Size() = " << buf.Size() << " (buf.Size() % sizeof(ting::u32)) = " << (buf.Size() % sizeof(ting::u32)))

		ting::u8* p = buf.Begin();
		for(; p != buf.End(); p += sizeof(ting::u32)){
			ASSERT_INFO_ALWAYS(p < (buf.End() - (sizeof(ting::u32) - 1)), "p = " << p << " buf.End() = " << buf.End())
			ting::util::Serialize32LE(this->cnt, p);
			++this->cnt;
		}
		ASSERT_ALWAYS(p == buf.End())

		this->Send_ts(buf);
	}


	void HandleReceivedData(const ting::Buffer<ting::u8>& d){
		for(const ting::u8* p = d.Begin(); p != d.End(); ++p){
			this->rbuf[this->rbufBytes] = *p;
			++this->rbufBytes;

			ASSERT_ALWAYS(this->rbufBytes <= this->rbuf.Size())

			if(this->rbufBytes == this->rbuf.Size()){
				this->rbufBytes = 0;
				ting::u32 num = ting::util::Deserialize32LE(this->rbuf.Begin());
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



	static ting::Ref<Connection> New(){
		return ting::Ref<Connection>(new Connection());
	}
};



class Server : private cliser::Listener, public cliser::ServerThread{
public:
	Server() :
			cliser::Listener(),
			cliser::ServerThread(DPort, 2, this, false, 100)
	{}

	~Server()throw(){
		ASSERT_INFO_ALWAYS(this->numConnections == 0, "this->numConnections = " << this->numConnections)
	}
private:
	ting::mt::Mutex numConsMut;
	ting::Inited<unsigned, 0> numConnections;
	
	//override
	ting::Ref<cliser::Connection> CreateConnectionObject(){
		return Connection::New();
	}

	//override
	void OnConnected_ts(const ting::Ref<cliser::Connection>& c){
		ting::Ref<Connection> conn = c.StaticCast<Connection>();
		ASSERT_ALWAYS(!conn->isConnected)
		conn->isConnected = true;

		{
			ting::mt::Mutex::Guard mutexGuard(this->numConsMut);
			++(this->numConnections);
			ASSERT_INFO_ALWAYS(this->numConnections <= DMaxConnections, "this->numConnections = " << this->numConnections)
		}


		TRACE_ALWAYS(<< "Server: sending data" << std::endl)
		c.StaticCast<Connection>()->SendPortion();
	}

	//override
	void OnDisconnected_ts(const ting::Ref<cliser::Connection>& c){
		ting::Ref<Connection> conn = c.StaticCast<Connection>();

		ASSERT_INFO_ALWAYS(conn->isConnected, "Server: disconnected non-connected connection")
		conn->isConnected = false;

		{
			ting::mt::Mutex::Guard mutexGuard(this->numConsMut);
			--(this->numConnections);
			ASSERT_INFO_ALWAYS(this->numConnections <= DMaxConnections, "this->numConnections = 0x" << std::hex << this->numConnections)
		}
	}

	//override
	bool OnDataReceived_ts(const ting::Ref<cliser::Connection>& c, const ting::Buffer<ting::u8>& d){
		ting::Ref<Connection> con = c.StaticCast<Connection>();

		con->HandleReceivedData(d);
		TRACE_ALWAYS(<< "Server: data received" << std::endl)
		return true;
	}

	//override
	void OnDataSent_ts(const ting::Ref<cliser::Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){
		if(numPacketsInQueue >= 2)
			return;

		TRACE_ALWAYS(<< "Server: sending data" << std::endl)
		c.StaticCast<Connection>()->SendPortion();
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
	ting::Inited<unsigned, 0> numConnections;
	
	//override
	ting::Ref<cliser::Connection> CreateConnectionObject(){
		return Connection::New();
	}

	//override
	void OnConnected_ts(const ting::Ref<cliser::Connection>& c){
		{
			ting::mt::Mutex::Guard mutexGuard(this->numConsMut);
			++(this->numConnections);
			ASSERT_INFO_ALWAYS(this->numConnections <= DMaxConnections, "this->numConnections = " << this->numConnections)
		}

		ting::Ref<Connection> conn = c.StaticCast<Connection>();
		ASSERT_ALWAYS(!conn->isConnected)
		conn->isConnected = true;

		TRACE_ALWAYS(<< "Client: sending data" << std::endl)
		conn->SendPortion();
	}

	//override
	void OnDisconnected_ts(const ting::Ref<cliser::Connection>& c){
		ting::Ref<Connection> conn = c.StaticCast<Connection>();
		
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

	class HandleDataMessage : public ting::mt::Message{
		ting::Ref<Connection> conn;
	public:
		HandleDataMessage(const ting::Ref<Connection>& conn) :
				conn(conn)
		{}

		//override
		void Handle(){
			if(ting::Array<ting::u8> d = this->conn->GetReceivedData_ts()){
				this->conn->HandleReceivedData(d);
			}else{
				ASSERT_ALWAYS(false)
			}
		}
	};


	//override
	bool OnDataReceived_ts(const ting::Ref<cliser::Connection>& c, const ting::Buffer<ting::u8>& d){
		TRACE_ALWAYS(<< "Client: data received" << std::endl)
		this->PushMessage(
				ting::Ptr<ting::mt::Message>(
						new HandleDataMessage(c.StaticCast<Connection>())
					)
			);

		return false;
	}

	//override
	void OnDataSent_ts(const ting::Ref<cliser::Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){
		if(numPacketsInQueue >= 2)
			return;
		
		TRACE_ALWAYS(<< "Client: sending data" << std::endl)
		c.StaticCast<Connection>()->SendPortion();
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
