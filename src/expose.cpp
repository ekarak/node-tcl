#include "expose.h"

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

	printf("objCmdProcDispatcher, interp=%p cmd=%s\n", interp, Tcl_GetString(objv[0]));
	for (int i = 1; i < objc; i++) {
		printf("\targ %d == %s\n", i, Tcl_GetString(objv[i]));
	}

	// bind Tcl arguments ("everything is a string") to the JS function
	Local < Array > jsFunctionArgs = Nan::New<Array>(objc-1);
	for ( int i = 1; i < objc; i++ ) {
		/* todo: parse json args on the fly as JS objects to JS func
		 * todo: use the JsBinding->args to map datatypes, if possible */
		Nan::Set(
			jsFunctionArgs, i - 1,
			Nan::New<String>( Tcl_GetString( objv[i] ) ).ToLocalChecked()
		);
	}
	Local<Value> args = Local<Value>::Cast(jsFunctionArgs);

	// doo the doo
	Nan::Callback*  f = jsb->jsFunc;
	Local<Value> retv = f->Call( objc, &args);

	printf("\treturn value isEmpty=%d isUndefined=%d\n", retv.IsEmpty(), retv->IsUndefined());
	if (! (retv.IsEmpty() || retv->IsUndefined()) ) {
		std::string res(*String::Utf8Value(retv));
		printf("\t\tResult == %s\n", res.c_str());
		Tcl_Obj* tclres = Tcl_NewStringObj(res.c_str(), res.size());
		Tcl_SetObjResult(interp, tclres);
	}

	// must return TCL_OK, TCL_ERROR, TCL_RETURN, TCL_BREAK, or TCL_CONTINUE.

	return TCL_OK;
};

/* Tcl_CmdDeleteProc: Procedure to call before cmdName is deleted from the
* interpreter; allows for command-specific cleanup. If NULL, then no procedure
* is called before the command is deleted. */
void objCmdDeleteProcDispatcher(
 ClientData clientData)
{
	std::string& cmdname = ((JsProxyBinding*) clientData)->cmdname;
	printf("objCmdDeleteProcDispatcher(%s)\n", cmdname.c_str());
	_jsExports.erase(cmdname);
};
