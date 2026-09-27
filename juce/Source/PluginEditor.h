#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class QuietMachineAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit QuietMachineAudioProcessorEditor(QuietMachineAudioProcessor&);
    ~QuietMachineAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static juce::WebBrowserComponent::Options makeBrowserOptions(QuietMachineAudioProcessor&);
    static std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    QuietMachineAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;
    juce::Label browserError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuietMachineAudioProcessorEditor)
};
