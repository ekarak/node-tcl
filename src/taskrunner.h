
#ifndef TASKRUNNER_H
#define TASKRUNNER_H

#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <string>
#include <condition_variable>

#include <nan.h>
#include "asynchandler.h"
#include "tcl.h"

using namespace v8;

/* a TaskRunner is able to queue tcl tasks to be run in a separate thread
with its own interp
*/
class TaskRunner {
public:

	struct task_t {
		int 				  argc = 0;
		Tcl_Obj*      argv = nullptr;
		AsyncHandler* handler;
	};

	TaskRunner();
	virtual ~TaskRunner();

	void queue( const task_t * task );

private:

	void worker();

	std::atomic< bool >   _terminate;
	std::queue< const task_t* > _tasks;

	std::thread _worker;
	std::mutex  _task_mutex;

	std::condition_variable _worker_cnd;

	//
	v8::Isolate* _isolate;
};

#endif /*! TASKRUNNER_H */
