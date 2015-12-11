/*
 * tclnotifier.h
 *
 *  Created on: 4 Δεκ 2015
 *      Author: ekarak
 */

#ifndef SRC_TCLNOTIFIER_H_
#define SRC_TCLNOTIFIER_H_


#include <map>
#include <tcl.h>
#include "nan.h"
#include "uv.h"

namespace NodeTclNotify {

  // Tcl has a concept of a "Notifier", a sort of API through which one can retarget its event processing
  // and integrate with other systems.  This is done by implementing a set of 8 functions for it to call
  // as needed.  This is described in more detail in the Notifier man page supplied with the Tcl distribution,
  // but I found that without the examples tclUnixNotfy.c and tclXtNotify.c (included in the distribution)
  // I could not have understood what was required.

  // I've implemented a Notifier in C++ through a singleton class with static methods matching the Notifier
  // API and non-static methods, where needed, to access the Qt signal/slot mechanism

  class NodeTclFileHandler;   // forward ref
  // Singleton class.  Static methods to work with Tcl callbacks, non-static for Qt signal/slot mechanism
  class NodeTclNotifier {

    public:
    typedef std::map<int, NodeTclFileHandler*> HandlerMap;

    // the key Tcl Notifier functions
    static void  SetTimer(Tcl_Time* timePtr);
    static int   WaitForEvent(Tcl_Time* timePtr);
    static void  CreateFileHandler(int fd, int mask, Tcl_FileProc* proc, ClientData clientData);
    static void  DeleteFileHandler(int fd);
    static void* InitNotifier();
    static void  FinalizeNotifier(ClientData clientData);
    static void  AlertNotifier(ClientData clientData);
    static void  ServiceModeHook(int mode);

    static NodeTclNotifier* getInstance();

    static void setup();

    void readReady(int fd);
    void writeReady(int fd);
    void exception(int fd);
    void resetTimer();

  private:
    NodeTclNotifier();    // singleton
    ~NodeTclNotifier();

    template<int TclActivityType> static void perform_callback(int fd);
    HandlerMap m_handlers;

    uv_async_t m_timer_async;
    uv_timer_t m_timer;
    uint64_t   m_timer_timeout;

    static NodeTclNotifier* m_notifier;   // pointer to the single instance we allow

  };

  // NodeTclFileHandler objects aggregate the activity mask/callback function/client data for a given file descriptor
  // it will also "own" (in a Qt sense - object hierarchy) the QSocketNotifiers created for it
  // This way they will get destroyed when the parent does and I won't have to clean them up
  class NodeTclFileHandler {
  public:
    NodeTclFileHandler(Tcl_FileProc* proc, ClientData clientData, int mask) : m_proc(proc), m_clientData(clientData), m_mask(mask) {}
    void perform_callback(int type, int fd);
  private:
    Tcl_FileProc* m_proc;      // function to call
    ClientData m_clientData;   // extra data to supply
    int m_mask;                // types of activity supported
  };

}

void handle_timer(uv_timer_s* t);
void timer_async_callback(uv_async_t* handle);



#endif /* SRC_TCLNOTIFIER_H_ */
