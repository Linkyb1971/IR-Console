#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace irc
{
namespace Colours
{
    const juce::Colour panel     { 0xff23252a };
    const juce::Colour panelDark { 0xff17181b };
    const juce::Colour strip     { 0xff2c2f35 };
    const juce::Colour stripEdge { 0xff3c4048 };
    const juce::Colour text      { 0xffd8dadf };
    const juce::Colour textDim   { 0xff8a8f99 };
    const juce::Colour accent    { 0xffffa53a };
    const juce::Colour filter    { 0xff9aa0aa };
    const juce::Colour high      { 0xff4f9de8 };
    const juce::Colour mid       { 0xff4fc47a };
    const juce::Colour low       { 0xffe0604f };
    const juce::Colour pan       { 0xffe8e8e8 };
    const juce::Colour push      { 0xffffa53a };
    const juce::Colour mute      { 0xffe04848 };
    const juce::Colour solo      { 0xfff2c94c };
    const juce::Colour phase     { 0xff59b4e0 };
}

class ConsoleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ConsoleLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return juce::FontOptions (13.0f); }
};
}
