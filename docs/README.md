# RNBO MetaSounds Documentation

As this integration is in an experimental state, so is this documentation, and this guide should not be considered an encyclopedic reference, but rather a quick-start with a handful of ways to use this tool.

## Geting Started

*This section is a stub --*

* When exporting your RNBO C++ source, you can pick an export directory that should have a path like: `/<Your UE Project>/Plugins/RNBOMetasound/Exports/<Your RNBO Device Name>/`

* After you export, you must build your project.

## Input and Output Parameters 

You can set whether a parameter of your RNBO patcher will become an input or an output pin using parameter metadata. 

* `[param foo]` will create an input pin
* `[param foo @meta in:true]` will only create an input pin
* `[param foo @meta out:true]` will create both an input and an output pin
* `[param foo @meta in:false,out:true]` will only create an output pin

## Generating Pin Types

*This section is a stub --*

### Boolean

`[param foo @enum 0 1]` will be treated as a boolean type in the MS graph.

![boolean in Max](img/boolean-in-Max.png)
![boolean in UE](img/boolean-in-UE.png)

### Trigger
`[inport bar]` or `[output bar]` will create a `Trigger` input or output pin on the resulting MS node. 

Note that at present, the pin will only output a `Trigger` if the `outport` is set to output a `bang`.

## A note on buffers and audio files

## Transport

## MIDI



