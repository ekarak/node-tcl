#include "expose.h"
#include "util.h"

/*
   dispatch a tcl command invocation to its exposed Javascript equivalent
*/
int objCmdProcDispatcher(
				ClientData clientData, // (JsProxyBinding*)
				Tcl_Interp *interp,
				int objc,
				Tcl_Obj *const objv[])
{
	// get the JS proxy binding
	JsProxyBinding* jsb = ((JsProxyBinding*) clientData);

	/*printf("objCmdProcDispatcher, interp=%p cmd=%s\n", interp, Tcl_GetString(objv[0]));
	for (int i = 1; i < objc; i++) {
		printf("\targ %d == %s\n", i, Tcl_GetString(objv[i]));
	}*/

	// bind Tcl arguments ("everything is a string") to the JS function
	Local < Array > jsFunctionArgs = Nan::New<Array>(objc-1);
	for ( int i = 1; i < objc; i++ ) {
		/* todo: parse json args on the fly as JS objects to JS func
		 * todo: use the JsBinding->args to map datatypes, if possible */
		Nan::Set(
			jsFunctionArgs, i - 1,
			TclToV8(interp, objv[i])
		);
	}
	Local<Value> args = Local<Value>::Cast(jsFunctionArgs);

	Nan::TryCatch tc;
	// rematerialize handle to JS function
	v8::Local<v8::Function> function = v8::Local<v8::Function>::New(
		Nan::GetCurrentContext()->GetIsolate(), jsb->jsFunc
	);
	// call the exposed Javascript function
	Local<Value>  retv = function->Call( Nan::GetCurrentContext()->Global(), objc-1, &args );

	if ( tc.HasCaught() ) {

		// oops, exception raised from V8 while in JS land
		Local<Message> msg = tc.Message();
		std::string msgtext(*String::Utf8Value(msg->Get()));

		Tcl_SetObjResult(interp, Tcl_NewStringObj(msgtext.c_str(), -1));

		// pass back the Return Options with the stack details.
		String::Utf8Value filename(msg->GetScriptResourceName());
		String::Utf8Value sourceline(msg->GetSourceLine());
		int linenum = msg->GetLineNumber();

		Tcl_SetErrorCode(interp, *sourceline, NULL);
		Tcl_AddErrorInfo(interp, "\n   occurred in file ");
		Tcl_AddErrorInfo(interp, *filename);
		Tcl_AddErrorInfo(interp, " line ");
		Tcl_AppendObjToErrorInfo(interp, Tcl_NewIntObj(linenum));

		// append the full stack trace if it was present.
		MaybeLocal<Value> stack_trace = tc.StackTrace();
		if ( !stack_trace.IsEmpty() ) {
			String::Utf8Value stmsg(stack_trace.ToLocalChecked());
			Tcl_AddErrorInfo(interp, *stmsg);
		}

		return TCL_ERROR;

	} else {

		v8log("return value isEmpty=%d isUndefined=%d\n", retv.IsEmpty(), retv->IsUndefined());
		if ( !retv.IsEmpty() && !retv->IsUndefined() ) {
			std::string res(*String::Utf8Value(retv));
			v8log("Result == %s\n", res.c_str());
			Tcl_SetObjResult(interp, V8ToTcl(interp, Local<Object>::Cast(retv)));
		}
		return TCL_OK;

	}

	// must return TCL_OK, TCL_ERROR, TCL_RETURN, TCL_BREAK, or TCL_CONTINUE.
};

/* Tcl_CmdDeleteProc: Procedure to call before cmdName is deleted from the
* interpreter; allows for command-specific cleanup. If NULL, then no procedure
* is called before the command is deleted. */
void objCmdDeleteProcDispatcher(
 ClientData clientData)
{
	JsProxyBinding*  jsb = ((JsProxyBinding*) clientData);
	std::string& cmdname = jsb->cmdname;
	v8log("objCmdDeleteProcDispatcher(%s)\n", cmdname.c_str());
	// mark the persistent function handle as independent so that GC can pick this up
	jsb->jsFunc.MarkIndependent();
	_jsExports.erase(cmdname);
};
