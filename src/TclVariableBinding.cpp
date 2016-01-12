/*
 * TclVariableBinding.cpp
 *
 *  Created on: 11 Ιαν 2016
 *      Author: ekarak
 */

#include <map>
#include "TclVariableBinding.h"
#include "node.h"
#include "tcl.h"

using namespace v8;

// a class for wrapping the bindings of Tcl variables into V8 land
// the variable binding is composed from 1) the Tcl interpreter and 2) the variable's name
TclVariableBinding::TclVariableBinding(Tcl_Interp* interp, Tcl_Obj* varName) {
	m_interp = interp;
	m_name   = Tcl_GetString(varName);
	// store a pointer to the actual Tcl variable (not its name)
	m_tclvar = Tcl_ObjGetVar2(interp, varName, NULL, 0);
	m_v8val = TclToV8(m_interp, m_tclvar);
	// add the newly bound variable to V8's global template
	Nan::GetCurrentContext()->Global()->Set(
			Nan::New<String>(m_name).ToLocalChecked(),
			m_v8val
	);
}

void TclVariableBinding::GenericReader(
	Local< String > property,
	const PropertyCallbackInfo< Value > &info
) {
	v8log("Accessor Reader for %s called!\n", *String::Utf8Value(property));
	// V8 appears to have 4 data slots in each Isolate
	TclVariableBindingsMap* m_map = (TclVariableBindingsMap*) Isolate::GetCurrent()->GetData(0);
	TclVariableBindingsMap::const_iterator x;
	x = m_map->find(*String::Utf8Value(property));
	if (x != m_map->end()) {
		if (!x->second->m_v8val.IsEmpty()) {
			v8log("*** TODO: FIXME: unref/GC old value?\n");
		}
		char* varval = Tcl_GetString(x->second->m_tclvar);
		v8log("\tBIND: %s value=(%s)\n", x->second->m_name.c_str(), varval);
		info.GetReturnValue().Set(x->second->m_v8val);
	} else {
		std::string errmsg("GenericReader: Binding for ");
					errmsg.append(*String::Utf8Value(property));
					errmsg.append(" not found!");
		Nan::ThrowError(Nan::New<String>(errmsg).ToLocalChecked());
	}
};

// V8 updates property => map to Tcl object
void TclVariableBinding::GenericWriter(
		Local<String> property,
		Local<Value> value,
		const PropertyCallbackInfo<void> &info
) {
	v8log("Accessor Writer for %s called!\n", *String::Utf8Value(property));

	// V8 appears to have 4 data slots in each Isolate
	TclVariableBindingsMap* m_map = (TclVariableBindingsMap*) Isolate::GetCurrent()->GetData(0);
	TclVariableBindingsMap::const_iterator x;
	x = m_map->find(*String::Utf8Value(property));
	if (x != m_map->end()) {
		x->second->m_tclvar = V8ToTcl(x->second->m_interp, value);
		v8log("\tBIND: converting V8 value to Tcl (%s)\n", Tcl_GetString(x->second->m_tclvar));
	} else {
		std::string errmsg("GenericWriter: Binding for ");
					errmsg.append(*String::Utf8Value(property));
					errmsg.append(" not found!");
		Nan::ThrowError(Nan::New<String>(errmsg).ToLocalChecked());
	}
};



