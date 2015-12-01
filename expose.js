var binding = require( 'bindings' )( 'tcl' );
var tcl     = new binding.TclBinding();

tcl.expose( 'consolelog0',
	function() { 
		console.log('hello!') 
});

tcl.expose( 'consolelog1', 
	function(arg1) { 
		console.log('hello ' + arg1 + '!'); 
});

tcl.expose( 'square',
	function(n) { 
		return n*n; 
});

tcl.expose( 'puke',
	function() { 
		throw "Danger, Will Robinson!"; 
});

tcl.cmdSync( "consolelog0" );
tcl.cmdSync( "consolelog1 world" );
tcl.cmdSync( "set sq8 [square 8]; puts $sq8" );
tcl.cmdSync( 'if {[catch puke msg]} {puts "Tcl caught JS exception: $msg"}' );
try {
	tcl.cmdSync( 'puke' );
} catch (err) {
	console.log("caught Tcl error: "+err)
}
