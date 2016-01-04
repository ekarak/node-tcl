/*
 * util.h
 *
 *  Created on: 23 Δεκ 2015
 *      Author: ekarak
 */

#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include <map>

#include "nan.h"
#include "tcl.h"

//
Tcl_Interp* newTclInterp();

// convert any Tcl value into a v8 value
v8::Local<v8::Value> TclToV8(Tcl_Interp*, Tcl_Obj*);

// convert any v8 Value to its Tcl equivalent
Tcl_Obj* V8ToTcl(Tcl_Interp* interp, v8::Value* v8v);
Tcl_Obj* V8ToTcl(Tcl_Interp* interp, v8::Local<v8::Value> v8v);

v8::Isolate* newV8Isolate();

#endif /* SRC_UTIL_H_ */
