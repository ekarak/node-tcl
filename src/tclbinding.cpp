
#include "tclbinding.h"
#include "tclnotifier.h"
#include <cstring>
#include <iostream>
#include <functional>

#ifdef HAS_TCL_THREADS
#include "tclworker.h"
#endif

#define MSG_NO_TCL_THREADS       "Thread support disabled, please ensure Tcl is compiled with --enable-threads flag set"
#define MSG_NO_THREAD_SUPPORT    "Thread support disabled, check g++ version for c++11 and/or Tcl thread support"

#include "expose.h"
#include "util.h"

using namespace v8;

// initialise static vars
Nan::Persistent< v8::Function > TclBinding::constructor;

TclBinding::TclBinding() {

#if defined(HAS_CXX11) && defined(HAS_TCL_THREADS)
	_tasks = nullptr;
#endif

	// set up custom Tcl Event Loop
	NodeTclNotify::setup();

	// initialise Tcl interpreter
	_interp = Tcl_CreateInterp();
	if ( TCL_OK != Tcl_Init( _interp ) ) {
		Nan::ThrowError( "Failed to initialise Tcl interpreter" );
	}

	// add the generic jsEval hook in Tcl
	Tcl_CreateObjCommand(
		_interp,
		"jsEval",
		&TclBinding::jsEval,
		NULL, // clientData,
		NULL
	);

}

TclBinding::~TclBinding() {

	// cleanup
#if defined(HAS_CXX11) && defined(HAS_TCL_THREADS)
	if ( _tasks ) {
		delete _tasks;
	}
#endif

	Tcl_DeleteInterp( _interp );

}


static void gc_begin(GCType type, GCCallbackFlags flags){
  std::cout << "GC begins: " << type << std::endl;
}

static void gc_end(GCType type, GCCallbackFlags flags){
  std::cout << "GC ends: " << type << std::endl;
}

void TclBinding::init( v8::Local< v8::Object > exports ) {

	// ekarak: add GC hints
	v8::V8::AddGCPrologueCallback(&gc_begin);
	v8::V8::AddGCEpilogueCallback(&gc_end);

	// stack-allocated handle scope
	Nan::HandleScope scope;

	// prepare constructor template
	v8::Local< v8::FunctionTemplate > tpl = Nan::New< v8::FunctionTemplate >( construct );
	tpl->SetClassName( Nan::New( "TclBinding" ).ToLocalChecked() );
	tpl->InstanceTemplate()->SetInternalFieldCount( 1 );

	// prototypes
	Nan::SetPrototypeMethod( tpl, "cmd", cmd );
	Nan::SetPrototypeMethod( tpl, "cmdSync", cmdSync );
	Nan::SetPrototypeMethod( tpl, "queue", queue );
	Nan::SetPrototypeMethod( tpl, "toArray", toArray );
	Nan::SetPrototypeMethod( tpl, "expose", expose );

	constructor.Reset( tpl->GetFunction() );
	exports->Set( Nan::New( "TclBinding" ).ToLocalChecked(), tpl->GetFunction() );

}


void TclBinding::construct( const Nan::FunctionCallbackInfo< v8::Value > &info ) {

	if (! info.IsConstructCall() ) {

		// invoked as `TclBinding(...)`, convert to a constructor call
		const int argc = 1;
		v8::Local< v8::Value > argv[ argc ] = { info[0] };
		v8::Local< v8::Function > c = Nan::New< v8::Function >( constructor );
		return info.GetReturnValue().Set( c->NewInstance( argc, argv ) );

	}

	TclBinding * obj = new TclBinding();
	obj->Wrap( info.This() );
	info.GetReturnValue().Set( info.This() );

}


void TclBinding::cmd( const Nan::FunctionCallbackInfo< v8::Value > &info ) {

	// validate input params
	if ( info.Length() != 2 ) {
		return Nan::ThrowError( "Invalid number of arguments" );
	}

	if (! info[0]->IsString() ) {
		return Nan::ThrowTypeError( "Tcl command must be a string" );
	}

	if (! info[1]->IsFunction() ) {
		return Nan::ThrowTypeError( "Callback must be a function" );
	}


	Nan::Callback * callback = new Nan::Callback( info[1].As< v8::Function >() );

#ifdef HAS_TCL_THREADS
	// schedule an async task
	Nan::Utf8String cmd( info[0] );
	Nan::AsyncQueueWorker( new TclWorker( *cmd, callback ) );
#else
	v8::Local< v8::Value > argv[] = {
			Nan::Error( Nan::New< v8::String >( MSG_NO_TCL_THREADS ).ToLocalChecked() )
	};
	callback->Call( 1, argv );
#endif

	info.GetReturnValue().Set( Nan::Undefined() );

}


void TclBinding::cmdSync( const Nan::FunctionCallbackInfo< v8::Value > &info ) {

	// validate input params
	if ( info.Length() < 1 ) {
		return Nan::ThrowError( "Require a Tcl command to execute" );
	}

	if (! info[0]->IsString() ) {
		return Nan::ThrowTypeError( "Tcl command must be a string" );
	}

	TclBinding * binding = ObjectWrap::Unwrap< TclBinding >( info.Holder() );
	Nan::Utf8String cmd( info[0] );

	// evaluate command
	int code = Tcl_EvalEx( binding->_interp, *cmd, -1, 0 );

	// check for errors
	if ( code == TCL_ERROR ) {

		/*
		 * Tcl_GetReturnOptions retrieves the dictionary of return options from an interpreter
		 * following a script evaluation. Routines such as Tcl_Eval are called to evaluate a
		 * script in an interpreter. These routines return an integer completion code. These
		 * routines also leave in the interpreter both a result and a dictionary of return
		 * options generated by script evaluation. Just as Tcl_GetObjResult retrieves the
		 * result, Tcl_GetReturnOptions retrieves the dictionary of return options.
		 *
		 * The integer completion code should be passed as the code argument to
		 * Tcl_GetReturnOptions so that all required options will be present in the dictionary.
		 * Specifically, a code value of TCL_ERROR will ensure that entries for the keys
		 * -errorinfo, -errorcode, and -errorline will appear in the dictionary. Also, the
		 * entries for the keys -code and -level will be adjusted if necessary to agree with the
		 * value of code. The (Tcl_Obj *) returned by Tcl_GetReturnOptions points to an unshared
		 * Tcl_Obj with reference count of zero. The dictionary may be written to, either adding,
		 * removing, or overwriting any entries in it, without the need to check for a shared
		 * value. As with any Tcl_Obj with reference count of zero, it is up to the caller to
		 * arrange for its disposal with Tcl_DecrRefCount or to a reference to it via
		 * Tcl_IncrRefCount (or one of the many functions that call that, notably including
		 * Tcl_SetObjResult and Tcl_SetVar2Ex)
		 */
	    Tcl_Obj *options = Tcl_GetReturnOptions(binding->_interp, code);
	    Tcl_Obj *key = Tcl_NewStringObj("-errorinfo", -1);
	    Tcl_Obj *key2 = Tcl_NewStringObj("-errorcode", -1);
	    // -errorcode, and -errorline ?????
	    Tcl_Obj *stackTrace;
		Tcl_Obj *errorCode;
	    Tcl_IncrRefCount(key); Tcl_IncrRefCount(key2);
	    Tcl_DictObjGet(NULL, options, key, &stackTrace);
	    Tcl_DictObjGet(NULL, options, key2, &errorCode);
	    Tcl_DecrRefCount(key); Tcl_DecrRefCount(key2);
	    printf("Tcl stacktrace:\n\t%s\n", Tcl_GetString(stackTrace));
		printf("Tcl errorCode: %s\n", Tcl_GetString(errorCode));
	    // v8 error object
	    Local<Value> errobj = Nan::Error(Tcl_GetStringResult( binding->_interp ));
	    // TODO: how to add properties into errobj
	    // Nan::Set(errobj, Nan::New<String>("stack").ToLocalChecked(), Nan::New<String>(Tcl_GetString(stackTrace)).ToLocalChecked());
	    Tcl_DecrRefCount(options);

		// TODO: add columnNumber, lineNumber, fileName, stack, toSource() ?
		Nan::ThrowError( errobj );
		return;
	}

	// grab the result
	Tcl_Obj * result = Tcl_GetObjResult( binding->_interp );

	// return result as a string
	const char * str_result = Tcl_GetString( result );
	v8::Local< v8::String > r_string = Nan::New< v8::String >( str_result ).ToLocalChecked();

	info.GetReturnValue().Set( r_string );

}


void TclBinding::queue( const Nan::FunctionCallbackInfo< v8::Value > &info ) {

	// validate input params
	if ( info.Length() != 2 ) {
		return Nan::ThrowError( "Invalid number of arguments" );
	}

	if (! info[0]->IsString() ) {
		return Nan::ThrowTypeError( "Tcl command must be a string" );
	}

	if (! info[1]->IsFunction() ) {
		return Nan::ThrowTypeError( "Callback must be a function" );
	}


	Nan::Callback * callback = new Nan::Callback( info[1].As< v8::Function >() );

#if defined(HAS_CXX11) && defined(HAS_TCL_THREADS)
	TclBinding * binding = ObjectWrap::Unwrap< TclBinding >( info.Holder() );

	if ( binding->_tasks == nullptr ) {
		binding->_tasks = new TaskRunner();
	}

	// queue the task
	Nan::Utf8String cmd( info[0] );
	binding->_tasks->queue( * cmd, callback );
#else
	v8::Local< v8::Value > argv[] = {
			Nan::Error( Nan::New< v8::String >( MSG_NO_THREAD_SUPPORT ).ToLocalChecked() )
	};
	callback->Call( 1, argv );
#endif

	info.GetReturnValue().Set( Nan::Undefined() );

}


void TclBinding::toArray( const Nan::FunctionCallbackInfo< v8::Value > &info ) {

	// validate input params
	if ( info.Length() < 1 ) {
		return info.GetReturnValue().Set( Nan::Undefined() );
	}

	if (! info[0]->IsString() ) {
		return Nan::ThrowTypeError( "Input must be a string" );
	}

	TclBinding * binding = ObjectWrap::Unwrap< TclBinding >( info.Holder() );
	Nan::Utf8String str( info[0] );

	// create a Tcl string object
	Tcl_Obj * obj = Tcl_NewStringObj( *str, strlen( *str ) );

	int objc = 0;
	Tcl_Obj **objv;

	// attempt to parse as a Tcl list
	if ( Tcl_ListObjGetElements( binding->_interp, obj, &objc, &objv ) == TCL_OK ) {

		v8::Local< v8::Array > r_array = Nan::New< v8::Array >( objc );

		for ( int i = 0; i < objc; i++ ) {
			r_array->Set( i, Nan::New< v8::String >( Tcl_GetString( objv[i] ) ).ToLocalChecked() );
		}

		return info.GetReturnValue().Set( r_array );

	}

	// not a valid Tcl list
	info.GetReturnValue().Set( Nan::Null() );

}


/*
* Javascript: tcl.expose( 'name', <javascript closure> )
* exposes the JS function object to Tcl
*/
void TclBinding::expose( const Nan::FunctionCallbackInfo< v8::Value > &info ) {

	printf("(%p) TclBinding::expose\n", (void *)uv_thread_self());

	// validate input params
	if ( (info.Length() != 2) || (!info[0]->IsString()) || (!info[1]->IsFunction()) ) {
		return Nan::ThrowTypeError( "Usage: expose(name, function)" );
	}

	// get Tcl binding
	TclBinding * binding = ObjectWrap::Unwrap< TclBinding >( info.Holder() );

	// get handle to JS function and its Tcl name
	std::string  cmdname = (*String::Utf8Value( info[0]->ToString() ));

/*	printf("exposing %s: identityHash=%d to interpreter %p\n",
			cmdname.c_str(), fun->GetIdentityHash(), binding->_interp);
*/
	if (_jsExports.count(cmdname)) {
		printf("WARNING: expose() is overriding %s\n", cmdname.c_str());
	}

	// create a copyable persistent handle to this function
	Persistent<Function, CopyablePersistentTraits<Function>> pfun(
		Nan::GetCurrentContext()->GetIsolate(), info[1].As<Function>());

	// add the command
	JsProxyBinding* jsb = new JsProxyBinding(cmdname, &pfun);
	jsb->tclcmd = Tcl_CreateObjCommand(
		binding->_interp,
		cmdname.c_str(),
		&objCmdProcDispatcher,
		jsb, // clientData,
		&objCmdDeleteProcDispatcher
	);

	_jsExports[cmdname] = jsb;
}
