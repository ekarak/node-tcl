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

//tcl.cmdSync( "puts timer...;   after 100 {puts 100...}; after 250 {puts 250...}; after 500 {puts 500}");
//tcl.cmdSync( "puts timer2...; after 210 {puts 210..};  after 320 {puts 320...}; after 430 {puts 430}");
//tcl.cmdSync( "consolelog0" );
//tcl.cmdSync( "consolelog1 world" );
tcl.cmdSync( "set sq8 [square 8]; puts \"the square of 8 is $sq8\"; flush stdout" );
tcl.cmdSync( 'puts "About to throw a JS exception..."; if {[catch puke msg]} {puts "Tcl caught JS exception: $msg"}' );
try {
	tcl.cmdSync( 'puts "About to throw a Tcl error..."; error "Danger, Will Robinson!"' );
} catch (err) {
	console.log("Javascript caught: "+err)
}
tcl.cmdSync( "set fd [open /proc/cpuinfo]; set str [read $fd]; close $fd; puts \"Read [string length $str] bytes\"" );
tcl.cmdSync( "set lst [list 3 4 5];   set min [jsEval [list lst] { Math.min.apply( Math, lst ); }]; puts \" min=$min \" " );
tcl.cmdSync( "set dct [dict create a 1 b 2]; set sum [jsEval [list dct] { dct.a + dct.b }]; puts \" sum=$sum \" " );
