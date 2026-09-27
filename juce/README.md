# Quiet Machine VX JUCE target

This directory turns the HTML instrument surface into a JUCE 8 VST3 and
Standalone target. The browser build remains useful for UI iteration; the plugin
build owns audio rendering, MIDI, parameter automation, and state restoration.

## Configure on Windows

Install Visual Studio 2022 with Desktop C++ support, CMake, JUCE 8, and the
Microsoft WebView2 runtime/package. Then configure from this directory:

```powershell
cmake -S . -B build -DJUCE_DIR=C:/SDK/JUCE
cmake --build build --config Release
```

Build products are written under `build/QuietMachineVX_artefacts`. The VST3
target is deliberately not copied into the system plugin folder automatically.

## Architecture boundary

- `PluginProcessor` owns the original AUTO/PROG harmonic scheduler, DSP, MIDI,
  host automation, and lossless preset state.
- `PluginEditor` embeds the HTML with JUCE 8 WebView2 and exposes native bridge
  calls for parameter updates.
- Generated AUTO/PROG notes can be sent to the host on MIDI channel 16. The
  header's MIDI recorder captures those generated events into a standard MIDI
  file without echoing incoming keyboard MIDI.
- Incoming host MIDI directly plays the native tonal engine. While performance
  notes are held, they temporarily take priority over AUTO/PROG; the internal
  generator resumes after the final note-off.
- MIDI CHORD has its own keyboard shape, independent of the AUTO and PROG note
  sets. The lowest selected key anchors the stored intervals; each incoming
  physical MIDI note becomes the new root and transposes the complete chord.
  Drone voices every chord tone while Sub and Rumble follow its root. Note
  releases are overlap-safe and clear on transport stops, panic, or mode changes.
- Physical MIDI uses a short performance glide. The Glide control still applies
  its longer movement to internally generated ambient chord changes.
- Every keyboard key stores its own Drone/Sub voice profile: fine detune, stereo
  pan, one to four Drone oscillators, and oscillator spread. The profile follows
  that key's interval when MIDI CHORD transposes it from a physical root, and
  held voices update as soon as the controls move.
- The header `?` opens an in-plugin control reference with a dedicated Reason
  Rec Source walkthrough. It distinguishes the finished `Main Processed` output
  from the optional pre-master source stems.
- Attack, decay, sustain, and release are automatable native parameters. One
  shared paraphonic envelope shapes all twelve source groups, including the
  tape loops, Swell, Rumble, Static, and Hiss. MIDI performance takes temporary
  gate ownership so chord attacks and releases are not swallowed by the internal
  generator; master tape, delay, and reverb tails remain downstream and can ring.
- The stereo 0.6/0.8-second cross-feedback delay has an automatable Delay switch.
  Disabling it restores the immediate dry path without bypassing reverb or tape wow.
- The VST3 exposes a processed stereo Main output plus optional stereo outputs
  for Tape 1-5, Swell, Rumble, Piano, Drone, Sub, Static, and Hiss. Source stems
  are post source-level and global envelope, but pre master tape/filter/reverb.
- `../index.html` remains the visual source of truth and still runs directly in a
  regular browser using its Web Audio preview engine.

## Reason routing

Reason automatically connects the `Main Processed` stereo output to the
instrument's primary Mix Channel. This is the post-filter, post-tape, post-reverb
master signal. It does not let a VST create Reason tracks or cable its own auxiliary
outputs. To use stems, expose the optional output sockets on the VST device,
create the required Mix Channels, and cable the named outputs in Reason's rack.
Save that rack as a Reason template or Combinator patch to reuse the routing.

Reason, rather than the VST, decides which device receives the hardware MIDI
keyboard. In Reason's MIDI preferences, set Master Keyboard Input to `Separated`,
then click the device icon beside Quiet Machine's sequencer track so its grey MIDI
input indicator remains there while the plugin editor is used. The designated
Master Keyboard cannot be surface-locked; Reason only permits locking an
additional keyboard or control surface to a device. The editor and its WebView do
not request keyboard focus, but the plugin cannot recover MIDI that Reason routes
to another track.

To record the self-running instrument in real time without a sampler, enable `Rec
Source` on Quiet Machine's primary `Main Processed` Mix Channel. Create an Audio
Track, choose `Stereo Input` and that Mix Channel as its input, record-enable only
the Audio Track, and start recording while Quiet Machine runs. This captures the
actual processed plugin audio without requiring a MIDI clip. Do not select a named
Drone, Sub, Piano, or Tape stem for this recording: those optional outputs are
intentionally pre-master and will sound bare. Enable `Rec Source` on those connected
stem Mix Channels only when separate source recordings are required. `Bounce in
Place` remains useful for clip-driven passages but is not the live-generator
capture path.

Reason does not record MIDI generated by a VST; use Quiet Machine's MIDI REC export
and import the resulting `.mid` file when the internal chord generator is needed as
sequencer data.
