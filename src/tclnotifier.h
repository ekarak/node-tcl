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


   typedef std::map<int, NodeTclFileHandler*> HandlerMap;
   //template<int TclActivityType> void perform_callback(int fd);

   // the key Tcl Notifier functions
   void  SetTimer(Tcl_Time* timePtr);
   int   WaitForEvent(Tcl_Time* timePtr);
   void  CreateFileHandler(int fd, int mask, Tcl_FileProc* proc, ClientData clientData);
   void  DeleteFileHandler(int fd);
   void* InitNotifier();
   void  FinalizeNotifier(ClientData clientData);
   void  AlertNotifier(ClientData clientData);
   void  ServiceModeHook(int mode);

   void setup();
   void handle_timer(uv_timer_s* t);
   void timer_async_callback(uv_async_t* handle);

    void readReady(int fd);
    void writeReady(int fd);
    void exception(int fd);
    void resetTimer(uint64_t newtimeout);
}

#endif /* SRC_TCLNOTIFIER_H_ */
