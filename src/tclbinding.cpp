
#include "tclbinding.h"
#include "tclnotifier.h"
#include <cstring>

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
Nan::Persistent< Function > TclBinding::constructor;

TclBinding::TclBinding() {

#if defined(HAS_CXX11) && defined(HAS_TCL_THREADS)
	_tasks = nullptr;
#endif

	// initialise the root Tcl interpreter
	rootInterp = newTclInterp();
	v8log("new TclBinding %p\n", this);
}

TclBinding::~TclBinding() {

	// cleanup
#if defined(HAS_CXX11) && defined(HAS_TCL_THREADS)
	if ( _tasks ) {
		delete _tasks;
	}
#endif

	Tcl_DeleteInterp( rootInterp );

}

class MallocArrayBufferAllocator : public ArrayBuffer::Allocator {
  public:
    virtual void* Allocate(size_t length) {
        return calloc(length,1);
    }
    virtual void* AllocateUninitialized(size_t length) {
        return malloc(length);
    }
    // XXX we assume length is not needed
    virtual void Free(void*data, size_t length) {
        free(data);
    }
};


void TclBinding::init( Local< Object > exports ) {

	/*
	 * http://www.borisvanschooten.nl/blog/2014/06/23/typed-arrays-on-embedded-v8-2014-edition
	 */
	V8::SetArrayBufferAllocator(new MallocArrayBufferAllocator());

	// set up custom Tcl Event Loop
	NodeTclNotify::setup();

	// stack-allocated handle scope
	Nan::HandleScope scope;

	// prepare constructor template
	Local< FunctionTemplate > tpl = Nan::New< FunctionTemplate >( construct );
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


void TclBinding::construct( const Nan::FunctionCallbackInfo< Value > &info ) {

	if (! info.IsConstructCall() ) {

		// invoked as `TclBinding(...)`, convert to a constructor call
		const int argc = 1;
		Local< Value > argv[ argc ] = { info[0] };
		Local< Function > c = Nan::New< Function >( constructor );
		return info.GetReturnValue().Set( c->NewInstance( argc, argv ) );

	}

	TclBinding * obj = new TclBinding();
	obj->Wrap( info.This() );
	info.GetReturnValue().Set( info.This() );

}


void TclBinding::cmd( const Nan::FunctionCallbackInfo< Value > &info ) {

	Isolate* isolate = Nan::GetCurrentContext()->GetIsolate();
	// lock the Isolate for multi-threaded access (not reentrant on its own)
	Locker 			locker(isolate);
	Isolate::Scope 	isolate_scope(isolate);
	HandleScope 	handle_scope(isolate);

	// new v8 global template
	//global_templ = ObjectTemplate::New(isolate);

	// Create and Enter a new context for compiling and running scripts.
	//context = Context::New(isolate, NULL, global_templ);

	//node = node::CreateEnvironment(isolate, loop, context, 0, nullptr,0, nullptr);
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

	Nan::Callback * callback = new Nan::Callback( info[1].As< Function >() );

#ifdef HAS_TCL_THREADS
	// schedule an async task
	Nan::Utf8String cmd( info[0] );
	Nan::AsyncQueueWorker( new TclWorker( *cmd, callback ) );
#else
	Local< Value > argv[] = {
			Nan::Error( Nan::New< String >( MSG_NO_TCL_THREADS ).ToLocalChecked() )
	};
	callback->Call( 1, argv );
#endif

	info.GetReturnValue().Set( Nan::Undefined() );

}

/*
 * Run a Tcl script synchronously. This is obviously NOT the recommended way
 * in the Node.JS way of things.
 * TODO: set up a libuv timeout to guard against badly performing Tcl scripts.
 */
void TclBinding::cmdSync( const Nan::FunctionCallbackInfo< Value > &info ) {
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
	int code = Tcl_EvalEx( binding->rootInterp, *cmd, -1, 0 );

	// check for errors
	if ( code == TCL_ERROR ) {

		handleTclError(binding->rootInterp, code);
		return;
	}

	// grab the result
	Tcl_Obj * result = Tcl_GetObjResult( binding->rootInterp );

	// return result as a string
	const char * str_result = Tcl_GetString( result );
	Local< String > r_string = Nan::New< String >( str_result ).ToLocalChecked();

	info.GetReturnValue().Set( r_string );

}


void TclBinding::queue( const Nan::FunctionCallbackInfo< Value > &info ) {

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


	Nan::Callback * callback = new Nan::Callback( info[1].As< Function >() );

#if defined(HAS_CXX11) && defined(HAS_TCL_THREADS)
	TclBinding * binding = ObjectWrap::Unwrap< TclBinding >( info.Holder() );

	if ( binding->_tasks == nullptr ) {
		binding->_tasks = new TaskRunner();
	}

	// queue the task
	Nan::Utf8String cmd( info[0] );
	binding->_tasks->queue( * cmd, callback );
#else
	Local< Value > argv[] = {
			Nan::Error( Nan::New< String >( MSG_NO_THREAD_SUPPORT ).ToLocalChecked() )
	};
	callback->Call( 1, argv );
#endif

	info.GetReturnValue().Set( Nan::Undefined() );

}


void TclBinding::toArray( const Nan::FunctionCallbackInfo< Value > &info ) {

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
	if ( Tcl_ListObjGetElements( binding->rootInterp, obj, &objc, &objv ) == TCL_OK ) {

		Local< Array > r_array = Nan::New< Array >( objc );

		for ( int i = 0; i < objc; i++ ) {
			r_array->Set( i, Nan::New< String >( Tcl_GetString( objv[i] ) ).ToLocalChecked() );
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
void TclBinding::expose( const Nan::FunctionCallbackInfo< Value > &info ) {

	v8log("TclBinding::expose\n");

	// validate input params
	if ( (info.Length() != 2) || (!info[0]->IsString()) || (!info[1]->IsFunction()) ) {
		return Nan::ThrowTypeError( "Usage: expose(name, function)" );
	}

	// get Tcl binding
	TclBinding * binding = ObjectWrap::Unwrap< TclBinding >( info.Holder() );

	// get handle to JS function and its Tcl name
	std::string  cmdname = (*String::Utf8Value( info[0]->ToString() ));

	// create a copyable persistent handle to this function
	Persistent<Function, CopyablePersistentTraits<Function>> pfun(
		Nan::GetCurrentContext()->GetIsolate(), info[1].As<Function>());

	// add the exposed command to the root Tcl interpreter
	innerExpose(
		binding->rootInterp,
		cmdname.c_str(),
		new JsProxyBinding(cmdname, &pfun)
	);

}
