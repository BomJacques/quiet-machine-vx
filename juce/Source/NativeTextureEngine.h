#pragma once

#include <JuceHeader.h>
#include "SourceOutput.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace qm
{
class NativeTextureEngine final
{
public:
    NativeTextureEngine() = default;

    void prepare(double sampleRate, int maxBlock);
    void reset() noexcept;
    void consumeGeneratedMidi(const juce::MidiBuffer& generatedMidi) noexcept;
    void setPianoSequence(const std::array<std::array<int, 16>, 16>& notes,
                          const std::array<int, 16>& counts) noexcept;
    void process(const SourceOutput& destination,
                 int numSamples,
                 juce::AudioProcessorValueTreeState& parameters) noexcept;

private:
    static constexpr int maxPianoVoices = 64;
    static constexpr int maxChordNotes = 16;
    static constexpr int maxChordEventsPerBlock = 64;

    struct ChordEvent
    {
        std::array<int, maxChordNotes> notes {};
        int noteCount = 0;
        int sampleOffset = 0;
        bool performanceActive = false;
    };

    struct StateVariableFilter
    {
        void prepare(double newSampleRate) noexcept;
        void reset() noexcept;
        void setCutoffAndQ(float cutoffHz, float q) noexcept;
        float processBandpass(float input) noexcept;
        float processLowpass(float input) noexcept;
        float processHighpass(float input) noexcept;

    private:
        void processCore(float input, float& band, float& low, float& high) noexcept;

        double sampleRate = 44100.0;
        float a1 = 1.0f;
        float a2 = 0.0f;
        float a3 = 0.0f;
        float damping = 1.0f;
        float integrator1 = 0.0f;
        float integrator2 = 0.0f;
    };

    struct PianoVoice
    {
        void clear() noexcept;

        bool active = false;
        bool sequenced = false;
        int delaySamples = 0;
        int ageSamples = 0;
        int attackSamples = 1;
        int releaseStartSample = 1;
        int totalSamples = 1;
        int filterUpdateCountdown = 0;
        double carrierPhase = 0.0;
        double modulatorPhase = 0.0;
        double carrierDeltaScale = 0.0;
        double modulatorDelta = 0.0;
        float frequencyHz = 440.0f;
        float velocity = 0.0f;
        float envelope = 0.0f;
        float decayMultiplier = 1.0f;
        float modulationDepth = 10.0f;
        float modulationDepthMultiplier = 1.0f;
        float filterCutoff = 1500.0f;
        float filterCutoffMultiplier = 1.0f;
        float filterCoefficient = 1.0f;
        float filterState = 0.0f;
        std::uint64_t serial = 0;
    };

    struct ParameterPointers
    {
        const juce::AudioProcessorValueTreeState* owner = nullptr;
        std::atomic<float>* pianoGain = nullptr;
        std::atomic<float>* pianoMute = nullptr;
        std::atomic<float>* swellGain = nullptr;
        std::atomic<float>* swellMute = nullptr;
        std::atomic<float>* rumbleGain = nullptr;
        std::atomic<float>* rumbleMute = nullptr;
        std::atomic<float>* staticGain = nullptr;
        std::atomic<float>* staticMute = nullptr;
        std::atomic<float>* hissGain = nullptr;
        std::atomic<float>* hissMute = nullptr;
        std::atomic<float>* pianoSequencerEnabled = nullptr;
        std::atomic<float>* pianoRateHz = nullptr;
        std::atomic<float>* masterPitchSemitones = nullptr;
    };

    void cacheParameterPointers(juce::AudioProcessorValueTreeState& parameters) noexcept;
    static float readParameter(const std::atomic<float>* parameter, float fallback) noexcept;
    static float sourceLevel(const std::atomic<float>* gain,
                             const std::atomic<float>* mute) noexcept;

    void applyChordEvent(const ChordEvent&) noexcept;
    void updateRumbleTarget() noexcept;
    void triggerGenerativePulse() noexcept;
    void triggerSequenceStep(float rateHz) noexcept;
    void startPianoVoice(int midiNote, float velocity, int delaySamples,
                         bool sequenced, float durationSeconds) noexcept;
    float renderPianoVoice(PianoVoice& voice) noexcept;
    float renderStatic(std::uint32_t& state, float& transientLevel,
                       float& transientTarget, int& transientAttackSamples,
                       int& transientReleaseSamples) noexcept;

    static std::uint32_t nextRandom(std::uint32_t& state) noexcept;
    static float randomUnit(std::uint32_t& state) noexcept;
    static float randomBipolar(std::uint32_t& state) noexcept;
    static double wrapRadians(double phase) noexcept;
    static double wrapUnit(double phase) noexcept;

    double currentSampleRate = 44100.0;
    int maximumBlockSize = 0;
    std::array<PianoVoice, maxPianoVoices> pianoVoices {};
    std::uint64_t nextVoiceSerial = 1;

    std::array<std::array<int, 128>, 16> heldGeneratedNoteCounts {};
    std::array<ChordEvent, maxChordEventsPerBlock> pendingChordEvents {};
    int pendingChordEventCount = 0;
    std::array<int, maxChordNotes> activeChordNotes {};
    int activeChordCount = 0;
    bool chordGateOpen = false;
    bool performanceChordActive = false;

    std::array<std::array<int, 16>, 16> pianoSequenceNotes {};
    std::array<int, 16> pianoSequenceCounts {};
    int generativeStep = 0;
    int pianoSequenceStep = 0;
    double samplesUntilGenerativePulse = 0.0;
    double samplesUntilSequenceStep = 0.0;
    bool previousSequenceMode = false;
    bool sequenceModeKnown = false;
    float currentPitchSemitones = 0.0f;

    std::array<StateVariableFilter, 2> swellFilters {};
    std::array<StateVariableFilter, 2> hissFilters {};
    StateVariableFilter rumbleFilter;
    int slowControlCountdown = 0;
    double swellFrequencyPhase = 0.0;
    double swellAmplitudePhase = 0.0;
    double rumbleAmplitudePhase = 0.0;
    double rumblePhase1 = 0.0;
    double rumblePhase2 = 0.0;
    double swellFrequencyDelta = 0.0;
    double swellAmplitudeDelta = 0.0;
    double rumbleAmplitudeDelta = 0.0;
    double currentRumbleHz = 36.0;
    double targetRumbleHz = 36.0;
    double rumblePitchSmoothing = 0.0;
    double performanceRumblePitchSmoothing = 0.0;
    std::array<float, 2> staticTransientLevels {};
    std::array<float, 2> staticTransientTargets {};
    std::array<int, 2> staticTransientAttackSamples {};
    std::array<int, 2> staticTransientReleaseSamples {};

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pianoLevel;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> swellLevel;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> rumbleLevel;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> staticLevel;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> hissLevel;

    std::uint32_t pianoRandomState = 0x514d5658u;
    std::array<std::uint32_t, 2> swellRandomStates { 0x1982a7c1u, 0x6e624eb7u };
    std::array<std::uint32_t, 2> hissRandomStates { 0x9e3779b9u, 0x243f6a88u };
    std::array<std::uint32_t, 2> staticRandomStates { 0xb7e15162u, 0x8aed2a6bu };

    ParameterPointers parameterPointers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativeTextureEngine)
};
}
