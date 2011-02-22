#include "../src/cliser/srv/ServerThread.hpp"
#include "../src/cliser/clt/ClientThread.hpp"



class Connection : public cliser::Connection{
public:

	ting::StaticBuffer<ting::u8, sizeof(ting::u32)> rbuf;
	ting::Inited<unsigned, 0> rbufBytes;

	ting::Inited<ting::u32, 0> cnt;

	ting::Inited<ting::u32, 0> rcnt;

	Connection(){

	}


	void SendPortion(){
		ting::Array<ting::u8> buf(sizeof(ting::u32) * (0xfffff / 4));

		for(ting::u8* p = buf.Begin(); p != buf.End(); p += sizeof(ting::u32)){
			ting::Serialize32(this->cnt, p);
			++this->cnt;
		}

		this->Send_ts(buf);
	}


	static ting::Ref<Connection> New(){
		return ting::Ref<Connection>(ASS(new Connection()));
	}
};



class Server : public cliser::ServerThread{
public:
	Server() :
			cliser::ServerThread(13666, 2)
	{}

	//override
	ting::Ref<cliser::Connection> CreateClientObject(){
		return Connection::New();
	}

	//override
	void OnClientConnected_ts(const ting::Ref<cliser::Connection>& c){
		TRACE_ALWAYS(<< "Server: sending data" << std::endl)
		c.StaticCast<Connection>()->SendPortion();
	}

	//override
	void OnClientDisconnected_ts(const ting::Ref<cliser::Connection>& c){
		//do nothing
	}

	//override
	void OnDataReceived_ts(const ting::Ref<cliser::Connection>& c, const ting::Buffer<ting::u8>& d){
		ting::Ref<Connection> con = c.StaticCast<Connection>();

		for(const ting::u8* p = d.Begin(); p != d.End(); ++p){
			con->rbuf[con->rbufBytes] = *p;
			++con->rbufBytes;

			if(con->rbufBytes == con->rbuf.Size()){
				con->rbufBytes = 0;
				ting::u32 num = ting::Deserialize32(con->rbuf.Begin());
				ASSERT_INFO_ALWAYS(con->rcnt == num, num)
				++con->rcnt;
			}
		}
		TRACE_ALWAYS(<< "Server: data received" << std::endl)
	}

	//override
	void OnDataSent_ts(const ting::Ref<cliser::Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){
		if(addedToQueue && numPacketsInQueue >= 2)
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
	ting::Ref<cliser::Connection> CreateClientObject(){
		return Connection::New();
	}

	//override
	void OnConnectFailure(EConnectFailureReason failReason){
		ASSERT_ALWAYS(false)
	}

	//override
	void OnClientConnected_ts(const ting::Ref<cliser::Connection>& c){
		TRACE_ALWAYS(<< "Client: sending data" << std::endl)
		c.StaticCast<Connection>()->SendPortion();
	}

	//override
	void OnClientDisconnected_ts(const ting::Ref<cliser::Connection>& c){
		//do nothing
	}

	//override
	void OnDataReceived_ts(const ting::Ref<cliser::Connection>& c, const ting::Buffer<ting::u8>& d){
		ting::Ref<Connection> con = c.StaticCast<Connection>();

		for(const ting::u8* p = d.Begin(); p != d.End(); ++p){
			con->rbuf[con->rbufBytes] = *p;
			++con->rbufBytes;

			if(con->rbufBytes == con->rbuf.Size()){
				con->rbufBytes = 0;
				ting::u32 num = ting::Deserialize32(con->rbuf.Begin());
				ASSERT_INFO_ALWAYS(con->rcnt == num, num)
				++con->rcnt;
			}
		}
		TRACE_ALWAYS(<< "Client: data received" << std::endl)
	}

	//override
	void OnDataSent_ts(const ting::Ref<cliser::Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){
		if(addedToQueue && numPacketsInQueue >= 2)
			return;
		
		TRACE_ALWAYS(<< "Client: sending data" << std::endl)
		c.StaticCast<Connection>()->SendPortion();
	}
};



int main(int argc, char *argv[]){
//	TRACE_ALWAYS(<< "Socket test " << std::endl)

	ting::SocketLib socketsLib;

	Server server;
	server.Start();

	Client client;
	client.Start();

	for(unsigned i = 0; i < client.MaxConnections(); ++i){
		client.Connect_ts(ting::IPAddress("127.0.0.1", 13666));
	}



	while(true){
		ting::Thread::Sleep(10000000);
	}

	server.PushQuitMessage();
	server.Join();

	return 0;
}
