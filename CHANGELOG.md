## Develop

* migrated to Unreal `5.8`, will likely no longer build against `5.7` (see the `ue5.7` branch)
  * updated wave asset loading to the `FSoundWaveData` API
  * MetaSound nodes and data types now use a module-local registration list, so they unregister on module shutdown
* removed bespoke MIDI impl in favor of MIDI from `Harmonix`
  * removed `MIDIMerge` in favor of theirs
* migrated to Unreal `5.4`, will likely no longer build against `5.3`

