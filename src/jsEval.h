/*
 * jsEval.h
 *
 *  Created on: 25 Δεκ 2015
 *      Author: ekarak
 */

#ifndef SRC_JSEVAL_H_
#define SRC_JSEVAL_H_

#include "tcl.h"
#include "nan.h"
#include "tclbinding.h"

// a class for wrapping the bindings of Tcl variables into Gorgoroth (V8 land)
class TclVariableBinding;
typedef std::map<std::string, TclVariableBinding*> TclVariableBindingsMap;
TclVariableBindingsMap varbindings;
#endif /* SRC_JSEVAL_H_ */
