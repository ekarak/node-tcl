/*
 * Shamelessly copied from:
 *  https://github.com/jefftrull/EDASkel/tree/master/tclint
 */

#include "tclnotifier.h"


using namespace NodeTclNotify;

//
uint64_t   t0 = uv_hrtime(); // boot up time
//
uv_async_t m_timer_async;
uv_timer_t m_timer;
// estimated timestamp of next timer firing
uint64_t   timer_next_firing;

NodeTclNotify::HandlerMap m_handlers;


#define log(msg) \
	uint64_t t1 = uv_hrtime(); \
	printf("(%p) (%ld) %s\n", (void *) uv_thread_self(), \
			(t1-t0)/1000000, msg );


// Store the requested callback and establish one or more QSocketNotifier objects to link the activity to it
void NodeTclNotify::CreateFileHandler(int fd, int mask, Tcl_FileProc* proc,
		ClientData clientData) {
	printf("(%p) CreateFileHandler\n",
			(void *) uv_thread_self());
	// find any existing handler and deactivate it
	HandlerMap::iterator old_handler_it = m_handlers.find(fd);
	if (old_handler_it != m_handlers.end()) {
		// schedule NodeTclFileHandler QObject for deletion (and disconnection), along with its QSocketNotifiers,
		// when control returns to event loop
		// http://doc.qt.io/qt-5/qobject.html#deleteLater
		/*
		 * Schedules this object for deletion. The object will be deleted
		 * when control returns to the event loop.
		 */
		//   old_handler_it->second->deleteLater();
		// remove from map
		m_handlers.erase(old_handler_it);
	}

	NodeTclFileHandler* hdlr = new NodeTclFileHandler(proc, clientData, mask);

	if (mask & TCL_READABLE) {
		// create read activity socket notifier as a child of the handler (so will be destroyed at the same time)

		//QSocketNotifier* rdntf = new QSocketNotifier(fd, QSocketNotifier::Read, hdlr);
		//QObject::connect(rdntf, SIGNAL(activated(int)), getInstance(), SLOT(readReady(int)));
	}
	if (mask & TCL_WRITABLE) {
		//QSocketNotifier* wrntf = new QSocketNotifier(fd, QSocketNotifier::Write, hdlr);
		//QObject::connect(wrntf, SIGNAL(activated(int)), getInstance(), SLOT(writeReady(int)));
	}
	if (mask & TCL_EXCEPTION) {
		// QSocketNotifier* exntf = new QSocketNotifier(fd, QSocketNotifier::Exception, hdlr);
		// QObject::connect(exntf, SIGNAL(activated(int)), getInstance(), SLOT(exception(int)));
	}

	m_handlers.insert(std::make_pair(fd, hdlr));

}

// remove the handler for the given file descriptor and cancel its notifications
void NodeTclNotify::DeleteFileHandler(int fd) {
	printf("(%p) DeleteFileHandler\n",
			(void *) uv_thread_self());
	NodeTclNotify::HandlerMap::iterator old_handler_it = m_handlers.find(fd);
	if (old_handler_it != m_handlers.end()) {
		//   old_handler_it->second->deleteLater();
		m_handlers.erase(old_handler_it);
	}
	// Note: Tcl seems to call this thing with invalid fd's sometimes.  I had a debug message for that,
	// but got tired of seeing it fire all the time.
}

// find and execute handler for the given file descriptor and activity
template<int TclActivityType> void perform_callback(int fd) {
	printf("perform_callback\n");
	// find the handler
	NodeTclNotify::HandlerMap::const_iterator handler_it = m_handlers.find(fd);
	// check that it was found
	if (handler_it == m_handlers.end()) {
		printf("could not find a registered file handler for fd=%d\n", fd);
		return;
	}

	// pass the request to the filehandler object for execution
	handler_it->second->perform_callback(TclActivityType, fd);

}
void NodeTclNotify::readReady(int fd) {
	printf("(%p) readReady\n", (void *) uv_thread_self());
	perform_callback<TCL_READABLE>(fd);
}
void NodeTclNotify::writeReady(int fd) {
	printf("(%p) writeReady\n", (void *) uv_thread_self());
	perform_callback<TCL_WRITABLE>(fd);
}
void NodeTclNotify::exception(int fd) {
	printf("(%p) exception\n", (void *) uv_thread_self());
	perform_callback<TCL_EXCEPTION>(fd);
}

// If events are available process them, and otherwise wait up to a specified interval for one to occur
int NodeTclNotify::WaitForEvent(Tcl_Time* timePtr) {
	printf("(%p) WaitForEvent(%p => %ld:%ld)\n",
			(void *) uv_thread_self(), timePtr, timePtr->sec, timePtr->usec);

	int timeout;
	if (timePtr) {
		timeout = timePtr->sec * 1000 + timePtr->usec / 1000;
		if (timeout == 0) {
			// timeout 0 means "do not block". There are no events, so return without processing
			return 0;
		} else {
			// there are no events now, but maybe there will be some after we sleep the specified interval
			SetTimer(timePtr);
		}
	}
	// block if necessary until we have some events
	//QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
	return 1;
}

/*
 * Tcl_SetTimer is invoked by Tcl_SetMaxBlockTime whenever the maximum blocking time
 * has been reduced. Tcl_SetTimer should arrange for the external event loop to invoke
 * Tcl_ServiceAll after the specified interval even if no events have occurred.
 * This interface is needed because Tcl_WaitForEvent is not invoked when there is an
 * external event loop.
 *
 * Practically speaking, for every 'after' there is a call to SetTimer().
 */

// 10 microsecs opportunity window for successive calls to SetTimer
#define TIMERDELTA 10

void NodeTclNotify::SetTimer(Tcl_Time* timePtr) {
	if (timePtr && ((timePtr->sec > 0) || (timePtr->usec > 0))) {
		// set new timer interval in **microseconds**
		uint64_t timeout = (timePtr->sec * 1000000) + timePtr->usec;
		printf("(%p) SetTimer(%p => %.6f sec)\n",
				(void *) uv_thread_self(), timePtr, (float)timeout/(float)1000000);
		uint64_t t1 = uv_hrtime(); // nanoseconds
		//
		if (abs(timer_next_firing - ((t1/1000) + timeout)) > TIMERDELTA) {
			//
			if (timer_next_firing > 0) {
				printf("(%p)\tstopping previous timer\n", (void *) uv_thread_self());
				uv_timer_stop(&m_timer);
			}
			printf("(%p) (%ld)\tsetting timer to fire in %ld millisec %ld %ld diff = %ld\n",
					(void *) uv_thread_self(),
					(t1-t0)/1000000,
					timeout/1000,
					timer_next_firing, (t1/1000) + timeout,
					timer_next_firing - ((t1/1000) + timeout));
			//
			timer_next_firing = (t1/1000) + timeout; // microsecs
			// libuv only caters for *millisecond* accuracy
			uint64_t to = timeout / 1000;
			uv_timer_start(&m_timer, &NodeTclNotify::handle_timer, to, to);
			//uv_async_send(&(m_timer_async));
		}
	}
}


void walk_cb(uv_handle_t* handle, void* arg) {
	printf("\t(type: %d) (active: %d) (haas ref:%d) : %p\n",
			handle->type, uv_is_active(handle),uv_has_ref(handle), handle);
}

int idlecount = 0;
// What to do after the requested interval passes - always Tcl_ServiceAll()
void NodeTclNotify::handle_timer(uv_timer_s* timer) {
	uint64_t t1 = uv_hrtime();
	printf("(%p) (%ld) handle_timer, entering Tcl...\n", (void *) uv_thread_self(),
			(t1-t0)/1000000 );
	Tcl_SetServiceMode(TCL_SERVICE_ALL);
	int x = Tcl_ServiceAll();
	uint64_t t2 = uv_hrtime();
	printf("(%p) (%ld)\tTcl serviced %d events in %d usec\n", uv_thread_self(),
			(t2-t0)/1000000, x, (t2-t1)/1000);
	//uv_loop_t*  dl = uv_default_loop();
	uv_handle_t* h = (uv_handle_t*) &m_timer_async;
	//printf("active handles:%d\n", 	dl->active_handles);
	//uv_walk(dl, &walk_cb, 0);
	idlecount = (x == 0) ? idlecount + 1 : 0;
	if (idlecount > 2 && !uv_is_closing(h)) {
		idlecount = 0;
		printf("\t CLOSING uv_acync_t handle...\n");
		uv_close(h, NULL);
		uv_timer_stop(timer);
	}
}

// (v8 thread): reset the UV timer
void NodeTclNotify::timer_async_callback(uv_async_t* handle) {
	printf("\n(%p) timer_async_callback(%p)\n", (void *) uv_thread_self(),
			handle);
//	resetTimer();
	//	uv_mutex_lock(&timer_mutex);
	// pop timePtr from queue / map / whatever
	//	uv_mutex_unlock(&timer_mutex);
}

// STUB METHODS

// we don't use the client data information for this notifier
// This could be helpful for multi-thread support, though. TBD
void* NodeTclNotify::InitNotifier() {
	printf("InitNotifier\n");
	return 0;
}

void NodeTclNotify::AlertNotifier(ClientData cd) {
	printf("AlertNotifier\n");
}

void NodeTclNotify::FinalizeNotifier(ClientData cd) {
	printf("FinalizeNotifier\n");
}

// Can't find any examples of how this should work.  Unix implementation is empty
void NodeTclNotify::ServiceModeHook(int p) {
	//printf("(%p) ServiceModeHook (%d)\n",(void *) uv_thread_self(), p);
}

// only one method for NodeTclFileHandler - executing the callback (with type check)
void NodeTclFileHandler::perform_callback(int type, int fd // only for debug - remove?
		) {
	printf("perform_callback\n");
	// check that the mask includes the actual activity we had
	if (!(m_mask & type)) {
		printf("signal type received (%d) for fd %d should not be active!",
				type, fd);
		return;
	}

	// execute proc with stored client data
	(*m_proc)(m_clientData, type);
}

// Tell Tcl to replace its default notifier with ours
void NodeTclNotify::setup() {
	printf("(%p) setup()\n", (void *) uv_thread_self());
	Tcl_NotifierProcs notifier;
	notifier.setTimerProc     = NodeTclNotify::SetTimer;
	notifier.waitForEventProc = NodeTclNotify::WaitForEvent;
	notifier.initNotifierProc = NodeTclNotify::InitNotifier;
	notifier.alertNotifierProc = NodeTclNotify::AlertNotifier;
	notifier.createFileHandlerProc = NodeTclNotify::CreateFileHandler;
	notifier.deleteFileHandlerProc = NodeTclNotify::DeleteFileHandler;
	notifier.finalizeNotifierProc = NodeTclNotify::FinalizeNotifier;
	notifier.serviceModeHookProc = NodeTclNotify::ServiceModeHook;
	Tcl_SetNotifier(&notifier);
	uv_timer_init(uv_default_loop(), &m_timer);
	// set up an uv async for timers
	uv_async_init(uv_default_loop(), &m_timer_async, &NodeTclNotify::timer_async_callback);
	//m_timer_timeout = 0;
	//uv_mutex_init(&m_timer_mutex);

}
