/*
 * util.cpp
 *
 *  Created on: 23 Δεκ 2015
 *      Author: ekarak
 */

#include <iostream>
#include <chrono>
#include <ctime>

#include "util.h"
#include "jsEval.h"

using namespace v8;


Tcl_Interp* newTclInterp(ClientData* cd) {
	// initialise Tcl interpreter
	Tcl_Interp* _interp = Tcl_CreateInterp();

	v8log("new Tcl interpreter: %p\n", _interp);

	if ( TCL_OK != Tcl_Init( _interp ) ) {
		Nan::ThrowError( "Failed to initialise Tcl interpreter" );
	}

	// add the generic jsEval hook in Tcl
	Tcl_CreateObjCommand(_interp, "jsEval", &jsEval, cd, NULL);

	return _interp;
}

/* map ANY Tcl object to V8
 *
 */
Local<Value> TclToV8(Tcl_Interp* interp, Tcl_Obj* objPtr) {
	assert(objPtr != NULL);
	if ( objPtr->typePtr == NULL ) {
		// simplest case: no type information from Tcl, trying brute force
		long john;
		double shot;
		if (TCL_OK == Tcl_GetLongFromObj(interp, objPtr, &john)) {
			return Local<Value>::Cast(Nan::New<Number>(john));
		} else if (TCL_OK == Tcl_GetDoubleFromObj(interp, objPtr, &shot)) {
			return Local<Value>::Cast(Nan::New<Number>(shot));
		} else {
			v8log("FIXME: implicit conversion of '%s' (%p) to v8::String\n",
					Tcl_GetString(objPtr), objPtr);
			//
			return Local<Value>::Cast(
					Nan::New<String>(Tcl_GetString(objPtr)).ToLocalChecked()
			);
		}
	} else { // we got type information
		// =================================================
		if (!strcmp(objPtr->typePtr->name, "string"))
		// =================================================
		{
			return Local<Value>::Cast(
				Nan::New<String>(Tcl_GetString(objPtr)).ToLocalChecked()
			);
		}
		// =================================================
		else if (!strcmp(objPtr->typePtr->name, "int"))
		// =================================================
		{
			long john;
			if (TCL_OK == Tcl_GetLongFromObj(interp, objPtr, &john)) {
				return Local<Value>::Cast(Nan::New<Number>(john));
			}
		}
		// =================================================
		else if (!strcmp(objPtr->typePtr->name, "double"))
		// =================================================
		{
			double shot;
			if (TCL_OK == Tcl_GetDoubleFromObj(interp, objPtr, &shot)) {
				return Nan::New<Number>(shot);
			}
		}
		// =================================================
		else if (!strcmp(objPtr->typePtr->name, "booleanString"))
		// =================================================
		{
			int boolean;
			Tcl_GetBooleanFromObj(interp, objPtr, &boolean);
			return Local<Value>::Cast(Nan::New<Boolean>(boolean));
		}
		// =================================================
		else if (!strcmp(objPtr->typePtr->name, "list"))
		// =================================================
		{	// Tcl List => V8 Array
			int listLength;
			Tcl_ListObjLength(interp, objPtr, &listLength);
			Tcl_Obj* listItem;
			v8log("Tcl list, length=%d\n", listLength);
			Local<Array> arr = Nan::New<Array>(listLength);
			for (int i=0; i < listLength; i++) {
				// add each item in the Tcl list into the V8 array
				if (TCL_OK == Tcl_ListObjIndex(interp, objPtr, i, &listItem)) {
					// recursive call: TODO: check for self-pointers
					arr->Set(Nan::New<Integer>(i), TclToV8(interp, listItem));
				}
			}
			return arr;
		}
		// =================================================
		else if (!strcmp(objPtr->typePtr->name, "dict"))
		// =================================================
		{	// Tcl dict => V8 object
			Local <Object> v8obj = Nan::New<Object>();
			Tcl_DictSearch search;
			Tcl_Obj *key, *value;
			int done = 0;
			if (TCL_OK != Tcl_DictObjFirst(interp, objPtr, &search,
			        &key, &value, &done)) {
				Nan::ThrowError(Nan::New<String>("Error iterating Tcl dict").ToLocalChecked());
			}
			for (; done == 0; Tcl_DictObjNext(&search, &key, &value, &done))
			{
				v8log("Mapping dict %s => %s\n", Tcl_GetString(key), Tcl_GetString(value));
				Nan::Set(v8obj,
					Nan::New<String>(Tcl_GetString(key)).ToLocalChecked(),
					TclToV8(interp, value)
				);
			}
			Tcl_DictObjDone(&search);
			return v8obj;
		}
		else {
			v8log("TODO: TclToV8 %s\n", objPtr->typePtr->name);
		}
		return Nan::New<Object>();
	}
}

/* Maps ANY V8/Javascript object to its Tcl equivalent
 * Creates a NEW Tcl object for the given V8 Value
 */
Tcl_Obj* V8ToTcl(Tcl_Interp* interp, Value* v8v) {
	Tcl_Obj* to = nullptr;
		// ============
	if (v8v->IsString()) {
		// ============
		Local<String> s = v8v->ToString();
		// TODO: check that the Tcl interp is in UTF8 mode
		to = Tcl_NewStringObj(*String::Utf8Value(s), s->Length());
		// ===================
	} else if (v8v->IsNumber()) {
		// ===================
		if (v8v->IsInt32() ) {
			to = Tcl_NewIntObj(v8v->Int32Value());
		} else if (v8v->IsUint32()) {
			to = Tcl_NewIntObj(v8v->Uint32Value());
		} else if (v8v->IsFloat32x4()) {
			to = Tcl_NewDoubleObj(v8v->NumberValue());
		}
		// ===================
	} else if (v8v->IsArray()) {
		// ===================
		v8::Array*  arr = v8::Array::Cast(v8v);
		v8log("V8 Array, length=%d\n", arr->Length());
		to = Tcl_NewObj();
		// Tcl_NewListObj(arr->Length(), &to);
		for (uint32_t i=0; i < arr->Length(); i++) {
			Tcl_Obj* newelement = V8ToTcl(interp, *arr->Get(i));
			printf("V8 Array, item %d = %s\n", i, Tcl_GetString(newelement));
			Tcl_ListObjAppendElement(interp, to, newelement);
		}
		// =====================================
	} else if (v8v->IsMap() || v8v->IsObject()) {
		// =====================================
		to = Tcl_NewDictObj();
		v8::Object*  v8o = v8::Object::Cast(v8v);
		v8log("***V8ToTcl: IsMap=%d IsObject=%d\n",v8v->IsMap(), v8v->IsObject());

		Local<Array> property_names = v8o->GetOwnPropertyNames();
		for (uint32_t i = 0; i < property_names->Length(); ++i) {
		    Local<Value> key = property_names->Get(i);
		    Local<Value> value = v8o->Get(key);
		    v8log("***V8ToTcl: \tprop: %s val: %s\n",
		    		*String::Utf8Value(key),
					*String::Utf8Value(value));
		    Tcl_DictObjPut(interp, to, V8ToTcl(interp, key), V8ToTcl(interp, value));
		}
		// ===================
	} else if (v8v->IsRegExp()) {
		// ===================
		// todo
		v8log("***V8ToTcl: TODO IsRegExp\n");
	}
	//v8log("V8ToTcl: returning %p (%s: %s)\n", to, to->typePtr ? to->typePtr->name : "(untyped)", Tcl_GetString(to));
	return to;
}

Tcl_Obj* V8ToTcl(Tcl_Interp* interp, Local<Value> v8v) {
	return V8ToTcl(interp, *v8v);
}

static void gc_begin(GCType type, GCCallbackFlags flags){
  std::cout << "GC begins: " << type << std::endl;
}

static void gc_end(GCType type, GCCallbackFlags flags){
  std::cout << "GC ends: " << type << std::endl;
}

/*
 * Get a new instance of the V8 Engine (so conveniently called an 'Isolate') and ENTER it.
 * Its the caller's responsibility to properly call Exit() and Dispose()
 */
v8::Isolate* newV8Isolate() {
	v8::Isolate* isolate = Isolate::New();
	isolate->Enter();
	// ekarak: add GC hints
	V8::AddGCPrologueCallback(&gc_begin);
	V8::AddGCEpilogueCallback(&gc_end);

	return isolate;
}

// generic logging helper
mutex stderr_mutex;
void v8log(const char* format, ...) {
    va_list argptr;
    va_start(argptr, format);
    uv_thread_t tid = uv_thread_self();
    std::string f = std::string(" \033[38;5;%dm(%p) \033[0m");
    uint8_t colorcode = (tid / 0x1000) % 212;
	{
		mutex::scoped_lock sl(stderr_mutex);
		// prefix: timestamp
		// TODO: make this human-readable
		std::cerr << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		// prefix: colored thread id
	    fprintf (stderr, f.c_str(), colorcode, (void*) tid);
	    // actual message
	    vfprintf(stderr, format, argptr);
	    fflush  (stderr);
	}
    va_end(argptr);
}


void handleTclError(Tcl_Interp* interp, uint8_t code) {
	/*
	 * Tcl_GetReturnOptions retrieves the dictionary of return options from an interpreter
	 * following a script evaluation. Routines such as Tcl_Eval are called to evaluate a
	 * script in an interpreter. These routines return an integer completion code. These
	 * routines also leave in the interpreter both a result and a dictionary of return
	 * options generated by script evaluation. Just as Tcl_GetObjResult retrieves the
	 * result, Tcl_GetReturnOptions retrieves the dictionary of return options.
	 *
	 * The integer completion code should be passed as the code argument to
	 * Tcl_GetReturnOptions so that all required options will be present in the dictionary.
	 * Specifically, a code value of TCL_ERROR will ensure that entries for the keys
	 * -errorinfo, -errorcode, and -errorline will appear in the dictionary. Also, the
	 * entries for the keys -code and -level will be adjusted if necessary to agree with the
	 * value of code. The (Tcl_Obj *) returned by Tcl_GetReturnOptions points to an unshared
	 * Tcl_Obj with reference count of zero. The dictionary may be written to, either adding,
	 * removing, or overwriting any entries in it, without the need to check for a shared
	 * value. As with any Tcl_Obj with reference count of zero, it is up to the caller to
	 * arrange for its disposal with Tcl_DecrRefCount or to a reference to it via
	 * Tcl_IncrRefCount (or one of the many functions that call that, notably including
	 * Tcl_SetObjResult and Tcl_SetVar2Ex)
	 */
    Tcl_Obj *options = Tcl_GetReturnOptions(interp, code);
    Tcl_Obj *key = Tcl_NewStringObj("-errorinfo", -1);
    Tcl_Obj *key2 = Tcl_NewStringObj("-errorcode", -1);
    // -errorcode, and -errorline ?????
    Tcl_Obj *stackTrace;
	Tcl_Obj *errorCode;
    Tcl_IncrRefCount(key); Tcl_IncrRefCount(key2);
    Tcl_DictObjGet(NULL, options, key, &stackTrace);
    Tcl_DictObjGet(NULL, options, key2, &errorCode);
    Tcl_DecrRefCount(key); Tcl_DecrRefCount(key2);
    v8log("Tcl stacktrace:\n\t%s\n", Tcl_GetString(stackTrace));
    v8log("Tcl errorCode: %s\n", Tcl_GetString(errorCode));
    // v8 error object
    Local<Value> errobj = Nan::Error(Tcl_GetStringResult( interp ));
    // TODO: how to add properties into errobj
    // Nan::Set(errobj, Nan::New<String>("stack").ToLocalChecked(), Nan::New<String>(Tcl_GetString(stackTrace)).ToLocalChecked());
    Tcl_DecrRefCount(options);

	// TODO: add columnNumber, lineNumber, fileName, stack, toSource() ?
	Nan::ThrowError( errobj );
}
