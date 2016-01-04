
#include "tclworker.h"
#include "jsEval.h"
#include "util.h"

TclWorker::TclWorker( const char * cmd, Nan::Callback * callback )
	: Nan::AsyncWorker( callback ), _cmd( cmd ) {
		printf("(%p): new async TclWorker\n", (void*) uv_thread_self());
}


TclWorker::~TclWorker() {

}


void TclWorker::HandleOKCallback() {

	// stack-allocated handle scope
	Nan::HandleScope scope;

	v8::Local< v8::Value > argv[] = {
		Nan::Null(),
		Nan::New< v8::String >( _result ).ToLocalChecked()
	};

	callback->Call( 2, argv );
	return;

}


void TclWorker::Execute() {

	_isolate = newV8Isolate();
	_isolate->Enter();
	
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

}
