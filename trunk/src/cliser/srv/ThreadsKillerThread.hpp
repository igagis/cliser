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
//...



class ThreadsKillerThread : public ting::MsgThread{
	friend class Server;


private:
	ThreadsKillerThread(){};

	//override
	void Run();

private:
	class KillThreadMessage : public ting::Message{
		ThreadsKillerThread *thread;//to whom this message will be sent
		ting::Ptr<ting::MsgThread> thr;//thread to kill
	  public:
		KillThreadMessage(ThreadsKillerThread *threadKillerThread, ting::Ptr<ting::MsgThread> thread) :
				thread(threadKillerThread),
				thr(thread)
		{
			ASSERT(this->thread)
			ASSERT(this->thr.IsValid())
			this->thr->PushQuitMessage();//post a quit message to the thread before message is sent to threads kiler thread
		};

		//override
		void Handle();
	};
};

}//~namespace
