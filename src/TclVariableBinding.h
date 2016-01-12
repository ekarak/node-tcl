/*
 * TclVariableBinding.h
 *
 *  Created on: 11 Ιαν 2016
 *      Author: ekarak
 */

#ifndef SRC_TCLVARIABLEBINDING_H_
#define SRC_TCLVARIABLEBINDING_H_

#include "nan.h"
#include "tcl.h"
#include "util.h"

using namespace v8;

// a class for wrapping the bindings of Tcl variables into Gorgoroth (V8 land)
class TclVariableBinding {
public:
	Tcl_Interp*  m_interp;
	std::string  m_name;
	Tcl_Obj*     m_tclvar;
	Local<Value> m_v8val;

	TclVariableBinding(Tcl_Interp* interp, Tcl_Obj* varName);
	static void GenericReader(
			Local<String> property,
			const PropertyCallbackInfo<Value> &info
		);
	static void GenericWriter(
			Local<String> property,
			Local<Value> value,
			const PropertyCallbackInfo<void> &info
	);
};

typedef std::map<std::string, TclVariableBinding*> TclVariableBindingsMap;

#endif /* SRC_TCLVARIABLEBINDING_H_ */
