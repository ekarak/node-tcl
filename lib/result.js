
'use strict';

/**
*   Represents a Tcl command result
*
*   @constructor
*   @param {string} data - raw response data
*   @param {object} binding - Tcl interpreter binding
*/
var Result = function ( data, binding ) {

	// update internals
	this._result = {
		raw : data
	};

	this._binding = binding;

};


/**
*   Returns raw resonse data
*
*   @return {string} Raw response data
*/
Result.prototype.data = function () {
	return this._result.raw;
};


module.exports = Result;
