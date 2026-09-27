#include "NativeTextureEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace qm
{
namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;
constexpr float minimumFilterHz = 20.0f;
}

void NativeTextureEngine::StateVariableFilter::prepare(double newSampleRate) noexcept
{
    sampleRate = std::max(1.0, newSampleRate);
    reset();
}

void NativeTextureEngine::StateVariableFilter::reset() noexcept
{
    integrator1 = 0.0f;
    integrator2 = 0.0f;
}

void NativeTextureEngine::StateVariableFilter::setCutoffAndQ(float cutoffHz, float q) noexcept
{
    const auto boundedCutoff = juce::jlimit(minimumFilterHz,
                                             static_cast<float>(sampleRate * 0.45),
                                             cutoffHz);
    damping = 1.0f / juce::jmax(0.05f, q);
    const auto g = std::tan(juce::MathConstants<float>::pi
                            * boundedCutoff / static_cast<float>(sampleRate));
    a1 = 1.0f / (1.0f + g * (g + damping));
    a2 = g * a1;
    a3 = g * a2;
}

void NativeTextureEngine::StateVariableFilter::processCore(float input, float& band,
                                                            float& low, float& high) noexcept
{
    // TPT state-variable form remains stable while the swell cutoff moves.
    const auto v3 = input - integrator2;
    band = a1 * integrator1 + a2 * v3;
    low = integrator2 + a2 * integrator1 + a3 * v3;
    high = input - damping * band - low;
    integrator1 = 2.0f * band - integrator1;
    integrator2 = 2.0f * low - integrator2;
}

float NativeTextureEngine::StateVariableFilter::processBandpass(float input) noexcept
{
    float band = 0.0f, low = 0.0f, high = 0.0f;
    processCore(input, band, low, high);
    return band;
}

float NativeTextureEngine::StateVariableFilter::processLowpass(float input) noexcept
{
    float band = 0.0f, low = 0.0f, high = 0.0f;
    processCore(input, band, low, high);
    return low;
}

float NativeTextureEngine::StateVariableFilter::processHighpass(float input) noexcept
{
    float band = 0.0f, low = 0.0f, high = 0.0f;
    processCore(input, band, low, high);
    return high;
}

void NativeTextureEngine::PianoVoice::clear() noexcept
{
    *this = {};
}

void NativeTextureEngine::prepare(double sampleRate, int maxBlock)
{
    currentSampleRate = std::max(1.0, sampleRate);
    maximumBlockSize = juce::jmax(0, maxBlock);

    for (auto& filter : swellFilters)
    {
        filter.prepare(currentSampleRate);
        filter.setCutoffAndQ(800.0f, 0.7f);
    }
    for (auto& filter : hissFilters)
    {
        filter.prepare(currentSampleRate);
        filter.setCutoffAndQ(3000.0f, 0.707f);
    }
    rumbleFilter.prepare(currentSampleRate);
    rumbleFilter.setCutoffAndQ(200.0f, 2.0f);

    swellFrequencyDelta = twoPi * 0.02 / currentSampleRate;
    swellAmplitudeDelta = twoPi * 0.015 / currentSampleRate;
    rumbleAmplitudeDelta = twoPi * 0.08 / currentSampleRate;
    rumblePitchSmoothing = 1.0 - std::exp(-1.0 / (0.45 * currentSampleRate));
    performanceRumblePitchSmoothing = 1.0 - std::exp(-1.0 / (0.02 * currentSampleRate));

    for (auto* smoother : { &pianoLevel, &swellLevel, &rumbleLevel, &staticLevel, &hissLevel })
        smoother->reset(currentSampleRate, 0.02);

    reset();
}

void NativeTextureEngine::reset() noexcept
{
    for (auto& voice : pianoVoices)
        voice.clear();
    for (auto& filter : swellFilters)
        filter.reset();
    for (auto& filter : hissFilters)
        filter.reset();
    rumbleFilter.reset();

    for (auto& channel : heldGeneratedNoteCounts)
        channel.fill(0);
    pendingChordEventCount = 0;
    activeChordNotes.fill(0);
    activeChordCount = 0;
    chordGateOpen = false;
    performanceChordActive = false;
    generativeStep = 0;
    pianoSequenceStep = 0;
    samplesUntilGenerativePulse = currentSampleRate;
    samplesUntilSequenceStep = currentSampleRate * 0.1;
    sequenceModeKnown = false;
    previousSequenceMode = false;
    nextVoiceSerial = 1;

    slowControlCountdown = 0;
    swellFrequencyPhase = 0.0;
    swellAmplitudePhase = 0.0;
    rumbleAmplitudePhase = 0.0;
    rumblePhase1 = 0.0;
    rumblePhase2 = 0.0;
    currentRumbleHz = 36.0;
    targetRumbleHz = 36.0;
    staticTransientLevels.fill(0.0f);
    staticTransientTargets.fill(0.0f);
    staticTransientAttackSamples.fill(0);
    staticTransientReleaseSamples.fill(0);

    pianoLevel.setCurrentAndTargetValue(0.0f);
    swellLevel.setCurrentAndTargetValue(0.0f);
    rumbleLevel.setCurrentAndTargetValue(0.0f);
    staticLevel.setCurrentAndTargetValue(0.0f);
    hissLevel.setCurrentAndTargetValue(0.0f);

    pianoRandomState = 0x514d5658u;
    swellRandomStates = { 0x1982a7c1u, 0x6e624eb7u };
    hissRandomStates = { 0x9e3779b9u, 0x243f6a88u };
    staticRandomStates = { 0xb7e15162u, 0x8aed2a6bu };
}

void NativeTextureEngine::consumeGeneratedMidi(const juce::MidiBuffer& generatedMidi) noexcept
{
    pendingChordEventCount = 0;
    for (const auto metadata : generatedMidi)
    {
        if (metadata.numBytes > 3)
            continue;

        const auto message = metadata.getMessage();
        if (! message.isNoteOnOrOff() && ! message.isAllNotesOff() && ! message.isAllSoundOff())
            continue;

        const auto channel = (size_t) juce::jlimit(1, 16, message.getChannel()) - 1;
        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            heldGeneratedNoteCounts[channel].fill(0);
        }
        else if (message.isNoteOn())
        {
            const auto note = juce::jlimit(0, 127, message.getNoteNumber());
            heldGeneratedNoteCounts[channel][static_cast<std::size_t>(note)] = 1;
        }
        else if (message.isNoteOff())
        {
            const auto note = juce::jlimit(0, 127, message.getNoteNumber());
            auto& count = heldGeneratedNoteCounts[channel][static_cast<std::size_t>(note)];
            count = 0;
        }

        ChordEvent* event = nullptr;
        if (pendingChordEventCount > 0
            && pendingChordEvents[(size_t) (pendingChordEventCount - 1)].sampleOffset
                   == metadata.samplePosition)
        {
            event = &pendingChordEvents[(size_t) (pendingChordEventCount - 1)];
        }
        else if (pendingChordEventCount < maxChordEventsPerBlock)
        {
            event = &pendingChordEvents[(size_t) pendingChordEventCount++];
            *event = {};
            event->sampleOffset = juce::jmax(0, metadata.samplePosition);
        }
        else
        {
            event = &pendingChordEvents.back();
            *event = {};
            event->sampleOffset = juce::jmax(0, metadata.samplePosition);
        }

        event->noteCount = 0;
        for (int note = 0; note < 128 && event->noteCount < maxChordNotes; ++note)
        {
            bool held = false;
            for (const auto& channelNotes : heldGeneratedNoteCounts)
                held = held || channelNotes[(size_t) note] > 0;
            if (held)
                event->notes[(size_t) event->noteCount++] = note;
        }
        event->performanceActive = std::any_of(
            heldGeneratedNoteCounts[0].begin(), heldGeneratedNoteCounts[0].end(),
            [](int count) { return count > 0; });
    }
}

void NativeTextureEngine::setPianoSequence(
    const std::array<std::array<int, 16>, 16>& notes,
    const std::array<int, 16>& counts) noexcept
{
    for (std::size_t step = 0; step < pianoSequenceNotes.size(); ++step)
    {
        pianoSequenceCounts[step] = juce::jlimit(0, 16, counts[step]);
        for (std::size_t note = 0; note < pianoSequenceNotes[step].size(); ++note)
            pianoSequenceNotes[step][note] = juce::jlimit(0, 127, notes[step][note]);
    }
}

void NativeTextureEngine::cacheParameterPointers(
    juce::AudioProcessorValueTreeState& parameters) noexcept
{
    if (parameterPointers.owner == &parameters)
        return;

    parameterPointers.owner = &parameters;
    parameterPointers.pianoGain = parameters.getRawParameterValue(juce::StringRef("pianoGain"));
    parameterPointers.pianoMute = parameters.getRawParameterValue(juce::StringRef("pianoMute"));
    parameterPointers.swellGain = parameters.getRawParameterValue(juce::StringRef("swellGain"));
    parameterPointers.swellMute = parameters.getRawParameterValue(juce::StringRef("swellMute"));
    parameterPointers.rumbleGain = parameters.getRawParameterValue(juce::StringRef("rumbleGain"));
    parameterPointers.rumbleMute = parameters.getRawParameterValue(juce::StringRef("rumbleMute"));
    parameterPointers.staticGain = parameters.getRawParameterValue(juce::StringRef("staticGain"));
    parameterPointers.staticMute = parameters.getRawParameterValue(juce::StringRef("staticMute"));
    parameterPointers.hissGain = parameters.getRawParameterValue(juce::StringRef("hissGain"));
    parameterPointers.hissMute = parameters.getRawParameterValue(juce::StringRef("hissMute"));
    parameterPointers.pianoSequencerEnabled = parameters.getRawParameterValue(
        juce::StringRef("pianoSequencerEnabled"));
    parameterPointers.pianoRateHz = parameters.getRawParameterValue(juce::StringRef("pianoRateHz"));
    parameterPointers.masterPitchSemitones = parameters.getRawParameterValue(
        juce::StringRef("masterPitchSemitones"));
}

float NativeTextureEngine::readParameter(const std::atomic<float>* parameter,
                                         float fallback) noexcept
{
    return parameter != nullptr ? parameter->load(std::memory_order_relaxed) : fallback;
}

float NativeTextureEngine::sourceLevel(const std::atomic<float>* gain,
                                       const std::atomic<float>* mute) noexcept
{
    if (readParameter(mute, 0.0f) > 0.5f)
        return 0.0f;
    const auto value = juce::jlimit(0.0f, 1.0f, readParameter(gain, 0.0f));
    return value * value;
}

void NativeTextureEngine::applyChordEvent(const ChordEvent& event) noexcept
{
    // Keep the last chord available underneath the global amp envelope so
    // noise and texture sources can trail naturally during release.
    if (event.noteCount <= 0)
    {
        chordGateOpen = false;
        performanceChordActive = false;
        return;
    }

    activeChordCount = event.noteCount;
    activeChordNotes = event.notes;
    chordGateOpen = true;
    performanceChordActive = event.performanceActive;
    updateRumbleTarget();
}

void NativeTextureEngine::updateRumbleTarget() noexcept
{
    if (activeChordCount <= 0)
        return;

    auto lowestNote = activeChordNotes[0];
    for (int index = 1; index < activeChordCount; ++index)
        lowestNote = juce::jmin(lowestNote, activeChordNotes[(size_t) index]);

    auto rumbleMidiNote = (double) lowestNote - 24.0 + currentPitchSemitones;
    while (rumbleMidiNote < 24.0)
        rumbleMidiNote += 12.0;
    while (rumbleMidiNote > 40.0)
        rumbleMidiNote -= 12.0;
    const auto frequency = 440.0 * std::pow(2.0, (rumbleMidiNote - 69.0) / 12.0);
    targetRumbleHz = frequency;
}

void NativeTextureEngine::triggerGenerativePulse() noexcept
{
    const auto step16 = generativeStep % 16;
    const auto step15 = generativeStep % 15;
    const auto euclidFiveOfSixteen = (step16 * 5 % 16) < 5;
    const auto euclidFourOfFifteen = (step15 * 4 % 15) < 4;

    if (euclidFiveOfSixteen || euclidFourOfFifteen)
    {
        const auto maximumNotes = juce::jmin(2, activeChordCount);
        const auto notesToPlay = 1 + static_cast<int>(randomUnit(pianoRandomState)
                                                      * static_cast<float>(maximumNotes));
        for (int noteIndex = 0; noteIndex < notesToPlay; ++noteIndex)
        {
            const auto chordIndex = juce::jmin(activeChordCount - 1,
                static_cast<int>(randomUnit(pianoRandomState)
                                 * static_cast<float>(activeChordCount)));
            auto midiNote = activeChordNotes[static_cast<std::size_t>(chordIndex)];
            if (randomUnit(pianoRandomState) > 0.4f)
                midiNote = juce::jmin(127, midiNote + 12);

            const auto delay = static_cast<int>(randomUnit(pianoRandomState)
                                                * 0.15 * currentSampleRate);
            const auto velocity = euclidFiveOfSixteen && euclidFourOfFifteen
                ? 0.6f : 0.3f + randomUnit(pianoRandomState) * 0.2f;
            startPianoVoice(midiNote, velocity, delay, false, 6.0f);
        }
    }

    generativeStep = (generativeStep + 1) % 240;
}

void NativeTextureEngine::triggerSequenceStep(float rateHz) noexcept
{
    const auto step = pianoSequenceStep % 16;
    const auto count = pianoSequenceCounts[static_cast<std::size_t>(step)];
    const auto duration = 0.8f / juce::jmax(0.01f, rateHz);

    for (int index = 0; index < count; ++index)
    {
        const auto storedNote = pianoSequenceNotes[static_cast<std::size_t>(step)]
                                                  [static_cast<std::size_t>(index)];
        const auto midiNote = storedNote <= 35 ? storedNote + 48 : storedNote;
        const auto velocity = 0.5f + randomUnit(pianoRandomState) * 0.2f;
        startPianoVoice(midiNote, velocity, 0, true, duration);
    }

    pianoSequenceStep = (pianoSequenceStep + 1) % 16;
}

void NativeTextureEngine::startPianoVoice(int midiNote, float velocity, int delaySamples,
                                          bool sequenced, float durationSeconds) noexcept
{
    PianoVoice* selected = nullptr;
    std::uint64_t oldestSerial = std::numeric_limits<std::uint64_t>::max();
    for (auto& voice : pianoVoices)
    {
        if (! voice.active)
        {
            selected = &voice;
            break;
        }
        if (voice.serial < oldestSerial)
        {
            oldestSerial = voice.serial;
            selected = &voice;
        }
    }

    if (selected == nullptr)
        return;

    selected->clear();
    selected->active = true;
    selected->sequenced = sequenced;
    selected->delaySamples = juce::jmax(0, delaySamples);
    selected->serial = nextVoiceSerial++;
    selected->frequencyHz = static_cast<float>(440.0
        * std::pow(2.0, (static_cast<double>(midiNote) + currentPitchSemitones - 69.0) / 12.0));
    selected->velocity = juce::jlimit(0.0f, 1.0f, velocity);
    selected->carrierDeltaScale = twoPi / currentSampleRate;
    selected->modulatorDelta = twoPi * selected->frequencyHz * 2.0 / currentSampleRate;
    selected->attackSamples = juce::jmax(1, static_cast<int>(0.02 * currentSampleRate));
    selected->totalSamples = sequenced
        ? juce::jmax(selected->attackSamples + 2,
                     static_cast<int>(durationSeconds * currentSampleRate))
        : juce::jmax(selected->attackSamples + 2,
                     static_cast<int>(6.0 * currentSampleRate));
    selected->releaseStartSample = sequenced
        ? juce::jmax(selected->attackSamples,
                     static_cast<int>(0.7f * static_cast<float>(selected->totalSamples)))
        : selected->attackSamples;

    const auto decaySamples = juce::jmax(1, selected->totalSamples - selected->attackSamples);
    selected->decayMultiplier = std::pow(0.001f, 1.0f / static_cast<float>(decaySamples));
    selected->modulationDepth = juce::jmax(10.0f, selected->frequencyHz * 1.5f);
    const auto modulationSamples = juce::jmax(1, static_cast<int>(0.5 * currentSampleRate));
    selected->modulationDepthMultiplier = std::pow(10.0f / selected->modulationDepth,
                                                    1.0f / static_cast<float>(modulationSamples));
    selected->filterCutoff = 1500.0f;
    const auto filterFallSamples = juce::jmax(1, static_cast<int>(3.0 * currentSampleRate));
    selected->filterCutoffMultiplier = std::pow(300.0f / 1500.0f,
                                                1.0f / static_cast<float>(filterFallSamples));
    selected->filterCoefficient = 1.0f - std::exp(-juce::MathConstants<float>::twoPi
                                                  * selected->filterCutoff
                                                  / static_cast<float>(currentSampleRate));
}

float NativeTextureEngine::renderPianoVoice(PianoVoice& voice) noexcept
{
    if (! voice.active)
        return 0.0f;
    if (voice.delaySamples > 0)
    {
        --voice.delaySamples;
        return 0.0f;
    }
    if (voice.ageSamples >= voice.totalSamples)
    {
        voice.active = false;
        return 0.0f;
    }

    if (voice.ageSamples < voice.attackSamples)
        voice.envelope = static_cast<float>(voice.ageSamples + 1)
                       / static_cast<float>(voice.attackSamples);
    else if (voice.sequenced)
    {
        if (voice.ageSamples < voice.releaseStartSample)
            voice.envelope = 1.0f;
        else
        {
            const auto releaseLength = juce::jmax(1, voice.totalSamples - voice.releaseStartSample);
            voice.envelope = static_cast<float>(voice.totalSamples - voice.ageSamples - 1)
                           / static_cast<float>(releaseLength);
        }
    }
    else
    {
        voice.envelope = voice.ageSamples == voice.attackSamples
            ? 1.0f : voice.envelope * voice.decayMultiplier;
    }

    // The fast-decaying 2:1 FM index and falling lowpass form the glass bell.
    const auto modulator = static_cast<float>(std::sin(voice.modulatorPhase));
    const auto instantaneousHz = static_cast<double>(voice.frequencyHz
        + modulator * voice.modulationDepth);
    const auto carrier = static_cast<float>(std::sin(voice.carrierPhase));
    voice.carrierPhase = wrapRadians(voice.carrierPhase
                                     + instantaneousHz * voice.carrierDeltaScale);
    voice.modulatorPhase = wrapRadians(voice.modulatorPhase + voice.modulatorDelta);

    if (voice.modulationDepth > 10.0f)
        voice.modulationDepth = juce::jmax(10.0f,
            voice.modulationDepth * voice.modulationDepthMultiplier);
    if (voice.filterCutoff > 300.0f)
        voice.filterCutoff = juce::jmax(300.0f,
            voice.filterCutoff * voice.filterCutoffMultiplier);

    if (--voice.filterUpdateCountdown <= 0)
    {
        voice.filterCoefficient = 1.0f - std::exp(-juce::MathConstants<float>::twoPi
                                                  * voice.filterCutoff
                                                  / static_cast<float>(currentSampleRate));
        voice.filterUpdateCountdown = 16;
    }
    voice.filterState += voice.filterCoefficient * (carrier - voice.filterState);

    ++voice.ageSamples;
    return voice.filterState * voice.envelope * voice.velocity * 0.6f;
}

float NativeTextureEngine::renderStatic(std::uint32_t& state,
                                        float& transientLevel,
                                        float& transientTarget,
                                        int& transientAttackSamples,
                                        int& transientReleaseSamples) noexcept
{
    auto value = randomBipolar(state) * 0.02f;
    if (transientAttackSamples > 0)
    {
        transientLevel += (transientTarget - transientLevel)
                        / static_cast<float>(transientAttackSamples);
        --transientAttackSamples;
        if (transientAttackSamples == 0)
            transientReleaseSamples = juce::jmax(1, static_cast<int>(0.003 * currentSampleRate));
    }
    else if (transientReleaseSamples > 0)
    {
        transientLevel += (0.0f - transientLevel)
                        / static_cast<float>(transientReleaseSamples);
        --transientReleaseSamples;
        if (transientReleaseSamples == 0)
        {
            transientLevel = 0.0f;
            transientTarget = 0.0f;
        }
    }

    if (transientAttackSamples == 0 && transientReleaseSamples == 0
        && randomUnit(state) < 0.001f)
    {
        transientLevel = 0.0f;
        transientTarget = randomBipolar(state) * 0.8f;
        transientAttackSamples = juce::jmax(2, static_cast<int>(0.0005 * currentSampleRate));
    }
    else if (transientAttackSamples == 0 && transientReleaseSamples == 0
             && randomUnit(state) < 0.005f)
    {
        transientLevel = 0.0f;
        transientTarget = randomBipolar(state) * 0.3f;
        transientAttackSamples = juce::jmax(2, static_cast<int>(0.00035 * currentSampleRate));
    }

    return value + transientLevel;
}

void NativeTextureEngine::process(const SourceOutput& destination,
                                  int numSamples,
                                  juce::AudioProcessorValueTreeState& parameters) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    if (numSamples <= 0)
    {
        pendingChordEventCount = 0;
        return;
    }

    cacheParameterPointers(parameters);
    currentPitchSemitones = readParameter(parameterPointers.masterPitchSemitones, 0.0f);
    const auto sequenceEnabled = readParameter(parameterPointers.pianoSequencerEnabled, 0.0f) > 0.5f;
    const auto sequenceRate = juce::jlimit(0.5f, 20.0f,
        readParameter(parameterPointers.pianoRateHz, 4.0f));

    if (! sequenceModeKnown || sequenceEnabled != previousSequenceMode)
    {
        samplesUntilGenerativePulse = 0.0;
        samplesUntilSequenceStep = 0.0;
        if (sequenceEnabled)
            pianoSequenceStep = 0;
        previousSequenceMode = sequenceEnabled;
        sequenceModeKnown = true;
    }

    pianoLevel.setTargetValue(sourceLevel(parameterPointers.pianoGain, parameterPointers.pianoMute));

    const auto swellSourceLevel = sourceLevel(parameterPointers.swellGain, parameterPointers.swellMute);
    const auto rumbleSourceLevel = sourceLevel(parameterPointers.rumbleGain, parameterPointers.rumbleMute);
    const auto staticSourceLevel = sourceLevel(parameterPointers.staticGain, parameterPointers.staticMute);
    const auto hissSourceLevel = sourceLevel(parameterPointers.hissGain, parameterPointers.hissMute);
    updateRumbleTarget();
    const auto updateTextureTargets = [&]()
    {
        // These are source trims, not note envelopes. The shared amp ADSR
        // in PluginProcessor is the single gate for the complete machine.
        swellLevel.setTargetValue(swellSourceLevel);
        rumbleLevel.setTargetValue(rumbleSourceLevel);
        staticLevel.setTargetValue(staticSourceLevel);
        hissLevel.setTargetValue(hissSourceLevel);
    };
    updateTextureTargets();

    int nextChordEvent = 0;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (nextChordEvent < pendingChordEventCount
               && pendingChordEvents[(size_t) nextChordEvent].sampleOffset <= sample)
        {
            applyChordEvent(pendingChordEvents[(size_t) nextChordEvent]);
            ++nextChordEvent;
        }

        if (chordGateOpen && activeChordCount > 0)
        {
            if (sequenceEnabled)
            {
                if (samplesUntilSequenceStep <= 0.0)
                {
                    triggerSequenceStep(sequenceRate);
                    samplesUntilSequenceStep += currentSampleRate / sequenceRate;
                }
                samplesUntilSequenceStep -= 1.0;
            }
            else
            {
                if (samplesUntilGenerativePulse <= 0.0)
                {
                    triggerGenerativePulse();
                    samplesUntilGenerativePulse += currentSampleRate * 1.2;
                }
                samplesUntilGenerativePulse -= 1.0;
            }
        }

        float piano = 0.0f;
        for (auto& voice : pianoVoices)
            piano += renderPianoVoice(voice);
        piano *= pianoLevel.getNextValue();

        if (--slowControlCountdown <= 0)
        {
            const auto centreHz = 800.0f + 600.0f
                * static_cast<float>(std::sin(swellFrequencyPhase));
            for (auto& filter : swellFilters)
                filter.setCutoffAndQ(centreHz, 0.7f);
            slowControlCountdown = 64;
        }

        const auto swellAmplitude = 0.5f + 0.3f
            * static_cast<float>(std::sin(swellAmplitudePhase));
        const auto rumbleAmplitude = 0.3f + 0.3f
            * static_cast<float>(std::sin(rumbleAmplitudePhase));
        const auto sawRumble = static_cast<float>((2.0 * rumblePhase1 - 1.0)
                                                  + (2.0 * rumblePhase2 - 1.0));
        const auto rumble = rumbleFilter.processLowpass(sawRumble * rumbleAmplitude)
                          * rumbleLevel.getNextValue();

        std::array<float, 2> swellChannels {};
        std::array<float, 2> hissChannels {};
        std::array<float, 2> staticChannels {};
        const auto currentSwellLevel = swellLevel.getNextValue();
        const auto currentHissLevel = hissLevel.getNextValue();
        const auto currentStaticLevel = staticLevel.getNextValue();
        for (std::size_t channel = 0; channel < swellChannels.size(); ++channel)
        {
            const auto swellNoise = randomBipolar(swellRandomStates[channel]);
            const auto hissNoise = randomBipolar(hissRandomStates[channel]);
            swellChannels[channel] = swellFilters[channel].processBandpass(swellNoise)
                                   * swellAmplitude * currentSwellLevel;
            hissChannels[channel] = hissFilters[channel].processHighpass(hissNoise)
                                  * currentHissLevel;
            staticChannels[channel] = renderStatic(staticRandomStates[channel],
                                                   staticTransientLevels[channel],
                                                   staticTransientTargets[channel],
                                                   staticTransientAttackSamples[channel],
                                                   staticTransientReleaseSamples[channel])
                                    * currentStaticLevel;
        }

        destination.add(SourceIndex::piano, sample, piano, piano);
        destination.add(SourceIndex::swell, sample, swellChannels[0], swellChannels[1]);
        destination.add(SourceIndex::rumble, sample, rumble, rumble);
        destination.add(SourceIndex::staticNoise, sample, staticChannels[0], staticChannels[1]);
        destination.add(SourceIndex::hiss, sample, hissChannels[0], hissChannels[1]);

        swellFrequencyPhase = wrapRadians(swellFrequencyPhase + swellFrequencyDelta);
        swellAmplitudePhase = wrapRadians(swellAmplitudePhase + swellAmplitudeDelta);
        rumbleAmplitudePhase = wrapRadians(rumbleAmplitudePhase + rumbleAmplitudeDelta);
        const auto pitchSmoothing = performanceChordActive
            ? performanceRumblePitchSmoothing : rumblePitchSmoothing;
        currentRumbleHz += (targetRumbleHz - currentRumbleHz) * pitchSmoothing;
        rumblePhase1 = wrapUnit(rumblePhase1 + currentRumbleHz / currentSampleRate);
        rumblePhase2 = wrapUnit(rumblePhase2 + currentRumbleHz * 0.985 / currentSampleRate);
    }

    if (nextChordEvent < pendingChordEventCount)
        applyChordEvent(pendingChordEvents[(size_t) (pendingChordEventCount - 1)]);
    pendingChordEventCount = 0;

    juce::ignoreUnused(maximumBlockSize);
}

std::uint32_t NativeTextureEngine::nextRandom(std::uint32_t& state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float NativeTextureEngine::randomUnit(std::uint32_t& state) noexcept
{
    return static_cast<float>(nextRandom(state) & 0x00ffffffu)
         / static_cast<float>(0x01000000u);
}

float NativeTextureEngine::randomBipolar(std::uint32_t& state) noexcept
{
    return randomUnit(state) * 2.0f - 1.0f;
}

double NativeTextureEngine::wrapRadians(double phase) noexcept
{
    while (phase >= twoPi)
        phase -= twoPi;
    while (phase < 0.0)
        phase += twoPi;
    return phase;
}

double NativeTextureEngine::wrapUnit(double phase) noexcept
{
    while (phase >= 1.0)
        phase -= 1.0;
    while (phase < 0.0)
        phase += 1.0;
    return phase;
}
}
