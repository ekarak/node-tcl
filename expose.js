var binding = require( 'bindings' )( 'tcl' );
var tcl     = new binding.TclBinding();

tcl.expose(
	function() { console.log('hello!') }, 
	'consolelog0'
);

tcl.expose(
	function(arg1) { console.log('hello ' + arg1 + '!'); }, 
	'consolelog1'
);

tcl.expose(
	function(n) { return n*n; }, 
	'square'
);

tcl.expose(
	function() { throw "Danger, Will Robinson!"; }, 
	'puke'
);

tcl.cmdSync( "consolelog0" );
tcl.cmdSync( "consolelog1 world" );
tcl.cmdSync( 'puts "I think that 8*8 == [square 8]"' );
tcl.cmdSync( 'puke' );