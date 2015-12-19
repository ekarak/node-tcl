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

tcl.cmdSync( "puts 3...; after 1000 {puts 2...}; after 2000 {puts 1...}; after 3000 {puts goodbye}");
tcl.cmdSync( "puts 33...; after 1100 {puts 22..}; after 2200 {puts 11...}; after 3300 {puts goodbye}");
tcl.cmdSync( "consolelog0" );
tcl.cmdSync( "consolelog1 world" );
tcl.cmdSync( "set sq8 [square 8]; puts \"the square of 8 is $sq8\"; flush stdout" );
tcl.cmdSync( 'if {[catch puke msg]} {puts "Tcl caught JS exception: $msg"}' );
try {
	tcl.cmdSync( 'puke' );
} catch (err) {
	console.log("caught Tcl error: "+err)
}
tcl.cmdSync( "set fd [open /proc/cpuinfo]; set str [read $fd]; close $fd; puts \"Read [string length $str] bytes\"" );

