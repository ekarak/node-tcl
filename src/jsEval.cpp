/*
 * jsEval.cpp
 *
 *  Created on: 25 Δεκ 2015
 *      Author: ekarak
 */


#include "jsEval.h"
#include "util.h"
#include "NodeTclContext.h"
#include "TclVariableBinding.h"

using namespace v8;


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

	NodeTclContext* ntc = (NodeTclContext*) clientData;

	// Get handle to the V8 Isolate for this Tcl interpreter
	Isolate* isolate  = v8::Isolate::GetCurrent();
	assert(isolate != nullptr);
//	node::CreateEnvironment(isolate,
	// lock the Isolate for multi-threaded access (not reentrant on its own)
	//Locker locker(isolate);

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
	char* javascript;
	if (!(javascript = Tcl_GetString(objv[2])) ||
			strlen(javascript) == 0 ) {
		std::string msg("jsEval: 2nd arg not a string");
		Tcl_SetObjResult(interp, Tcl_NewStringObj(msg.c_str(), msg.length()));
		return TCL_ERROR;
	}

	// Create stack-allocated isolate and handle scopes.
	Isolate::Scope isolate_scope(isolate);
	HandleScope    handle_scope(isolate);

	int arglistLength;
	Tcl_ListObjLength(interp, arglist, &arglistLength);
	v8log("arg list length == %d\n", arglistLength);

	for ( int i = 0; i < arglistLength; i++ ) {
		// get the variable NAME
		Tcl_Obj* varName;
		Tcl_ListObjIndex(interp, arglist, i, &varName);
		char* vn = Tcl_GetString(varName);
		if ((vn != nullptr) && (strlen(vn) > 0)) {
			v8log("binding %s (idx: %d) to V8\n", vn, i);
			// then create a binding and store it
			varbindings[vn] = new TclVariableBinding(interp, varName);
			ntc->global_templ->SetNativeDataProperty(
				Nan::New<String>(vn).ToLocalChecked(),
				TclVariableBinding::GenericReader,
				TclVariableBinding::GenericWriter
			);
		}
	}

	// Compile
	v8log("compiling jsEval'ed script\n");
	v8::MaybeLocal<v8::Script> jsSource = v8::Script::Compile(ntc->context, Nan::New<String>(javascript).ToLocalChecked());
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
			return TCL_ERROR;
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
			Tcl_Obj* tclres = V8ToTcl(interp, retv.ToLocalChecked());
			v8log("jsEval: returning %p (%s: %s)\n", tclres, tclres->typePtr ? tclres->typePtr->name : "(untyped)", Tcl_GetString(tclres));
			Tcl_SetObjResult(interp, tclres);
		}
	}
	ntc->context->Exit();

// TODO: error handling!
	return TCL_OK;
}
