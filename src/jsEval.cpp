/*
 * jsEval.cpp
 *
 *  Created on: 25 Δεκ 2015
 *      Author: ekarak
 */


#include "jsEval.h"
#include "util.h"

using namespace v8;


// a class for wrapping the bindings of Tcl variables into V8 land
class TclVariableBinding {
public:
	Tcl_Interp*  m_interp;
	std::string  m_name;
	Tcl_Obj*     m_tclvar;
	Local<Value> m_v8val;

	// the variable binding is composed from 1) the Tcl interpreter and 2) the variable's name
	TclVariableBinding(Tcl_Interp* interp, Tcl_Obj* varName) {
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

	static void GenericReader(
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
	static void GenericWriter(
			Local<String> property,
		    Local<Value> value,
		    const PropertyCallbackInfo<void>& info
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
			std::string errmsg("GenericReader: Binding for ");
						errmsg.append(*String::Utf8Value(property));
						errmsg.append(" not found!");
			Nan::ThrowError(Nan::New<String>(errmsg).ToLocalChecked());
		}
	};
};




/*
 * Tcl proc to embed Javascript
 */
int jsEval(
	ClientData clientData,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[]
){
	v8log("TclBinding::jsEval (interp=%p)\n", interp);

	// Get handle to the V8 Isolate for this Tcl interpreter
	Isolate* isolate  = v8::Isolate::GetCurrent();
	assert(isolate != nullptr);

	// lock the Isolate for multi-threaded access (not reentrant on its own)
	Locker locker(isolate);

	// create a bindings map and store it in the current V8 Isolate
	TclVariableBindingsMap varbindings;
	isolate->SetData(0, &varbindings);

	// validate input params
	if ( (objc != 3) ) {
		std::string msg("usage: jsEval [list of tcl vars to add to v8 context] { javascript }");
		Tcl_SetObjResult(interp, Tcl_NewStringObj(msg.c_str(), msg.length()));
		return TCL_ERROR;
	}

	// get the argument list
	Tcl_Obj* arglist = objv[1];
	if ( (arglist->typePtr == NULL) || strcmp(arglist->typePtr->name, "list") ) {
		std::string msg("jsEval: 1st arg not a list");
		Tcl_SetObjResult(interp, Tcl_NewStringObj(msg.c_str(), msg.length()));
		return TCL_ERROR;
	}

	// get the JS snippet
	char* javascript = Tcl_GetString(objv[2]);
	if ( strlen(javascript) == 0 ) {
		std::string msg("jsEval: 2nd arg not a string");
		Tcl_SetObjResult(interp, Tcl_NewStringObj(msg.c_str(), msg.length()));
		return TCL_ERROR;
	}

	// Create stack-allocated isolate and handle scopes.
	Isolate::Scope isolate_scope(isolate);
	HandleScope    handle_scope(isolate);

	// new v8 global template
	Handle<ObjectTemplate> global_templ = ObjectTemplate::New(isolate);

	// Create and Enter a new context for compiling and running the script.
	Handle<Context> context = Context::New(isolate, NULL, global_templ);
	context->Enter();

	int arglistLength;
	Tcl_ListObjLength(interp, arglist, &arglistLength);
	v8log("arg list length == %d\n", arglistLength);

	for ( int i = 0; i < arglistLength; i++ ) {
		// get the variable NAME
		Tcl_Obj* varName;
		Tcl_ListObjIndex(interp, arglist, i, &varName);
		char* vn = Tcl_GetString(varName);
		v8log("binding %s (idx: %d) to V8\n", vn, i);
		// then create a binding  and store it
		varbindings[vn] = new TclVariableBinding(interp, varName);
		global_templ->SetNativeDataProperty(
				Nan::New<String>(vn).ToLocalChecked(),
				TclVariableBinding::GenericReader,
				TclVariableBinding::GenericWriter
			);
	}

	// Compile
	v8log("compiling jsEval'ed script\n");
	v8::MaybeLocal<v8::Script> jsSource = v8::Script::Compile(context, Nan::New<String>(javascript).ToLocalChecked());
	if (jsSource.IsEmpty()) {
		v8log("*** jsSource.IsEmpty()\n");
	} else {
		// Run
		v8log("running jsEval'ed script\n");
		Nan::TryCatch tc;
		Nan::MaybeLocal<v8::Value> retv = jsSource.ToLocalChecked()->Run();

		if ( tc.HasCaught() ) {
			// oops, exception raised from V8 while in JS land
			Local<Message> msg = tc.Message();
			std::string msgtext(*String::Utf8Value(msg->Get()));
			v8log("*** caught JS exception: %s\n", msgtext.c_str());
		}

		TclVariableBindingsMap::const_iterator it;
		for (it = varbindings.begin(); it != varbindings.end(); it++) {
			TclVariableBinding* vb = it->second;
			v8log("reverse mapping of %s (v8: %p) to Tcl...\n", it->first.c_str(), vb->m_v8val);
			Tcl_SetVar2Ex(vb->m_interp, it->first.c_str(), NULL, vb->m_tclvar, 0);
			delete vb;
		}
		varbindings.clear();
		if (!retv.IsEmpty()) {
			std::string res(*String::Utf8Value(retv.ToLocalChecked()));
			v8log("Result == %s\n", res.c_str());
			Tcl_Obj* tclres = Tcl_NewStringObj(res.c_str(), res.size());
			Tcl_SetObjResult(interp, tclres);
		}
	}
	context->Exit();

// TODO: error handling!
	return TCL_OK;
}
