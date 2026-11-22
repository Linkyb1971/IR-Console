#include "ConsoleLookAndFeel.h"

namespace irc
{
ConsoleLookAndFeel::ConsoleLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::panel);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::TextButton::buttonColourId, Colours::panelDark);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, Colours::panelDark);
    setColour (juce::ComboBox::outlineColourId, Colours::stripEdge);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panelDark);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.8f));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::BubbleComponent::backgroundColourId, Colours::panelDark);
    setColour (juce::BubbleComponent::outlineColourId, Colours::accent);
    setColour (juce::TooltipWindow::backgroundColourId, Colours::panelDark);
    setColour (juce::TooltipWindow::textColourId, Colours::text);
    setColour (juce::AlertWindow::backgroundColourId, Colours::panel);
    setColour (juce::AlertWindow::textColourId, Colours::text);
}

void ConsoleLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float start, float end, juce::Slider& s)
{
    const auto cap = s.findColour (juce::Slider::rotarySliderFillColourId);
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);

    // Value arc (from centre for bipolar controls).
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? start + (float) s.valueToProportionOfLength (0.0) * (end - start) : start;
    juce::Path track, arc;
    track.addCentredArc (c.x, c.y, radius - 1.5f, radius - 1.5f, 0.0f, start, end, true);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.strokePath (track, juce::PathStrokeType (2.5f));
    arc.addCentredArc (c.x, c.y, radius - 1.5f, radius - 1.5f, 0.0f, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
    g.setColour (cap.withAlpha (s.isEnabled() ? 0.95f : 0.3f));
    g.strokePath (arc, juce::PathStrokeType (2.5f));

    // Knob body.
    const float kr = radius - 5.0f;
    juce::ColourGradient body (juce::Colour (0xff4a4e56), c.x, c.y - kr, juce::Colour (0xff1c1d21), c.x, c.y + kr, false);
    g.setGradientFill (body);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);

    // Coloured cap + pointer.
    const float cr = kr * 0.55f;
    g.setColour (cap.darker (0.35f));
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -kr + 1.5f, 3.0f, kr * 0.62f, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void ConsoleLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const auto cap = s.findColour (juce::Slider::thumbColourId);
    const float cx = x + w * 0.5f;
    const bool small = w < 30;

    // Slot.
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.fillRoundedRectangle (cx - 2.5f, (float) y, 5.0f, (float) h, 2.5f);

    // Scale ticks.
    g.setColour (Colours::textDim.withAlpha (0.5f));
    for (int i = 0; i <= 10; ++i)
    {
        const float ty = y + h * (float) i / 10.0f;
        const float tw = (i % 5 == 0) ? (small ? 7.0f : 12.0f) : (small ? 4.0f : 7.0f);
        g.drawHorizontalLine ((int) ty, cx - 6.0f - tw, cx - 6.0f);
        g.drawHorizontalLine ((int) ty, cx + 6.0f, cx + 6.0f + tw);
    }

    // Fader cap.
    const float capW = small ? (float) w - 4.0f : juce::jmin ((float) w - 6.0f, 38.0f);
    const float capH = small ? 14.0f : 30.0f;
    auto r = juce::Rectangle<float> (capW, capH).withCentre ({ cx, pos });
    juce::ColourGradient grad (juce::Colour (0xff6a6e76), r.getX(), r.getY(), juce::Colour (0xff2a2c31), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (cap);
    g.fillRect (r.getX() + 3.0f, r.getCentreY() - 1.0f, r.getWidth() - 6.0f, 2.0f);
}

void ConsoleLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto onColour = b.findColour (juce::TextButton::buttonOnColourId);
    auto base = on ? onColour : b.findColour (juce::TextButton::buttonColourId);
    if (hi) base = base.brighter (0.12f);
    if (down) base = base.darker (0.2f);
    g.setColour (base);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (on ? onColour.brighter (0.4f) : Colours::stripEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void ConsoleLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool down)
{
    // Console-style square switch with LED.
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto led = b.findColour (juce::ToggleButton::tickColourId);
    g.setColour (hi ? Colours::panelDark.brighter (0.1f) : Colours::panelDark);
    if (down) g.setColour (Colours::panelDark.darker());
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (on ? led : Colours::stripEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    auto ledR = r.removeFromLeft (14.0f).withSizeKeepingCentre (7.0f, 7.0f).translated (4.0f, 0.0f);
    g.setColour (on ? led : juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (ledR);
    if (on) { g.setColour (led.withAlpha (0.3f)); g.fillEllipse (ledR.expanded (3.0f)); }
    g.setColour (on ? Colours::text : Colours::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (4.0f), juce::Justification::centred);
}

juce::Font ConsoleLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return juce::FontOptions (juce::jmin (13.0f, h * 0.6f), juce::Font::bold);
}

juce::Label* ConsoleLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, Colours::text);
    l->setFont (juce::FontOptions (11.0f));
    return l;
}

void ConsoleLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (Colours::panelDark);
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (Colours::accent);
    g.drawRoundedRectangle (body, 4.0f, 1.0f);
}
}
