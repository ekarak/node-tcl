#!/usr/bin/tclsh
#

set tclconfig {}

if {[info exists env(TCLCONFIG)]} {
	set tclconfig $env(TCLCONFIG)
} else {
	package require Tcl

	set libdir [::tcl::pkgconfig get libdir,runtime]
	set tclconfig [file join $libdir tclConfig.sh]
	if {![file exists $tclconfig]} {
		set tclconfig [file join $libdir tcl$tcl_version tclConfig.sh]
	}
	if {![file exists $tclconfig]} {
		error "can't locate tclConfig.sh - is the libtcl development package installed?"
	}
}

puts $tclconfig
exit [expr {$tclconfig == {}}]
