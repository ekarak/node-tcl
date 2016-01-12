/*
 * jsEval.h
 *
 *  Created on: 25 Δεκ 2015
 *      Author: ekarak
 */

#ifndef SRC_JSEVAL_H_
#define SRC_JSEVAL_H_

#include "tcl.h"
#include "nan.h"
#include "tclbinding.h"
#include "node.h"



int jsEval(ClientData clientData, // (JsProxyBinding*)
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const objv[]);

#endif /* SRC_JSEVAL_H_ */
