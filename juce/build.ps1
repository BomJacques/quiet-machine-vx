param(
    [string]$JuceDir = "C:\SDK\JUCE",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [ValidateSet("Standalone", "VST3", "All")]
    [string]$Target = "Standalone"
)

$ErrorActionPreference = "Stop"
$projectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $projectDir "build"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake was not found. Install Visual Studio's Desktop development with C++ workload and CMake tools."
}

if (-not (Test-Path -LiteralPath (Join-Path $JuceDir "CMakeLists.txt"))) {
    throw "JUCE was not found at '$JuceDir'. Pass -JuceDir with the path to a JUCE 8 checkout."
}

& cmake -S $projectDir -B $buildDir "-DJUCE_DIR=$JuceDir"
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

$cmakeTarget = switch ($Target) {
    "Standalone" { "QuietMachineVX_Standalone" }
    "VST3" { "QuietMachineVX_VST3" }
    default { "QuietMachineVX_All" }
}

& cmake --build $buildDir --config $Configuration --target $cmakeTarget
if ($LASTEXITCODE -ne 0) { throw "The $cmakeTarget build failed." }

Write-Host "Build complete: $buildDir\QuietMachineVX_artefacts\$Configuration"

