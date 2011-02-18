// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//

#include "Server.hpp"

using namespace cliser;



void ThreadsKillerThread::Run(){
    while(!this->quitFlag){
//        TRACE(<<"ThreadsKillerThread::Run(): going to get message"<<std::endl)
        this->queue.GetMsg()->Handle();
    }
}



//override
void ThreadsKillerThread::KillThreadMessage::Handle(){
//	TRACE(<<"C_KillThreadMessage::Handle(): invoked"<<std::endl)

	this->thr->Join();//wait for thread finish
}


