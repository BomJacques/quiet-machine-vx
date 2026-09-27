#include "PluginEditor.h"
#include <QuietMachineWebData.h>

namespace
{
std::vector<std::byte> toBytes(const char* data, int size)
{
    std::vector<std::byte> result((size_t) size);
    std::memcpy(result.data(), data, (size_t) size);
    return result;
}
}

juce::WebBrowserComponent::Options QuietMachineAudioProcessorEditor::makeBrowserOptions(QuietMachineAudioProcessor& processor)
{
#if ! QM_ENABLE_WEBVIEW2
    juce::ignoreUnused(processor);
    return {};
#else
    return juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2{}
            .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("QuietMachineVX-WebView2-"
                    + juce::String::toHexString((juce::pointer_sized_int) &processor))))
        .withNativeIntegrationEnabled()
        .withNativeFunction("setParameter", [&processor](const auto& args, auto complete)
        {
            if (args.size() >= 2)
                processor.setParameterFromUi(args[0].toString(), (float) args[1]);
            complete(juce::var());
        })
        .withNativeFunction("beginParameterGesture", [&processor](const auto& args, auto complete)
        {
            if (! args.isEmpty())
                processor.beginParameterGesture(args[0].toString());
            complete(juce::var());
        })
        .withNativeFunction("endParameterGesture", [&processor](const auto& args, auto complete)
        {
            if (! args.isEmpty())
                processor.endParameterGesture(args[0].toString());
            complete(juce::var());
        })
        .withNativeFunction("getParameterState", [&processor](const auto&, auto complete)
        {
            complete(processor.getParameterStateForUi());
        })
        .withNativeFunction("setPatchState", [&processor](const auto& args, auto complete)
        {
            if (! args.isEmpty())
                processor.setPatchStateFromUi(args[0].toString());
            complete(juce::var());
        })
        .withNativeFunction("getPatchState", [&processor](const auto&, auto complete)
        {
            complete(processor.getPatchStateForUi());
        })
        .withNativeFunction("getRecordedMidi", [&processor](const auto&, auto complete)
        {
            complete(processor.getMidiCaptureAsBase64());
        })
        .withNativeFunction("clearMidiRecording", [&processor](const auto&, auto complete)
        {
            processor.clearMidiCapture();
            complete(juce::var());
        })
        .withNativeFunction("noteOn", [&processor](const auto& args, auto complete)
        {
            if (args.size() >= 2)
                processor.noteOnFromUi((int) args[0], (float) args[1]);
            complete(juce::var());
        })
        .withNativeFunction("noteOff", [&processor](const auto& args, auto complete)
        {
            if (! args.isEmpty())
                processor.noteOffFromUi((int) args[0]);
            complete(juce::var());
        })
        .withNativeFunction("panic", [&processor](const auto&, auto complete)
        {
            processor.panicFromUi();
            complete(juce::var());
        })
        .withResourceProvider([](const auto& url) { return getResource(url); });
#endif
}

std::optional<juce::WebBrowserComponent::Resource> QuietMachineAudioProcessorEditor::getResource(const juce::String& url)
{
    const auto path = url == "/" ? juce::String("index.html")
                                  : url.fromFirstOccurrenceOf("/", false, false);

    if (path == "index.html")
        return juce::WebBrowserComponent::Resource {
            toBytes(QuietMachineWeb::index_html, QuietMachineWeb::index_htmlSize), "text/html"
        };

    if (path == "juce/index.js")
        return juce::WebBrowserComponent::Resource {
            toBytes(QuietMachineWeb::index_js, QuietMachineWeb::index_jsSize), "text/javascript"
        };

    if (path == "juce/check_native_interop.js")
        return juce::WebBrowserComponent::Resource {
            toBytes(QuietMachineWeb::check_native_interop_js,
                    QuietMachineWeb::check_native_interop_jsSize),
            "text/javascript"
        };

    if (path == "vendor/tailwind-3.4.17.js")
        return juce::WebBrowserComponent::Resource {
            toBytes(QuietMachineWeb::tailwind3_4_17_js, QuietMachineWeb::tailwind3_4_17_jsSize), "text/javascript"
        };

    if (path == "vendor/react-18.2.0.js")
        return juce::WebBrowserComponent::Resource {
            toBytes(QuietMachineWeb::react18_2_0_js, QuietMachineWeb::react18_2_0_jsSize), "text/javascript"
        };

    if (path == "vendor/react-dom-18.2.0.js")
        return juce::WebBrowserComponent::Resource {
            toBytes(QuietMachineWeb::reactdom18_2_0_js, QuietMachineWeb::reactdom18_2_0_jsSize), "text/javascript"
        };

    return std::nullopt;
}

QuietMachineAudioProcessorEditor::QuietMachineAudioProcessorEditor(QuietMachineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    setResizable(true, true);
    setResizeLimits(760, 690, 1720, 1200);
    setSize(1180, 760);

#if QM_ENABLE_WEBVIEW2
    const auto options = makeBrowserOptions(p);
    if (juce::WebBrowserComponent::areOptionsSupported(options))
    {
        browser = std::make_unique<juce::WebBrowserComponent>(options);
        browser->setWantsKeyboardFocus(false);
        browser->setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(*browser);
        browser->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    }
    else
    {
        browserError.setText("Quiet Machine VX requires the Microsoft Edge WebView2 Runtime.",
                             juce::dontSendNotification);
        browserError.setColour(juce::Label::textColourId, juce::Colours::white);
        browserError.setFont(juce::FontOptions(18.0f));
        browserError.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(browserError);
    }
#else
    browserError.setText("Quiet Machine VX was built without WebView2 editor support.",
                         juce::dontSendNotification);
    browserError.setColour(juce::Label::textColourId, juce::Colours::white);
    browserError.setFont(juce::FontOptions(18.0f));
    browserError.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(browserError);
#endif

    resized();
}

void QuietMachineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(16, 19, 17));
}

void QuietMachineAudioProcessorEditor::resized()
{
    if (browser != nullptr)
        browser->setBounds(getLocalBounds());
    browserError.setBounds(getLocalBounds().reduced(32));
}
