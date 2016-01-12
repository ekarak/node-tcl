/*
 * NodeTclContext.h
 *
 *  Created on: 11 Ιαν 2016
 *      Author: ekarak
 */

#ifndef SRC_NODETCLCONTEXT_H_
#define SRC_NODETCLCONTEXT_H_

#include "node.h"
#include "uv.h"

#include "util.h"

using namespace v8;

class NodeTclContext {
public:
	// libuv
	uv_loop_t*   			loop;
	// v8
	Isolate* 				isolate;
	Handle<ObjectTemplate> 	global_templ;
	Handle<Context> 		context;
	// Node
	node::Environment* 		node;

	NodeTclContext();
};


#endif /* SRC_NODETCLCONTEXT_H_ */
