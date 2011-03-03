#include <ting/debug.hpp>

#include "../src/cliser/ServerThread.hpp"
#include "../src/cliser/ClientThread.hpp"



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

	~Connection(){
//		TRACE(<< "Connection::" << __func__ << "(): invoked" << std::endl)
	}

	void SendPortion(){
		ting::Array<ting::u8> buf(sizeof(ting::u32) * (0xfffff / 4));

		for(ting::u8* p = buf.Begin(); p != buf.End(); p += sizeof(ting::u32)){
			ting::Serialize32(this->cnt, p);
			++this->cnt;
		}

		this->Send_ts(buf);
	}


	void HandleReceivedData(const ting::Buffer<ting::u8>& d){
		for(const ting::u8* p = d.Begin(); p != d.End(); ++p){
			this->rbuf[this->rbufBytes] = *p;
			++this->rbufBytes;

			if(this->rbufBytes == this->rbuf.Size()){
				this->rbufBytes = 0;
				ting::u32 num = ting::Deserialize32(this->rbuf.Begin());
				ASSERT_INFO_ALWAYS(this->rcnt == num, "num = " << num << " rcnt = " << this->rcnt)
				++this->rcnt;
			}
		}
	}



	static ting::Ref<Connection> New(){
		return ting::Ref<Connection>(new Connection());
	}
};



class Server : public cliser::ServerThread{
public:
	Server() :
			cliser::ServerThread(13666, 2)
	{}

	//override
	ting::Ref<cliser::Connection> CreateConnectionObject(){
		return Connection::New();
	}

	//override
	void OnConnected_ts(const ting::Ref<cliser::Connection>& c){
		TRACE_ALWAYS(<< "Server: sending data" << std::endl)
		c.StaticCast<Connection>()->SendPortion();
	}

	//override
	void OnDisconnected_ts(const ting::Ref<cliser::Connection>& c){
		//do nothing
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



class Client : public cliser::ClientThread{
public:
	Client() :
			cliser::ClientThread(63) //max connections
	{}

	//override
	ting::Ref<cliser::Connection> CreateConnectionObject(){
		return Connection::New();
	}

	//override
	void OnConnected_ts(const ting::Ref<cliser::Connection>& c){
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
		}else{
			//if we get here then it is a connect request failure
			ASSERT_ALWAYS(conn->isConnected)
		}
	}

	class HandleDataMessage : public ting::Message{
		ting::Ref<Connection> conn;
	public:
		HandleDataMessage(const ting::Ref<Connection>& conn) :
				conn(conn)
		{}

		//override
		void Handle(){
			if(ting::Array<ting::u8> d = this->conn->GetReceivedData_ts()){
				this->conn->HandleReceivedData(d);
			}
		}
	};


	//override
	bool OnDataReceived_ts(const ting::Ref<cliser::Connection>& c, const ting::Buffer<ting::u8>& d){
		TRACE_ALWAYS(<< "Client: data received" << std::endl)
		this->PushMessage(
				ting::Ptr<ting::Message>(
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

	ting::SocketLib socketsLib;

	Server server;
	server.Start();

	ting::Thread::Sleep(100);//give server thread some time to start waiting on the socket

	Client client;
	client.Start();

	for(unsigned i = 0; i < client.MaxConnections(); ++i){
		client.Connect_ts(ting::IPAddress("127.0.0.1", 13666));
	}

	ting::Thread::Sleep(2500);

    client.PushQuitMessage();
    client.Join();

	server.PushQuitMessage();
	server.Join();

	return 0;
}
