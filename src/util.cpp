/*
 * util.cpp
 *
 *  Created on: 23 Δεκ 2015
 *      Author: ekarak
 */

#include "util.h"

using namespace v8;

// map ANY Tcl object to V8
Local<Value> TclToV8(Tcl_Interp* interp, Tcl_Obj* objPtr) {
	assert(objPtr != NULL);
	if ( objPtr->typePtr == NULL ) {
		// simplest case: no type information from Tcl, assuming this is a string
		return Local<Value>::Cast(
			Nan::New<String>(Tcl_GetString(objPtr)).ToLocalChecked()
		);
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
				int rc = Tcl_ListObjIndex(interp, objPtr, i, &listItem);
				// recursive call: TODO: check for self-pointers
				arr->Set(Nan::New<Integer>(i), TclToV8(interp, listItem));
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
			int rc, done = 0;
			if (Tcl_DictObjFirst(interp, objPtr, &search,
			        &key, &value, &done) != TCL_OK) {
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
		//	return NULL;
		}
	}
}
