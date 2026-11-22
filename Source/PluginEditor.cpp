#include "PluginEditor.h"

namespace irc
{
namespace
{
    juce::String formatHz (double v)
    {
        if (v < 1000.0) return juce::String ((int) std::round (v));
        const double k = v / 1000.0;
        if (std::abs (k - std::round (k)) < 0.01) return juce::String ((int) std::round (k)) + "k";
        return juce::String (k, k >= 10.0 ? 1 : 2).trimCharactersAtEnd ("0") + "k";
    }

    void drawSectionLabel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text)
    {
        g.setColour (Colours::textDim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (text, r, juce::Justification::centredLeft);
    }
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& labelText,
            juce::Colour colour, int decimals)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setPopupDisplayEnabled (true, true, nullptr, 1500);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    addAndMakeVisible (slider);
    attachment = std::make_unique<SliderAttachment> (state, paramId, slider);

    auto* param = state.getParameter (paramId);
    const bool isHz = param->getLabel() == "Hz";
    slider.textFromValueFunction = [isHz, decimals, param] (double v)
    {
        if (isHz) return formatHz (v) + " Hz";
        return juce::String (v, decimals) + (param->getLabel().isNotEmpty() ? " " + param->getLabel() : juce::String());
    };
    slider.setTooltip (param->getName (64));
    // Double-click resets to default.
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, Colours::textDim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (13));
    slider.setBounds (r);
}

Switch::Switch (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text, juce::Colour led)
{
    setButtonText (text);
    setColour (juce::ToggleButton::tickColourId, led);
    attachment = std::make_unique<ButtonAttachment> (state, paramId, *this);
    setTooltip (state.getParameter (paramId)->getName (64));
}

//==============================================================================
ChannelStrip::ChannelStrip (IRConsoleProcessor& p, int i)
    : proc (p), index (i),
      hpf  (p.apvts, IRConsoleProcessor::chId (i, "hpf"),      "HPF",   Colours::filter),
      lpf  (p.apvts, IRConsoleProcessor::chId (i, "lpf"),      "LPF",   Colours::filter),
      hiG  (p.apvts, IRConsoleProcessor::chId (i, "highGain"), "HF",    Colours::high),
      hiF  (p.apvts, IRConsoleProcessor::chId (i, "highFreq"), "HF Hz", Colours::high),
      midG (p.apvts, IRConsoleProcessor::chId (i, "midGain"),  "MF",    Colours::mid),
      midF (p.apvts, IRConsoleProcessor::chId (i, "midFreq"),  "MF Hz", Colours::mid),
      midQ (p.apvts, IRConsoleProcessor::chId (i, "midQ"),     "MF Q",  Colours::mid, 2),
      align(p.apvts, IRConsoleProcessor::chId (i, "delay"),    "ALIGN", Colours::filter, 3),
      loG  (p.apvts, IRConsoleProcessor::chId (i, "lowGain"),  "LF",    Colours::low),
      loF  (p.apvts, IRConsoleProcessor::chId (i, "lowFreq"),  "LF Hz", Colours::low),
      pan  (p.apvts, IRConsoleProcessor::chId (i, "pan"),      "PAN",   Colours::pan, 2),
      mute (p.apvts, IRConsoleProcessor::chId (i, "mute"),     "M",     Colours::mute),
      solo (p.apvts, IRConsoleProcessor::chId (i, "solo"),     "S",     Colours::solo),
      invert (p.apvts, IRConsoleProcessor::chId (i, "invert"), juce::String::fromUTF8 ("\xc3\x98"), Colours::phase)
{
    irButton.setColour (juce::TextButton::buttonColourId, Colours::panelDark);
    irButton.onClick = [this]
    {
        if (juce::ModifierKeys::currentModifiers.isPopupMenu() || proc.getIR (index) != nullptr) showMenu();
        else choose();
    };
    addAndMakeVisible (irButton);

    for (auto* k : { &hpf, &lpf, &hiG, &hiF, &midG, &midF, &midQ, &align, &loG, &loF, &pan })
        addAndMakeVisible (k);
    for (auto* s : { &mute, &solo, &invert })
        addAndMakeVisible (s);

    pan.slider.textFromValueFunction = [] (double v)
    {
        if (std::abs (v) < 0.005) return juce::String ("C");
        return (v < 0 ? "L" : "R") + juce::String ((int) std::round (std::abs (v) * 100.0));
    };

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    fader.setColour (juce::Slider::thumbColourId, Colours::accent);
    faderAttachment = std::make_unique<SliderAttachment> (p.apvts, IRConsoleProcessor::chId (i, "gain"), fader);
    fader.textFromValueFunction = [] (double v) { return v <= -59.9 ? juce::String ("-inf") : juce::String (v, 1) + " dB"; };
    fader.setDoubleClickReturnValue (true, 0.0);
    fader.updateText();
    addAndMakeVisible (fader);
    refreshIR();
}

void ChannelStrip::refreshIR()
{
    auto ir = proc.getIR (index);
    thumbPath.clear();
    isStereoIR = ir != nullptr && ir->data.getNumChannels() > 1;
    if (ir == nullptr)
    {
        irButton.setButtonText ("LOAD IR");
        irButton.setTooltip ("Click to load an impulse response, or drag a WAV file onto this channel");
    }
    else
    {
        irButton.setButtonText (ir->name);
        irButton.setTooltip (ir->name + "\n" + juce::String (ir->data.getNumSamples()) + " samples @ "
                             + juce::String (ir->sampleRate / 1000.0, 1) + " kHz, "
                             + (isStereoIR ? "stereo" : "mono") + "\n" + ir->path);

        // Thumbnail of the first ~30 ms, peak per pixel.
        const int w = juce::jmax (1, thumbArea.getWidth());
        const int len = juce::jmin (ir->data.getNumSamples(), (int) (0.03 * ir->sampleRate));
        const float peak = juce::jmax (1e-6f, ir->data.getMagnitude (0, 0, ir->data.getNumSamples()));
        const float* d = ir->data.getReadPointer (0);
        const float mid = thumbArea.getCentreY(), half = thumbArea.getHeight() * 0.45f;
        thumbPath.startNewSubPath ((float) thumbArea.getX(), mid);
        for (int x = 0; x < w; ++x)
        {
            const int a = x * len / w, b = juce::jmax (a + 1, (x + 1) * len / w);
            float lo = 0, hi = 0;
            for (int n = a; n < b && n < len; ++n) { lo = juce::jmin (lo, d[n]); hi = juce::jmax (hi, d[n]); }
            thumbPath.lineTo ((float) (thumbArea.getX() + x), mid - hi / peak * half);
            thumbPath.lineTo ((float) (thumbArea.getX() + x) + 0.5f, mid - lo / peak * half);
        }
    }
    repaint();
}

void ChannelStrip::choose()
{
    chooser = std::make_unique<juce::FileChooser> ("Load IR for channel " + juce::String (index + 1),
                                                   juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<ChannelStrip> (this)] (const juce::FileChooser& fc)
                          {
                              if (safe != nullptr && fc.getResult().existsAsFile()) safe->loadFile (fc.getResult());
                          });
}

void ChannelStrip::showMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Load IR...");
    m.addItem (2, "Clear channel", proc.getIR (index) != nullptr);
    m.addItem (3, "Reset channel controls");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (irButton),
                     [safe = juce::Component::SafePointer<ChannelStrip> (this)] (int r)
                     {
                         if (safe == nullptr) return;
                         if (r == 1) safe->choose();
                         if (r == 2) safe->proc.clearIR (safe->index);
                         if (r == 3)
                             for (auto* prm : safe->proc.getParameters())
                                 if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                                     if (rp->getParameterID().startsWith (IRConsoleProcessor::chId (safe->index, "")))
                                         rp->setValueNotifyingHost (rp->getDefaultValue());
                     });
}

void ChannelStrip::loadFile (const juce::File& f)
{
    const auto err = proc.loadIR (index, f);
    if (err.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load IR", err);
}

void ChannelStrip::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    repaint();
    if (! files.isEmpty()) loadFile (juce::File (files[0]));
}

void ChannelStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (Colours::strip);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (dragOver ? Colours::accent : Colours::stripEdge);
    g.drawRoundedRectangle (r, 5.0f, dragOver ? 2.0f : 1.0f);

    // Channel number badge.
    g.setColour (Colours::accent);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (juce::String (index + 1), getLocalBounds().removeFromTop (24).reduced (8, 0), juce::Justification::centredLeft);
    g.setColour (Colours::textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    auto ir = proc.getIR (index);
    g.drawText (ir == nullptr ? "EMPTY" : (isStereoIR ? "STEREO IR" : "MONO IR"),
                getLocalBounds().removeFromTop (24).reduced (8, 0), juce::Justification::centredRight);

    // Waveform thumbnail.
    g.setColour (Colours::panelDark);
    g.fillRect (thumbArea);
    if (! thumbPath.isEmpty())
    {
        g.setColour (Colours::accent.withAlpha (0.85f));
        g.strokePath (thumbPath, juce::PathStrokeType (1.0f));
    }
    else
    {
        g.setColour (Colours::textDim.withAlpha (0.6f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("drop IR here", thumbArea, juce::Justification::centred);
    }

    // Section separators.
    g.setColour (Colours::stripEdge);
    for (auto* k : { &hiG, &pan })
        g.drawHorizontalLine (k->getY() - 3, 8.0f, getWidth() - 8.0f);
    g.drawHorizontalLine (fader.getY() - 6, 8.0f, getWidth() - 8.0f);
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced (6, 4);
    r.removeFromTop (22);
    irButton.setBounds (r.removeFromTop (24));
    r.removeFromTop (4);
    thumbArea = r.removeFromTop (34);
    r.removeFromTop (6);

    const int rowH = 56;
    auto row = [&] (Knob& a, Knob& b)
    {
        auto line = r.removeFromTop (rowH);
        a.setBounds (line.removeFromLeft (line.getWidth() / 2));
        b.setBounds (line);
    };
    row (hpf, lpf);
    r.removeFromTop (6);
    row (hiG, hiF);
    row (midG, midF);
    row (midQ, align);
    row (loG, loF);
    r.removeFromTop (6);
    pan.setBounds (r.removeFromTop (rowH).withSizeKeepingCentre (60, rowH));
    r.removeFromTop (4);
    auto buttons = r.removeFromTop (24);
    const int bw = buttons.getWidth() / 3;
    mute.setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    solo.setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
    invert.setBounds (buttons.reduced (2, 0));
    r.removeFromTop (12);
    fader.setBounds (r);
    refreshIR();
}

//==============================================================================
void ResponseView::update()
{
    display = proc.getDisplay();
    repaint();
}

void ResponseView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff101113));
    g.fillRoundedRectangle (r, 4.0f);
    auto plot = r.reduced (30.0f, 14.0f).withTrimmedBottom (4.0f);
    const float rangeDb = 24.0f;

    auto xFor = [&] (double f) { return plot.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * plot.getWidth(); };
    auto yFor = [&] (float db) { return plot.getCentreY() - juce::jlimit (-rangeDb, rangeDb, db) / rangeDb * plot.getHeight() * 0.5f; };

    g.setFont (juce::FontOptions (9.5f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawVerticalLine ((int) xFor (f), plot.getY(), plot.getBottom());
        g.setColour (Colours::textDim);
        g.drawText (formatHz (f), juce::Rectangle<float> (40, 12).withCentre ({ xFor (f), plot.getBottom() + 9 }), juce::Justification::centred);
    }
    for (float db : { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f })
    {
        g.setColour (juce::Colours::white.withAlpha (db == 0.0f ? 0.15f : 0.06f));
        g.drawHorizontalLine ((int) yFor (db), plot.getX(), plot.getRight());
        g.setColour (Colours::textDim);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (26, 12).withCentre ({ plot.getX() - 15, yFor (db) }), juce::Justification::centred);
    }

    const auto settings = proc.getCurrentSettings();
    const int np = IRConsoleProcessor::displayPoints;
    auto curveFor = [&] (const std::vector<float>& resp, bool withEq)
    {
        // Centre the IR curve on its 200 Hz - 4 kHz average, then add the live master EQ.
        double ref = 0; int cnt = 0;
        for (int i = 0; i < np && i < (int) resp.size(); ++i)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / (np - 1));
            if (f > 200 && f < 4000) { ref += resp[(size_t) i]; ++cnt; }
        }
        ref = cnt > 0 ? ref / cnt : 0.0;
        juce::Path p;
        for (int i = 0; i < np; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / (np - 1));
            float db = display.empty || resp.empty() ? 0.0f : (float) (resp[(size_t) i] - ref);
            if (withEq && settings.masterEqOn) db += masterEqResponseDb (settings.masterEq, 48000.0, f);
            const float x = xFor (f), y = yFor (db);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };

    if (display.stereo && ! display.responseR.empty())
    {
        g.setColour (Colours::high.withAlpha (0.8f));
        g.strokePath (curveFor (display.responseR, true), juce::PathStrokeType (1.4f));
    }
    g.setColour (Colours::accent);
    g.strokePath (curveFor (display.responseL, true), juce::PathStrokeType (1.8f));

    // Master EQ curve alone (dashed reference).
    if (settings.masterEqOn)
    {
        juce::Path eq;
        for (int i = 0; i < np; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / (np - 1));
            const float x = xFor (f), y = yFor (masterEqResponseDb (settings.masterEq, 48000.0, f));
            if (i == 0) eq.startNewSubPath (x, y); else eq.lineTo (x, y);
        }
        juce::Path dashed;
        const float dashes[] = { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, eq, dashes, 2);
        g.setColour (Colours::mid.withAlpha (0.7f));
        g.fillPath (dashed);
    }

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.setColour (Colours::accent);
    g.drawText (display.stereo ? "L" : "MONO", plot.withHeight (12).translated (4, -10), juce::Justification::centredRight);
    if (display.stereo)
    {
        g.setColour (Colours::high);
        g.drawText ("R      ", plot.withHeight (12).translated (4, -10), juce::Justification::centredRight);
    }
    g.setColour (Colours::mid);
    g.drawText ("- - MASTER EQ", plot.withHeight (12).translated (0, -10), juce::Justification::centredLeft);
    if (display.empty)
    {
        g.setColour (Colours::textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Load IRs onto the channels to start blending", plot, juce::Justification::centred);
    }
}

//==============================================================================
namespace
{
    struct Preset { const char* name; int stereo; int rateIdx; int bitsIdx; int lenIdx; bool eq; bool norm; };
    // rate: 0=44.1 1=48 2=96 ; bits: 0=16 1=24 2=32f ; length: 0=1024 1=2048 2=4096 3=8192 4=500ms 5=full
    const Preset presets[] = {
        { "Line 6 Helix / HX (48k, 16-bit, mono, 2048)",     0, 1, 0, 1, true, true },
        { "Neural DSP Quad Cortex (48k, 24-bit, mono, 1024)", 0, 1, 1, 0, true, true },
        { "Generic hardware (48k, 24-bit, mono, 2048)",       0, 1, 1, 1, true, true },
        { "IR Console / DAW stereo (48k, 32-bit float, full)",1, 1, 2, 5, true, false },
        { "IR Console / DAW mono (48k, 24-bit, full)",        0, 1, 1, 5, true, false },
    };
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int bitDepths[] = { 16, 24, 32 };
}

ExportPanel::ExportPanel (IRConsoleProcessor& p) : proc (p)
{
    title.setText ("EXPORT BLENDED IR", juce::dontSendNotification);
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, Colours::accent);
    addAndMakeVisible (title);

    auto setup = [this] (juce::ComboBox& c, juce::Label& l, const char* text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, Colours::textDim);
        l.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (l);
        addAndMakeVisible (c);
        c.onChange = [this] { info.setText (describe(), juce::dontSendNotification); };
    };
    setup (preset, presetL, "TARGET");
    setup (channels, channelsL, "CHANNELS");
    setup (rate, rateL, "SAMPLE RATE");
    setup (bits, bitsL, "BIT DEPTH");
    setup (length, lengthL, "LENGTH");

    int id = 1;
    for (auto& pr : presets) preset.addItem (pr.name, id++);
    preset.addItem ("Custom", id);
    channels.addItemList ({ "Mono", "Stereo" }, 1);
    rate.addItemList ({ "44.1 kHz", "48 kHz", "96 kHz" }, 1);
    bits.addItemList ({ "16-bit", "24-bit", "32-bit float" }, 1);
    length.addItemList ({ "1024 samples", "2048 samples", "4096 samples", "8192 samples", "500 ms", "Full (auto-trimmed)" }, 1);
    preset.onChange = [this] { applyPreset(); };
    for (auto* c : { &channels, &rate, &bits, &length })
        c->onChange = [this, c]
        {
            juce::ignoreUnused (c);
            preset.setSelectedId (preset.getNumItems(), juce::dontSendNotification);
            info.setText (describe(), juce::dontSendNotification);
        };

    for (auto* t : { &includeEq, &normalise })
    {
        t->setColour (juce::ToggleButton::tickColourId, Colours::accent);
        t->onClick = [this] { info.setText (describe(), juce::dontSendNotification); };
        addAndMakeVisible (t);
    }
    info.setColour (juce::Label::textColourId, Colours::text);
    info.setFont (juce::FontOptions (12.0f));
    info.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (info);

    exportButton.setColour (juce::TextButton::buttonColourId, Colours::accent.darker (0.3f));
    exportButton.onClick = [this] { doExport(); };
    cancel.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (exportButton);
    addAndMakeVisible (cancel);

    preset.setSelectedId (1, juce::sendNotificationSync);
}

void ExportPanel::applyPreset()
{
    const int i = preset.getSelectedId() - 1;
    if (i < 0 || i >= (int) std::size (presets)) return;
    const auto& pr = presets[i];
    channels.setSelectedItemIndex (pr.stereo, juce::dontSendNotification);
    rate.setSelectedItemIndex (pr.rateIdx, juce::dontSendNotification);
    bits.setSelectedItemIndex (pr.bitsIdx, juce::dontSendNotification);
    length.setSelectedItemIndex (pr.lenIdx, juce::dontSendNotification);
    includeEq.setToggleState (pr.eq, juce::dontSendNotification);
    normalise.setToggleState (pr.norm, juce::dontSendNotification);
    info.setText (describe(), juce::dontSendNotification);
}

juce::String ExportPanel::describe() const
{
    juce::String s;
    const double sr = rates[juce::jmax (0, rate.getSelectedItemIndex())];
    const int li = length.getSelectedItemIndex();
    const int lens[] = { 1024, 2048, 4096, 8192, (int) (0.5 * sr), 0 };
    if (li >= 0 && lens[li] > 0)
        s << lens[li] << " samples = " << juce::String (lens[li] * 1000.0 / sr, 1) << " ms at " << sr / 1000.0 << " kHz.\n";
    else
        s << "Full blended length, trimmed where the tail decays below -100 dB.\n";
    if (channels.getSelectedItemIndex() == 0)
        s << "Mono: all channels are summed (pan is ignored).\n";
    else
        s << "Stereo: L/R follow the channel pans. Most hardware uses only the left channel, so use Mono for hardware.\n";
    s << "Speaker Push and input/output level are live processing and are not part of the IR.";
    if (! includeEq.getToggleState()) s << " Master EQ is not included.";
    const int pi = preset.getSelectedId() - 1;
    if (pi == 0) s << "\nHelix: import through HX Edit / Helix Native's IR tab.";
    if (pi == 1) s << "\nQuad Cortex: import through Cortex Control. The QC has been reported to use only the first 1024 samples; check your firmware.";
    return s;
}

void ExportPanel::doExport()
{
    const int pi = preset.getSelectedId() - 1;
    juce::String suggested = "IR Console Blend";
    if (pi == 0) suggested << " - Helix";
    if (pi == 1) suggested << " - QC";
    suggested << (channels.getSelectedItemIndex() == 1 ? " (Stereo)" : " (Mono)") << ".wav";

    chooser = std::make_unique<juce::FileChooser> ("Export blended IR",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (suggested), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<ExportPanel> (this)] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            auto file = fc.getResult();
            if (file == juce::File()) return;
            file = file.withFileExtension ("wav");

            auto& self = *safe;
            auto s = self.proc.getCurrentSettings();
            s.sampleRate = rates[juce::jmax (0, self.rate.getSelectedItemIndex())];
            s.stereo = self.channels.getSelectedItemIndex() == 1;
            s.includeMasterEq = self.includeEq.getToggleState();
            s.peakNormalise = self.normalise.getToggleState();
            const int lens[] = { 1024, 2048, 4096, 8192, (int) (0.5 * s.sampleRate), 0 };
            s.maxLength = lens[juce::jmax (0, self.length.getSelectedItemIndex())];

            const auto irs = self.proc.getIRs();
            if (std::none_of (irs.begin(), irs.end(), [] (auto& p) { return p != nullptr; }))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Nothing to export", "Load at least one IR first.");
                return;
            }
            const auto blend = renderBlend (irs, s);
            juce::String err;
            const int bitDepth = bitDepths[juce::jmax (0, self.bits.getSelectedItemIndex())];
            if (writeWav (file, blend, s.sampleRate, bitDepth, err))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Export complete",
                    file.getFileName() + "\n" + juce::String (blend.getNumSamples()) + " samples, "
                    + (s.stereo ? "stereo" : "mono") + ", " + juce::String (s.sampleRate / 1000.0) + " kHz, "
                    + juce::String (bitDepth) + (bitDepth == 32 ? "-bit float" : "-bit"));
                if (self.onClose) self.onClose();
            }
            else
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export failed", err);
        });
}

void ExportPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (box.toFloat(), 8.0f);
    g.setColour (Colours::accent.withAlpha (0.6f));
    g.drawRoundedRectangle (box.toFloat(), 8.0f, 1.5f);
}

void ExportPanel::resized()
{
    box = getLocalBounds().withSizeKeepingCentre (560, 470);
    auto r = box.reduced (24);
    title.setBounds (r.removeFromTop (30));
    r.removeFromTop (10);
    auto row = [&] (juce::Label& l, juce::ComboBox& c)
    {
        auto line = r.removeFromTop (28);
        l.setBounds (line.removeFromLeft (120));
        c.setBounds (line);
        r.removeFromTop (8);
    };
    row (presetL, preset);
    row (channelsL, channels);
    row (rateL, rate);
    row (bitsL, bits);
    row (lengthL, length);
    auto toggles = r.removeFromTop (26);
    includeEq.setBounds (toggles.removeFromLeft (200));
    normalise.setBounds (toggles);
    r.removeFromTop (10);
    auto buttons = r.removeFromBottom (34);
    cancel.setBounds (buttons.removeFromLeft (120));
    exportButton.setBounds (buttons.removeFromRight (200));
    info.setBounds (r);
}

//==============================================================================
ConsoleView::ConsoleView (IRConsoleProcessor& p)
    : proc (p), response (p),
      stereo (p.apvts, "stereo", "STEREO", Colours::accent),
      autoLevel (p.apvts, "autoLevel", "AUTO LEVEL", Colours::mid),
      eqOn (p.apvts, "eqOn", "EQ IN", Colours::mid),
      input (p.apvts, "input", "INPUT", Colours::filter),
      push (p.apvts, "push", "PUSH", Colours::push, 0),
      reso (p.apvts, "pushReso", "RESO", Colours::push, 0)
{
    for (int i = 0; i < numChannels; ++i)
        addAndMakeVisible (strips.add (new ChannelStrip (p, i)));
    addAndMakeVisible (response);

    stereo.onStateChange = [this] { stereo.setButtonText (stereo.getToggleState() ? "STEREO" : "MONO"); };
    stereo.setButtonText (stereo.getToggleState() ? "STEREO" : "MONO");
    stereo.setTooltip ("Switch the whole console between mono and stereo operation");
    autoLevel.setTooltip ("Keep the blend at a consistent loudness, whatever you load");
    push.slider.setTooltip ("Speaker Push: drives the virtual speaker into power compression and cone breakup");
    reso.slider.setTooltip ("Speaker Push resonance: the cone's low-frequency resonance that blooms as you push it");
    for (auto* c : std::initializer_list<juce::Component*> { &stereo, &autoLevel, &eqOn, &input, &push, &reso })
        addAndMakeVisible (c);

    for (int b = 0; b < numEqBands; ++b)
    {
        auto* s = eqSliders.add (new juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox));
        s->setColour (juce::Slider::thumbColourId, Colours::mid);
        s->setPopupDisplayEnabled (true, true, this, 1500);
        s->setDoubleClickReturnValue (true, 0.0);
        eqAttachments.add (new SliderAttachment (p.apvts, IRConsoleProcessor::eqId (b), *s));
        s->textFromValueFunction = [b] (double v)
        { return formatHz (eqBandFreqs[(size_t) b]) + " Hz  " + (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
        s->onValueChange = [this] { response.repaint(); };
        addAndMakeVisible (s);
    }
    flattenEq.setTooltip ("Reset all master EQ bands to 0 dB");
    flattenEq.onClick = [this]
    {
        for (int b = 0; b < numEqBands; ++b)
            if (auto* prm = proc.apvts.getParameter (IRConsoleProcessor::eqId (b)))
                prm->setValueNotifyingHost (prm->getDefaultValue());
    };
    addAndMakeVisible (flattenEq);
    eqOn.onClick = [this] { response.repaint(); };

    output.setSliderStyle (juce::Slider::LinearVertical);
    output.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    output.setColour (juce::Slider::thumbColourId, Colours::accent);
    outputAttachment = std::make_unique<SliderAttachment> (p.apvts, "output", output);
    output.textFromValueFunction = [] (double v) { return v <= -59.9 ? juce::String ("-inf") : juce::String (v, 1) + " dB"; };
    output.setDoubleClickReturnValue (true, 0.0);
    output.updateText();
    addAndMakeVisible (output);

    exportButton.setColour (juce::TextButton::buttonColourId, Colours::accent.darker (0.35f));
    exportButton.setTooltip ("Render the blend to a WAV for this plugin, your DAW, or hardware (Helix, Quad Cortex...)");
    exportButton.onClick = [this]
    {
        exportPanel = std::make_unique<ExportPanel> (proc);
        exportPanel->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ConsoleView> (this)]
                                                                         { if (safe != nullptr) safe->exportPanel.reset(); }); };
        addAndMakeVisible (*exportPanel);
        exportPanel->setBounds (getLocalBounds());
    };
    addAndMakeVisible (exportButton);

    blendInfo.setFont (juce::FontOptions (11.0f));
    blendInfo.setColour (juce::Label::textColourId, Colours::textDim);
    blendInfo.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (blendInfo);

    setSize (designW, designH);
    startTimerHz (30);
}

void ConsoleView::timerCallback()
{
    if (proc.getIRVersion() != lastIRVersion)
    {
        lastIRVersion = proc.getIRVersion();
        for (auto* s : strips) s->refreshIR();
    }
    if (proc.getDisplayVersion() != lastDisplayVersion)
    {
        lastDisplayVersion = proc.getDisplayVersion();
        response.update();
        const auto d = proc.getDisplay();
        if (d.empty) blendInfo.setText ("No IRs loaded - signal passes through", juce::dontSendNotification);
        else blendInfo.setText ("Blend: " + juce::String (d.lengthSamples) + " samples (" + juce::String (d.lengthSamples * 1000.0 / d.sampleRate, 1)
                                + " ms) " + (d.stereo ? "stereo" : "mono"), juce::dontSendNotification);
    }
    auto toDb = [] (float v) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -60.0f) + 60.0f) / 66.0f); };
    const float l = toDb (proc.getOutputPeak (0)), r = toDb (proc.getOutputPeak (1));
    if (std::abs (l - meterL) > 0.003f || std::abs (r - meterR) > 0.003f)
    {
        meterL = l; meterR = r;
        repaint (meterArea);
    }
}

void ConsoleView::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panelDark);

    // Top bar.
    auto top = getLocalBounds().removeFromTop (46);
    g.setColour (Colours::panel);
    g.fillRect (top);
    g.setColour (Colours::accent);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("IR CONSOLE", top.reduced (16, 0), juce::Justification::centredLeft);
    g.setColour (Colours::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("8-CHANNEL CAB IR MIXER", top.reduced (16, 0).withTrimmedLeft (150), juce::Justification::centredLeft);

    // Master section.
    g.setColour (Colours::strip);
    g.fillRoundedRectangle (masterArea.toFloat(), 5.0f);
    g.setColour (Colours::stripEdge);
    g.drawRoundedRectangle (masterArea.toFloat(), 5.0f, 1.0f);

    drawSectionLabel (g, pushArea.withHeight (16).translated (0, -18), "SPEAKER PUSH");
    drawSectionLabel (g, eqArea.withHeight (16).translated (0, -20), "MASTER 10-BAND GRAPHIC EQ");

    // EQ scale and band labels.
    if (! eqSliders.isEmpty())
    {
        auto* first = eqSliders.getFirst();
        const float top = first->getY() + 15.0f, bot = first->getBottom() - 15.0f;   // matches slider travel
        g.setFont (juce::FontOptions (9.5f));
        for (int db : { 12, 6, 0, -6, -12 })
        {
            const float y = juce::jmap ((float) db, 12.0f, -12.0f, top, bot);
            g.setColour (Colours::textDim);
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> ((float) eqArea.getX() - 4, y - 6, 22, 12), juce::Justification::centredLeft);
        }
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        for (int b = 0; b < numEqBands; ++b)
        {
            auto* s = eqSliders[b];
            g.setColour (Colours::text);
            g.drawText (formatHz (eqBandFreqs[(size_t) b]), s->getBounds().withY (s->getBottom() + 2).withHeight (14).expanded (6, 0), juce::Justification::centred);
        }
    }

    // Output meter.
    drawSectionLabel (g, output.getBounds().withRight (meterArea.getRight()).withHeight (16).translated (0, -22), "OUTPUT");
    auto m = meterArea.toFloat();
    const float bw = (m.getWidth() - 3.0f) / 2.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = juce::Rectangle<float> (m.getX() + ch * (bw + 3.0f), m.getY(), bw, m.getHeight());
        g.setColour (juce::Colours::black);
        g.fillRect (bar);
        const float v = ch == 0 ? meterL : meterR;
        auto fill = bar.withTop (bar.getBottom() - v * bar.getHeight());
        juce::ColourGradient grad (Colours::mid, 0, bar.getBottom(), Colours::mute, 0, bar.getY(), false);
        grad.addColour (0.8, Colours::solo);
        g.setGradientFill (grad);
        g.fillRect (fill);
    }
}

void ConsoleView::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (46);
    exportButton.setBounds (top.removeFromRight (200).reduced (10, 8));
    stereo.setBounds (top.removeFromRight (110).reduced (6, 10));
    autoLevel.setBounds (top.removeFromRight (130).reduced (6, 10));
    blendInfo.setBounds (top.removeFromRight (300));

    r.reduce (8, 8);
    auto stripArea = r.removeFromLeft (numChannels * 124);
    const int sw = stripArea.getWidth() / numChannels;
    for (auto* s : strips)
        s->setBounds (stripArea.removeFromLeft (sw));

    r.removeFromLeft (8);
    masterArea = r;
    auto m = r.reduced (12);
    response.setBounds (m.removeFromTop (190));
    m.removeFromTop (30);

    pushArea = m.removeFromTop (76);
    {
        auto pa = pushArea;
        const int kw = 76;
        input.setBounds (pa.removeFromLeft (kw));
        pa.removeFromLeft (10);
        push.setBounds (pa.removeFromLeft (kw));
        reso.setBounds (pa.removeFromLeft (kw));
    }
    m.removeFromTop (34);

    auto outCol = m.removeFromRight (110);
    m.removeFromRight (10);
    eqArea = m.removeFromTop (290);
    {
        auto header = eqArea.withHeight (20).translated (0, -22);
        eqOn.setBounds (header.removeFromRight (80));
        header.removeFromRight (6);
        flattenEq.setBounds (header.removeFromRight (50));
        auto sliders = eqArea.withTrimmedLeft (20).withTrimmedBottom (18);
        const int w = sliders.getWidth() / numEqBands;
        for (auto* s : eqSliders)
            s->setBounds (sliders.removeFromLeft (w));
    }

    auto outArea = outCol.withTrimmedTop (-30);
    outArea.removeFromTop (30);
    meterArea = outArea.removeFromRight (22).withTrimmedBottom (20).reduced (0, 4);
    output.setBounds (outArea);

    if (exportPanel) exportPanel->setBounds (getLocalBounds());
}
} // namespace irc

//==============================================================================
IRConsoleEditor::IRConsoleEditor (IRConsoleProcessor& p) : AudioProcessorEditor (p), view (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) irc::ConsoleView::designW / irc::ConsoleView::designH);
    setResizeLimits (irc::ConsoleView::designW / 2, irc::ConsoleView::designH / 2,
                     irc::ConsoleView::designW * 2, irc::ConsoleView::designH * 2);
    setSize (1184, 632);   // 80% default to fit laptop screens
}

IRConsoleEditor::~IRConsoleEditor()
{
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void IRConsoleEditor::resized()
{
    const float scale = (float) getWidth() / (float) irc::ConsoleView::designW;
    view.setTransform (juce::AffineTransform::scale (scale));
    view.setBounds (0, 0, irc::ConsoleView::designW, irc::ConsoleView::designH);
}
