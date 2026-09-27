#pragma once

#include <JuceHeader.h>
#include "SourceOutput.h"

#include <array>
#include <cstdint>

namespace qm
{
inline constexpr int keyVoiceCount = 36;

struct KeyVoiceProfile
{
    float detuneCents = 0.0f;
    float pan = 0.0f;
    int oscillatorCount = 2;
    float oscillatorSpreadCents = 7.0f;
};

struct PerformanceSourceEvent
{
    int note = 0;
    int sourceKey = -1;
    int sampleOffset = 0;
};

class NativeAmbientEngine final
{
public:
    NativeAmbientEngine() = default;

    void prepare(double sampleRate, int maximumBlockSize);
    void reset();
    void setVoiceProfiles(const std::array<KeyVoiceProfile, keyVoiceCount>& profiles) noexcept;
    void consumeGeneratedMidi(const juce::MidiBuffer& midi,
                              const PerformanceSourceEvent* sourceEvents = nullptr,
                              int sourceEventCount = 0);
    void process(const SourceOutput& output,
                 int numSamples,
                 juce::AudioProcessorValueTreeState& parameters);

private:
    static constexpr int loopCount = 5;
    static constexpr int maximumChordNotes = 16;
    static constexpr int maximumChordEventsPerBlock = 4096;

    struct ChordEvent
    {
        std::array<int, maximumChordNotes> notes {};
        std::array<int, maximumChordNotes> sourceKeys {};
        int noteCount = 0;
        int sampleOffset = 0;
        bool performanceActive = false;
    };

    struct LoopVoice
    {
        double baseIntervalSeconds = 17.3;
        double samplesUntilTrigger = 0.0;
        double durationSamples = 1.0;
        double attackSamples = 1.0;
        double releaseSamples = 1.0;
        double ageSamples = 0.0;
        double mainPhase = 0.0;
        double companionPhase = 0.0;
        double mainDelta = 0.0;
        double companionDelta = 0.0;
        float lowpassState1 = 0.0f;
        float lowpassState2 = 0.0f;
        float lowpassCoefficient = 1.0f;
        int filterUpdateCountdown = 0;
        float pan = 0.0f;
        float panLeft = 0.70710678f;
        float panRight = 0.70710678f;
        int chordDegree = 0;
        int octave = 0;
        bool active = false;
    };

    struct DroneVoice
    {
        std::array<double, 4> phases {};
        double currentHz = 0.0;
        double targetHz = 0.0;
        float level = 0.0f;
        float targetLevel = 0.0f;
        float pan = 0.0f;
        float targetPan = 0.0f;
        int oscillatorCount = 2;
        float oscillatorSpreadCents = 7.0f;
        std::array<double, 4> oscillatorRatios { 1.0, 1.0, 1.0, 1.0 };
        std::array<float, 4> oscillatorGains {};
        std::array<float, 4> targetOscillatorGains {};
        int sourceKey = -1;
    };

    struct BlockParameters
    {
        float tapeSpeed = 1.0f;
        float tapeAttack = 0.3f;
        float tapeDrive = 0.3f;
        float glideSeconds = 2.0f;
        float masterPitchSemitones = 0.0f;
        int mainWave = 0;
        int companionWave = 1;
        std::array<float, loopCount> loopLevels {};
        float droneLevel = 0.0f;
        float subLevel = 0.0f;
    };

    static float readParameter(const juce::AudioProcessorValueTreeState& state,
                               const char* parameterId) noexcept;
    static float waveform(double phase, int wave) noexcept;
    static double wrapPhase(double phase) noexcept;
    static float equalPowerLeft(float pan) noexcept;
    static float equalPowerRight(float pan) noexcept;
    static int complementaryWave(int mainWave) noexcept;
    static double midiNoteToHz(double midiNote) noexcept;

    BlockParameters readBlockParameters(
        const juce::AudioProcessorValueTreeState& parameters) const noexcept;
    void applyChordEvent(const ChordEvent& event, const BlockParameters& parameters) noexcept;
    void refreshActiveVoiceProfiles(const BlockParameters& parameters) noexcept;
    void triggerLoop(int loopIndex, const BlockParameters& parameters) noexcept;
    float renderLoop(LoopVoice& voice, int mainWave, int companionWave) noexcept;
    KeyVoiceProfile getProfileForSourceKey(int sourceKey) const noexcept;
    float nextRandomFloat() noexcept;

    std::array<LoopVoice, loopCount> loops {};
    std::array<DroneVoice, maximumChordNotes> droneVoices {};
    std::array<ChordEvent, maximumChordEventsPerBlock> pendingChordEvents {};
    std::array<int, maximumChordNotes> currentChord {};
    std::array<int, maximumChordNotes> currentChordSourceKeys {};
    std::array<std::array<int, 128>, 16> heldNoteCounts {};
    std::array<std::array<int, 128>, 16> heldSourceKeys {};
    std::array<KeyVoiceProfile, keyVoiceCount> keyVoiceProfiles {};
    std::array<int, 128> performanceSourceKeyByNote {};
    int pendingChordEventCount = 0;
    int currentChordNoteCount = 0;
    bool performanceChordActive = false;

    double currentSampleRate = 44100.0;
    int preparedMaximumBlockSize = 0;
    uint32_t randomState = 0x514d4145u;

    double subPhase = 0.0;
    double currentSubHz = 0.0;
    double targetSubHz = 0.0;
    float currentSubPan = 0.0f;
    float targetSubPan = 0.0f;
    float tonalEnvelope = 0.0f;
    std::array<juce::SmoothedValue<float>, loopCount> loopLevelSmoothers;
    juce::SmoothedValue<float> droneLevelSmoother;
    juce::SmoothedValue<float> subLevelSmoother;
    juce::SmoothedValue<float> tapeDriveSmoother;
};
}
