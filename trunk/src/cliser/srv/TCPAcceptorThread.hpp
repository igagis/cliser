// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          TCP Acceptor Thread class

#pragma once

#include <ting/types.hpp>

namespace cliser{

//forward declarations
class Server;

class TCPAcceptorThread : public ting::MsgThread{
    Server *serverMainThread;

	ting::u16 port;
public:
    TCPAcceptorThread(Server *smt, ting::u16 listeningPort) :
            serverMainThread(smt),
			port(listeningPort)
    {}
    
    //override
    void Run();
};

}//~namespace
