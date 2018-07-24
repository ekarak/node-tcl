/*
 * util.h
 *
 *  Created on: 23 Δεκ 2015
 *      Author: ekarak
 */

#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include <map>

#include "nan.h"
#include "tcl.h"

//
Tcl_Interp* newTclInterp();

// convert any Tcl value into a v8 value
v8::Local<v8::Value> TclToV8(Tcl_Interp*, Tcl_Obj*);

// convert any v8 Value to its Tcl equivalent
Tcl_Obj* V8ToTcl(Tcl_Interp* interp, v8::Value* v8v);
Tcl_Obj* V8ToTcl(Tcl_Interp* interp, v8::Local<v8::Value> v8v);
const char* V8ValueType(v8::Local<v8::Value> v);

// init new Isolate and enter it
v8::Isolate* newV8Isolate();

extern Nan::AsyncResource *resource;
extern Nan::CopyablePersistentTraits<v8::Object>::CopyablePersistent ctx_obj;

// generic logger
void v8log(const char* format, ...);

#include <unistd.h>
#include <pthread.h>
	class mutex {
	private:
		pthread_mutex_t _mutex;

	public:
		mutex()             { pthread_mutex_init(&_mutex, NULL); }
		~mutex()            { pthread_mutex_destroy(&_mutex); }
		inline void lock()  { pthread_mutex_lock(&_mutex); }
		inline void unlock(){ pthread_mutex_unlock(&_mutex); }


	class scoped_lock {
		public:
			inline explicit scoped_lock(mutex & sp) : _sl(sp) { _sl.lock(); }
			inline ~scoped_lock()                             { _sl.unlock(); }
		private:
			scoped_lock(scoped_lock const &);
			scoped_lock & operator=(scoped_lock const &);
			mutex& _sl;
		};

	};

#endif /* SRC_UTIL_H_ */
