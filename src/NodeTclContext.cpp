/*
 * NodeTclContext.cpp
 *
 *  Created on: 11 Ιαν 2016
 *      Author: ekarak
 */

#include "NodeTclContext.h"
#include "node.h"

NodeTclContext::NodeTclContext() {
	isolate  = newV8Isolate();
	loop = uv_loop_new();
	uv_loop_init(loop);
	//uv_loop_configure()


}
