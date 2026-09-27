#include "NativeMasterFx.h"

#include <algorithm>
#include <cmath>

namespace qm
{
namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;
constexpr float crossFeedback = 0.4f;
constexpr double delaySecondsLeft = 0.6;
constexpr double delaySecondsRight = 0.8;
constexpr double wowBaseSeconds = 0.05;
constexpr double wowMaximumModulationSeconds = 0.005;
}

void NativeMasterFx::prepare(double sampleRate, int maximumBlockSize)
{
    currentSampleRate = juce::jmax(1.0, sampleRate);
    maximumPreparedBlockSize = juce::jmax(1, maximumBlockSize);

    const auto delaySize = juce::roundToInt(std::ceil(currentSampleRate * 0.82))
                         + maximumPreparedBlockSize + 4;
    const auto wowSize = juce::roundToInt(std::ceil(currentSampleRate * 0.07))
                       + maximumPreparedBlockSize + 4;
    const auto freezeSize = juce::roundToInt(std::ceil(currentSampleRate * 0.21))
                          + maximumPreparedBlockSize + 4;

    delayLeft.assign((size_t) delaySize, 0.0f);
    delayRight.assign((size_t) delaySize, 0.0f);
    wowLeft.assign((size_t) wowSize, 0.0f);
    wowRight.assign((size_t) wowSize, 0.0f);
    freezeLeft.assign((size_t) freezeSize, 0.0f);
    freezeRight.assign((size_t) freezeSize, 0.0f);

    delaySamplesLeft = juce::jmax(1, juce::roundToInt(delaySecondsLeft * currentSampleRate));
    delaySamplesRight = juce::jmax(1, juce::roundToInt(delaySecondsRight * currentSampleRate));

    const juce::dsp::ProcessSpec spec {
        currentSampleRate, (juce::uint32) maximumPreparedBlockSize, 2
    };
    lowpass.prepare(spec);
    lowpass.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    highpass.prepare(spec);
    highpass.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    airHighpass.prepare(spec);
    airHighpass.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    airHighpass.setCutoffFrequency(3000.0f);
    airHighpass.setResonance(0.7071f);
    reverb.prepare(spec);
    outputGain.reset(currentSampleRate, 0.12);
    delayMixSmoother.reset(currentSampleRate, 0.02);
    freezeMixSmoother.reset(currentSampleRate, 0.02);
    lowpassHzSmoother.reset(currentSampleRate, 0.025);
    lowpassQSmoother.reset(currentSampleRate, 0.025);
    highpassHzSmoother.reset(currentSampleRate, 0.025);
    wowDepthSmoother.reset(currentSampleRate, 0.12);
    warpDepthSmoother.reset(currentSampleRate, 0.25);
    wearDepthSmoother.reset(currentSampleRate, 0.25);

    prepared = true;
    reset();
}

void NativeMasterFx::reset()
{
    std::fill(delayLeft.begin(), delayLeft.end(), 0.0f);
    std::fill(delayRight.begin(), delayRight.end(), 0.0f);
    std::fill(wowLeft.begin(), wowLeft.end(), 0.0f);
    std::fill(wowRight.begin(), wowRight.end(), 0.0f);
    std::fill(freezeLeft.begin(), freezeLeft.end(), 0.0f);
    std::fill(freezeRight.begin(), freezeRight.end(), 0.0f);

    lowpass.reset();
    highpass.reset();
    airHighpass.reset();
    reverb.reset();
    outputGain.setCurrentAndTargetValue(1.0f);
    delayMixSmoother.setCurrentAndTargetValue(1.0f);
    freezeMixSmoother.setCurrentAndTargetValue(0.0f);
    lowpassHzSmoother.setCurrentAndTargetValue(2000.0f);
    lowpassQSmoother.setCurrentAndTargetValue(0.5f);
    highpassHzSmoother.setCurrentAndTargetValue(20.0f);
    wowDepthSmoother.setCurrentAndTargetValue(0.0f);
    warpDepthSmoother.setCurrentAndTargetValue(0.0f);
    wearDepthSmoother.setCurrentAndTargetValue(0.0f);

    delayWriteIndex = 0;
    delayWasEnabled = true;
    wowWriteIndex = 0;
    freezeWriteIndex = 0;
    freezeWasEnabled = false;
    freezeStartPosition = 0.0;
    freezeReadPhase = 0.0;
    freezeLengthSamples = juce::jmax(1.0, currentSampleRate * 0.15);
    freezeTargetLengthSamples = freezeLengthSamples;
    samplesUntilFreezeEvolution = currentSampleRate * 2.0;

    sweepPhase = 0.0;
    wowPhase = 0.0;
    warpState = 0.0;
    warpTarget = 0.0;
    samplesUntilWarpTarget = currentSampleRate * 2.0;
    wearNoiseState = 0.0f;

    dropoutActive = false;
    dropoutElapsedSamples = 0.0;
    dropoutDurationSamples = currentSampleRate * 0.1;
    samplesUntilDropout = currentSampleRate * 12.0;
    dropoutDepth = 0.0f;
    randomState = 0x514d4d46u;
}

void NativeMasterFx::process(juce::AudioBuffer<float>& buffer,
                             juce::AudioProcessorValueTreeState& parameters)
{
    if (! prepared || buffer.getNumChannels() != 2 || buffer.getNumSamples() <= 0)
        return;

    const auto block = readBlockParameters(parameters);
    const auto warpSmoothing = 1.0 - std::exp(-1.0 / (4.0 * currentSampleRate));
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    if (block.freezeEnabled && ! freezeWasEnabled)
        beginFreeze();
    freezeWasEnabled = block.freezeEnabled;

    outputGain.setTargetValue(block.masterGain);
    if (block.delayEnabled && ! delayWasEnabled)
    {
        std::fill(delayLeft.begin(), delayLeft.end(), 0.0f);
        std::fill(delayRight.begin(), delayRight.end(), 0.0f);
        delayWriteIndex = 0;
    }
    delayWasEnabled = block.delayEnabled;
    delayMixSmoother.setTargetValue(block.delayEnabled ? 1.0f : 0.0f);
    freezeMixSmoother.setTargetValue(block.freezeEnabled ? 1.0f : 0.0f);
    lowpassHzSmoother.setTargetValue(block.lowpassHz);
    lowpassQSmoother.setTargetValue(block.lowpassQ);
    highpassHzSmoother.setTargetValue(block.highpassHz);
    wowDepthSmoother.setTargetValue(block.wowDepth);
    warpDepthSmoother.setTargetValue(block.warpDepth);
    wearDepthSmoother.setTargetValue(block.tapeWear);
    highpass.setResonance(0.7071f);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto drivenLeft = left[sample];
        const auto drivenRight = right[sample];
        const auto airLeft = airHighpass.processSample(0, drivenLeft) * 0.35f;
        const auto airRight = airHighpass.processSample(1, drivenRight) * 0.35f;

        // Unequal taps feed the opposite delay line, producing the browser graph's
        // slowly widening 0.6/0.8-second cross-feedback field.
        const auto readLeft = wrapIndex(delayWriteIndex - delaySamplesLeft,
                                        (int) delayLeft.size());
        const auto readRight = wrapIndex(delayWriteIndex - delaySamplesRight,
                                         (int) delayRight.size());
        const auto delayedLeft = delayLeft[(size_t) readLeft];
        const auto delayedRight = delayRight[(size_t) readRight];
        const auto delayMix = delayMixSmoother.getNextValue();
        delayLeft[(size_t) delayWriteIndex] = (drivenLeft + delayedRight * crossFeedback)
            * delayMix;
        delayRight[(size_t) delayWriteIndex] = (drivenRight + delayedLeft * crossFeedback)
            * delayMix;
        delayWriteIndex = wrapIndex(delayWriteIndex + 1, (int) delayLeft.size());

        const auto sweepOffset = (float) std::sin(sweepPhase)
                               * block.sweepDepth * 4000.0f;
        lowpass.setResonance(lowpassQSmoother.getNextValue());
        lowpass.setCutoffFrequency(juce::jlimit(80.0f, 18000.0f,
                                                lowpassHzSmoother.getNextValue()
                                                + sweepOffset));
        highpass.setCutoffFrequency(highpassHzSmoother.getNextValue());
        sweepPhase += twoPi * 0.05 / currentSampleRate;
        if (sweepPhase >= twoPi)
            sweepPhase -= twoPi;

        const auto dryMix = 1.0f - delayMix * 0.45f;
        auto filteredLeft = lowpass.processSample(
            0, drivenLeft * dryMix
                 + (delayedLeft * 0.55f + delayedRight * 0.20f) * delayMix);
        auto filteredRight = lowpass.processSample(
            1, drivenRight * dryMix
                 + (delayedRight * 0.55f + delayedLeft * 0.20f) * delayMix);
        filteredLeft = highpass.processSample(0, filteredLeft);
        filteredRight = highpass.processSample(1, filteredRight);
        filteredLeft += airLeft;
        filteredRight += airRight;

        if (! block.freezeEnabled)
        {
            freezeLeft[(size_t) freezeWriteIndex] = filteredLeft;
            freezeRight[(size_t) freezeWriteIndex] = filteredRight;
            freezeWriteIndex = wrapIndex(freezeWriteIndex + 1, (int) freezeLeft.size());
        }

        const auto freezeMix = freezeMixSmoother.getNextValue();
        if (freezeMix > 0.000001f)
        {
            // Freeze closes the live input and recirculates the captured grain.
            // A short tail/head crossfade keeps each wrap smooth as its length evolves.
            updateFreezeEvolution();
            const auto crossfadeSamples = juce::jmax(
                1.0, juce::jmin(currentSampleRate * 0.005, freezeLengthSamples * 0.25));
            const auto tailPosition = freezeStartPosition + freezeReadPhase;
            auto frozenLeft = readCircular(freezeLeft, tailPosition);
            auto frozenRight = readCircular(freezeRight, tailPosition);

            if (freezeReadPhase >= freezeLengthSamples - crossfadeSamples)
            {
                const auto blend = smoothStep((float) ((freezeReadPhase
                    - (freezeLengthSamples - crossfadeSamples)) / crossfadeSamples));
                const auto headPhase = freezeReadPhase - (freezeLengthSamples - crossfadeSamples);
                const auto headLeft = readCircular(freezeLeft, freezeStartPosition + headPhase);
                const auto headRight = readCircular(freezeRight, freezeStartPosition + headPhase);
                frozenLeft += (headLeft - frozenLeft) * blend;
                frozenRight += (headRight - frozenRight) * blend;
            }

            filteredLeft += frozenLeft * 0.8f * freezeMix;
            filteredRight += frozenRight * 0.8f * freezeMix;

            freezeReadPhase += 1.0;
            if (freezeReadPhase >= freezeLengthSamples)
            {
                freezeReadPhase -= freezeLengthSamples;
                freezeLengthSamples += (freezeTargetLengthSamples - freezeLengthSamples) * 0.35;
                freezeLengthSamples = juce::jlimit(currentSampleRate * 0.05,
                                                   currentSampleRate * 0.20,
                                                   freezeLengthSamples);
                freezeStartPosition = wrapPosition(
                    freezeWriteIndex - freezeLengthSamples, (int) freezeLeft.size());
            }
        }

        left[sample] = filteredLeft;
        right[sample] = filteredRight;
    }

    juce::dsp::Reverb::Parameters reverbParameters;
    reverbParameters.roomSize = 0.92f;
    reverbParameters.damping = 0.42f;
    reverbParameters.wetLevel = block.reverbMix * 0.46f;
    reverbParameters.dryLevel = 1.0f - block.reverbMix * 0.24f;
    reverbParameters.width = 1.0f;
    reverbParameters.freezeMode = 0.0f;
    reverb.setParameters(reverbParameters);
    juce::dsp::AudioBlock<float> audioBlock(buffer);
    reverb.process(juce::dsp::ProcessContextReplacing<float>(audioBlock));

    const auto wearNoiseSmoothing = 1.0f
        - std::exp(-2.0f * juce::MathConstants<float>::pi * 40.0f
                   / (float) currentSampleRate);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        if (samplesUntilWarpTarget <= 0.0)
        {
            warpTarget = nextRandomFloat() * 2.0f - 1.0f;
            samplesUntilWarpTarget = currentSampleRate * (2.0 + nextRandomFloat() * 4.0);
        }
        samplesUntilWarpTarget -= 1.0;
        warpState += (warpTarget - warpState) * warpSmoothing;

        const auto wowDepth = wowDepthSmoother.getNextValue();
        const auto warpDepth = warpDepthSmoother.getNextValue();
        const auto wearDepth = wearDepthSmoother.getNextValue();
        const auto wearNoise = nextRandomFloat() * 2.0f - 1.0f;
        wearNoiseState += (wearNoise - wearNoiseState) * wearNoiseSmoothing;

        const auto warpedRate = block.wowRateHz
            * juce::jlimit(0.5, 1.5, 1.0 + warpState * warpDepth * 0.35);
        const auto wowModulationSeconds = std::sin(wowPhase)
            * wowDepth * wowMaximumModulationSeconds;
        const auto wearModulationSeconds = wearNoiseState * wearDepth * 0.003f;
        const auto modulationSeconds = wowModulationSeconds + wearModulationSeconds;
        const auto delayInSamples = (wowBaseSeconds + modulationSeconds) * currentSampleRate;

        const auto wowReadPosition = wrapPosition(
            wowWriteIndex - delayInSamples, (int) wowLeft.size());
        const auto wowOutputLeft = readCircular(wowLeft, wowReadPosition);
        const auto wowOutputRight = readCircular(wowRight, wowReadPosition);
        const auto wearGain = renderDropoutGain(wearDepth);

        wowLeft[(size_t) wowWriteIndex] = left[sample] * wearGain;
        wowRight[(size_t) wowWriteIndex] = right[sample] * wearGain;
        wowWriteIndex = wrapIndex(wowWriteIndex + 1, (int) wowLeft.size());

        wowPhase += twoPi * warpedRate / currentSampleRate;
        if (wowPhase >= twoPi)
            wowPhase -= twoPi;

        const auto gain = outputGain.getNextValue();
        constexpr float limiterDrive = 1.15f;
        left[sample] = std::tanh(wowOutputLeft * gain * limiterDrive);
        right[sample] = std::tanh(wowOutputRight * gain * limiterDrive);
    }
}

float NativeMasterFx::readParameter(const juce::AudioProcessorValueTreeState& state,
                                    const char* parameterId) noexcept
{
    if (const auto* value = state.getRawParameterValue(parameterId))
        return value->load(std::memory_order_relaxed);
    return 0.0f;
}

NativeMasterFx::BlockParameters NativeMasterFx::readBlockParameters(
    const juce::AudioProcessorValueTreeState& parameters) const noexcept
{
    BlockParameters result;
    result.lowpassHz = juce::jlimit(80.0f, 18000.0f,
                                    readParameter(parameters, "lowpassHz"));
    result.lowpassQ = juce::jlimit(0.1f, 20.0f,
                                   readParameter(parameters, "lowpassQ"));
    result.highpassHz = juce::jlimit(10.0f, 2000.0f,
                                     readParameter(parameters, "highpassHz"));
    result.sweepDepth = juce::jlimit(0.0f, 1.0f,
                                     readParameter(parameters, "filterSweepDepth"));
    result.wowRateHz = juce::jlimit(0.1f, 10.0f,
                                    readParameter(parameters, "wowRateHz"));
    result.wowDepth = juce::jlimit(0.0f, 1.0f,
                                   readParameter(parameters, "wowDepth"));
    result.warpDepth = juce::jlimit(0.0f, 1.0f,
                                    readParameter(parameters, "warpDepth"));
    result.tapeWear = juce::jlimit(0.0f, 1.0f,
                                   readParameter(parameters, "tapeWear"));
    result.reverbMix = juce::jlimit(0.0f, 1.0f,
                                    readParameter(parameters, "reverbMix"));
    result.delayEnabled = readParameter(parameters, "delayEnabled") > 0.5f;
    result.masterGain = juce::jlimit(0.0f, 1.0f,
                                     readParameter(parameters, "masterGain"));
    result.freezeEnabled = readParameter(parameters, "freezeEnabled") > 0.5f;
    return result;
}

void NativeMasterFx::beginFreeze() noexcept
{
    freezeLengthSamples = juce::jlimit(currentSampleRate * 0.05,
                                       currentSampleRate * 0.20,
                                       currentSampleRate * 0.15);
    freezeTargetLengthSamples = freezeLengthSamples;
    freezeReadPhase = 0.0;
    freezeStartPosition = wrapPosition(
        freezeWriteIndex - freezeLengthSamples, (int) freezeLeft.size());
    samplesUntilFreezeEvolution = currentSampleRate * (1.0 + nextRandomFloat() * 2.0);
}

void NativeMasterFx::updateFreezeEvolution() noexcept
{
    samplesUntilFreezeEvolution -= 1.0;
    if (samplesUntilFreezeEvolution > 0.0)
        return;

    freezeTargetLengthSamples = currentSampleRate * (0.05 + nextRandomFloat() * 0.15);
    samplesUntilFreezeEvolution = currentSampleRate * (1.0 + nextRandomFloat() * 3.0);
}

void NativeMasterFx::scheduleNextDropout(float wear) noexcept
{
    const auto intervalBaseSeconds = 18.0f - 12.0f * wear;
    const auto intervalSeconds = intervalBaseSeconds * (0.75f + nextRandomFloat() * 2.5f);
    samplesUntilDropout = currentSampleRate * juce::jmax(4.0f, intervalSeconds);
}

float NativeMasterFx::renderDropoutGain(float wear) noexcept
{
    if (! dropoutActive)
    {
        if (wear <= 0.0001f)
            return 1.0f;

        samplesUntilDropout -= 1.0;
        if (samplesUntilDropout > 0.0)
            return 1.0f;

        dropoutActive = true;
        dropoutElapsedSamples = 0.0;
        dropoutDurationSamples = currentSampleRate * (0.03 + nextRandomFloat() * 0.12);
        dropoutDepth = wear * wear * (0.08f + nextRandomFloat() * 0.42f);
    }

    const auto progress = juce::jlimit(
        0.0f, 1.0f, (float) (dropoutElapsedSamples / dropoutDurationSamples));
    float shape = 1.0f;
    if (progress < 0.2f)
        shape = smoothStep(progress / 0.2f);
    else if (progress > 0.8f)
        shape = smoothStep((1.0f - progress) / 0.2f);

    dropoutElapsedSamples += 1.0;
    if (dropoutElapsedSamples >= dropoutDurationSamples)
    {
        dropoutActive = false;
        scheduleNextDropout(wear);
    }
    return 1.0f - dropoutDepth * shape;
}

float NativeMasterFx::readCircular(const std::vector<float>& buffer,
                                   double position) noexcept
{
    const auto size = (int) buffer.size();
    const auto wrapped = wrapPosition(position, size);
    const auto index1 = (int) wrapped;
    const auto index2 = wrapIndex(index1 + 1, size);
    const auto fraction = (float) (wrapped - index1);
    return buffer[(size_t) index1]
         + (buffer[(size_t) index2] - buffer[(size_t) index1]) * fraction;
}

int NativeMasterFx::wrapIndex(int index, int size) noexcept
{
    while (index < 0)
        index += size;
    while (index >= size)
        index -= size;
    return index;
}

double NativeMasterFx::wrapPosition(double position, int size) noexcept
{
    while (position < 0.0)
        position += size;
    while (position >= size)
        position -= size;
    return position;
}

float NativeMasterFx::smoothStep(float value) noexcept
{
    const auto x = juce::jlimit(0.0f, 1.0f, value);
    return x * x * (3.0f - 2.0f * x);
}

float NativeMasterFx::nextRandomFloat() noexcept
{
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return (float) (randomState & 0x00ffffffu) / (float) 0x01000000u;
}
}
