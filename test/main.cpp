#include "../src/cliser/srv/Server.hpp"



namespace srv{

class Connection : public cliser::Connection{
public:

	ting::StaticBuffer<ting::u8, sizeof(ting::u32)> rbuf;
	ting::Inited<unsigned, 0> rbufBytes;

	ting::Inited<ting::u32, 0> cnt;

	ting::Inited<ting::u32, 0> rcnt;

	Connection(){
		
	}


	void SendPortion(){
		ting::Array<ting::u8> buf(sizeof(ting::u32) * (0xffff / 4));

		for(ting::u8* p = buf.Begin(); p != buf.End(); p += sizeof(ting::u32)){
			ting::Serialize32(this->cnt, p);
			++this->cnt;
		}

		this->Send_ts(buf);
	}


	static ting::Ref<Connection> New(){
		return ting::Ref<Connection>(new Connection());
	}
};



class Server : public cliser::Server{
public:
	Server() :
			cliser::Server(13666, 2)
	{}

	//override
	ting::Ref<cliser::Connection> CreateClientObject(){
		return Connection::New();
	}

	//override
	void OnClientConnected_ts(ting::Ref<cliser::Connection>& c){
		c.StaticCast<Connection>()->SendPortion();
		//TODO: how to determine that 2 packets are in queue for sending???
	}

	//override
	void OnClientDisconnected_ts(ting::Ref<cliser::Connection>& c){
		//do nothing
	}

	//override
	void OnDataReceived_ts(ting::Ref<cliser::Connection>& c, const ting::Buffer<ting::u8>& d){
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
	}

	//override
	void OnDataSent_ts(ting::Ref<cliser::Connection>& c){
		c.StaticCast<Connection>()->SendPortion();
	}
};

}//~namespace



int main(int argc, char *argv[]){
//	TRACE_ALWAYS(<<"Socket test "<<std::endl)


//	ting::SocketLib socketsLib;

	srv::Server server;


	server.Start();


	//TODO:




	while(true){
		ting::Thread::Sleep(10000000);
	}

	server.PushQuitMessage();
	server.Join();

	return 0;
}
