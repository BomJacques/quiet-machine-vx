# Quiet Machine VX

Ambient instrument with a JUCE VST3/standalone audio engine and embedded web editor.
Current source version: 0.6.2.

Open `index.html` for the browser prototype. Native builds render audio and handle
MIDI inside the JUCE processor. See [native build instructions](juce/README.md).

```powershell
cmake -S juce -B build -DJUCE_DIR=C:/SDK/JUCE
cmake --build build --config Release
```

The native Windows editor requires WebView2. JUCE and its licensing terms apply
separately. This repository contains source, tests and documentation; packaged
plugins, build caches and old release copies are not committed.
