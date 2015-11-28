
#ifndef TCLBINDING_H
#define TCLBINDING_H

#include <nan.h>
#include <tcl.h>
#include <map>

#if defined(HAS_CXX11) && defined(HAS_TCL_THREADS)
#include "taskrunner.h"
#endif

typedef struct {
	Nan::Callback*                        jsFunc;
	Nan::FunctionCallbackInfo<v8::Value>* args;
} JsProxyBinding;

class TclBinding : public node::ObjectWrap {
public:
	static void init( v8::Local< v8::Object > exports );

private:
	explicit TclBinding();
	~TclBinding();

	static void construct( const Nan::FunctionCallbackInfo< v8::Value > &info );
	static void cmd( const Nan::FunctionCallbackInfo< v8::Value > &info );
	static void cmdSync( const Nan::FunctionCallbackInfo< v8::Value > &info );
	static void queue( const Nan::FunctionCallbackInfo< v8::Value > &info );
	static void toArray( const Nan::FunctionCallbackInfo< v8::Value > &info );
	static void expose( const Nan::FunctionCallbackInfo< v8::Value > &info );

	static Nan::Persistent< v8::Function > constructor;

	Tcl_Interp * _interp;

#if defined(HAS_CXX11) && defined(HAS_TCL_THREADS)
	TaskRunner * _tasks;
#endif

};

// map of Javascript functions available to TclBinding
static std::map<std::string, JsProxyBinding*> _jsExports;

// node addon initialisation
NODE_MODULE( tcl, TclBinding::init )

#endif /* !TCLBINDING_H */
