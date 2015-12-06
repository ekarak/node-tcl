/*
 * Shamelessly copied from:
 *  https://github.com/jefftrull/EDASkel/tree/master/tclint
 */

#include "tclnotifier.h"

using namespace NodeTclNotify;

// initialize static data storage
NodeTclNotifier* NodeTclNotifier::m_notifier = 0;
uv_async_t timer_async;
uv_timer_t timer;
uv_mutex_t timer_mutex;

// What to do after the requested interval passes - always Tcl_ServiceAll()
void handle_timer2(uv_timer_s* t) {
  printf("(%lp) handle_timer2\n", uv_thread_self());
  //Tcl_ServiceAll();
  Tcl_DoOneEvent(TCL_ALL_EVENTS);
}

// (v8 thread):
void timer_async_callback(uv_async_t* handle) {
	printf("(%lp) timer_async_callback(%p)\n", uv_thread_self(), handle);
	uv_timer_start(&timer, &handle_timer2, 1000, 0);
}

// xtor/dtor for singleton class.  Called when getInstance() is first called
NodeTclNotifier::NodeTclNotifier() {
	printf("(%lp) NodeTclNotifier::NodeTclNotifier()\n", uv_thread_self());
	int err1 = uv_timer_init(uv_default_loop(), &timer);
	int err2 = uv_mutex_init(&timer_mutex);

/*  m_timer = new QTimer(this);
  m_timer->setSingleShot(true);
  QObject::connect(m_timer, SIGNAL(timeout()),
		   this, SLOT(handle_timer()));*/
}
NodeTclNotifier::~NodeTclNotifier() {
	printf("(%lp) NodeTclNotifier::~NodeTclNotifier()\n", uv_thread_self());
	uv_mutex_destroy(&timer_mutex);
}

// Tell Tcl to replace its default notifier with ours
void NodeTclNotifier::setup() {
	printf("(%lp) NodeTclNotifier::setup()\n", uv_thread_self());
  Tcl_NotifierProcs notifier;
  notifier.createFileHandlerProc = CreateFileHandler;
  notifier.deleteFileHandlerProc = DeleteFileHandler;
  notifier.setTimerProc = SetTimer;
  notifier.waitForEventProc = WaitForEvent;
  notifier.initNotifierProc = InitNotifier;
  notifier.finalizeNotifierProc = FinalizeNotifier;
  notifier.alertNotifierProc = AlertNotifier;
  notifier.serviceModeHookProc = ServiceModeHook;
  Tcl_SetNotifier(&notifier);
  //
  uv_async_init(uv_default_loop(), &timer_async, &timer_async_callback);
}

// Store the requested callback and establish one or more QSocketNotifier objects to link the activity to it
void NodeTclNotifier::CreateFileHandler(int fd, int mask, Tcl_FileProc* proc, ClientData clientData) {
	printf("(%lp) NodeTclNotifier::CreateFileHandler\n", uv_thread_self());
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
  printf("(%lp) NodeTclNotifier::DeleteFileHandler\n", uv_thread_self());
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
void NodeTclNotifier::readReady(int fd) { perform_callback<TCL_READABLE>(fd); }
void NodeTclNotifier::writeReady(int fd) { perform_callback<TCL_WRITABLE>(fd); }
void NodeTclNotifier::exception(int fd) { perform_callback<TCL_EXCEPTION>(fd); }





// (Tcl thread): inform V8 that a new timer needs to be set (if its a new timer!)
void NodeTclNotifier::SetTimer(Tcl_Time* timePtr) {
	printf("(%lp) NodeTclNotifier::SetTimer(%p => %d:%d)\n", uv_thread_self(), timePtr, timePtr->sec, timePtr->usec);

	uv_mutex_lock(&timer_mutex);
	// add timePtr to queue / map / whatever
	uv_mutex_unlock(&timer_mutex);

	uv_async_send(&timer_async);

	//uv_timer_init(uv_default_loop(), &timer_req);
	//uv_timer_start(&timer_req, &handle_timer2, 5000, 0);
	//getInstance()->m_timer.
/*  if (getInstance()->m_timer->isActive()) {
    getInstance()->m_timer->stop();
  }
  if (timePtr) {
    getInstance()->m_timer->start(timePtr->sec * 1000 + timePtr->usec / 1000);
  }
*/
}

// If events are available process them, and otherwise wait up to a specified interval for one to occur
int NodeTclNotifier::WaitForEvent(Tcl_Time* timePtr) {
  printf("(%lp) NodeTclNotifier::WaitForEvent(%p => %d:%d)\n", uv_thread_self(), timePtr, timePtr->sec, timePtr->usec);

  // following tclXtNotify.c here.  Hope the analogies hold.
  int timeout;
  if (timePtr) {
    timeout = timePtr->sec * 1000 + timePtr->usec / 1000;
    if (timeout == 0) {
      /*if (!QCoreApplication::hasPendingEvents()) {
    	  // timeout 0 means "do not block". There are no events, so return without processing
    	  return 0;
      }*/
    } else {
      // there are no events now, but maybe there will be some after we sleep the specified interval
      SetTimer(timePtr);
    }
  }
  // block if necessary until we have some events
  //QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
  return 1;
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
void NodeTclNotifier::FinalizeNotifier(ClientData) {}
void NodeTclNotifier::AlertNotifier(ClientData) {
	printf("NodeTclNotifier::AlertNotifier\n");
}

// Can't find any examples of how this should work.  Unix implementation is empty
void NodeTclNotifier::ServiceModeHook(int) {
	printf("NodeTclNotifier::ServiceModeHook\n");
}



// only one method for NodeTclFileHandler - executing the callback (with type check)
void NodeTclFileHandler::perform_callback(int type,
					int fd      // only for debug - remove?
					) {
	printf("NodeTclNotifier::perform_callback\n");
  // check that the mask includes the actual activity we had
  if (!(m_mask & type)) {
    printf("signal type received (%d) for fd %d should not be active!", type, fd);
    return;
  }

  // execute proc with stored client data
  (*m_proc)(m_clientData, type);
}
