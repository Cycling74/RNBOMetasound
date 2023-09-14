# RNBO MetaSounds

## Parameter metadata

* `[param foo]` will create an input pin
* `[param foo @meta in:true]` will only create an input pin
* `[param foo @meta out:true]` will create both an input and an output pin
* `[param foo @meta in:false,out:true]` will only create an output pin


## Links

* [MetaSounds Reference Guide](https://docs.unrealengine.com/5.2/en-US/metasounds-reference-guide-in-unreal-engine/)
* [Unreal Build Tool](https://docs.unrealengine.com/5.2/en-US/unreal-build-tool-in-unreal-engine/)

## Notes

* When exporting your RNBO C++ source, you can pick an export directory that should have a path like: `/<Your UE Project>/Plugins/RNBOMetasound/Exports/<Your RNBO Device Name>/`

* to recompile/load metasound nodes:
  * Tools -> Debug -> Modules
    * search for RNBO then click the appropriate button
    * note: you must have "Live Coding" disabled 
