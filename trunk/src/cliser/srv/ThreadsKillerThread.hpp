// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Terminal Input Thread class

#pragma once

#include <ting/Ptr.hpp>
#include <ting/Thread.hpp>

namespace cliser{

//forward declarations
class Server;
class ThreadsKillerThread;
class C_KillThreadMessage;

class ThreadsKillerThread : public ting::MsgThread{
	friend class C_KillThreadMessage;

public:
	ThreadsKillerThread(){};

	//override
	void Run();

private:
	class C_KillThreadMessage : public ting::Message{
		ThreadsKillerThread *tkt;//to whom this message will be sent
		ting::Ptr<ting::MsgThread> thr;//thread to kill
	  public:
		C_KillThreadMessage(ThreadsKillerThread *threadKillerThread, ting::Ptr<ting::MsgThread> thread) :
				tkt(threadKillerThread),
				thr(thread)
		{
			ASSERT(this->tkt)
			ASSERT(this->thr.IsValid())
			this->thr->PushQuitMessage();//post a quit message to the thread before message is sent to threads kiler thread
		};

		//override
		void Handle();
	};
};

}//~namespace
