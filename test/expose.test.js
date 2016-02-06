
'use strict';

var expect  = require( 'chai' ).expect;
var binding = require( 'bindings' )( 'tcl' );
var tclb    = new binding.TclBinding();


describe( 'TclBinding', function () {

	context( 'expose', function () {

		it( 'should return values from JS functions exposed to the Tcl interp', function () {
			var result;

			try {
				tclb.expose( 'square', function(n) { return n*n; } );
				result = tclb.cmdSync('square 8');
			} catch ( e ) {
				result = e;
			}
			expect( result ).to.equal( 64 );
		} );

		it( 'should properly handle errors thrown from JS', function () {
			var result;

			try {
				tclb.expose( 'puke', function() { throw "Danger!"; } );
				result = tclb.cmdSync( 'if {[catch puke msg]} {return puked}' );
			} catch ( e ) {
				result = e;
			}

			expect( result ).to.equal( 'puked' );
		} );

		it( 'should properly propagate errors thrown from JS', function () {
			var result;

			try {
				tclb.cmdSync( 'puke' );
			} catch ( e ) {
				result = e;
			}
			expect( result ).to.be.an.instanceof( Error );
			expect( result.message ).to.be.string( 'Danger!' );
		} );

	} );
} );
