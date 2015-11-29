/*
 * expose.h
 *
 *  Created on: 28 Νοε 2015
 *      Author: ekarak
 */

#ifndef SRC_EXPOSE_H_
#define SRC_EXPOSE_H_

#include <nan.h>
#include <tcl.h>
#include <map>

using namespace v8;

typedef struct JsProxyBinding {
	std::string   cmdname;
	Nan::Callback *jsFunc;
	// perhaps argument info, fetched from jsFunc's prototype?;
	JsProxyBinding(std::string s, Nan::Callback* cb) : cmdname(s), jsFunc(cb) {}
} JsProxyBinding;

// map of Javascript functions available to TclBinding
static std::map<std::string, JsProxyBinding*> _jsExports;

// dispatcher proc called by the Tcl interpreter when invoking an exposed JS function
int objCmdProcDispatcher(
				ClientData clientData, // (JsProxyBinding*)
				Tcl_Interp *interp,
				int objc,
				Tcl_Obj *const objv[]
);

// disposer
void objCmdDeleteProcDispatcher(ClientData clientData);

#endif /* SRC_EXPOSE_H_ */
