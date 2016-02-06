
#ifndef TCLWORKER_H
#define TCLWORKER_H

#include <nan.h>
#include <string>
#include <tcl.h>
#include "v8.h"

class TclWorker : public Nan::AsyncWorker {
public:

	TclWorker( const char * cmd, Nan::Callback * callback );
	virtual ~TclWorker();

	void Execute();

protected:

	void HandleOKCallback();
void HandleErrorCallback();

private:

	std::string _cmd;
	std::string _result;
	//
	v8::Isolate* _isolate;
};

#endif /* !TCLWORKER_H */
