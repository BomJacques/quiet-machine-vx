# Quiet Machine VX: Beginner JUCE 8 Build Wizard

This is an interactive, one-checkpoint-at-a-time guide for Windows, Visual Studio 2026, JUCE 8, Standalone, and VST3.

**Do not start this wizard until Codex has confirmed that Quiet Machine VX is code-complete.** The browser preview is not enough. The native C++ engine, patch recall, automation, WebView bridge, five tape loops, generative piano, degradation, freeze, MIDI input, and MIDI output must already be implemented and tested in code.

The project now declares `NEEDS_MIDI_OUTPUT TRUE`, reports `producesMidi() == true`, and contains the generated-MIDI capture path. Do not begin despite that milestone: the remaining native-audio parity and validation gates in Step 0 must also be complete.

## How To Use This Wizard

1. Perform only the current numbered step.
2. Compare your screen with **Success looks like**.
3. If it matches, send Codex the exact checkpoint reply shown at the end of the step.
4. If it does not match, stop and send Codex the full error text or a screenshot.
5. Do not continue to the next step until Codex confirms it is safe.

Project directory:

```text
C:\Users\YourName\Documents\Codex\2026-06-17\files-mentioned-by-the-user-index\outputs\quiet-machine-vx\juce
```

JUCE directory used by this guide:

```text
C:\SDK\JUCE
```

## Step 0: Confirm The Project Is Ready

### Click or type

Do not install anything yet. Send Codex this exact message:

```text
CHECKPOINT 0: Please confirm Quiet Machine VX is code-complete, native-audio complete, state-complete, and MIDI-output complete before I begin the JUCE wizard.
```

Codex should inspect the code and explicitly confirm all of these:

- The original ambient scheduler and audio engine run in C++ without relying on browser audio.
- The five tape loops, piano, drone, sub, swell, rumble, static, hiss, degradation, delay, reverb, and freeze are implemented.
- Complete patches restore through native JUCE state.
- `NEEDS_MIDI_OUTPUT TRUE` is present in `CMakeLists.txt`.
- `producesMidi()` returns `true`.
- Generated notes are written into the VST3 MIDI event output.
- The WebView is a controller/view in plugin mode, not a second audio engine.

### Success looks like

Codex replies that every item is implemented and that this wizard may begin.

### Likely error recovery

If Codex finds unfinished native behavior, stop here. Building an incomplete plugin can produce a window that looks correct while controls, effects, state, or MIDI output do nothing.

> **STOP / CHECKPOINT 0:** Wait for Codex to say the project is code-complete. Do not continue on an estimate or partial pass.

## Step 1: Choose A JUCE Licensing Route

JUCE 8 is dual-licensed. This is a licensing choice, not a compiler setting.

### Click or type

1. Open a web browser.
2. Go to [JUCE licensing and plans](https://juce.com/get-juce/).
3. Read the JUCE 8 licensing FAQ and the linked JUCE 8 EULA.
4. Decide which route applies before distributing anything:
   - Use JUCE under AGPLv3 only if the complete distributed product will comply with AGPLv3.
   - Use an appropriate JUCE commercial plan if distributing a closed-source product.
5. Sign in to the JUCE account used for the project and record the selected route somewhere private.

This guide is not legal advice. Local pre-release development and public distribution are not the same thing. Do not send builds to testers or customers until the licensing route is settled.

### Success looks like

You can state either `AGPLv3 route reviewed` or `JUCE commercial route reviewed`, and you know that distribution must remain blocked until its requirements are met.

### Likely error recovery

If the plan limits, revenue/funding rules, or open-source obligations are unclear, stop and contact JUCE sales or obtain legal advice. Do not guess based on a summary.

> **STOP / CHECKPOINT 1:** Send Codex `CHECKPOINT 1 COMPLETE: [AGPLv3 or commercial] route reviewed.` Wait for confirmation.

## Step 2: Install Visual Studio 2026 Community

### Click or type

1. Open [Visual Studio downloads](https://visualstudio.microsoft.com/downloads/).
2. Under **Visual Studio 2026**, click **Free download** beneath **Community**.
3. Open the downloaded `VisualStudioSetup.exe`.
4. If Windows asks whether to allow changes, click **Yes**.
5. Wait for **Visual Studio Installer** to open.
6. Click the **Workloads** tab.
7. Tick **Desktop development with C++**.
8. In **Installation details** on the right, make sure these are selected:
   - **MSVC v145 - VS 2026 C++ x64/x86 build tools**
   - A current **Windows 11 SDK** or **Windows 10 SDK**
   - **C++ CMake tools for Windows**
9. Click the **Individual components** tab and search for each missing item by name if necessary.
10. Leave the installation location at its default unless there is a specific reason to change it.
11. Click **Install while downloading**.
12. Wait for installation to finish, then restart Windows if prompted.

Microsoft's supported workload is **Desktop development with C++**; Visual Studio alone without this workload cannot build the project.

### Success looks like

The Start menu contains both **Visual Studio 2026** and **Developer PowerShell for VS 2026**. Visual Studio Installer shows **Desktop development with C++** as installed.

### Likely error recovery

- If only Visual Studio Code appears, that is a different program. Return to the Visual Studio 2026 installer.
- If installation fails, click **Retry** in Visual Studio Installer after restarting Windows.
- If the Windows SDK is missing, open Visual Studio Installer, click **Modify**, select the SDK under **Individual components**, and click **Modify**.
- If disk space is low, free at least 15 GB and retry.

> **STOP / CHECKPOINT 2:** Send Codex `CHECKPOINT 2 COMPLETE: Visual Studio 2026 C++ workload installed.` Wait for confirmation.

## Step 3: Verify The Compiler, CMake, And Git

### Click or type

1. Open the Windows Start menu.
2. Type `Developer PowerShell for VS 2026`.
3. Click **Developer PowerShell for VS 2026**. Do not use Command Prompt for this check.
4. Type each command separately and press Enter after each one:

```powershell
cl
```

```powershell
cmake --version
```

```powershell
git --version
```

### Success looks like

- `cl` prints `Microsoft (R) C/C++ Optimizing Compiler` and a version beginning with `19`. It may then complain that no source files were supplied; that part is expected.
- `cmake --version` reports CMake 3.22 or newer.
- `git --version` reports a version number.

### Likely error recovery

- If `cl` is not recognized, close the window and reopen **Developer PowerShell for VS 2026**. If it still fails, modify Visual Studio and add MSVC v145.
- If `cmake` is not recognized, modify Visual Studio and add **C++ CMake tools for Windows**.
- If `git` is not recognized, install [Git for Windows](https://git-scm.com/download/win), accept its default options, then reopen Developer PowerShell.
- If CMake is older than 3.22, update Visual Studio Installer or install a current CMake from [cmake.org](https://cmake.org/download/).

> **STOP / CHECKPOINT 3:** Send Codex the three version lines and `CHECKPOINT 3 COMPLETE.` Wait for confirmation.

## Step 4: Check Microsoft Edge WebView2 Runtime

The plugin embeds its HTML interface with JUCE 8's WebView2 backend.

### Click or type

1. Press `Windows + I` to open **Settings**.
2. Click **Apps**.
3. Click **Installed apps**.
4. In the search box, type `WebView2`.
5. Look for **Microsoft Edge WebView2 Runtime**.

If it is missing:

1. Open [Microsoft WebView2 downloads](https://developer.microsoft.com/microsoft-edge/webview2/).
2. Find **Evergreen Standalone Installer**.
3. Under **x64**, click **Download**.
4. Accept the license terms.
5. Open the downloaded installer and click **Yes** when Windows asks for permission.
6. Return to **Installed apps** and search again.

### Success looks like

**Microsoft Edge WebView2 Runtime** appears in Installed apps.

### Likely error recovery

- If the installer says WebView2 is already installed, that is success; restart Windows if it still does not appear in Settings.
- If installation is blocked by policy, send Codex the exact message and contact the computer administrator.
- If a later build succeeds but the plugin window is blank, return here and use **Modify** or **Repair** on WebView2 Runtime.

> **STOP / CHECKPOINT 4:** Send Codex `CHECKPOINT 4 COMPLETE: WebView2 Runtime is installed.` Wait for confirmation.

## Step 5: Download The Pinned JUCE 8 Source

This project deliberately uses JUCE 8, not JUCE 9. The commands below pin JUCE `8.0.15` so a future JUCE release cannot silently change the build.

### Click or type

1. Open **Developer PowerShell for VS 2026**.
2. Type these commands one at a time:

```powershell
New-Item -ItemType Directory -Path C:\SDK -Force
```

```powershell
git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git C:\SDK\JUCE
```

```powershell
git -C C:\SDK\JUCE describe --tags --always
```

```powershell
Test-Path C:\SDK\JUCE\CMakeLists.txt
```

### Success looks like

The third command prints `8.0.15`, and the last command prints `True`.

### Likely error recovery

- If Git says `destination path ... already exists`, do not delete it. Run the last two commands and send Codex their output.
- If the version is not `8.0.15`, stop and let Codex inspect the existing JUCE folder.
- If cloning fails with a network or certificate error, confirm GitHub opens in a browser, then retry once. Send Codex the complete error if it fails again.
- If access to `C:\SDK` is denied, reopen Developer PowerShell with **Run as administrator**, create the folder, then return to a normal Developer PowerShell.

> **STOP / CHECKPOINT 5:** Send Codex `CHECKPOINT 5 COMPLETE: JUCE 8.0.15 is at C:\SDK\JUCE.` Wait for confirmation.

## Step 6: Run The Code-Completion Preflight

This step intentionally fails if MIDI output or another required native declaration has not been completed.

### Click or type

In **Developer PowerShell for VS 2026**, enter:

```powershell
Set-Location "C:\Users\YourName\Documents\Codex\2026-06-17\files-mentioned-by-the-user-index\outputs\quiet-machine-vx\juce"
```

Then run these one at a time:

```powershell
Get-Location
```

```powershell
Test-Path .\CMakeLists.txt
```

```powershell
Select-String -Path .\CMakeLists.txt -Pattern "NEEDS_MIDI_OUTPUT TRUE"
```

```powershell
Select-String -Path .\Source\PluginProcessor.h -Pattern "producesMidi"
```

### Success looks like

- The location ends in `outputs\quiet-machine-vx\juce`.
- `Test-Path` prints `True`.
- The MIDI-output search prints the line containing `NEEDS_MIDI_OUTPUT TRUE`.
- The processor search shows `producesMidi()` returning `true`.

### Likely error recovery

- If either MIDI search is blank or says `false`, stop. The project is not ready for this wizard; ask Codex to finish the code.
- If the path is not found, copy the path from the top of this document exactly, including quotation marks.
- Do not edit either source file manually during this wizard.

> **STOP / CHECKPOINT 6:** Send Codex the four command outputs and `CHECKPOINT 6 COMPLETE.` Wait for confirmation.

## Step 7: Configure The Visual Studio Solution With CMake

The first configure needs an internet connection because this project downloads pinned React and Tailwind files for the embedded editor.

### Click or type

Confirm Developer PowerShell is still in the `juce` directory, then type:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/SDK/JUCE
```

Wait until the command returns to the PowerShell prompt.

### Success looks like

Near the bottom, CMake prints all three of these:

```text
Configuring done
Generating done
Build files have been written to: ...\quiet-machine-vx\juce\build
```

The following command prints `True`:

```powershell
Test-Path .\build\QuietMachineVX.slnx
```

### Likely error recovery

- `Set JUCE_DIR`: repeat the command exactly and confirm `C:\SDK\JUCE\CMakeLists.txt` exists.
- `Could not download`: check the internet connection, VPN, proxy, antivirus, and access to `cdn.tailwindcss.com` and `cdnjs.cloudflare.com`, then rerun configure.
- `generator does not match`: do not delete anything. Rename the old build folder:

```powershell
Rename-Item -LiteralPath .\build -NewName ("build-old-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
```

Then rerun the configure command.

- WebView2 or NuGet error: copy the entire diagnostic to Codex. Do not randomly install packages named WebView2.
- Red text is not automatically a failure. The command has failed only if it ends with `Configuring incomplete, errors occurred` or no solution is generated.

> **STOP / CHECKPOINT 7:** Send Codex the last 20 lines of CMake output and `CHECKPOINT 7 COMPLETE.` Wait for confirmation.

## Step 8: Open The Generated Solution

### Click or type

In Developer PowerShell, type:

```powershell
Start-Process .\build\QuietMachineVX.slnx
```

In Visual Studio:

1. Wait until the status bar stops saying **Loading**.
2. If **Solution Explorer** is hidden, click **View > Solution Explorer**.
3. Find the projects named `QuietMachineVX_Standalone` and `QuietMachineVX_VST3`.

### Success looks like

The title bar includes `QuietMachineVX`, Solution Explorer contains both targets, and there are no dialogs reporting that projects failed to load.

### Likely error recovery

- If Windows asks which app opens `.slnx`, select **Visual Studio 2026** and tick **Always**.
- If the solution file does not exist, return to Step 7.
- If Visual Studio offers to retarget the solution, cancel and send Codex a screenshot; the generated solution should already target the installed v145 toolset.

> **STOP / CHECKPOINT 8:** Send Codex `CHECKPOINT 8 COMPLETE: The solution is open and both targets are visible.` Wait for confirmation.

## Step 9: Build The Debug Standalone Application

### Click or type

1. In Visual Studio's top toolbar, set **Solution Configurations** to **Debug**.
2. Set **Solution Platforms** to **x64**.
3. In Solution Explorer, right-click `QuietMachineVX_Standalone`.
4. Click **Build**.
5. If the Output window is hidden, click **View > Output**.
6. In the Output window's **Show output from** menu, select **Build**.

### Success looks like

The final summary says `1 succeeded, 0 failed` or otherwise reports zero failed projects. This file exists:

```text
C:\Users\YourName\Documents\Codex\2026-06-17\files-mentioned-by-the-user-index\outputs\quiet-machine-vx\juce\build\QuietMachineVX_artefacts\Debug\Standalone\Quiet Machine VX.exe
```

You can verify it in Developer PowerShell:

```powershell
Test-Path ".\build\QuietMachineVX_artefacts\Debug\Standalone\Quiet Machine VX.exe"
```

### Likely error recovery

- If Visual Studio says `Build failed`, do not build repeatedly. In Output, click inside the text, press `Ctrl+A`, then `Ctrl+C`, and send the complete build output to Codex.
- If `QuietMachineWebData.h` is missing, send that exact error; it indicates the binary-data target or dependency is wrong.
- If a React/Tailwind vendor file is missing, rerun Step 7 with internet access.
- If `MSB8020` mentions v145, modify Visual Studio and install MSVC v145.
- Ignore warnings for now only if the build reports zero failures.

> **STOP / CHECKPOINT 9:** Send Codex the build summary and `CHECKPOINT 9 COMPLETE.` Wait for confirmation.

## Step 10: Launch And Test Standalone

### Click or type

1. In Solution Explorer, right-click `QuietMachineVX_Standalone`.
2. Click **Set as Startup Project**.
3. Click **Debug > Start Without Debugging**, or press `Ctrl+F5`.
4. Wait for **Quiet Machine VX** to open.
5. If the JUCE wrapper shows an audio settings button, open it and select your normal Windows output device. Start with a 44.1 kHz or 48 kHz sample rate and a 256-sample buffer.
6. In Quiet Machine VX, select a four-note chord on the keyboard.
7. Select **AUTO GEN**, choose **HOLD**, and press **PLAY**.
8. Raise `DRONE`, `PIANO`, and at least one `LOOP` fader.
9. Move `WOW DEPTH`, `WEAR/DUST`, `SATURATION`, `REVERB`, and `CUTOFF` one at a time.
10. Stop playback, close the app, and reopen it with `Ctrl+F5`.

### Success looks like

- The complete original interface appears rather than a generic JUCE window.
- Audio comes from the selected chord and evolves without incoming MIDI.
- Tape loops enter asynchronously.
- Each tested effect produces an obvious but controlled audible change.
- PLAY and STOP do not click violently or hang.
- Closing and reopening restores the saved patch/state expected by the completed implementation.

### Likely error recovery

- Blank editor: repair/install WebView2 Runtime, restart Windows, and retry.
- Correct UI but silence: verify the audio output device, PLAY state, source faders, mutes, master volume, and selected notes. Then send Codex a screenshot.
- Sound from the browser but not Standalone: close any browser copy. Only the Standalone result counts.
- Generic synth sound, missing tape loops, or inert effects: stop. Native parity is incomplete; do not continue to VST3.
- Crash or hang: reopen Visual Studio, use **Debug > Start Debugging**, reproduce once, then send Codex the exception and Call Stack screenshot.

> **STOP / CHECKPOINT 10:** Send Codex `CHECKPOINT 10 COMPLETE` plus a short report for audio, tape loops, effects, PLAY/STOP, and state recall. Wait for confirmation.

## Step 11: Build The Release VST3

### Click or type

1. Close Quiet Machine VX Standalone.
2. In Visual Studio, change **Solution Configurations** from **Debug** to **Release**.
3. Confirm **Solution Platforms** remains **x64**.
4. In Solution Explorer, right-click `QuietMachineVX_VST3`.
5. Click **Build**.
6. Watch **View > Output > Build** until it finishes.

### Success looks like

The build reports zero failed projects, and this bundle exists:

```text
C:\Users\YourName\Documents\Codex\2026-06-17\files-mentioned-by-the-user-index\outputs\quiet-machine-vx\juce\build\QuietMachineVX_artefacts\Release\VST3\Quiet Machine VX.vst3
```

Verify it in Developer PowerShell:

```powershell
Test-Path ".\build\QuietMachineVX_artefacts\Release\VST3\Quiet Machine VX.vst3\Contents\x86_64-win\Quiet Machine VX.vst3"
```

### Likely error recovery

- If the Debug build passed but Release fails, copy the complete Release build output to Codex. Do not substitute the Debug plugin for DAW testing.
- If the folder exists but the inner `x86_64-win` file does not, the VST3 bundle is incomplete.
- If Visual Studio builds the wrong target, verify that you right-clicked `QuietMachineVX_VST3`, not `QuietMachineVX_Standalone`.

> **STOP / CHECKPOINT 11:** Send Codex the Release build summary and `CHECKPOINT 11 COMPLETE.` Wait for confirmation.

## Step 12: Install The VST3 For Reason

Reason automatically scans the standard system VST3 folder on Windows. Installing there requires administrator access.

### Click or type

1. Close every DAW.
2. Open the Start menu and type `Developer PowerShell for VS 2026`.
3. Right-click **Developer PowerShell for VS 2026**, select **Run as administrator**, and click **Yes** in the Windows prompt.
4. Enter the project directory:

```powershell
Set-Location "C:\Users\YourName\Documents\Codex\2026-06-17\files-mentioned-by-the-user-index\outputs\quiet-machine-vx\juce"
```

5. Run this block exactly:

```powershell
$sourcePlugin = Resolve-Path ".\build\QuietMachineVX_artefacts\Release\VST3\Quiet Machine VX.vst3"
$vstRoot = "C:\Program Files\Common Files\VST3"
$installedPlugin = Join-Path $vstRoot "Quiet Machine VX.vst3"
New-Item -ItemType Directory -Path $vstRoot -Force | Out-Null
if (Test-Path $installedPlugin) {
    Rename-Item -LiteralPath $installedPlugin -NewName ("Quiet Machine VX.vst3.backup-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
Copy-Item -LiteralPath $sourcePlugin.Path -Destination $installedPlugin -Recurse
Test-Path (Join-Path $installedPlugin "Contents\x86_64-win\Quiet Machine VX.vst3")
```

### Success looks like

The last command prints `True`. File Explorer shows the installed bundle at:

```text
C:\Program Files\Common Files\VST3\Quiet Machine VX.vst3
```

This is the VST3 folder Reason scans automatically on Windows. A `.vst3` is a bundle folder, so copy the entire folder, not only the inner binary.

### Likely error recovery

- If `Resolve-Path` fails, return to Step 11 and confirm the Release bundle exists.
- If copying is denied, make sure Reason is closed and confirm the PowerShell title begins with `Administrator:`.
- If an older plugin was present, the script keeps it as a timestamped backup instead of deleting it.

> **STOP / CHECKPOINT 12:** Send Codex `CHECKPOINT 12 COMPLETE: Installed VST3 verification returned True.` Wait for confirmation.

## Step 13: Scan And Open The Plugin In A DAW

These exact clicks use REAPER as the reference host. If you use another DAW, stop and tell Codex its exact name and version so the clicks can be adapted accurately.

### Click or type in REAPER

1. Open REAPER.
2. Click **Options > Preferences**.
3. In the left column, expand **Plug-ins** and click **VST**.
4. In **VST plug-in paths**, confirm this path is present:

```text
C:\Users\YourName\AppData\Local\Programs\Common\VST3
```

5. If missing, click **Edit path list... > Add path...**, select that folder, and click **Select Folder**.
6. Click **Re-scan > Clear cache and re-scan VST paths for all plug-ins**.
7. Wait for scanning to finish, then click **OK**.
8. Click **Insert > Virtual instrument on new track...**.
9. Type `Quiet Machine VX` in the filter box.
10. Double-click **VST3i: Quiet Machine VX (Quiet Machine Audio)**.

### Success looks like

REAPER creates an armed instrument track and opens the full Quiet Machine VX interface. The FX browser identifies it as VST3i.

### Likely error recovery

- Not listed: reopen Preferences and confirm the exact path, then clear the filter and rescan again.
- Scan reports a crash: copy the REAPER scan error and Windows Event Viewer entry to Codex. Do not force-enable a crashing plugin.
- Listed as an effect rather than an instrument: stop; the CMake/plugin declarations are wrong.
- Blank interface: repair WebView2 Runtime and rescan after restarting REAPER.
- Duplicate entries: remove or rename old Quiet Machine VX bundles from other VST3 folders, then clear cache and rescan.

> **STOP / CHECKPOINT 13:** Send Codex `CHECKPOINT 13 COMPLETE: Quiet Machine VX opens as VST3i in REAPER.` Wait for confirmation.

## Step 14: Test Audio, Host State, And Automation

### Click or type in REAPER

1. Press REAPER's main **Play** button.
2. Press **PLAY** inside Quiet Machine VX if the completed design uses an internal run control.
3. Select notes and confirm the autonomous chord movement and tape loops are audible.
4. Click the track's **Trim** button.
5. In the automation parameter list, select `lowpassHz`, `wowDepth`, or another completed native parameter.
6. Add two automation points and move one point up and the other down.
7. Play across the automation and watch the corresponding control move in the plugin.
8. Save with `Ctrl+S`, close REAPER, reopen the project, and play again.

### Success looks like

- Audio is rendered through the DAW, not directly through a browser audio device.
- Host automation changes the sound and updates the visible control.
- Notes, chord banks, seed, mixer, effects, and sequence state survive close/reopen.
- Duplicating the plugin produces an independent instance rather than sharing state.

### Likely error recovery

- Automation changes sound but not the UI: the WebView parameter relay is incomplete.
- UI moves but sound does not change: the parameter is not connected to native DSP.
- Project reload loses notes or sequences: structured state is not in native `ValueTree` state.
- Two instances affect each other: browser `localStorage` is still being used in native mode.
- Stop and report any of these; do not proceed to MIDI testing.

> **STOP / CHECKPOINT 14:** Send Codex `CHECKPOINT 14 COMPLETE` plus results for audio, automation, reload, and two-instance isolation. Wait for confirmation.

## Step 15: Route Quiet Machine MIDI Into Another Instrument

This verifies generated MIDI output, which is separate from Quiet Machine's own audio.

### Click or type in REAPER

1. Open the Quiet Machine track's FX window by clicking its **FX** button.
2. Confirm **Quiet Machine VX** is first in the FX chain.
3. Click **Add** at the bottom-left of the FX window.
4. Choose a second instrument you already own, such as ReaSynth, and click **Add**.
5. Confirm the second instrument appears below Quiet Machine VX in the chain.
6. In Quiet Machine VX, choose a clear chord, select **AUTO GEN**, choose **HOLD**, and start its generator.
7. Temporarily lower Quiet Machine's own audio mixer channels if necessary so the second instrument is easy to identify.

### Success looks like

The second instrument plays the notes generated by Quiet Machine. Changing the selected chord changes the notes received by the second instrument. Stopping or using panic produces matching note-offs with no stuck notes.

### Likely error recovery

- Quiet Machine makes sound but the second instrument does not: recheck Step 6. `NEEDS_MIDI_OUTPUT`, `producesMidi()`, and native `MidiBuffer` output must all be implemented.
- Stuck notes: stop playback, use Quiet Machine's panic control, and report the exact action that caused the stuck note.
- Second instrument plays keyboard input but not generated notes: the scheduler is not writing generated events to the plugin's output MIDI buffer.
- If the host hides VST3 MIDI output entirely, send Codex the REAPER version and plugin pin-connector screenshot.

> **STOP / CHECKPOINT 15:** Send Codex `CHECKPOINT 15 COMPLETE: A second instrument receives generated notes, including note-offs.` Wait for confirmation.

## Step 16: Record The Generated MIDI In REAPER

### Click or type

1. Keep Quiet Machine VX followed by the second instrument on the same track.
2. Right-click the track's red **Record Arm** button.
3. In the menu, choose **Record: output**.
4. In its submenu, choose **Record: output (MIDI)**.
5. Left-click the Record Arm button so it is lit red.
6. Move REAPER's play cursor to an empty area.
7. Press REAPER's main **Record** button or press `Ctrl+R`.
8. Let Quiet Machine generate notes for at least 30 seconds.
9. Press the spacebar to stop.
10. In the recording dialog, click **Save all**.
11. Double-click the new item to open REAPER's MIDI editor.

### Success looks like

A MIDI item appears on the track. The MIDI editor contains note-on events with sensible lengths, no endlessly held notes, and pitches matching Quiet Machine's selected and generated chords. Playing the item back drives the second instrument.

### Likely error recovery

- An audio waveform is recorded: right-click Record Arm again and choose **Record: output (MIDI)**, not stereo output.
- The item is empty: generated events are not reaching the VST3 MIDI output; return to Step 15.
- Notes have zero length or never end: native note-off scheduling is faulty. Stop and send Codex the MIDI item screenshot.
- Both live generation and the recorded item play at once: stop Quiet Machine's generator before playing the recorded item alone.

> **STOP / CHECKPOINT 16:** Send Codex `CHECKPOINT 16 COMPLETE` and report whether the recorded pitches, lengths, and note-offs are correct. Wait for confirmation.

## Step 17: Download pluginval

pluginval deliberately stresses plugin lifecycle, parameters, state, buses, and editor behavior more aggressively than ordinary DAW use.

### Click or type

1. Close REAPER and Visual Studio debugging sessions.
2. Open Developer PowerShell.
3. Run this block exactly:

```powershell
$pluginvalZip = Join-Path $env:TEMP "pluginval_Windows.zip"
New-Item -ItemType Directory -Path C:\Tools\pluginval -Force | Out-Null
Invoke-WebRequest "https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip" -OutFile $pluginvalZip
Expand-Archive -LiteralPath $pluginvalZip -DestinationPath C:\Tools\pluginval -Force
Get-ChildItem C:\Tools\pluginval -Filter pluginval.exe -Recurse | Select-Object -ExpandProperty FullName
```

### Success looks like

The final command prints a full path ending in `pluginval.exe`.

### Likely error recovery

- Download failure: open [pluginval releases](https://github.com/Tracktion/pluginval/releases), expand the latest release's **Assets**, and download `pluginval_Windows.zip` manually.
- `Expand-Archive` says the file is invalid: delete only the temporary zip shown in the error, download it again, and retry.
- No executable found: send Codex the output of `Get-ChildItem C:\Tools\pluginval -Recurse`.
- Windows SmartScreen may appear on first launch. Click **More info**, verify the file came from Tracktion's official GitHub release, then click **Run anyway** only if you are comfortable proceeding.

> **STOP / CHECKPOINT 17:** Send Codex the printed `pluginval.exe` path and `CHECKPOINT 17 COMPLETE.` Wait for confirmation.

## Step 18: Run pluginval At Strictness Level 5

Level 5 is the minimum generally recognized by pluginval for host compatibility.

### Click or type

In Developer PowerShell, run:

```powershell
$pluginvalExe = (Get-ChildItem C:\Tools\pluginval -Filter pluginval.exe -Recurse | Select-Object -First 1).FullName
$quietMachineVst = Join-Path $env:LOCALAPPDATA "Programs\Common\VST3\Quiet Machine VX.vst3"
& $pluginvalExe --strictness-level 5 $quietMachineVst
$LASTEXITCODE
```

Do not touch the plugin window while the automated tests are running.

### Success looks like

pluginval reports that all tests passed, returns to PowerShell, and the final line is `0`.

### Likely error recovery

- Exit code `1`: save the entire console output and send it to Codex. Do not dismiss a failure because the DAW appeared to work.
- Timeout or hang: wait one minute, then capture the last visible test name before closing pluginval.
- Editor test fails or shows blank: check WebView2 and the editor lifecycle.
- State restoration failure: native parameter or structured patch state is incomplete.
- MIDI/bus failure: recheck VST3 MIDI-output declarations and event handling.
- Crash: open Windows **Event Viewer > Windows Logs > Application**, find the pluginval error, and send its faulting-module details to Codex.

> **STOP / CHECKPOINT 18:** Send Codex the pluginval summary, exit code, and `CHECKPOINT 18 COMPLETE.` Wait for confirmation.

## Step 19: Run The Release Gate At Strictness Level 10

Only do this after level 5 passes cleanly and Codex has reviewed the result.

### Click or type

Run:

```powershell
& $pluginvalExe --strictness-level 10 $quietMachineVst
$LASTEXITCODE
```

### Success looks like

All tests pass and the final exit code is `0`. The plugin survives parameter fuzzing, repeated state restoration, editor open/close, bus changes, sample-rate changes, and unusual block sizes.

### Likely error recovery

- Treat every failure as a code defect until investigated.
- Copy the full output into a text file or send it directly to Codex.
- Do not weaken the strictness level, skip tests, or distribute the build to make the failure disappear.
- After any code fix, rebuild Release, reinstall the bundle, clear the DAW/pluginval cache if needed, and repeat Steps 14 through 19.

> **STOP / CHECKPOINT 19:** Send Codex the complete level-10 summary and exit code. Wait for an explicit release-readiness decision.

## Final Completion Criteria

The setup/build path is complete only when all of the following are true:

- JUCE licensing/distribution route has been reviewed.
- JUCE 8.0.15, VS 2026 v145, CMake, Git, and WebView2 are installed.
- Debug Standalone builds and passes the original-engine listening checks.
- Release VST3 builds, installs, scans, and opens as an instrument.
- Native automation and full state recall work across DAW restart and multiple instances.
- Generated MIDI drives a second instrument and records as valid MIDI notes.
- pluginval levels 5 and 10 both return exit code `0`.
- Codex has reviewed the final logs and confirmed release readiness.

Passing this wizard proves that the code-complete project builds and behaves correctly on this Windows development machine. It does not replace testing in additional DAWs, sample rates, buffer sizes, Windows display scales, fresh user accounts, or a clean installer before public distribution.

## Official References

- [Microsoft: Install C++ support in Visual Studio](https://learn.microsoft.com/en-us/cpp/build/vscpp-step-0-installation?view=msvc-170)
- [JUCE: Licensing and JUCE 8 plans](https://juce.com/get-juce/)
- [JUCE 8.0.15 release](https://github.com/juce-framework/JUCE/releases/tag/8.0.15)
- [Microsoft: WebView2 Runtime deployment](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/evergreen-vs-fixed-version)
- [Steinberg: Official Windows VST3 locations](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BLocations.html)
- [Tracktion: pluginval](https://github.com/Tracktion/pluginval)
