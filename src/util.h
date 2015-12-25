/*
 * util.h
 *
 *  Created on: 23 Δεκ 2015
 *      Author: ekarak
 */

#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include "nan.h"
#include "tcl.h"

v8::Local<v8::Value> TclToV8(Tcl_Interp*, Tcl_Obj*);


#endif /* SRC_UTIL_H_ */
