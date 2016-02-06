
#include "taskrunner.h"
#include "util.h"

using namespace v8;

TaskRunner::TaskRunner() : _terminate( false ) {

	// initialise worker thread
	_worker = std::thread( &TaskRunner::worker, this );

	printf("(%p): new async TaskRunner => new worker thread=%p\n",
	 	(void*) uv_thread_self(),
		(void*) _worker.native_handle());
}


TaskRunner::~TaskRunner() {

	// flag termination
	_terminate = true;

	// notify worker
	_worker_cnd.notify_all();

	// wait for the worker thread
	_worker.join();

}


void TaskRunner::queue( const task_t* task ) {

	{
		std::unique_lock< std::mutex > lock( _task_mutex );
		_tasks.push( task );

		// schedule an async worker outside of main event loop
		Nan::AsyncQueueWorker( task->handler );
	}

	// notify worker thread
	_worker_cnd.notify_one();

}


void TaskRunner::worker() {

	const task_t* task;

	v8log("TaskRunner::worker() creating new v8::Isolate\n");
	_isolate = newV8Isolate();
	// lock the v8 isolate for this thread
	Locker locker(_isolate);

  v8log("TaskRunner::worker() creating new Tcl interp\n");
	Tcl_Interp * interp = newTclInterp();
	int status = Tcl_Init( interp );
  v8log("TaskRunner::worker() before main loop\n");
	while (! _terminate ) {

		{
			std::unique_lock< std::mutex > lock( _task_mutex );
			while ( (! _terminate ) && _tasks.empty() ) {
				_worker_cnd.wait( lock );
			}

			if ( _terminate && _tasks.empty() ) {
				break;
			}

			task = _tasks.front();
			_tasks.pop();
		}
		v8log("TaskRunner::worker() popped new task\n");

		if ( status == TCL_OK ) {
			int taskResultCode;
		/*	if (task->argc == 1) {
				// no arguments passed, use EvalEx
				v8log("TaskRunner::worker() calling Tcl_EvalEx\n");
				taskResultCode = Tcl_EvalEx( interp, Tcl_GetString(task->argv), -1, 0 );
			} else { */
				v8log("TaskRunner::worker() calling Tcl_EvalObjEx, argc=%d, argv=%s\n",
				task->argc,  Tcl_GetString(task->argv));
				taskResultCode = Tcl_EvalObjEx( interp, task->argv,  TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT );
			//}
			const char* result = Tcl_GetStringResult( interp );
			if ( taskResultCode == TCL_ERROR ) {
				v8log("*** TCL_ERROR: %s\n", result);
				// FIXME: proper JS error, not just a string message
				task->handler->notify( result, "" );
			} else {
				task->handler->notify( "", result );
			}

		} else {
			task->handler->notify( "Failed to initialise Tcl interpreter", "" );
		}

		delete task;
	}

	// cleanup
	Tcl_DeleteInterp( interp );

	_isolate->Exit();
	_isolate->Dispose();
}
