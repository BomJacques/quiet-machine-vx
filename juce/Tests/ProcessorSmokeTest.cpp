#include "../Source/PluginProcessor.h"

#include <cmath>
#include <iostream>

namespace
{
bool fail(const char* message)
{
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

double bufferEnergy(const juce::AudioBuffer<float>& audio)
{
    double energy = 0.0;
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            const auto value = audio.getSample(channel, sample);
            energy += (double) value * value;
        }
    return energy;
}

double bufferEnergyRange(const juce::AudioBuffer<float>& audio, int startSample, int numSamples)
{
    double energy = 0.0;
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = startSample; sample < startSample + numSamples; ++sample)
        {
            const auto value = audio.getSample(channel, sample);
            energy += (double) value * value;
        }
    return energy;
}

double channelEnergy(const juce::AudioBuffer<float>& audio, int channel)
{
    double energy = 0.0;
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        const auto value = audio.getSample(channel, sample);
        energy += (double) value * value;
    }
    return energy;
}

double spectralMagnitude(const std::vector<float>& samples, double frequency, double sampleRate)
{
    double real = 0.0;
    double imaginary = 0.0;
    for (size_t index = 0; index < samples.size(); ++index)
    {
        const auto window = 0.5 - 0.5 * std::cos(
            juce::MathConstants<double>::twoPi * (double) index
            / (double) juce::jmax((size_t) 1, samples.size() - 1));
        const auto phase = juce::MathConstants<double>::twoPi * frequency
                         * (double) index / sampleRate;
        real += samples[index] * window * std::cos(phase);
        imaginary -= samples[index] * window * std::sin(phase);
    }
    return 2.0 * std::sqrt(real * real + imaginary * imaginary)
         / (double) juce::jmax((size_t) 1, samples.size());
}

void configurePlayableSub(QuietMachineAudioProcessor& processor)
{
    for (const auto* source : qm::sourceNames)
        processor.setParameterFromUi(juce::String(source) + "Gain", 0.0f);
    processor.setParameterFromUi("subGain", 1.0f);
    processor.setParameterFromUi("oscillatorWave", 3.0f);
    processor.setParameterFromUi("glideSeconds", 0.0f);
    processor.setParameterFromUi("ampAttack", 0.005f);
    processor.setParameterFromUi("ampDecay", 0.01f);
    processor.setParameterFromUi("ampSustain", 1.0f);
    processor.setParameterFromUi("ampRelease", 0.01f);
    processor.setParameterFromUi("reverbMix", 0.0f);
    processor.setParameterFromUi("wowDepth", 0.0f);
    processor.setParameterFromUi("warpDepth", 0.0f);
    processor.setParameterFromUi("tapeWear", 0.0f);
    processor.setParameterFromUi("tapeDrive", 0.0f);
    processor.setParameterFromUi("lowpassHz", 18000.0f);
    processor.setParameterFromUi("highpassHz", 20.0f);
    processor.setParameterFromUi("masterGain", 1.0f);
}

void configureStaticOnly(QuietMachineAudioProcessor& processor)
{
    for (const auto* source : qm::sourceNames)
        processor.setParameterFromUi(juce::String(source) + "Gain", 0.0f);
    processor.setParameterFromUi("staticGain", 1.0f);
    processor.setParameterFromUi("ampAttack", 0.005f);
    processor.setParameterFromUi("ampDecay", 0.01f);
    processor.setParameterFromUi("ampSustain", 1.0f);
    processor.setParameterFromUi("ampRelease", 0.01f);
    processor.setParameterFromUi("reverbMix", 0.0f);
    processor.setParameterFromUi("delayEnabled", 0.0f);
    processor.setParameterFromUi("wowDepth", 0.0f);
    processor.setParameterFromUi("warpDepth", 0.0f);
    processor.setParameterFromUi("tapeWear", 0.0f);
    processor.setParameterFromUi("tapeDrive", 0.0f);
    processor.setParameterFromUi("lowpassHz", 18000.0f);
    processor.setParameterFromUi("highpassHz", 20.0f);
    processor.setParameterFromUi("masterGain", 1.0f);
}

juce::String makeKeyVoicePatch(const qm::KeyVoiceProfile& keyZero)
{
    juce::var root = new juce::DynamicObject();
    auto* object = root.getDynamicObject();
    object->setProperty("seedText", "PROFILE1");
    object->setProperty("engineMode", "AUTO");
    object->setProperty("navMode", "HOLD");

    juce::Array<juce::var> notes;
    notes.add(0);
    object->setProperty("autoNotes", juce::var(notes));
    object->setProperty("midiChordNotes", juce::var(notes));

    juce::Array<juce::var> voices;
    voices.ensureStorageAllocated(qm::keyVoiceCount);
    for (int index = 0; index < qm::keyVoiceCount; ++index)
    {
        const auto profile = index == 0 ? keyZero : qm::KeyVoiceProfile {};
        juce::var voice = new juce::DynamicObject();
        auto* voiceObject = voice.getDynamicObject();
        voiceObject->setProperty("detuneCents", profile.detuneCents);
        voiceObject->setProperty("pan", profile.pan);
        voiceObject->setProperty("oscillatorCount", profile.oscillatorCount);
        voiceObject->setProperty("oscillatorSpreadCents", profile.oscillatorSpreadCents);
        voices.add(std::move(voice));
    }
    object->setProperty("keyVoices", juce::var(voices));
    return juce::JSON::toString(root);
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    auto processor = std::make_unique<QuietMachineAudioProcessor>();
    constexpr std::array<const char*, 13> expectedBusNames {
        "Main Processed", "Tape 1", "Tape 2", "Tape 3", "Tape 4", "Tape 5", "Swell",
        "Rumble", "Piano", "Drone", "Sub", "Static", "Hiss"
    };
    if (processor->getBusCount(false) != (int) expectedBusNames.size())
        return fail("processor does not expose main plus 12 stem buses") ? 0 : 1;
    for (int bus = 0; bus < processor->getBusCount(false); ++bus)
        if (processor->getBus(false, bus)->getName() != expectedBusNames[(size_t) bus])
            return fail("stem bus name or order is incorrect") ? 0 : 1;
    if (processor->getTotalNumOutputChannels() != 2)
        return fail("optional stem buses were not disabled by default") ? 0 : 1;

    processor->setPlayConfigDetails(0, 2, 44100.0, 512);
    processor->prepareToPlay(44100.0, 512);
    processor->setPatchStateFromUi(R"json({
        "seedText":"SMOKE01","engineMode":"AUTO","navMode":"WANDER",
        "autoNotes":[0,7,14,19],"seqSteps":[],"pianoSeq":[[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[]]
    })json");

    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    processor->processBlock(audio, midi);
    double startupEnergy = 0.0;
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            const auto value = audio.getSample(channel, sample);
            startupEnergy += (double) value * value;
        }
    if (startupEnergy > 0.000000001)
        return fail("native engine produced audio before transport or MIDI input") ? 0 : 1;

    const auto renderDelayTail = [](bool delayEnabled)
    {
        auto delayProcessor = std::make_unique<QuietMachineAudioProcessor>();
        delayProcessor->setPlayConfigDetails(0, 2, 44100.0, 512);
        delayProcessor->prepareToPlay(44100.0, 512);
        configurePlayableSub(*delayProcessor);
        delayProcessor->setParameterFromUi("ampRelease", 2.0f);
        delayProcessor->setParameterFromUi("delayEnabled", delayEnabled ? 1.0f : 0.0f);
        juce::AudioBuffer<float> delayAudio(2, 512);
        juce::MidiBuffer delayMidi;
        for (int block = 0; block < 4; ++block)
        {
            delayAudio.clear();
            delayProcessor->processBlock(delayAudio, delayMidi);
        }
        delayMidi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
        delayMidi.addEvent(juce::MidiMessage::noteOff(1, 60), 256);
        delayAudio.clear();
        delayProcessor->processBlock(delayAudio, delayMidi);
        delayMidi.clear();
        // Remove fresh source input, leaving only the delay under the master
        // release. A short global release intentionally silences all echoes.
        delayProcessor->setParameterFromUi("subGain", 0.0f);

        double tailEnergy = 0.0;
        for (int block = 0; block < 72; ++block)
        {
            delayAudio.clear();
            delayProcessor->processBlock(delayAudio, delayMidi);
            if (block >= 48)
                tailEnergy += bufferEnergy(delayAudio);
        }
        return tailEnergy;
    };

    const auto enabledDelayTail = renderDelayTail(true);
    const auto bypassedDelayTail = renderDelayTail(false);
    if (enabledDelayTail <= 0.000000001
        || enabledDelayTail <= bypassedDelayTail * 100.0)
        return fail("Delay switch did not remove the long stereo echo tail") ? 0 : 1;

    auto playable = std::make_unique<QuietMachineAudioProcessor>();
    if (! playable->enableAllBuses())
        return fail("playable test could not enable stem buses") ? 0 : 1;
    playable->setRateAndBufferSizeDetails(44100.0, 512);
    playable->prepareToPlay(44100.0, 512);
    configurePlayableSub(*playable);
    juce::AudioBuffer<float> playableAudio(playable->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer playableMidi;
    playableMidi.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8) 100), 96);
    double playableEnergy = 0.0;
    for (int block = 0; block < 8; ++block)
    {
        playableAudio.clear();
        playable->processBlock(playableAudio, playableMidi);
        auto playableSub = playable->getBusBuffer(playableAudio, false, 10);
        playableEnergy += bufferEnergy(playableSub);
        playableMidi.clear();
    }
    if (playableEnergy <= 0.000001)
        return fail("direct host MIDI note did not produce pitched audio") ? 0 : 1;

    playableMidi.addEvent(juce::MidiMessage::noteOff(1, 72), 128);
    for (int block = 0; block < 8; ++block)
    {
        playableAudio.clear();
        playable->processBlock(playableAudio, playableMidi);
        playableMidi.clear();
    }
    auto releasedSub = playable->getBusBuffer(playableAudio, false, 10);
    if (bufferEnergy(releasedSub) > 0.000001)
        return fail("ADSR release did not close after direct MIDI note-off") ? 0 : 1;

    playable->setPatchStateFromUi(R"json({
        "seedText":"CHORD01","engineMode":"AUTO","navMode":"HOLD",
        "autoNotes":[0,4,7],"seqSteps":[],"pianoSeq":[[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[]]
    })json");
    playable->setParameterFromUi("midiChordMode", 1.0f);
    playableMidi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 64);
    double chordModeEnergy = 0.0;
    for (int block = 0; block < 8; ++block)
    {
        playableAudio.clear();
        playable->processBlock(playableAudio, playableMidi);
        chordModeEnergy += bufferEnergy(playable->getBusBuffer(playableAudio, false, 10));
        playableMidi.clear();
    }
    if (chordModeEnergy <= 0.000001)
        return fail("MIDI chord mode produced no pitched audio") ? 0 : 1;

    playableMidi.addEvent(juce::MidiMessage::noteOff(1, 60), 64);
    for (int block = 0; block < 8; ++block)
    {
        playableAudio.clear();
        playable->processBlock(playableAudio, playableMidi);
        playableMidi.clear();
    }
    releasedSub = playable->getBusBuffer(playableAudio, false, 10);
    if (bufferEnergy(releasedSub) > 0.000001)
        return fail("MIDI chord mode left expanded notes held after root note-off") ? 0 : 1;
    playable->setParameterFromUi("midiChordMode", 0.0f);

    auto overallEnvelope = std::make_unique<QuietMachineAudioProcessor>();
    if (! overallEnvelope->enableAllBuses())
        return fail("overall ADSR test could not enable stem buses") ? 0 : 1;
    overallEnvelope->setRateAndBufferSizeDetails(44100.0, 512);
    overallEnvelope->prepareToPlay(44100.0, 512);
    for (const auto* source : qm::sourceNames)
        overallEnvelope->setParameterFromUi(juce::String(source) + "Gain", 1.0f);
    overallEnvelope->setParameterFromUi("ampAttack", 0.2f);
    overallEnvelope->setParameterFromUi("ampDecay", 0.01f);
    overallEnvelope->setParameterFromUi("ampSustain", 1.0f);
    overallEnvelope->setParameterFromUi("ampRelease", 0.05f);
    overallEnvelope->setParameterFromUi("midiChordMode", 1.0f);
    overallEnvelope->setParameterFromUi("generatorEnabled", 0.0f);
    overallEnvelope->setPatchStateFromUi(R"json({
        "seedText":"ENVALL1","engineMode":"AUTO","navMode":"HOLD",
        "autoNotes":[0,4,7],"midiChordNotes":[0,4,7],"seqSteps":[],
        "pianoSeq":[[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[]]
    })json");

    juce::AudioBuffer<float> envelopeAudio(overallEnvelope->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer envelopeTestMidi;
    for (int block = 0; block < 4; ++block)
    {
        envelopeAudio.clear();
        overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
    }
    envelopeTestMidi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    constexpr std::array<int, qm::sourceCount> envelopeStemBuses {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
    };
    std::array<double, envelopeStemBuses.size()> envelopedSourceEnergy {};
    double earlyStaticEnergy = 0.0;
    double earlyHissEnergy = 0.0;
    double lateStaticEnergy = 0.0;
    double lateHissEnergy = 0.0;
    for (int block = 0; block < 28; ++block)
    {
        envelopeAudio.clear();
        overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
        envelopeTestMidi.clear();
        for (size_t source = 0; source < envelopeStemBuses.size(); ++source)
            envelopedSourceEnergy[source] += bufferEnergy(
                overallEnvelope->getBusBuffer(envelopeAudio, false, envelopeStemBuses[source]));
        if (block == 0)
        {
            earlyStaticEnergy = bufferEnergy(overallEnvelope->getBusBuffer(envelopeAudio, false, 11));
            earlyHissEnergy = bufferEnergy(overallEnvelope->getBusBuffer(envelopeAudio, false, 12));
        }
        if (block == 24)
        {
            lateStaticEnergy = bufferEnergy(overallEnvelope->getBusBuffer(envelopeAudio, false, 11));
            lateHissEnergy = bufferEnergy(overallEnvelope->getBusBuffer(envelopeAudio, false, 12));
        }
    }
    auto activeEnvelopeSources = 0;
    for (size_t source = 0; source < envelopedSourceEnergy.size(); ++source)
        if (envelopedSourceEnergy[source] > 0.000000001)
            ++activeEnvelopeSources;
    if (activeEnvelopeSources < 6)
        return fail("overall ADSR test did not exercise enough independent source groups") ? 0 : 1;
    if (lateStaticEnergy <= earlyStaticEnergy * 2.0
        || lateHissEnergy <= earlyHissEnergy * 2.0)
        return fail("ADSR attack did not shape Static and Hiss in MIDI chord mode") ? 0 : 1;

    envelopeTestMidi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    double firstReleaseStaticEnergy = 0.0;
    double firstReleaseHissEnergy = 0.0;
    for (int block = 0; block < 32; ++block)
    {
        envelopeAudio.clear();
        overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
        envelopeTestMidi.clear();
        if (block == 0)
        {
            firstReleaseStaticEnergy = bufferEnergy(
                overallEnvelope->getBusBuffer(envelopeAudio, false, 11));
            firstReleaseHissEnergy = bufferEnergy(
                overallEnvelope->getBusBuffer(envelopeAudio, false, 12));
        }
    }
    if (firstReleaseStaticEnergy <= 0.000000001
        || firstReleaseHissEnergy <= 0.000000001)
        return fail("Static and Hiss were cut off at note-off instead of following amp release") ? 0 : 1;
    for (const auto bus : envelopeStemBuses)
        if (bufferEnergy(overallEnvelope->getBusBuffer(envelopeAudio, false, bus)) > 0.000000001)
            return fail("MIDI chord ADSR release was cancelled by generator resumption") ? 0 : 1;

    overallEnvelope->setParameterFromUi("generatorEnabled", 0.0f);
    // A short key press released during attack must retain the full release
    // duration, and the processed master must also reach silence.
    overallEnvelope->setParameterFromUi("ampAttack", 1.0f);
    overallEnvelope->setParameterFromUi("ampSustain", 0.0f);
    overallEnvelope->setParameterFromUi("ampRelease", 2.0f);
    envelopeTestMidi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
    overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
    envelopeTestMidi.clear();
    envelopeTestMidi.addEvent(juce::MidiMessage::noteOff(1, 64), 0);
    for (int block = 0; block < 86; ++block)
    {
        overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
        envelopeTestMidi.clear();
    }
    for (const auto bus : { 0, 6, 7, 9, 10, 11, 12 })
        if (bufferEnergy(overallEnvelope->getBusBuffer(envelopeAudio, false, bus)) <= 1.0e-12)
            return fail("whole-machine release ended early after a short note with zero sustain") ? 0 : 1;
    for (int block = 0; block < 100; ++block)
        overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
    for (int bus = 0; bus <= qm::sourceCount; ++bus)
        if (bufferEnergy(overallEnvelope->getBusBuffer(envelopeAudio, false, bus)) > 1.0e-12)
            return fail("master effects or stems escaped the complete ADSR release") ? 0 : 1;
    overallEnvelope->setParameterFromUi("ampSustain", 1.0f);
    overallEnvelope->setParameterFromUi("ampAttack", 0.005f);
    overallEnvelope->setParameterFromUi("ampRelease", 20.0f);
    envelopeTestMidi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
    for (int block = 0; block < 8; ++block)
    {
        envelopeAudio.clear();
        overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
        envelopeTestMidi.clear();
    }
    if (bufferEnergy(overallEnvelope->getBusBuffer(envelopeAudio, false, 11))
        <= 0.000000001)
        return fail("panic ADSR test did not establish an audible source") ? 0 : 1;
    overallEnvelope->panicFromUi();
    envelopeAudio.clear();
    overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
    for (int bus = 0; bus <= qm::sourceCount; ++bus)
        if (bufferEnergy(overallEnvelope->getBusBuffer(envelopeAudio, false, bus))
            > 0.000000001)
            return fail("panic followed the long ADSR release instead of silencing immediately") ? 0 : 1;

    envelopeTestMidi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
    for (int block = 0; block < 8; ++block)
    {
        envelopeAudio.clear();
        overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
        envelopeTestMidi.clear();
    }
    envelopeTestMidi.addEvent(juce::MidiMessage::allSoundOff(1), 256);
    envelopeAudio.clear();
    overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
    envelopeTestMidi.clear();
    const auto allSoundOffStatic = overallEnvelope->getBusBuffer(envelopeAudio, false, 11);
    if (bufferEnergyRange(allSoundOffStatic, 0, 256) <= 0.000000001
        || bufferEnergyRange(allSoundOffStatic, 256, 256) > 0.000000001)
        return fail("All Sound Off did not hard-stop the envelope at its sample offset") ? 0 : 1;
    const auto allSoundOffMain = overallEnvelope->getBusBuffer(envelopeAudio, false, 0);
    if (bufferEnergyRange(allSoundOffMain, 256, 256) > 0.000000001)
        return fail("All Sound Off left a master effect tail after its sample offset") ? 0 : 1;

    overallEnvelope->setParameterFromUi("ampRelease", 0.1f);
    overallEnvelope->setParameterFromUi("generatorEnabled", 1.0f);
    for (int block = 0; block < 4; ++block)
    {
        envelopeAudio.clear();
        overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
    }
    envelopeTestMidi.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8) 100), 100);
    envelopeTestMidi.addEvent(juce::MidiMessage::noteOff(1, 67), 120);
    envelopeAudio.clear();
    overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
    envelopeTestMidi.clear();
    envelopeAudio.clear();
    overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
    if (bufferEnergy(overallEnvelope->getBusBuffer(envelopeAudio, false, 11))
        <= 0.000000001)
        return fail("short MIDI note collapsed the configured ADSR release into the handoff") ? 0 : 1;
    for (int block = 0; block < 16; ++block)
    {
        envelopeAudio.clear();
        overallEnvelope->processBlock(envelopeAudio, envelopeTestMidi);
    }
    if (bufferEnergy(overallEnvelope->getBusBuffer(envelopeAudio, false, 11))
        <= 0.000000001)
        return fail("generator did not recover after the short-note ADSR release") ? 0 : 1;

    playableMidi.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8) 100), 0);
    playableMidi.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8) 100), 64);
    playableAudio.clear();
    playable->processBlock(playableAudio, playableMidi);
    playableMidi.clear();
    playableMidi.addEvent(juce::MidiMessage::noteOff(1, 67), 64);
    for (int block = 0; block < 8; ++block)
    {
        playableAudio.clear();
        playable->processBlock(playableAudio, playableMidi);
        playableMidi.clear();
    }
    releasedSub = playable->getBusBuffer(playableAudio, false, 10);
    if (bufferEnergy(releasedSub) > 0.000001)
        return fail("duplicate MIDI note-on required more than one note-off") ? 0 : 1;

    playable->setPatchStateFromUi(R"json({
        "seedText":"CHORD01","engineMode":"AUTO","navMode":"HOLD",
        "autoNotes":[0,7,14,19],"seqSteps":[],"pianoSeq":[[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[]]
    })json");
    playable->setParameterFromUi("midiChordMode", 1.0f);
    playableMidi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    playableAudio.clear();
    playable->processBlock(playableAudio, playableMidi);
    playableMidi.clear();
    playable->setParameterFromUi("midiChordMode", 0.0f);
    for (int block = 0; block < 8; ++block)
    {
        playableAudio.clear();
        playable->processBlock(playableAudio, playableMidi);
    }
    releasedSub = playable->getBusBuffer(playableAudio, false, 10);
    if (bufferEnergy(releasedSub) > 0.000001)
        return fail("changing MIDI chord mode left performance notes active") ? 0 : 1;

    playableMidi.addEvent(juce::MidiMessage::noteOn(1, 79, (juce::uint8) 100), 400);
    playable->panicFromUi();
    for (int block = 0; block < 8; ++block)
    {
        playableAudio.clear();
        playable->processBlock(playableAudio, playableMidi);
        playableMidi.clear();
    }
    releasedSub = playable->getBusBuffer(playableAudio, false, 10);
    if (bufferEnergy(releasedSub) > 0.000001)
        return fail("panic left a later performance note-on active") ? 0 : 1;

    const auto countSubCrossings = [&playable, &playableAudio, &playableMidi](int note)
    {
        playableMidi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 100), 0);
        int crossings = 0;
        float previous = 0.0f;
        for (int block = 0; block < 20; ++block)
        {
            playableAudio.clear();
            playable->processBlock(playableAudio, playableMidi);
            playableMidi.clear();
            if (block < 4)
                continue;
            auto sub = playable->getBusBuffer(playableAudio, false, 10);
            for (int sample = 0; sample < sub.getNumSamples(); ++sample)
            {
                const auto current = sub.getSample(0, sample);
                if (previous <= 0.0f && current > 0.0f)
                    ++crossings;
                previous = current;
            }
        }
        playableMidi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
        for (int block = 0; block < 4; ++block)
        {
            playableAudio.clear();
            playable->processBlock(playableAudio, playableMidi);
            playableMidi.clear();
        }
        return crossings;
    };
    const auto lowPitchCrossings = countSubCrossings(48);
    const auto highPitchCrossings = countSubCrossings(72);
    if (lowPitchCrossings <= 0 || highPitchCrossings < lowPitchCrossings * 2)
        return fail("different MIDI notes did not change the native synth pitch") ? 0 : 1;

    auto chordVoice = std::make_unique<QuietMachineAudioProcessor>();
    if (! chordVoice->enableAllBuses())
        return fail("MIDI chord test could not enable stem buses") ? 0 : 1;
    chordVoice->setRateAndBufferSizeDetails(44100.0, 512);
    chordVoice->prepareToPlay(44100.0, 512);
    configurePlayableSub(*chordVoice);
    chordVoice->setParameterFromUi("subGain", 0.0f);
    chordVoice->setParameterFromUi("droneGain", 1.0f);
    chordVoice->setParameterFromUi("midiChordMode", 1.0f);
    chordVoice->setPatchStateFromUi(R"json({
        "seedText":"VOICE03","engineMode":"AUTO","navMode":"HOLD",
        "autoNotes":[0,7,14,19],"midiChordNotes":[0,4,7],
        "seqSteps":[],"pianoSeq":[[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[]]
    })json");
    juce::AudioBuffer<float> chordAudio(chordVoice->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer chordMidi;
    chordVoice->processBlock(chordAudio, chordMidi);
    const auto captureChord = [&chordVoice, &chordAudio, &chordMidi](int root)
    {
        chordMidi.addEvent(juce::MidiMessage::noteOn(1, root, (juce::uint8) 100), 0);
        std::vector<float> captured;
        captured.reserve(24 * 512);
        for (int block = 0; block < 40; ++block)
        {
            chordAudio.clear();
            chordVoice->processBlock(chordAudio, chordMidi);
            chordMidi.clear();
            if (block < 16)
                continue;
            const auto drone = chordVoice->getBusBuffer(chordAudio, false, 9);
            captured.insert(captured.end(), drone.getReadPointer(0),
                            drone.getReadPointer(0) + drone.getNumSamples());
        }
        chordMidi.addEvent(juce::MidiMessage::noteOff(1, root), 0);
        for (int block = 0; block < 20; ++block)
        {
            chordAudio.clear();
            chordVoice->processBlock(chordAudio, chordMidi);
            chordMidi.clear();
        }
        return captured;
    };
    const auto firstChord = captureChord(60);
    for (const auto frequency : { 130.8128, 164.8138, 195.9977 })
        if (spectralMagnitude(firstChord, frequency, 44100.0) < 0.002)
            return fail("MIDI chord mode did not voice every selected drone pitch") ? 0 : 1;
    const auto movedChord = captureChord(62);
    for (const auto frequency : { 146.8324, 184.9972, 220.0 })
        if (spectralMagnitude(movedChord, frequency, 44100.0) < 0.002)
            return fail("physical MIDI root did not transpose the complete chord") ? 0 : 1;
    if (bufferEnergy(chordVoice->getBusBuffer(chordAudio, false, 9)) > 0.000001)
        return fail("MIDI chord note-off left stale drone voices sounding") ? 0 : 1;

    auto profiled = std::make_unique<QuietMachineAudioProcessor>();
    if (! profiled->enableAllBuses())
        return fail("key profile test could not enable stem buses") ? 0 : 1;
    profiled->setRateAndBufferSizeDetails(44100.0, 512);
    profiled->prepareToPlay(44100.0, 512);
    configurePlayableSub(*profiled);
    profiled->setParameterFromUi("droneGain", 1.0f);
    profiled->setParameterFromUi("subGain", 1.0f);
    profiled->setParameterFromUi("midiChordMode", 1.0f);

    qm::KeyVoiceProfile firstProfile;
    firstProfile.detuneCents = 50.0f;
    firstProfile.pan = -1.0f;
    firstProfile.oscillatorCount = 1;
    firstProfile.oscillatorSpreadCents = 0.0f;
    profiled->setPatchStateFromUi(makeKeyVoicePatch(firstProfile));

    juce::AudioBuffer<float> profileAudio(profiled->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer profileMidi;
    profileAudio.clear();
    profiled->processBlock(profileAudio, profileMidi);
    profileMidi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);

    const auto captureProfile = [&profiled, &profileAudio, &profileMidi](int blocksToSkip,
                                                                        int blocksToCapture)
    {
        std::array<std::vector<float>, 4> captured;
        for (auto& channel : captured)
            channel.reserve((size_t) blocksToCapture * 512);
        for (int block = 0; block < blocksToSkip + blocksToCapture; ++block)
        {
            profileAudio.clear();
            profiled->processBlock(profileAudio, profileMidi);
            profileMidi.clear();
            if (block < blocksToSkip)
                continue;
            const auto drone = profiled->getBusBuffer(profileAudio, false, 9);
            const auto sub = profiled->getBusBuffer(profileAudio, false, 10);
            for (int sample = 0; sample < drone.getNumSamples(); ++sample)
            {
                captured[0].push_back(drone.getSample(0, sample));
                captured[1].push_back(drone.getSample(1, sample));
                captured[2].push_back(sub.getSample(0, sample));
                captured[3].push_back(sub.getSample(1, sample));
            }
        }
        return captured;
    };

    const auto firstCapture = captureProfile(16, 48);
    const auto vectorEnergy = [](const std::vector<float>& samples)
    {
        double result = 0.0;
        for (const auto sample : samples)
            result += (double) sample * sample;
        return result;
    };
    if (vectorEnergy(firstCapture[0]) < vectorEnergy(firstCapture[1]) * 20.0
        || vectorEnergy(firstCapture[2]) < vectorEnergy(firstCapture[3]) * 1.5)
        return fail("per-key pan did not reach the Drone and narrowed Sub root") ? 0 : 1;

    const auto dronePlusFifty = 130.81278265 * std::pow(2.0, 50.0 / 1200.0);
    const auto subPlusFifty = 65.40639133 * std::pow(2.0, 50.0 / 1200.0);
    if (spectralMagnitude(firstCapture[0], dronePlusFifty, 44100.0) < 0.003
        || spectralMagnitude(firstCapture[2], subPlusFifty, 44100.0) < 0.003)
        return fail("per-key detune did not reach Drone and Sub") ? 0 : 1;

    qm::KeyVoiceProfile updatedProfile;
    updatedProfile.detuneCents = -50.0f;
    updatedProfile.pan = 1.0f;
    updatedProfile.oscillatorCount = 4;
    updatedProfile.oscillatorSpreadCents = 30.0f;
    profiled->setPatchStateFromUi(makeKeyVoicePatch(updatedProfile));
    profileAudio.clear();
    profiled->processBlock(profileAudio, profileMidi);
    profileMidi.clear();
    const auto transitionDrone = profiled->getBusBuffer(profileAudio, false, 9);
    if (bufferEnergy(transitionDrone) <= 0.000001
        || channelEnergy(transitionDrone, 1) <= 0.000000001)
        return fail("held key profile was not applied on the next audio block") ? 0 : 1;
    const auto updatedCapture = captureProfile(16, 96);
    if (vectorEnergy(updatedCapture[1]) < vectorEnergy(updatedCapture[0]) * 20.0
        || vectorEnergy(updatedCapture[3]) < vectorEnergy(updatedCapture[2]) * 1.5)
        return fail("held per-key pan did not update on the next profile snapshot") ? 0 : 1;

    const auto droneMinusFifty = 130.81278265 * std::pow(2.0, -50.0 / 1200.0);
    const auto subMinusFifty = 65.40639133 * std::pow(2.0, -50.0 / 1200.0);
    if (spectralMagnitude(updatedCapture[3], subMinusFifty, 44100.0) < 0.003)
        return fail("held Sub detune did not update without retrigger") ? 0 : 1;
    for (const auto spreadCents : { -15.0, -5.0, 5.0, 15.0 })
        if (spectralMagnitude(updatedCapture[1],
                              droneMinusFifty * std::pow(2.0, spreadCents / 1200.0),
                              44100.0) < 0.001)
            return fail("per-key oscillator count/spread did not render all oscillator lanes") ? 0 : 1;

    profileMidi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    for (int block = 0; block < 20; ++block)
    {
        profileAudio.clear();
        profiled->processBlock(profileAudio, profileMidi);
        profileMidi.clear();
    }

    profileMidi.addEvent(juce::MidiMessage::noteOn(1, 62, (juce::uint8) 100), 0);
    const auto transposedCapture = captureProfile(16, 64);
    const auto transposedDroneLane = 146.83238396 * std::pow(2.0, -65.0 / 1200.0);
    const auto transposedSub = 73.41619198 * std::pow(2.0, -50.0 / 1200.0);
    if (vectorEnergy(transposedCapture[1]) < vectorEnergy(transposedCapture[0]) * 20.0
        || spectralMagnitude(transposedCapture[1], transposedDroneLane, 44100.0) < 0.001
        || spectralMagnitude(transposedCapture[3], transposedSub, 44100.0) < 0.003)
        return fail("source-key profile did not follow a transposed physical MIDI root") ? 0 : 1;
    profileMidi.addEvent(juce::MidiMessage::noteOff(1, 62), 0);
    for (int block = 0; block < 20; ++block)
    {
        profileAudio.clear();
        profiled->processBlock(profileAudio, profileMidi);
        profileMidi.clear();
    }

    auto rumble = std::make_unique<QuietMachineAudioProcessor>();
    if (! rumble->enableAllBuses())
        return fail("rumble test could not enable stem buses") ? 0 : 1;
    rumble->setRateAndBufferSizeDetails(44100.0, 512);
    rumble->prepareToPlay(44100.0, 512);
    for (const auto* source : qm::sourceNames)
        rumble->setParameterFromUi(juce::String(source) + "Gain", 0.0f);
    rumble->setParameterFromUi("rumbleGain", 1.0f);
    juce::AudioBuffer<float> rumbleAudio(rumble->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer rumbleMidi;
    const auto estimateRumblePitch = [&rumble, &rumbleAudio, &rumbleMidi](int note)
    {
        rumbleMidi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 100), 0);
        std::vector<float> captured;
        captured.reserve(45 * 512);
        for (int block = 0; block < 55; ++block)
        {
            rumbleAudio.clear();
            rumble->processBlock(rumbleAudio, rumbleMidi);
            rumbleMidi.clear();
            if (block < 10)
                continue;
            auto stem = rumble->getBusBuffer(rumbleAudio, false, 7);
            for (int sample = 0; sample < stem.getNumSamples(); ++sample)
                captured.push_back(stem.getSample(0, sample));
        }
        rumbleMidi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
        for (int block = 0; block < 8; ++block)
        {
            rumbleAudio.clear();
            rumble->processBlock(rumbleAudio, rumbleMidi);
            rumbleMidi.clear();
        }
        const auto expectedFrequency = 440.0 * std::pow(2.0, ((double) note - 24.0 - 69.0) / 12.0);
        const auto minimumLag = (int) std::floor(44100.0 / (expectedFrequency * 1.2));
        const auto maximumLag = (int) std::ceil(44100.0 / (expectedFrequency * 0.8));
        auto strongestCorrelation = -1.0;
        auto strongestLag = 0;
        for (int lag = minimumLag; lag <= maximumLag; ++lag)
        {
            double correlation = 0.0;
            double energyA = 0.0;
            double energyB = 0.0;
            for (size_t sample = (size_t) lag; sample < captured.size(); ++sample)
            {
                const auto a = captured[sample];
                const auto b = captured[sample - (size_t) lag];
                correlation += a * b;
                energyA += a * a;
                energyB += b * b;
            }
            correlation /= std::sqrt(juce::jmax(1.0e-20, energyA * energyB));
            if (correlation > strongestCorrelation)
            {
                strongestCorrelation = correlation;
                strongestLag = lag;
            }
        }
        return std::pair { strongestLag > 0 ? 44100.0 / strongestLag : 0.0,
                           strongestCorrelation };
    };
    const auto [lowRumblePitch, lowRumbleCorrelation] = estimateRumblePitch(48);
    const auto [highRumblePitch, highRumbleCorrelation] = estimateRumblePitch(60);
    if (lowRumbleCorrelation < 0.2 || highRumbleCorrelation < 0.2
        || lowRumblePitch < 25.0 || highRumblePitch < lowRumblePitch * 1.6)
        return fail("rumble pitch did not follow chord-root changes") ? 0 : 1;

    auto staticClickCheck = std::make_unique<QuietMachineAudioProcessor>();
    staticClickCheck->setRateAndBufferSizeDetails(44100.0, 512);
    staticClickCheck->prepareToPlay(44100.0, 512);
    configureStaticOnly(*staticClickCheck);
    juce::AudioBuffer<float> staticAudio(2, 512);
    juce::MidiBuffer staticMidi;
    staticMidi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    std::array<float, 2> previousStaticSample {};
    auto havePreviousStaticSample = false;
    auto maximumStaticStep = 0.0f;
    for (int block = 0; block < 64; ++block)
    {
        staticAudio.clear();
        staticClickCheck->processBlock(staticAudio, staticMidi);
        staticMidi.clear();
        for (int sample = 0; sample < staticAudio.getNumSamples(); ++sample)
            for (int channel = 0; channel < staticAudio.getNumChannels(); ++channel)
            {
                const auto value = staticAudio.getSample(channel, sample);
                if (havePreviousStaticSample)
                    maximumStaticStep = juce::jmax(
                        maximumStaticStep, std::abs(value - previousStaticSample[(size_t) channel]));
                previousStaticSample[(size_t) channel] = value;
            }
        havePreviousStaticSample = true;
    }
    if (maximumStaticStep > 0.18f)
        return fail("static texture contains an audible one-sample click") ? 0 : 1;

    auto stems = std::make_unique<QuietMachineAudioProcessor>();
    if (! stems->enableAllBuses())
        return fail("optional stem buses could not be enabled") ? 0 : 1;
    stems->setRateAndBufferSizeDetails(44100.0, 512);
    stems->prepareToPlay(44100.0, 512);
    configurePlayableSub(*stems);
    stems->setParameterFromUi("masterGain", 0.0f);
    juce::AudioBuffer<float> stemAudio(stems->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer stemMidi;
    stemMidi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    for (int block = 0; block < 24; ++block)
    {
        stemAudio.clear();
        stems->processBlock(stemAudio, stemMidi);
        stemMidi.clear();
    }
    auto mainBus = stems->getBusBuffer(stemAudio, false, 0);
    auto droneBus = stems->getBusBuffer(stemAudio, false, 9);
    auto subBus = stems->getBusBuffer(stemAudio, false, 10);
    auto hissBus = stems->getBusBuffer(stemAudio, false, 12);
    if (bufferEnergy(subBus) <= 0.000001)
        return fail("pre-master Sub stem produced silence") ? 0 : 1;
    if (bufferEnergy(mainBus) > bufferEnergy(subBus) * 0.0001)
        return fail("bus 0 was not processed by NativeMasterFx independently of stems") ? 0 : 1;
    if (bufferEnergy(droneBus) > 0.000000001 || bufferEnergy(hissBus) > 0.000000001)
        return fail("source audio leaked into an unrelated stem") ? 0 : 1;

    auto ownership = std::make_unique<QuietMachineAudioProcessor>();
    if (! ownership->enableAllBuses())
        return fail("ownership test could not enable stem buses") ? 0 : 1;
    ownership->setRateAndBufferSizeDetails(44100.0, 512);
    ownership->prepareToPlay(44100.0, 512);
    configurePlayableSub(*ownership);
    ownership->setParameterFromUi("generatorEnabled", 1.0f);
    juce::AudioBuffer<float> ownershipAudio(ownership->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer ownershipMidi;
    ownership->processBlock(ownershipAudio, ownershipMidi);
    ownershipAudio.clear();
    ownershipMidi.clear();
    ownershipMidi.addEvent(juce::MidiMessage::allNotesOff(1), 128);
    ownership->processBlock(ownershipAudio, ownershipMidi);
    auto ownedSub = ownership->getBusBuffer(ownershipAudio, false, 10);
    double postAllNotesOffEnergy = 0.0;
    for (int sample = 256; sample < ownedSub.getNumSamples(); ++sample)
        for (int channel = 0; channel < ownedSub.getNumChannels(); ++channel)
        {
            const auto value = ownedSub.getSample(channel, sample);
            postAllNotesOffEnergy += (double) value * value;
        }
    if (postAllNotesOffEnergy <= 0.000001)
        return fail("performance All Notes Off erased generator-owned notes") ? 0 : 1;

    processor->setParameterFromUi("followHostTransport", 0.0f);
    processor->setParameterFromUi("generatorEnabled", 1.0f);
    processor->setParameterFromUi("midiOutEnabled", 1.0f);
    processor->clearMidiCapture();
    processor->setParameterFromUi("midiRecordEnabled", 1.0f);

    audio.clear();
    midi.clear();
    processor->processBlock(audio, midi);
    juce::MidiBuffer performanceMidi;
    performanceMidi.addEvent(juce::MidiMessage::noteOn(1, 76, (juce::uint8) 100), 300);
    audio.clear();
    processor->processBlock(audio, performanceMidi);
    bool generatorReleasedForPerformance = false;
    for (const auto metadata : performanceMidi)
        generatorReleasedForPerformance = generatorReleasedForPerformance
                                       || (metadata.getMessage().isNoteOff()
                                           && metadata.samplePosition == 300);
    if (! generatorReleasedForPerformance)
        return fail("generator did not yield to direct MIDI performance") ? 0 : 1;

    performanceMidi.clear();
    performanceMidi.addEvent(juce::MidiMessage::noteOff(1, 76), 300);
    audio.clear();
    processor->processBlock(audio, performanceMidi);
    bool generatorResumed = false;
    for (const auto metadata : performanceMidi)
        generatorResumed = generatorResumed
                        || (metadata.getMessage().isNoteOn()
                            && metadata.samplePosition == 300);
    if (! generatorResumed)
        return fail("generator did not resume after direct MIDI performance") ? 0 : 1;

    performanceMidi.clear();
    performanceMidi.addEvent(juce::MidiMessage::noteOn(1, 79, (juce::uint8) 100), 100);
    performanceMidi.addEvent(juce::MidiMessage::noteOff(1, 79), 300);
    audio.clear();
    processor->processBlock(audio, performanceMidi);
    bool yieldedInsideBlock = false;
    bool resumedInsideBlock = false;
    for (const auto metadata : performanceMidi)
    {
        yieldedInsideBlock = yieldedInsideBlock
                          || (metadata.getMessage().isNoteOff()
                              && metadata.samplePosition == 100);
        resumedInsideBlock = resumedInsideBlock
                          || (metadata.getMessage().isNoteOn()
                              && metadata.samplePosition == 300);
    }
    if (! yieldedInsideBlock || ! resumedInsideBlock)
        return fail("same-block MIDI press/release did not hand generator ownership back accurately") ? 0 : 1;
    processor->setParameterFromUi("ampAttack", 0.73f);
    processor->setParameterFromUi("midiChordMode", 1.0f);

    double energy = 0.0;
    int midiEvents = 0;

    for (int block = 0; block < 430; ++block)
    {
        audio.clear();
        midi.clear();
        processor->processBlock(audio, midi);
        midiEvents += midi.getNumEvents();
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            {
                const auto value = audio.getSample(channel, sample);
                if (! std::isfinite(value))
                    return fail("audio contains NaN or infinity") ? 0 : 1;
                energy += (double) value * value;
            }
    }

    processor->setParameterFromUi("generatorEnabled", 0.0f);
    processor->setParameterFromUi("midiRecordEnabled", 0.0f);
    audio.clear();
    midi.clear();
    processor->processBlock(audio, midi);

    if (energy <= 0.000001)
        return fail("native engine produced silence") ? 0 : 1;
    if (midiEvents <= 0)
        return fail("generator produced no MIDI output") ? 0 : 1;

    juce::MemoryBlock midiBytes;
    if (! midiBytes.fromBase64Encoding(processor->getMidiCaptureAsBase64()) || midiBytes.isEmpty())
        return fail("MIDI capture export is empty") ? 0 : 1;
    juce::MemoryInputStream midiStream(midiBytes, false);
    juce::MidiFile midiFile;
    if (! midiFile.readFrom(midiStream) || midiFile.getNumTracks() <= 0)
        return fail("MIDI capture export is not a readable SMF") ? 0 : 1;

    juce::MemoryBlock state;
    processor->setParameterFromUi("generatorEnabled", 1.0f);
    processor->setParameterFromUi("midiRecordEnabled", 1.0f);
    processor->setParameterFromUi("delayEnabled", 0.0f);
    processor->getStateInformation(state);
    auto restored = std::make_unique<QuietMachineAudioProcessor>();
    restored->setStateInformation(state.getData(), (int) state.getSize());
    if (restored->getPatchStateForUi().isEmpty())
        return fail("browser patch did not survive state round-trip") ? 0 : 1;

    const auto restoredParameters = restored->getParameterStateForUi();
    if (const auto* object = restoredParameters.getDynamicObject();
        object == nullptr
        || (float) object->getProperty("generatorEnabled") >= 0.5f
        || (float) object->getProperty("midiRecordEnabled") >= 0.5f
        || std::abs((float) object->getProperty("ampAttack") - 0.73f) > 0.001f
        || (float) object->getProperty("delayEnabled") >= 0.5f
        || (float) object->getProperty("midiChordMode") < 0.5f)
        return fail("transient transport state survived state round-trip") ? 0 : 1;

    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor->createEditor());
    if (editor == nullptr || editor->getWidth() < 760 || editor->getHeight() < 690)
        return fail("plugin editor was not created at a valid size") ? 0 : 1;
    if (editor->getWantsKeyboardFocus() || editor->getMouseClickGrabsKeyboardFocus())
        return fail("plugin editor still requests keyboard focus from the host") ? 0 : 1;

    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);
    if (editor->getPeer() == nullptr)
        return fail("plugin editor did not create a native window peer") ? 0 : 1;
    juce::Timer::callAfterDelay(1000, []
    {
        juce::MessageManager::getInstance()->stopDispatchLoop();
    });
    juce::MessageManager::getInstance()->runDispatchLoop();
    editor->setVisible(false);
    editor->removeFromDesktop();

    std::cout << "PASS energy=" << energy << " midiEvents=" << midiEvents
              << " midiBytes=" << midiBytes.getSize() << " editor="
              << editor->getWidth() << 'x' << editor->getHeight() << '\n';
    return 0;
}
