#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ConsoleLookAndFeel.h"

namespace irc
{
using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

/** Labelled rotary knob attached to a parameter. */
struct Knob : juce::Component
{
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& labelText,
          juce::Colour colour, int decimals = 1);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<SliderAttachment> attachment;
};

/** Small LED switch attached to a bool parameter. */
struct Switch : juce::ToggleButton
{
    Switch (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text, juce::Colour led);
    std::unique_ptr<ButtonAttachment> attachment;
};

class ChannelStrip : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    ChannelStrip (IRConsoleProcessor&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshIR();
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }

private:
    void choose();
    void showMenu();
    void loadFile (const juce::File&);

    IRConsoleProcessor& proc;
    const int index;
    juce::TextButton irButton;
    juce::Rectangle<int> thumbArea;
    juce::Path thumbPath;
    bool dragOver = false, isStereoIR = false;
    Knob hpf, lpf, hiG, hiF, midG, midF, midQ, align, loG, loF, pan;
    Switch mute, solo, invert;
    juce::Slider fader;
    std::unique_ptr<SliderAttachment> faderAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
};

class ResponseView : public juce::Component
{
public:
    explicit ResponseView (IRConsoleProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void update();
private:
    IRConsoleProcessor& proc;
    IRConsoleProcessor::Display display;
};

class ExportPanel : public juce::Component
{
public:
    explicit ExportPanel (IRConsoleProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override {}
    std::function<void()> onClose;

private:
    void applyPreset();
    void doExport();
    juce::String describe() const;

    IRConsoleProcessor& proc;
    juce::Rectangle<int> box;
    juce::Label title, info;
    juce::ComboBox preset, channels, rate, bits, length;
    juce::Label presetL, channelsL, rateL, bitsL, lengthL;
    juce::ToggleButton includeEq { "Include master EQ" }, normalise { "Normalise peak (-0.1 dBFS)" };
    juce::TextButton exportButton { "EXPORT WAV..." }, cancel { "CANCEL" };
    std::unique_ptr<juce::FileChooser> chooser;
};

/** The whole console at its design size; the editor scales it. */
class ConsoleView : public juce::Component, private juce::Timer
{
public:
    explicit ConsoleView (IRConsoleProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int designW = 1480, designH = 790;

private:
    void timerCallback() override;

    IRConsoleProcessor& proc;
    juce::OwnedArray<ChannelStrip> strips;
    ResponseView response;
    Switch stereo, autoLevel, eqOn;
    Knob input, push, reso;
    juce::OwnedArray<juce::Slider> eqSliders;
    juce::OwnedArray<SliderAttachment> eqAttachments;
    juce::Slider output;
    std::unique_ptr<SliderAttachment> outputAttachment;
    juce::TextButton exportButton { "EXPORT BLENDED IR" }, flattenEq { "FLAT" };
    juce::Label blendInfo;
    juce::Rectangle<int> masterArea, eqArea, meterArea, pushArea;
    float meterL = 0, meterR = 0;
    int lastIRVersion = -1, lastDisplayVersion = -1;
    std::unique_ptr<ExportPanel> exportPanel;
};
} // namespace irc

class IRConsoleEditor : public juce::AudioProcessorEditor
{
public:
    explicit IRConsoleEditor (IRConsoleProcessor&);
    ~IRConsoleEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (irc::Colours::panelDark); }

private:
    irc::ConsoleLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    irc::ConsoleView view;
};
