/*
 * Shamelessly copied from:
 *  https://github.com/jefftrull/EDASkel/tree/master/tclint
 */

#include "tclnotifier.h"

using namespace NodeTclNotify;

// initialize static data storage
NodeTclNotifier* NodeTclNotifier::m_notifier = 0;

// xtor/dtor for singleton class.  Called when getInstance() is first called
NodeTclNotifier::NodeTclNotifier() {
	printf("(%p) NodeTclNotifier::NodeTclNotifier()\n",
			(void *) uv_thread_self());
	uv_timer_init(uv_default_loop(), &m_timer);
	// set up an uv async for timers
	uv_async_init(uv_default_loop(), &m_timer_async, &timer_async_callback);
	m_timer_timeout = 0;
	//uv_mutex_init(&m_timer_mutex);
}
NodeTclNotifier::~NodeTclNotifier() {
	printf("(%p) NodeTclNotifier::~NodeTclNotifier()\n",
			(void *) uv_thread_self());
	//uv_mutex_destroy(&timer_mutex);
}

// Tell Tcl to replace its default notifier with ours
void NodeTclNotifier::setup() {
	printf("(%p) NodeTclNotifier::setup()\n", (void *) uv_thread_self());
	Tcl_NotifierProcs notifier;
	notifier.setTimerProc = SetTimer;
	notifier.waitForEventProc = WaitForEvent;
	notifier.initNotifierProc = InitNotifier;
	notifier.alertNotifierProc = AlertNotifier;
	notifier.createFileHandlerProc = CreateFileHandler;
	notifier.deleteFileHandlerProc = DeleteFileHandler;
	notifier.finalizeNotifierProc = FinalizeNotifier;
	notifier.serviceModeHookProc = ServiceModeHook;
	Tcl_SetNotifier(&notifier);
}

// Store the requested callback and establish one or more QSocketNotifier objects to link the activity to it
void NodeTclNotifier::CreateFileHandler(int fd, int mask, Tcl_FileProc* proc,
		ClientData clientData) {
	printf("(%p) NodeTclNotifier::CreateFileHandler\n",
			(void *) uv_thread_self());
	// find any existing handler and deactivate it
	HandlerMap::iterator old_handler_it = getInstance()->m_handlers.find(fd);
	if (old_handler_it != getInstance()->m_handlers.end()) {
		// schedule NodeTclFileHandler QObject for deletion (and disconnection), along with its QSocketNotifiers,
		// when control returns to event loop
		// http://doc.qt.io/qt-5/qobject.html#deleteLater
		/*
		 * Schedules this object for deletion. The object will be deleted
		 * when control returns to the event loop.
		 */
		//   old_handler_it->second->deleteLater();
		// remove from map
		getInstance()->m_handlers.erase(old_handler_it);
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

	getInstance()->m_handlers.insert(std::make_pair(fd, hdlr));

}

// remove the handler for the given file descriptor and cancel its notifications
void NodeTclNotifier::DeleteFileHandler(int fd) {
	printf("(%p) NodeTclNotifier::DeleteFileHandler\n",
			(void *) uv_thread_self());
	HandlerMap::iterator old_handler_it = getInstance()->m_handlers.find(fd);
	if (old_handler_it != getInstance()->m_handlers.end()) {
		//   old_handler_it->second->deleteLater();
		getInstance()->m_handlers.erase(old_handler_it);
	}
	// Note: Tcl seems to call this thing with invalid fd's sometimes.  I had a debug message for that,
	// but got tired of seeing it fire all the time.
}

// find and execute handler for the given file descriptor and activity
template<int TclActivityType> void NodeTclNotifier::perform_callback(int fd) {
	printf("NodeTclNotifier::perform_callback\n");
	// find the handler
	HandlerMap::const_iterator handler_it = getInstance()->m_handlers.find(fd);
	// check that it was found
	if (handler_it == getInstance()->m_handlers.end()) {
		printf("could not find a registered file handler for fd=%d\n", fd);
		return;
	}

	// pass the request to the filehandler object for execution
	handler_it->second->perform_callback(TclActivityType, fd);

}
void NodeTclNotifier::readReady(int fd) {
	printf("(%p) NodeTclNotifier::readReady\n", (void *) uv_thread_self());
	perform_callback<TCL_READABLE>(fd);
}
void NodeTclNotifier::writeReady(int fd) {
	printf("(%p) NodeTclNotifier::writeReady\n", (void *) uv_thread_self());
	perform_callback<TCL_WRITABLE>(fd);
}
void NodeTclNotifier::exception(int fd) {
	printf("(%p) NodeTclNotifier::exception\n", (void *) uv_thread_self());
	perform_callback<TCL_EXCEPTION>(fd);
}

// If events are available process them, and otherwise wait up to a specified interval for one to occur
int NodeTclNotifier::WaitForEvent(Tcl_Time* timePtr) {
	printf("(%p) NodeTclNotifier::WaitForEvent(%p => %ld:%ld)\n",
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
 * external event loop
 */
void NodeTclNotifier::SetTimer(Tcl_Time* timePtr) {
	if (timePtr) {
		printf("(%p) NodeTclNotifier::SetTimer(%p => %ld:%ld)\n",
				(void *) uv_thread_self(), timePtr, timePtr->sec, timePtr->usec);

		// set new timer interval in **microseconds**
		uint64_t newtimeout = (timePtr->sec * 1000000) + timePtr->usec;

		if (getInstance()->m_timer.timeout - newtimeout > 0.1) {
			printf("(%p) NodeTclNotifier::SetTimer, resetting timer to %ld \n",
					(void *) uv_thread_self(), newtimeout);
			getInstance()->m_timer_timeout = newtimeout;
			uv_async_send(&(getInstance()->m_timer_async));
		}
	}
}

void NodeTclNotifier::resetTimer() {
	// libuv only caters for *millisecond* accuracy
	uint64_t time = m_timer_timeout / 1000;
	uv_timer_start(&m_timer, &handle_timer, time, time);
}

// What to do after the requested interval passes - always Tcl_ServiceAll()
void handle_timer(uv_timer_s* timer) {
	printf("(%p) handle_timer, entering Tcl...\n", (void *) uv_thread_self());
	Tcl_SetServiceMode(TCL_SERVICE_ALL);
	int x = Tcl_ServiceAll();
	printf("(%p) handle_timer, Tcl serviced %d events...\n", uv_thread_self(),
			x);
}

// (v8 thread):
void timer_async_callback(uv_async_t* handle) {
	printf("(%p) timer_async_callback(%p)\n", (void *) uv_thread_self(),
			handle);
	NodeTclNotifier::getInstance()->resetTimer();
	//	uv_mutex_lock(&timer_mutex);
	// pop timePtr from queue / map / whatever
	//	uv_mutex_unlock(&timer_mutex);
}

// Singleton class implementation trick
NodeTclNotifier* NodeTclNotifier::getInstance() {
	if (!m_notifier) {
		m_notifier = new NodeTclNotifier();
	}
	return m_notifier;
}

// STUB METHODS

// we don't use the client data information for this notifier
// This could be helpful for multi-thread support, though. TBD
void* NodeTclNotifier::InitNotifier() {
	printf("NodeTclNotifier::InitNotifier\n");
	return 0;
}

void NodeTclNotifier::AlertNotifier(ClientData cd) {
	printf("NodeTclNotifier::AlertNotifier\n");
}

void NodeTclNotifier::FinalizeNotifier(ClientData cd) {
	printf("NodeTclNotifier::FinalizeNotifier\n");
}

// Can't find any examples of how this should work.  Unix implementation is empty
void NodeTclNotifier::ServiceModeHook(int p) {
	printf("(%p) NodeTclNotifier::ServiceModeHook (%d)\n",
			(void *) uv_thread_self(), p);
}

// only one method for NodeTclFileHandler - executing the callback (with type check)
void NodeTclFileHandler::perform_callback(int type, int fd // only for debug - remove?
		) {
	printf("NodeTclNotifier::perform_callback\n");
	// check that the mask includes the actual activity we had
	if (!(m_mask & type)) {
		printf("signal type received (%d) for fd %d should not be active!",
				type, fd);
		return;
	}

	// execute proc with stored client data
	(*m_proc)(m_clientData, type);
}
