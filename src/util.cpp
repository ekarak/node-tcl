/*
 * util.cpp
 *
 *  Created on: 23 Δεκ 2015
 *      Author: ekarak
 */

#include "util.h"
#include "jsEval.h"

using namespace v8;

Tcl_Interp* newTclInterp() {
	// initialise Tcl interpreter
	Tcl_Interp* _interp = Tcl_CreateInterp();

	printf("%p: new Tcl interpreter: %p\n", (void*) uv_thread_self(), _interp);

	if ( TCL_OK != Tcl_Init( _interp ) ) {
		Nan::ThrowError( "Failed to initialise Tcl interpreter" );
	}

	// add the generic jsEval hook in Tcl
	Tcl_CreateObjCommand(
		_interp,
		"jsEval",
		&jsEval,
		NULL, // clientData,
		NULL
	);

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
			printf("Tcl list, length=%d\n", listLength);
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
				//printf("Mapping dict %s => %s\n", Tcl_GetString(key), Tcl_GetString(value));
				Nan::Set(v8obj,
					Nan::New<String>(Tcl_GetString(key)).ToLocalChecked(),
					TclToV8(interp, value)
				);
			}
			Tcl_DictObjDone(&search);
			return v8obj;
		}
		else {
			printf("TODO: TclToV8 %s\n", objPtr->typePtr->name);
			//return nullptr;
		}
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
		printf("V8 Array, length=%d\n", arr->Length());
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
		printf("***V8ToTcl: IsMap=%d IsObject=%d\n",v8v->IsMap(), v8v->IsObject());

		Local<Array> property_names = v8o->GetOwnPropertyNames();
		for (uint32_t i = 0; i < property_names->Length(); ++i) {
		    Local<Value> key = property_names->Get(i);
		    Local<Value> value = v8o->Get(key);
		    printf("***V8ToTcl: \tprop: %s val: %s\n",
		    		*String::Utf8Value(key),
					*String::Utf8Value(value));
		    Tcl_DictObjPut(interp, to, V8ToTcl(interp, key), V8ToTcl(interp, value));
		}
		// ===================
	} else if (v8v->IsRegExp()) {
		// ===================
		// todo
		printf("***V8ToTcl: TODO IsRegExp\n");
	}
	return to;
}

Tcl_Obj* V8ToTcl(Tcl_Interp* interp, Local<Value> v8v) {
	return V8ToTcl(interp, *v8v);
}


class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* data = AllocateUninitialized(length);
    return data == NULL ? data : memset(data, 0, length);
  }
  virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
  virtual void Free(void* data, size_t) { free(data); }
};

v8::Isolate* newV8Isolate() {
	// Get a new instance of the V8 Engine (so conveniently called an 'Isolate')
	printf("(%p) TaskRunner::worker: creating new v8::Isolate\n", (void *)uv_thread_self());
	ArrayBufferAllocator  allocator;
	Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = &allocator;
	//
	v8::Isolate* _isolate = Isolate::New(create_params);
	return _isolate;
}
