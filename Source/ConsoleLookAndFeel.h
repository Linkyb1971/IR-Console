#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace irc
{
namespace Colours
{
    // Vintage console: slate-blue painted steel, cream screen-printing, walnut trim.
    const juce::Colour panel     { 0xff3b444b };
    const juce::Colour panelDark { 0xff1d2124 };
    const juce::Colour strip     { 0xff4f5d68 };
    const juce::Colour stripEdge { 0xff36414a };
    const juce::Colour text      { 0xffefe6cf };
    const juce::Colour textDim   { 0xffc4b99d };
    const juce::Colour accent    { 0xffe9a23b };
    const juce::Colour filter    { 0xffa9a699 };
    const juce::Colour high      { 0xff3f7fc0 };
    const juce::Colour mid       { 0xff4f9a5e };
    const juce::Colour low       { 0xffb5483b };
    const juce::Colour pan       { 0xffe9e0c8 };
    const juce::Colour push      { 0xffe08a2e };
    const juce::Colour mute      { 0xffe04848 };
    const juce::Colour solo      { 0xfff2c94c };
    const juce::Colour phase     { 0xff59b4e0 };
    const juce::Colour ivory     { 0xffeee4c8 };
    const juce::Colour walnut    { 0xff5a3520 };
    const juce::Colour walnutLt  { 0xff8a5634 };
}

/** Painted helpers shared by the console components. */
void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius, float angle = 0.6f);
void drawWood (juce::Graphics&, juce::Rectangle<float> area, bool vertical);

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
    void drawLabel (juce::Graphics&, juce::Label&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return juce::FontOptions (13.0f); }
};
}
