
#include "tclworker.h"
#include "jsEval.h"
#include "util.h"

TclWorker::TclWorker( const char * cmd, Nan::Callback * callback )
	: Nan::AsyncWorker( callback ), _cmd( cmd ) {
	v8log("new async TclWorker\n");
}


TclWorker::~TclWorker() {

}


void TclWorker::HandleOKCallback() {
	v8log("TclWorker::HandleOKCallback, callback=%lp\n", (void*) callback);
	// stack-allocated handle scope
	Nan::HandleScope scope;

	v8::Local< v8::Value > argv[] = {
		Nan::Null(),
		Nan::New< v8::String >( _result ).ToLocalChecked()
	};

  callback->Call( Nan::New(ctx_obj), 2, argv, resource );
}

void TclWorker::HandleErrorCallback() {
	v8log("TclWorker::HandleErrorCallback, callback=%lp\n", (void*) callback);
	// stack-allocated handle scope
	Nan::HandleScope scope;

	v8::Local< v8::Value > argv[] = {
		Nan::New< v8::String >( _result ).ToLocalChecked(),
		Nan::Null()
	};

  callback->Call( Nan::New(ctx_obj), 2, argv, resource );
}


void TclWorker::Execute() {
	v8log("TclWorker::Execute() creating new v8::Isolate\n");
	_isolate = newV8Isolate();

	// initialise a new Tcl interpreter for the thread
	Tcl_Interp * interp = newTclInterp();

	if ( TCL_OK == Tcl_Init( interp ) ) {

		// evaluate command
		int code = Tcl_EvalEx( interp, _cmd.c_str(), -1, TCL_EVAL_DIRECT );

		if ( code == TCL_ERROR ) {
			SetErrorMessage( Tcl_GetStringResult( interp ) );
		} else {
			_result = std::string( Tcl_GetStringResult( interp ) );
		}

	} else {
		SetErrorMessage( "Failed to initialise Tcl interpreter" );
	}

	// cleanup
	Tcl_DeleteInterp( interp );

	_isolate->Exit();
	_isolate->Dispose();
}
