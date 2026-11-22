#include "ConsoleLookAndFeel.h"

namespace irc
{
void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    juce::ColourGradient grad (juce::Colour (0xffd8d8d4), c.x - r, c.y - r, juce::Colour (0xff6c6c68), c.x + r, c.y + r, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);
    juce::Line<float> slot (c.x - r * 0.75f, c.y, c.x + r * 0.75f, c.y);
    slot.applyTransform (juce::AffineTransform::rotation (angle, c.x, c.y));
    g.setColour (juce::Colour (0xff3a3a38));
    g.drawLine (slot, 1.4f);
}

void drawWood (juce::Graphics& g, juce::Rectangle<float> a, bool vertical)
{
    juce::ColourGradient base (Colours::walnutLt, a.getX(), a.getY(),
                               Colours::walnut, vertical ? a.getRight() : a.getX(), vertical ? a.getY() : a.getBottom(), false);
    base.addColour (0.5, Colours::walnut.brighter (0.15f));
    g.setGradientFill (base);
    g.fillRect (a);
    // Grain: deterministic wavy lines.
    juce::Random rnd (1234);
    const int lines = (int) ((vertical ? a.getWidth() : a.getHeight()) / 2.2f);
    for (int i = 0; i < lines; ++i)
    {
        juce::Path grain;
        const float off = (float) i * 2.2f + rnd.nextFloat();
        const float len = vertical ? a.getHeight() : a.getWidth();
        const float amp = 0.6f + rnd.nextFloat() * 1.4f, freq = 0.004f + rnd.nextFloat() * 0.01f, ph = rnd.nextFloat() * 6.0f;
        for (float t = 0; t <= len; t += 8.0f)
        {
            const float w = off + amp * std::sin (t * freq + ph);
            const juce::Point<float> pt = vertical ? juce::Point<float> (a.getX() + w, a.getY() + t) : juce::Point<float> (a.getX() + t, a.getY() + w);
            if (t == 0) grain.startNewSubPath (pt); else grain.lineTo (pt);
        }
        g.setColour (juce::Colours::black.withAlpha (0.05f + rnd.nextFloat() * 0.1f));
        g.strokePath (grain, juce::PathStrokeType (0.8f));
    }
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRect (a, 1.0f);
}

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

    // Knurled metal skirt.
    const float kr = radius - 5.0f;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - kr + 1.5f, c.y - kr + 2.5f, kr * 2, kr * 2);   // drop shadow
    juce::ColourGradient skirt (juce::Colour (0xffcfcfc8), c.x - kr, c.y - kr, juce::Colour (0xff5d5d58), c.x + kr, c.y + kr, false);
    g.setGradientFill (skirt);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    for (int i = 0; i < 36; ++i)
    {
        const float t = juce::MathConstants<float>::twoPi * i / 36.0f;
        g.drawLine (c.x + std::sin (t) * kr * 0.82f, c.y - std::cos (t) * kr * 0.82f,
                    c.x + std::sin (t) * kr, c.y - std::cos (t) * kr, 0.7f);
    }

    // Bakelite body.
    const float br = kr * 0.78f;
    juce::ColourGradient body (juce::Colour (0xff3b3b3d), c.x - br * 0.5f, c.y - br, juce::Colour (0xff0d0d0e), c.x + br * 0.3f, c.y + br, false);
    g.setGradientFill (body);
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);

    // Coloured cap with a soft highlight.
    const float cr = br * 0.62f;
    juce::ColourGradient capGrad (cap.brighter (0.35f), c.x - cr * 0.6f, c.y - cr * 0.7f, cap.darker (0.45f), c.x + cr, c.y + cr, true);
    g.setGradientFill (capGrad);
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);

    // Pointer line through the body.
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.3f, -br + 1.0f, 2.6f, br * 0.55f, 1.2f);
    g.setColour (Colours::ivory);
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
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (1.5f, 3.0f), 3.0f);
    juce::ColourGradient grad (Colours::ivory.brighter (0.1f), r.getX(), r.getY(), Colours::ivory.darker (0.45f), r.getX(), r.getBottom(), false);
    grad.addColour (0.48, Colours::ivory.darker (0.05f));
    grad.addColour (0.52, Colours::ivory.darker (0.3f));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (cap.darker (0.2f));
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

void ConsoleLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) == nullptr || label.isBeingEdited())
    {
        LookAndFeel_V4::drawLabel (g, label);
        return;
    }
    // Slider readouts: engraved-style text on a small dark window, no outline.
    auto r = label.getLocalBounds().toFloat().reduced (6.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (label.getText(), r, juce::Justification::centred);
}

void ConsoleLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (Colours::panelDark);
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (Colours::accent);
    g.drawRoundedRectangle (body, 4.0f, 1.0f);
}
}
