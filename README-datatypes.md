### Datatypes

Variable types in Tcl and V8 are not exactly mappable
dynamically converted according to Tcl's "everything is a string" dogma. We're trying

 and applying the following rules:

- "Primitive" Tcl datatypes are mapped directly (with the notable exception of BigNums)

- Tcl lists are mapped to V8 arrays
- Tcl arrays are mapped to V8 objects
- Tcl dicts are mapped to V8 maps
