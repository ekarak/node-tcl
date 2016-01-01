/*
 * jsEval.cpp
 *
 *  Created on: 25 Δεκ 2015
 *      Author: ekarak
 */


#include "jsEval.h"
#include "util.h"

using namespace v8;

TclVariableBindingsMap varbindings;

// a class for wrapping the bindings of Tcl variables into Gorgoroth (V8 land)
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
		printf("\t\tBIND: converting Tcl value to V8\n");
		m_v8val = TclToV8(m_interp, m_tclvar);

		varbindings[m_name] =  this;
	}
	static void GenericReader(
		Local< String > property,
		const PropertyCallbackInfo< Value > &info
	) {
		printf("Accessor Reader for %s called!\n", *String::Utf8Value(property));
		TclVariableBindingsMap::const_iterator x;
		x = varbindings.find(*String::Utf8Value(property));
		if (x != varbindings.end()) {
			if (!x->second->m_v8val.IsEmpty()) {
				// TODO: FIXME: unref/GC old value?
			}
			char* varval = Tcl_GetString(x->second->m_tclvar);
			printf("\t\tBIND: %s value=(%s)\n", x->second->m_name.c_str(), varval);
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
		printf("Accessor Writer for %s called!\n", *String::Utf8Value(property));
		TclVariableBindingsMap::const_iterator x;
		x = varbindings.find(*String::Utf8Value(property));
		if (x != varbindings.end()) {
			x->second->m_tclvar = V8ToTcl(x->second->m_interp, value);
			printf("\t\tBIND: converting V8 value to Tcl (%s)\n", Tcl_GetString(x->second->m_tclvar));
		} else {
			std::string errmsg("GenericReader: Binding for ");
						errmsg.append(*String::Utf8Value(property));
						errmsg.append(" not found!");
			Nan::ThrowError(Nan::New<String>(errmsg).ToLocalChecked());
		}
	};
};

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* data = AllocateUninitialized(length);
    return data == NULL ? data : memset(data, 0, length);
  }
  virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
  virtual void Free(void* data, size_t) { free(data); }
};


/*
 * proc to pollute Tcl with Javascript.
 */
int jsEval(ClientData clientData, // (JsProxyBinding*)
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const objv[])
{
	printf("(%p) TclBinding::jsEval (interp=%p)\n", (void *)uv_thread_self(), interp);

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

	Isolate* isolate  = v8::Isolate::GetCurrent();
	if (!isolate) {
		// Get a new instance of the V8 Engine (so conveniently called an 'Isolate')
		printf("(%p) TclBinding::jsEval (interp=%p) creating v8::Isolate\n", (void *)uv_thread_self(), interp);
		ArrayBufferAllocator  allocator;
		Isolate::CreateParams create_params;
		create_params.array_buffer_allocator = &allocator;
		isolate = Isolate::New(create_params);
	}
	isolate->Enter();
//    Isolate::Scope isolate_scope(isolate);

    // Create a stack-allocated handle scope.
    HandleScope handle_scope(isolate);

	// new v8 global template
	Handle<ObjectTemplate> global_templ  = ObjectTemplate::New(isolate);

	int arglistLength;
	Tcl_ListObjLength(interp, arglist, &arglistLength);
	printf("arg list length == %d\n", arglistLength);

	for ( int i = 0; i < arglistLength; i++ ) {
		// get the variable NAME
		Tcl_Obj* varName;
		Tcl_ListObjIndex(interp, arglist, i, &varName);
		char* vn = Tcl_GetString(varName);
		printf("\tbinding %s (idx: %d) to V8\n", vn, i);
		// then get its value
		TclVariableBinding* varbind = new TclVariableBinding(interp, varName);
		global_templ->SetAccessor(
				Nan::New<String>(vn).ToLocalChecked(),
				TclVariableBinding::GenericReader,
				TclVariableBinding::GenericWriter
			);
	}

	// Create a new context.
	Handle<Context> context = Context::New(isolate, NULL, global_templ);

	// Enter the created context for compiling and running the script.
	context->Enter();
	v8::Handle<v8::Script> jsSource = v8::Script::Compile(Nan::New<String>(javascript).ToLocalChecked());
	printf("before run\n");
	Nan::MaybeLocal<v8::Value> retv = jsSource->Run();
	printf("after run\n");
	TclVariableBindingsMap::const_iterator it;
	for (it = varbindings.begin(); it != varbindings.end(); it++) {
		TclVariableBinding* vb = it->second;
		printf("reverse mapping of %s to Tcl...\n", it->first.c_str());
		Tcl_SetVar2Ex(vb->m_interp, it->first.c_str(), NULL, vb->m_tclvar, 0);
		delete vb;
	}
	varbindings.clear();
	if (!retv.IsEmpty()) {
		std::string res(*String::Utf8Value(retv.ToLocalChecked()));
		printf("\t\tResult == %s\n", res.c_str());
		Tcl_Obj* tclres = Tcl_NewStringObj(res.c_str(), res.size());
		Tcl_SetObjResult(interp, tclres);
	}

	context->Exit();
	isolate->Exit();
	isolate->Dispose();

// TODO: error handling!
	return TCL_OK;
}
