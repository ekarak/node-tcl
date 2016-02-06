/**
 *
 */

'use strict';

var expect  = require( 'chai' ).expect;
var binding = require( 'bindings' )( 'tcl' );
var tclb    = new binding.TclBinding();


describe( 'TclBinding', function () {

	context( 'jsEval', function () {

		it( 'should bind correctly Tcl list values to V8', function () {
			var result;
			try {
				result = tclb.cmdSync(
						"set lst [list 66 33 44 55]; " +
						"return [jsEval [list lst] "+
						"   { Math.min.apply( Math, lst ); }" +
						"]" );
			} catch ( e ) {
				result = e;
			}
			expect( result ).to.equal( 33 );
		} );


		it( 'should bind correctly Tcl dict values to V8', function () {
			var result;
			try {
				result = tclb.cmdSync(
						"set dct [dict create a 1 b 2]; " +
						"set sum [jsEval [list dct] " +
						"	{ dct.a + dct.b } " +
						"]; return $sum" );
			} catch ( e ) {
				result = e;
			}
			expect( result ).to.equal( 3 );
		} );

		it( 'should bind values mutated in V8 back to Tcl', function () {
			var result;
			try {
				result = tclb.cmdSync(
					"set dct [dict create a 1 b 2]; " +
					"jsEval [list dct] {" +
					"   dct = { a: {"+
					"        subelem1: {tree1: 1, tree2: 2}, "+
					"        subelem2: 20 "+
					"   }}; " +
					"};  return [dict get $dct a subelem2]" );
			} catch ( e ) {
				result = e;
			}
			expect( result ).to.equal( 20 );
		} );

		it( 'should work correctly with async mode', function (done) {
			var result;
			try {
				tclb.cmd("set lst [list 66 44 55]; " +
						"return [jsEval [list lst] { Math.min.apply( Math, lst ); }]",
						function(err, res) {
							console.log(`*** jsEval done, err=${err}, result=${res}`);
							result = res;
							done();
						}
				);
			} catch ( e ) {
				result = e;
			}
			expect( result ).to.equal( 44 );
		} );

	} );
} );
