
#ifndef TCLWORKER_H
#define TCLWORKER_H

#include <nan.h>
#include <string>
#include <tcl.h>
#include "v8.h"
#include "NodeTclContext.h"

class TclWorker : public Nan::AsyncWorker {
public:

	TclWorker( const char * cmd, Nan::Callback * callback );
	virtual ~TclWorker();

	void Execute();

protected:

	void HandleOKCallback();


private:

	std::string _cmd;
	std::string _result;
	//
	NodeTclContext _ntc;
};

#endif /* !TCLWORKER_H */
