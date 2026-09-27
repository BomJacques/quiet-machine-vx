# Quiet Machine VX: JUCE implementation walkthrough

This walkthrough starts from the current state of this computer. At the time the
project was created, neither CMake, a Visual Studio C++ installation, nor a JUCE
checkout was detected in the usual locations.

## 1. Install the Windows build tools

1. Open **Visual Studio Installer**.
2. Install **Visual Studio 2022 Community** or modify the existing installation.
3. Select the **Desktop development with C++** workload.
4. In the individual components panel, make sure these are selected:
   - MSVC v143 C++ x64/x86 build tools
   - Windows 10 or Windows 11 SDK
   - C++ CMake tools for Windows
   - Git for Windows, if Git is not already installed
5. Restart PowerShell after installation so the tools appear on `PATH`.
6. Verify:

```powershell
cmake --version
git --version
```

## 2. Get JUCE 8

Clone the JUCE repository into a stable SDK directory:

```powershell
New-Item -ItemType Directory -Force C:\SDK
git clone --depth 1 --branch master https://github.com/juce-framework/JUCE.git C:\SDK\JUCE
```

The project targets JUCE 8 because JUCE 8 provides native JavaScript functions,
resource providers, parameter relays, and the modern WebView2 backend used by the
editor. Review JUCE's AGPL/commercial licensing terms before distributing a
closed-source plugin.

## 3. Configure the project

Open PowerShell in this directory:

```powershell
Set-Location "C:\Users\YourName\Documents\Codex\2026-06-17\files-mentioned-by-the-user-index\outputs\quiet-machine-vx\juce"
cmake -S . -B build -DJUCE_DIR=C:/SDK/JUCE
```

A successful configure creates `build/QuietMachineVX.sln` and reports targets for
VST3 and Standalone. If CMake reports that WebView2 is missing, install the
Microsoft WebView2 NuGet package/runtime indicated by JUCE's diagnostic and run
the configure command again.

## 4. Build Standalone first

The Standalone target gives the shortest debugging loop:

```powershell
cmake --build build --config Debug --target QuietMachineVX_Standalone
```

Or use the included helper from the `juce` directory:

```powershell
.\build.ps1 -JuceDir C:\SDK\JUCE -Configuration Debug -Target Standalone
```

Launch the executable from:

```text
build\QuietMachineVX_artefacts\Debug\Standalone\Quiet Machine VX.exe
```

Expected first result: the same Ambient Directive interface appears inside a
JUCE window, including its keyboard, AUTO/PROG workflows, mixer, patch memory,
and factory-patch strip. In plugin mode, the browser clock is disabled and PLAY
starts the native sample-clock harmonic scheduler.

## 5. Test the HTML-to-C++ bridge

1. Select notes on the keyboard and switch between `AUTO` and `PROG`.
2. Move `LPF`, `WOW DEPTH`, `TAPE DRIVE`, and `MASTER`.
3. Load each factory patch and confirm that the keyboard selection, navigation,
   tape controls, and source mixer all update.
4. Enable `MIDI OUT`, place a second instrument after Quiet Machine, and confirm
   that its generated notes and note-offs reach that instrument.
5. Start `MIDI REC`, let the engine run, stop recording, and confirm that a `.mid`
   file is saved with editable note lengths.
6. Close and reopen the Standalone app.
7. Confirm that the complete patch, including chord banks and piano roll, is restored.

The editor sends a lossless copy of the original patch JSON to JUCE. The
processor maps automatable values into `AudioProcessorValueTreeState`, stores
the structured chord and sequence state, and saves both in the DAW project.

## 6. Build VST3

```powershell
cmake --build build --config Release --target QuietMachineVX_VST3
```

The VST3 bundle is written under:

```text
build\QuietMachineVX_artefacts\Release\VST3\Quiet Machine VX.vst3
```

The CMake project deliberately does not copy into the system VST3 directory.
After a successful build, copy the bundle to:

```text
C:\Program Files\Common Files\VST3
```

Administrator permission is normally required for that copy.

## 7. Scan in a DAW

1. Open the DAW's plugin manager.
2. Rescan VST3 plugins.
3. Insert **Quiet Machine VX** on an instrument track.
4. Send MIDI notes from a keyboard or piano roll.
5. Automate `tempoBpm`, `lowpassHz`, `wowDepth`, `tapeDrive`, and `masterGain`.
6. Save, close, and reopen the DAW project to verify state restoration.

## 8. Complete native parity

The scaffold now has the original parameter names, lossless patch state, seeded
AUTO navigation, eight PROG chord steps, waveform pairing, source levels, tape
modulation, filtering, reverb/freeze, and the native generator clock. Continue
without changing the browser workflow:

1. Port the five independent 17.3/21.1/23.5/29.8/37.0-second tape-loop voices.
2. Port the generative 5/16 + 4/15 piano and the optional 16-step piano roll.
3. Match the drone, sub, swell, rumble, static, and hiss source paths exactly.
4. Match wear dropouts, cross-delay, freeze recapture, and the six-second reverb.
5. Add bidirectional parameter relays so DAW automation moves on-screen controls.
6. Add native master/stem WAV export or optional multi-output buses.
7. Run deterministic event-trace comparisons between browser and C++ engines.
8. Run JUCE's `pluginval` checks, then test Ableton Live, Reaper, FL Studio, and a
   second sample rate before signing a release build.

## 9. Release checks

- No allocations, file access, locks, or UI calls in `processBlock`.
- Parameters use stable IDs and remain backward compatible.
- The UI works at every supported plugin size and Windows display scale.
- State survives DAW save/load and plugin duplication.
- MIDI note-off, sustain, panic, and transport-stop behavior are correct.
- Release builds are validated with `pluginval` and at least two DAWs.
- JUCE and any third-party DSP licensing is settled before distribution.
