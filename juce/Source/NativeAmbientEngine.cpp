#include "NativeAmbientEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace qm
{
namespace
{
constexpr std::array<double, 5> loopIntervals {
    17.3, 21.1, 23.5, 29.8, 37.0
};

constexpr std::array<float, 5> loopPans {
    -0.72f, -0.36f, 0.0f, 0.36f, 0.72f
};

constexpr std::array<int, 5> loopOctaves {
    0, 0, 1, 1, -1
};

constexpr std::array<const char*, 5> loopGainIds {
    "tape1Gain", "tape2Gain", "tape3Gain", "tape4Gain", "tape5Gain"
};

constexpr std::array<const char*, 5> loopMuteIds {
    "tape1Mute", "tape2Mute", "tape3Mute", "tape4Mute", "tape5Mute"
};

constexpr double twoPi = juce::MathConstants<double>::twoPi;
}

void NativeAmbientEngine::prepare(double sampleRate, int maximumBlockSize)
{
    currentSampleRate = juce::jmax(1.0, sampleRate);
    preparedMaximumBlockSize = juce::jmax(0, maximumBlockSize);
    for (auto& smoother : loopLevelSmoothers)
        smoother.reset(currentSampleRate, 0.02);
    droneLevelSmoother.reset(currentSampleRate, 0.02);
    subLevelSmoother.reset(currentSampleRate, 0.02);
    tapeDriveSmoother.reset(currentSampleRate, 0.02);
    reset();
}

void NativeAmbientEngine::reset()
{
    pendingChordEventCount = 0;
    currentChordNoteCount = 0;
    performanceChordActive = false;
    currentChord.fill(0);
    currentChordSourceKeys.fill(-1);
    for (auto& channel : heldNoteCounts)
        channel.fill(0);
    for (auto& channel : heldSourceKeys)
        channel.fill(-1);
    performanceSourceKeyByNote.fill(-1);
    randomState = 0x514d4145u;

    for (int index = 0; index < loopCount; ++index)
    {
        auto& loop = loops[(size_t) index];
        loop = {};
        loop.baseIntervalSeconds = loopIntervals[(size_t) index];
        loop.samplesUntilTrigger = index * 2.0 * currentSampleRate;
        loop.pan = loopPans[(size_t) index];
        loop.panLeft = equalPowerLeft(loop.pan);
        loop.panRight = equalPowerRight(loop.pan);
        loop.chordDegree = index;
        loop.octave = loopOctaves[(size_t) index];
    }

    for (auto& voice : droneVoices)
        voice = {};
    subPhase = 0.0;
    currentSubHz = 0.0;
    targetSubHz = 0.0;
    currentSubPan = 0.0f;
    targetSubPan = 0.0f;
    tonalEnvelope = 0.0f;
    for (auto& smoother : loopLevelSmoothers)
        smoother.setCurrentAndTargetValue(0.0f);
    droneLevelSmoother.setCurrentAndTargetValue(0.0f);
    subLevelSmoother.setCurrentAndTargetValue(0.0f);
    tapeDriveSmoother.setCurrentAndTargetValue(0.0f);
}

void NativeAmbientEngine::setVoiceProfiles(
    const std::array<KeyVoiceProfile, keyVoiceCount>& profiles) noexcept
{
    for (int index = 0; index < keyVoiceCount; ++index)
    {
        const auto& source = profiles[(size_t) index];
        auto& destination = keyVoiceProfiles[(size_t) index];
        destination.detuneCents = juce::jlimit(-50.0f, 50.0f, source.detuneCents);
        destination.pan = juce::jlimit(-1.0f, 1.0f, source.pan);
        destination.oscillatorCount = juce::jlimit(1, 4, source.oscillatorCount);
        destination.oscillatorSpreadCents = juce::jlimit(
            0.0f, 30.0f, source.oscillatorSpreadCents);
    }

}

void NativeAmbientEngine::consumeGeneratedMidi(const juce::MidiBuffer& midi,
                                                const PerformanceSourceEvent* sourceEvents,
                                                int sourceEventCount)
{
    pendingChordEventCount = 0;

    const auto appendChordSnapshot = [this](int sampleOffset)
    {
        ChordEvent* destination = nullptr;
        if (pendingChordEventCount > 0
            && pendingChordEvents[(size_t) (pendingChordEventCount - 1)].sampleOffset
                   == sampleOffset)
        {
            destination = &pendingChordEvents[(size_t) (pendingChordEventCount - 1)];
        }
        else if (pendingChordEventCount < maximumChordEventsPerBlock)
        {
            destination = &pendingChordEvents[(size_t) pendingChordEventCount++];
            *destination = {};
            destination->sampleOffset = juce::jmax(0, sampleOffset);
        }
        else
        {
            destination = &pendingChordEvents.back();
            *destination = {};
            destination->sampleOffset = juce::jmax(0, sampleOffset);
        }

        destination->noteCount = 0;
        for (int note = 0; note < 128 && destination->noteCount < maximumChordNotes; ++note)
        {
            bool held = false;
            auto sourceKey = -1;
            for (size_t heldChannel = 0; heldChannel < heldNoteCounts.size(); ++heldChannel)
                if (heldNoteCounts[heldChannel][(size_t) note] > 0)
                {
                    held = true;
                    sourceKey = heldSourceKeys[heldChannel][(size_t) note];
                    if (heldChannel == 0)
                        break;
                }
            if (held)
            {
                destination->sourceKeys[(size_t) destination->noteCount] = sourceKey;
                destination->notes[(size_t) destination->noteCount++] = note;
            }
        }
        destination->performanceActive = std::any_of(
            heldNoteCounts[0].begin(), heldNoteCounts[0].end(),
            [](int count) { return count > 0; });
    };

    auto nextSourceEvent = 0;
    const auto applySourceEventsThrough = [&](int sampleOffset)
    {
        while (nextSourceEvent < sourceEventCount
               && sourceEvents != nullptr
               && sourceEvents[nextSourceEvent].sampleOffset <= sampleOffset)
        {
            const auto& event = sourceEvents[nextSourceEvent++];
            const auto note = (size_t) juce::jlimit(0, 127, event.note);
            performanceSourceKeyByNote[note] = juce::jlimit(
                -1, keyVoiceCount - 1, event.sourceKey);
            if (heldNoteCounts[0][note] > 0)
                heldSourceKeys[0][note] = performanceSourceKeyByNote[note];
            appendChordSnapshot(event.sampleOffset);
        }
    };

    for (const auto metadata : midi)
    {
        if (metadata.numBytes > 3)
            continue;

        const auto message = metadata.getMessage();
        if (! message.isNoteOnOrOff() && ! message.isAllNotesOff() && ! message.isAllSoundOff())
            continue;

        applySourceEventsThrough(metadata.samplePosition);

        const auto channel = (size_t) juce::jlimit(1, 16, message.getChannel()) - 1;
        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            heldNoteCounts[channel].fill(0);
            heldSourceKeys[channel].fill(-1);
        }
        else
        {
            const auto note = (size_t) juce::jlimit(0, 127, message.getNoteNumber());
            if (message.isNoteOn())
            {
                heldNoteCounts[channel][note] = 1;
                const auto internalSourceKey = (int) note - 48;
                heldSourceKeys[channel][note] = channel == 0
                    ? performanceSourceKeyByNote[note]
                    : (channel == 15
                           && juce::isPositiveAndBelow(internalSourceKey, keyVoiceCount)
                        ? internalSourceKey : -1);
            }
            else
            {
                heldNoteCounts[channel][note] = 0;
                heldSourceKeys[channel][note] = -1;
            }
        }

        appendChordSnapshot(metadata.samplePosition);
    }

    applySourceEventsThrough(std::numeric_limits<int>::max());
}

void NativeAmbientEngine::process(const SourceOutput& output,
                                  int numSamples,
                                  juce::AudioProcessorValueTreeState& parameters)
{
    if (numSamples <= 0)
    {
        pendingChordEventCount = 0;
        return;
    }

    juce::ignoreUnused(preparedMaximumBlockSize);
    const auto block = readBlockParameters(parameters);
    for (int index = 0; index < loopCount; ++index)
        loopLevelSmoothers[(size_t) index].setTargetValue(block.loopLevels[(size_t) index]);
    droneLevelSmoother.setTargetValue(block.droneLevel);
    subLevelSmoother.setTargetValue(block.subLevel);
    tapeDriveSmoother.setTargetValue(block.tapeDrive);
    refreshActiveVoiceProfiles(block);

    int nextChordEvent = 0;
    const auto generatorGlideCoefficient = block.glideSeconds <= 0.001f
        ? 1.0 : 1.0 - std::exp(-1.0 / (block.glideSeconds * currentSampleRate));
    constexpr auto performanceGlideSeconds = 0.02;
    const auto performanceGlideCoefficient = 1.0
        - std::exp(-1.0 / (performanceGlideSeconds * currentSampleRate));
    const auto tonalAttackCoefficient = 1.0 - std::exp(-1.0 / (0.005 * currentSampleRate));
    const auto tonalReleaseCoefficient = 1.0 - std::exp(-1.0 / (0.005 * currentSampleRate));
    const auto droneVoiceAttackCoefficient = 1.0 - std::exp(-1.0 / (0.005 * currentSampleRate));
    const auto droneVoiceReleaseCoefficient = 1.0 - std::exp(-1.0 / (0.04 * currentSampleRate));
    const auto panSmoothingCoefficient = 1.0 - std::exp(-1.0 / (0.02 * currentSampleRate));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (nextChordEvent < pendingChordEventCount
               && pendingChordEvents[(size_t) nextChordEvent].sampleOffset <= sample)
        {
            applyChordEvent(pendingChordEvents[(size_t) nextChordEvent], block);
            ++nextChordEvent;
        }

        float loopLeft = 0.0f;
        float loopRight = 0.0f;
        for (int index = 0; index < loopCount; ++index)
        {
            auto& loop = loops[(size_t) index];
            if (loop.samplesUntilTrigger <= 0.0)
                triggerLoop(index, block);

            loop.samplesUntilTrigger -= 1.0;
            auto signal = renderLoop(loop, block.mainWave, block.companionWave);
            const auto tapeDrive = tapeDriveSmoother.getNextValue();
            if (tapeDrive > 0.0001f)
            {
                const auto drive = 1.0f + tapeDrive * 8.0f;
                const auto saturated = std::tanh(signal * drive) / std::tanh(drive);
                signal += (saturated - signal) * tapeDrive;
            }
            signal *= loopLevelSmoothers[(size_t) index].getNextValue();
            const auto sourceLeft = signal * loop.panLeft;
            const auto sourceRight = signal * loop.panRight;
            loopLeft += sourceLeft;
            loopRight += sourceRight;
            output.add(static_cast<SourceIndex>(index), sample, sourceLeft, sourceRight);
        }

        const auto glideCoefficient = performanceChordActive
            ? performanceGlideCoefficient : generatorGlideCoefficient;
        currentSubHz += (targetSubHz - currentSubHz) * glideCoefficient;

        const auto envelopeTarget = currentChordNoteCount > 0 ? 1.0f : 0.0f;
        const auto envelopeCoefficient = envelopeTarget > tonalEnvelope
            ? tonalAttackCoefficient : tonalReleaseCoefficient;
        tonalEnvelope += (envelopeTarget - tonalEnvelope) * (float) envelopeCoefficient;

        float droneLeft = 0.0f;
        float droneRight = 0.0f;
        float droneLevelEnergy = 0.0f;
        for (auto& voice : droneVoices)
        {
            voice.currentHz += (voice.targetHz - voice.currentHz) * glideCoefficient;
            voice.pan += (voice.targetPan - voice.pan) * (float) panSmoothingCoefficient;
            const auto levelCoefficient = voice.targetLevel > voice.level
                ? droneVoiceAttackCoefficient : droneVoiceReleaseCoefficient;
            voice.level += (voice.targetLevel - voice.level) * (float) levelCoefficient;
            if (voice.level < 0.00001f && voice.targetLevel <= 0.0f)
                continue;

            float oscillatorMix = 0.0f;
            float oscillatorWeight = 0.0f;
            for (int oscillator = 0; oscillator < 4; ++oscillator)
            {
                auto& gain = voice.oscillatorGains[(size_t) oscillator];
                gain += (voice.targetOscillatorGains[(size_t) oscillator] - gain)
                      * (float) panSmoothingCoefficient;
                const auto wave = (oscillator & 1) == 0
                    ? block.mainWave : block.companionWave;
                oscillatorMix += waveform(voice.phases[(size_t) oscillator], wave) * gain;
                oscillatorWeight += gain;
                voice.phases[(size_t) oscillator] = wrapPhase(
                    voice.phases[(size_t) oscillator]
                    + twoPi * voice.currentHz * voice.oscillatorRatios[(size_t) oscillator]
                        / currentSampleRate);
            }
            oscillatorMix /= juce::jmax(0.0001f, oscillatorWeight);
            const auto pan = juce::jlimit(-1.0f, 1.0f, voice.pan);
            droneLeft += oscillatorMix * voice.level * std::sqrt(1.0f - pan);
            droneRight += oscillatorMix * voice.level * std::sqrt(1.0f + pan);
            droneLevelEnergy += voice.level * voice.level;
        }
        const auto droneNormalisation = 1.0f / std::sqrt(juce::jmax(1.0f, droneLevelEnergy));
        const auto droneLevel = droneLevelSmoother.getNextValue()
                              * tonalEnvelope * 0.07f * droneNormalisation;
        droneLeft *= droneLevel;
        droneRight *= droneLevel;
        currentSubPan += (targetSubPan - currentSubPan) * (float) panSmoothingCoefficient;
        const auto subPan = juce::jlimit(-0.35f, 0.35f, currentSubPan);
        const auto subSignal = (float) std::sin(subPhase)
                             * subLevelSmoother.getNextValue() * tonalEnvelope * 0.08f;
        const auto subLeft = subSignal * std::sqrt(1.0f - subPan);
        const auto subRight = subSignal * std::sqrt(1.0f + subPan);

        subPhase = wrapPhase(subPhase + twoPi * currentSubHz / currentSampleRate);

        output.add(SourceIndex::drone, sample, droneLeft, droneRight);
        output.add(SourceIndex::sub, sample, subLeft, subRight);
        juce::ignoreUnused(loopLeft, loopRight);
    }

    if (nextChordEvent < pendingChordEventCount)
        applyChordEvent(pendingChordEvents[(size_t) (pendingChordEventCount - 1)], block);
    pendingChordEventCount = 0;
}

float NativeAmbientEngine::readParameter(const juce::AudioProcessorValueTreeState& state,
                                         const char* parameterId) noexcept
{
    if (const auto* value = state.getRawParameterValue(parameterId))
        return value->load(std::memory_order_relaxed);
    return 0.0f;
}

NativeAmbientEngine::BlockParameters NativeAmbientEngine::readBlockParameters(
    const juce::AudioProcessorValueTreeState& parameters) const noexcept
{
    BlockParameters result;
    result.tapeSpeed = juce::jlimit(0.05f, 8.0f, readParameter(parameters, "tapeSpeed"));
    result.tapeAttack = juce::jlimit(0.05f, 0.8f, readParameter(parameters, "tapeAttack"));
    result.tapeDrive = juce::jlimit(0.0f, 1.0f, readParameter(parameters, "tapeDrive"));
    result.glideSeconds = juce::jlimit(0.0f, 10.0f, readParameter(parameters, "glideSeconds"));
    result.masterPitchSemitones = juce::jlimit(
        -24.0f, 24.0f, readParameter(parameters, "masterPitchSemitones"));
    result.mainWave = juce::jlimit(0, 3,
        juce::roundToInt(readParameter(parameters, "oscillatorWave")));
    result.companionWave = complementaryWave(result.mainWave);

    for (int index = 0; index < loopCount; ++index)
    {
        const auto muted = readParameter(parameters, loopMuteIds[(size_t) index]) > 0.5f;
        const auto gain = juce::jlimit(
            0.0f, 1.0f, readParameter(parameters, loopGainIds[(size_t) index]));
        result.loopLevels[(size_t) index] = muted ? 0.0f : gain * gain;
    }

    const auto droneMuted = readParameter(parameters, "droneMute") > 0.5f;
    const auto droneGain = juce::jlimit(0.0f, 1.0f, readParameter(parameters, "droneGain"));
    result.droneLevel = droneMuted ? 0.0f : droneGain * droneGain;

    const auto subMuted = readParameter(parameters, "subMute") > 0.5f;
    const auto subGain = juce::jlimit(0.0f, 1.0f, readParameter(parameters, "subGain"));
    result.subLevel = subMuted ? 0.0f : subGain * subGain;
    return result;
}

void NativeAmbientEngine::applyChordEvent(const ChordEvent& event,
                                          const BlockParameters& parameters) noexcept
{
    if (event.noteCount <= 0)
    {
        // Leave the last chord and voice targets alive. The processor-level
        // amp ADSR now controls the release for every source, including the
        // tape-like noise and harmonic tails.
        performanceChordActive = false;
        return;
    }

    currentChordNoteCount = juce::jmin(maximumChordNotes, event.noteCount);
    performanceChordActive = event.performanceActive;
    for (int index = 0; index < currentChordNoteCount; ++index)
    {
        currentChord[(size_t) index] = event.notes[(size_t) index];
        currentChordSourceKeys[(size_t) index] = event.sourceKeys[(size_t) index];
    }

    for (int index = 0; index < currentChordNoteCount; ++index)
    {
        auto& voice = droneVoices[(size_t) index];
        voice.targetLevel = 1.0f;
    }
    for (int index = currentChordNoteCount; index < maximumChordNotes; ++index)
        droneVoices[(size_t) index].targetLevel = 0.0f;
    refreshActiveVoiceProfiles(parameters);
    for (int index = 0; index < currentChordNoteCount; ++index)
        if (droneVoices[(size_t) index].currentHz <= 0.0)
            droneVoices[(size_t) index].currentHz = droneVoices[(size_t) index].targetHz;
    if (currentSubHz <= 0.0)
        currentSubHz = targetSubHz;
}

void NativeAmbientEngine::refreshActiveVoiceProfiles(
    const BlockParameters& parameters) noexcept
{
    if (currentChordNoteCount <= 0)
        return;

    for (int index = 0; index < currentChordNoteCount; ++index)
    {
        auto& voice = droneVoices[(size_t) index];
        const auto profile = getProfileForSourceKey(currentChordSourceKeys[(size_t) index]);
        voice.sourceKey = currentChordSourceKeys[(size_t) index];
        voice.targetHz = midiNoteToHz(
            (double) currentChord[(size_t) index] + parameters.masterPitchSemitones - 12.0
            + (double) profile.detuneCents / 100.0);
        voice.targetPan = profile.pan;
        voice.oscillatorCount = juce::jlimit(1, 4, profile.oscillatorCount);
        voice.oscillatorSpreadCents = profile.oscillatorSpreadCents;

        for (int oscillator = 0; oscillator < 4; ++oscillator)
        {
            const auto position = voice.oscillatorCount <= 1
                ? 0.0
                : (double) oscillator / (double) (voice.oscillatorCount - 1) - 0.5;
            const auto spreadCents = oscillator < voice.oscillatorCount
                ? position * (double) profile.oscillatorSpreadCents : 0.0;
            voice.oscillatorRatios[(size_t) oscillator] = std::pow(
                2.0, spreadCents / 1200.0);
            voice.targetOscillatorGains[(size_t) oscillator]
                = oscillator < voice.oscillatorCount ? 1.0f : 0.0f;
        }
    }

    const auto rootProfile = getProfileForSourceKey(currentChordSourceKeys[0]);
    targetSubHz = midiNoteToHz(
        (double) currentChord[0] + parameters.masterPitchSemitones - 24.0
        + (double) rootProfile.detuneCents / 100.0);
    targetSubPan = juce::jlimit(-0.35f, 0.35f, rootProfile.pan * 0.35f);
}

KeyVoiceProfile NativeAmbientEngine::getProfileForSourceKey(int sourceKey) const noexcept
{
    if (juce::isPositiveAndBelow(sourceKey, keyVoiceCount))
        return keyVoiceProfiles[(size_t) sourceKey];
    return {};
}

void NativeAmbientEngine::triggerLoop(int loopIndex,
                                      const BlockParameters& parameters) noexcept
{
    auto& loop = loops[(size_t) loopIndex];
    const auto intervalSeconds = loop.baseIntervalSeconds / parameters.tapeSpeed;
    loop.samplesUntilTrigger += intervalSeconds * currentSampleRate;

    if (currentChordNoteCount <= 0)
        return;

    const auto degree = loop.chordDegree % currentChordNoteCount;
    const auto octaveSemitones = loop.octave * 12;
    const auto midiNote = (double) currentChord[(size_t) degree]
                        + octaveSemitones + parameters.masterPitchSemitones;
    const auto frequency = midiNoteToHz(midiNote);

    loop.mainDelta = twoPi * frequency / currentSampleRate;
    loop.companionDelta = twoPi * frequency * 0.995 / currentSampleRate;
    loop.durationSamples = juce::jmax(1.0, intervalSeconds * 0.8 * currentSampleRate);
    loop.attackSamples = juce::jmax(1.0, loop.durationSamples * parameters.tapeAttack);
    loop.releaseSamples = juce::jmax(1.0, loop.durationSamples * 0.4);
    loop.ageSamples = 0.0;
    loop.lowpassState1 = 0.0f;
    loop.lowpassState2 = 0.0f;
    loop.filterUpdateCountdown = 0;
    loop.active = true;

    if (nextRandomFloat() > 0.6f)
    {
        const auto direction = nextRandomFloat() > 0.5f ? 1 : -1;
        loop.chordDegree = (loop.chordDegree + direction + currentChordNoteCount)
                         % currentChordNoteCount;
    }
}

float NativeAmbientEngine::renderLoop(LoopVoice& loop,
                                      int mainWave,
                                      int companionWave) noexcept
{
    if (! loop.active)
        return 0.0f;

    const auto releaseStart = juce::jmax(loop.attackSamples,
                                         loop.durationSamples - loop.releaseSamples);
    float envelope = 1.0f;
    if (loop.ageSamples < loop.attackSamples)
        envelope = (float) (loop.ageSamples / loop.attackSamples);
    else if (loop.ageSamples >= releaseStart)
        envelope = (float) ((loop.durationSamples - loop.ageSamples - 1.0)
                            / juce::jmax(1.0, loop.durationSamples - releaseStart));
    envelope = juce::jlimit(0.0f, 1.0f, envelope);

    const auto contour = loop.ageSamples < loop.attackSamples
        ? loop.ageSamples / loop.attackSamples
        : 1.0 - (loop.ageSamples - loop.attackSamples)
                    / juce::jmax(1.0, loop.durationSamples - loop.attackSamples);
    if (--loop.filterUpdateCountdown <= 0)
    {
        const auto cutoff = 150.0 * std::pow(1200.0 / 150.0,
                                             juce::jlimit(0.0, 1.0, contour));
        loop.lowpassCoefficient = (float) (1.0 - std::exp(-twoPi * cutoff / currentSampleRate));
        loop.filterUpdateCountdown = 32;
    }

    const auto raw = (waveform(loop.mainPhase, mainWave)
                    + waveform(loop.companionPhase, companionWave)) * 0.5f;
    loop.lowpassState1 += (raw - loop.lowpassState1) * loop.lowpassCoefficient;
    loop.lowpassState2 += (loop.lowpassState1 - loop.lowpassState2) * loop.lowpassCoefficient;
    loop.mainPhase = wrapPhase(loop.mainPhase + loop.mainDelta);
    loop.companionPhase = wrapPhase(loop.companionPhase + loop.companionDelta);
    loop.ageSamples += 1.0;

    if (loop.ageSamples >= loop.durationSamples)
        loop.active = false;
    return loop.lowpassState2 * envelope * 0.06f;
}

float NativeAmbientEngine::waveform(double phase, int wave) noexcept
{
    const auto normalised = phase / twoPi;
    switch (wave)
    {
        case 0: return (float) (2.0 * normalised - 1.0);
        case 1: return phase < juce::MathConstants<double>::pi ? 1.0f : -1.0f;
        case 2: return (float) (2.0 / juce::MathConstants<double>::pi
                                * std::asin(std::sin(phase)));
        default: return (float) std::sin(phase);
    }
}

double NativeAmbientEngine::wrapPhase(double phase) noexcept
{
    while (phase >= twoPi)
        phase -= twoPi;
    while (phase < 0.0)
        phase += twoPi;
    return phase;
}

float NativeAmbientEngine::equalPowerLeft(float pan) noexcept
{
    return std::sqrt(0.5f * (1.0f - juce::jlimit(-1.0f, 1.0f, pan)));
}

float NativeAmbientEngine::equalPowerRight(float pan) noexcept
{
    return std::sqrt(0.5f * (1.0f + juce::jlimit(-1.0f, 1.0f, pan)));
}

int NativeAmbientEngine::complementaryWave(int mainWave) noexcept
{
    switch (mainWave)
    {
        case 0: return 1;
        case 1: return 2;
        case 2: return 3;
        default: return 2;
    }
}

double NativeAmbientEngine::midiNoteToHz(double midiNote) noexcept
{
    return 440.0 * std::pow(2.0, (midiNote - 69.0) / 12.0);
}

float NativeAmbientEngine::nextRandomFloat() noexcept
{
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return (float) (randomState & 0x00ffffffu) / (float) 0x01000000u;
}
}
