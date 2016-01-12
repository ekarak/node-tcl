
#include "tclworker.h"
#include "jsEval.h"
#include "util.h"
#include "NodeTclContext.h"

using namespace v8;

TclWorker::TclWorker( const char * cmd, Nan::Callback * callback )
	: Nan::AsyncWorker( callback ), _cmd( cmd ) {
	v8log("new async TclWorker\n");

}


TclWorker::~TclWorker() {
	_ntc.isolate->Dispose();
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
	v8log("TclWorker::Execute()\n");

	_ntc.context->Enter();
	
	// initialise a new Tcl interpreter for the thread
	Tcl_Interp * interp = newTclInterp((ClientData*) &_ntc);

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

	_ntc.isolate->Exit();
}
