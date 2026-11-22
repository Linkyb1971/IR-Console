#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace irc;
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using NR  = juce::NormalisableRange<float>;

namespace
{
    NR freqRange (float lo, float hi, float centre)
    {
        NR r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }
    juce::AudioParameterFloatAttributes hz()  { return juce::AudioParameterFloatAttributes().withLabel ("Hz"); }
    juce::AudioParameterFloatAttributes db()  { return juce::AudioParameterFloatAttributes().withLabel ("dB"); }
}

juce::AudioProcessorValueTreeState::ParameterLayout IRConsoleProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto pre = "Ch " + juce::String (ch + 1) + " ";
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("ch" + juce::String (ch + 1), pre.trim(), "|");
        group->addChild (std::make_unique<APF> (juce::ParameterID { chId (ch, "gain"), 1 }, pre + "Volume", NR (-60.0f, 12.0f, 0.01f, 2.0f), 0.0f, db()));
        group->addChild (std::make_unique<APF> (juce::ParameterID { chId (ch, "pan"), 1 }, pre + "Pan", NR (-1.0f, 1.0f), 0.0f));
        group->addChild (std::make_unique<APB> (juce::ParameterID { chId (ch, "mute"), 1 }, pre + "Mute", false));
        group->addChild (std::make_unique<APB> (juce::ParameterID { chId (ch, "solo"), 1 }, pre + "Solo", false));
        group->addChild (std::make_unique<APB> (juce::ParameterID { chId (ch, "invert"), 1 }, pre + "Polarity", false));
        group->addChild (std::make_unique<APF> (juce::ParameterID { chId (ch, "delay"), 1 }, pre + "Align", NR (0.0f, 5.0f, 0.001f), 0.0f,
                                                juce::AudioParameterFloatAttributes().withLabel ("ms")));
        group->addChild (std::make_unique<APF> (juce::ParameterID { chId (ch, "hpf"), 1 }, pre + "HPF", freqRange (20.0f, 1000.0f, 150.0f), 20.0f, hz()));
        group->addChild (std::make_unique<APF> (juce::ParameterID { chId (ch, "lpf"), 1 }, pre + "LPF", freqRange (1000.0f, 20000.0f, 6000.0f), 20000.0f, hz()));
        group->addChild (std::make_unique<APF> (juce::ParameterID { chId (ch, "lowGain"), 1 }, pre + "Low Gain", NR (-15.0f, 15.0f, 0.1f), 0.0f, db()));
        group->addChild (std::make_unique<APF> (juce::ParameterID { chId (ch, "lowFreq"), 1 }, pre + "Low Freq", freqRange (40.0f, 600.0f, 150.0f), 120.0f, hz()));
        group->addChild (std::make_unique<APF> (juce::ParameterID { chId (ch, "midGain"), 1 }, pre + "Mid Gain", NR (-15.0f, 15.0f, 0.1f), 0.0f, db()));
        group->addChild (std::make_unique<APF> (juce::ParameterID { chId (ch, "midFreq"), 1 }, pre + "Mid Freq", freqRange (200.0f, 8000.0f, 1200.0f), 1200.0f, hz()));
        group->addChild (std::make_unique<APF> (juce::ParameterID { chId (ch, "midQ"), 1 }, pre + "Mid Q", freqRange (0.3f, 6.0f, 1.2f), 1.0f));
        group->addChild (std::make_unique<APF> (juce::ParameterID { chId (ch, "highGain"), 1 }, pre + "High Gain", NR (-15.0f, 15.0f, 0.1f), 0.0f, db()));
        group->addChild (std::make_unique<APF> (juce::ParameterID { chId (ch, "highFreq"), 1 }, pre + "High Freq", freqRange (1500.0f, 16000.0f, 5000.0f), 6000.0f, hz()));
        layout.add (std::move (group));
    }

    auto master = std::make_unique<juce::AudioProcessorParameterGroup> ("master", "Master", "|");
    master->addChild (std::make_unique<APB> (juce::ParameterID { "stereo", 1 }, "Stereo", true));
    master->addChild (std::make_unique<APB> (juce::ParameterID { "autoLevel", 1 }, "Auto Level", true));
    master->addChild (std::make_unique<APF> (juce::ParameterID { "input", 1 }, "Input", NR (-24.0f, 24.0f, 0.1f), 0.0f, db()));
    master->addChild (std::make_unique<APF> (juce::ParameterID { "push", 1 }, "Speaker Push", NR (0.0f, 100.0f, 0.1f), 0.0f,
                                             juce::AudioParameterFloatAttributes().withLabel ("%")));
    master->addChild (std::make_unique<APF> (juce::ParameterID { "pushReso", 1 }, "Push Reso", freqRange (40.0f, 250.0f, 100.0f), 100.0f, hz()));
    master->addChild (std::make_unique<APB> (juce::ParameterID { "eqOn", 1 }, "Master EQ In", true));
    for (int b = 0; b < numEqBands; ++b)
    {
        const float f = eqBandFreqs[(size_t) b];
        const auto name = "EQ " + (f >= 1000.0f ? juce::String (f / 1000.0f) + "k" : juce::String ((int) f));
        master->addChild (std::make_unique<APF> (juce::ParameterID { eqId (b), 1 }, name, NR (-12.0f, 12.0f, 0.1f), 0.0f, db()));
    }
    master->addChild (std::make_unique<APF> (juce::ParameterID { "output", 1 }, "Output", NR (-60.0f, 12.0f, 0.01f, 2.0f), 0.0f, db()));
    layout.add (std::move (master));
    return layout;
}

IRConsoleProcessor::IRConsoleProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pStereo = apvts.getRawParameterValue ("stereo");
    pAutoLevel = apvts.getRawParameterValue ("autoLevel");
    pPush = apvts.getRawParameterValue ("push");
    pReso = apvts.getRawParameterValue ("pushReso");
    pEqOn = apvts.getRawParameterValue ("eqOn");
    pIn = apvts.getRawParameterValue ("input");
    pOut = apvts.getRawParameterValue ("output");
    for (int b = 0; b < numEqBands; ++b) pEq[(size_t) b] = apvts.getRawParameterValue (eqId (b));
    lastEqGains.fill (1000.0f);

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            if (id.startsWith ("ch") || id == "stereo" || id == "autoLevel")
                apvts.addParameterListener (id, this);
        }
    blendThread.startThread();
}

IRConsoleProcessor::~IRConsoleProcessor()
{
    blendThread.stopThread (4000);
}

bool IRConsoleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    const bool inOk  = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return outOk && inOk && in.size() <= out.size();
}

void IRConsoleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    prepared = false;
    currentRate = sampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    convolution.prepare (spec);
    push.prepare (spec);
    setLatencySamples (push.getLatencySamples());
    for (auto& band : masterEq) for (auto& f : band) f.reset();
    lastEqGains.fill (1000.0f);
    updateMasterEq();
    work.setSize (2, samplesPerBlock);
    inGain.reset (sampleRate, 0.03);  inGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pIn->load()));
    outGain.reset (sampleRate, 0.03); outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load(), -60.0f));
    prepared = true;
    dirty = true;
    rebuildBlend();   // have an IR ready before the first block
}

void IRConsoleProcessor::updateMasterEq()
{
    const double sr = currentRate.load();
    for (int b = 0; b < numEqBands; ++b)
    {
        const float g = pEq[(size_t) b]->load();
        if (std::abs (g - lastEqGains[(size_t) b]) < 0.001f) continue;
        lastEqGains[(size_t) b] = g;
        auto c = makeMasterEqBand (sr, b, g);
        for (auto& f : masterEq[b]) *f.coefficients = *c;
    }
}

void IRConsoleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    if (n == 0) return;
    if (work.getNumSamples() < n) work.setSize (2, n, false, false, true);

    // Gather input into a 2-channel work buffer.
    work.copyFrom (0, 0, buffer, 0, 0, n);
    work.copyFrom (1, 0, buffer, numIn > 1 ? 1 : 0, 0, n);

    const bool stereo = pStereo->load() > 0.5f && numOut > 1;
    if (! stereo)
    {
        work.addFrom (0, 0, work, 1, 0, n);
        work.applyGain (0, 0, n, 0.5f);
        work.copyFrom (1, 0, work, 0, 0, n);
    }

    inGain.setTargetValue (juce::Decibels::decibelsToGain (pIn->load()));
    inGain.applyGain (work, n);
    inPeak = juce::jmax (inPeak.load() * 0.9f, work.getMagnitude (0, 0, n));

    // Speaker Push (before the cab, as on a real speaker).
    push.setParameters (pPush->load() * 0.01f, pReso->load());
    push.process (work, 2);

    // Blended cab IR.
    {
        juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) n);
        convolution.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    // Master graphic EQ.
    if (pEqOn->load() > 0.5f)
    {
        updateMasterEq();
        for (int b = 0; b < numEqBands; ++b)
        {
            if (std::abs (lastEqGains[(size_t) b]) < 0.01f) continue;
            for (int ch = 0; ch < 2; ++ch)
            {
                float* d = work.getWritePointer (ch);
                auto& f = masterEq[b][ch];
                for (int i = 0; i < n; ++i) d[i] = f.processSample (d[i]);
            }
        }
    }

    const float outDb = pOut->load();
    outGain.setTargetValue (outDb <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (outDb));
    outGain.applyGain (work, n);

    if (numOut > 1)
    {
        buffer.copyFrom (0, 0, work, 0, 0, n);
        buffer.copyFrom (1, 0, work, 1, 0, n);
    }
    else
    {
        buffer.copyFrom (0, 0, work, 0, 0, n);
        buffer.addFrom (0, 0, work, 1, 0, n);
        buffer.applyGain (0, 0, n, 0.5f);
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        outPeak[ch] = juce::jmax (outPeak[ch].load() * 0.9f, work.getMagnitude (ch, 0, n));
        outRms[ch] = work.getRMSLevel (ch, 0, n);
    }
}

//==============================================================================
void IRConsoleProcessor::parameterChanged (const juce::String&, float) { dirty = true; }

void IRConsoleProcessor::BlendThread::run()
{
    while (! threadShouldExit())
    {
        wait (25);
        if (owner.prepared && owner.dirty.exchange (false))
            owner.rebuildBlend();
    }
}

BlendSettings IRConsoleProcessor::getCurrentSettings() const
{
    BlendSettings s;
    auto v = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& c = s.channels[(size_t) ch];
        c.gainDb = v (chId (ch, "gain"));
        if (c.gainDb <= -59.9f) c.gainDb = -200.0f;
        c.pan = v (chId (ch, "pan"));
        c.mute = v (chId (ch, "mute")) > 0.5f;
        c.solo = v (chId (ch, "solo")) > 0.5f;
        c.invert = v (chId (ch, "invert")) > 0.5f;
        c.delayMs = v (chId (ch, "delay"));
        c.hpfHz = v (chId (ch, "hpf"));
        c.lpfHz = v (chId (ch, "lpf"));
        c.lowGain = v (chId (ch, "lowGain"));   c.lowFreq = v (chId (ch, "lowFreq"));
        c.midGain = v (chId (ch, "midGain"));   c.midFreq = v (chId (ch, "midFreq")); c.midQ = v (chId (ch, "midQ"));
        c.highGain = v (chId (ch, "highGain")); c.highFreq = v (chId (ch, "highFreq"));
    }
    s.stereo = pStereo->load() > 0.5f;
    s.autoLevel = pAutoLevel->load() > 0.5f;
    s.masterEqOn = pEqOn->load() > 0.5f;
    for (int b = 0; b < numEqBands; ++b) s.masterEq[(size_t) b] = pEq[(size_t) b]->load();
    s.sampleRate = currentRate.load();
    return s;
}

void IRConsoleProcessor::rebuildBlend()
{
    const juce::ScopedLock buildGuard (buildLock);
    const auto settings = getCurrentSettings();
    const auto all = getIRs();
    const bool anyLoaded = std::any_of (all.begin(), all.end(), [] (auto& p) { return p != nullptr; });

    juce::AudioBuffer<float> blend;
    if (anyLoaded)
        blend = renderBlend (all, settings);
    else
    {
        blend.setSize (settings.stereo ? 2 : 1, 1);   // nothing loaded: pass the signal through
        for (int i = 0; i < blend.getNumChannels(); ++i) blend.setSample (i, 0, 1.0f);
    }

    Display d;
    d.empty = ! anyLoaded;
    d.stereo = settings.stereo;
    d.sampleRate = settings.sampleRate;
    d.lengthSamples = blend.getNumSamples();
    d.responseL = computeResponseDb (blend.getReadPointer (0), blend.getNumSamples(), settings.sampleRate, displayPoints);
    if (settings.stereo)
        d.responseR = computeResponseDb (blend.getReadPointer (1), blend.getNumSamples(), settings.sampleRate, displayPoints);

    convolution.loadImpulseResponse (std::move (blend), settings.sampleRate,
                                     settings.stereo ? juce::dsp::Convolution::Stereo::yes : juce::dsp::Convolution::Stereo::no,
                                     juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    {
        const juce::ScopedLock sl (displayLock);
        display = std::move (d);
    }
    ++displayVersion;
}

IRConsoleProcessor::Display IRConsoleProcessor::getDisplay() const
{
    const juce::ScopedLock sl (displayLock);
    return display;
}

juce::String IRConsoleProcessor::loadIR (int channel, const juce::File& file)
{
    juce::String error;
    auto ir = loadIRFile (file, error);
    if (ir == nullptr) return error;
    setIR (channel, ir);
    return {};
}

void IRConsoleProcessor::setIR (int channel, ChannelIRPtr ir)
{
    if (! juce::isPositiveAndBelow (channel, numChannels)) return;
    {
        const juce::ScopedLock sl (irLock);
        irs[(size_t) channel] = std::move (ir);
    }
    ++irVersion;
    dirty = true;
}

ChannelIRPtr IRConsoleProcessor::getIR (int channel) const
{
    const juce::ScopedLock sl (irLock);
    return irs[(size_t) channel];
}

std::array<ChannelIRPtr, numChannels> IRConsoleProcessor::getIRs() const
{
    const juce::ScopedLock sl (irLock);
    return irs;
}

//==============================================================================
void IRConsoleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("IRConsoleState");
    root.setProperty ("version", 1, nullptr);
    root.appendChild (apvts.copyState(), nullptr);

    juce::ValueTree irTree ("IRs");
    const auto all = getIRs();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto& ir = all[(size_t) ch];
        if (ir == nullptr) continue;
        juce::ValueTree t ("IR");
        t.setProperty ("channel", ch, nullptr);
        t.setProperty ("name", ir->name, nullptr);
        t.setProperty ("path", ir->path, nullptr);
        t.setProperty ("sampleRate", ir->sampleRate, nullptr);
        t.setProperty ("numChannels", ir->data.getNumChannels(), nullptr);
        t.setProperty ("numSamples", ir->data.getNumSamples(), nullptr);
        juce::MemoryBlock mb;
        for (int i = 0; i < ir->data.getNumChannels(); ++i)
            mb.append (ir->data.getReadPointer (i), sizeof (float) * (size_t) ir->data.getNumSamples());
        t.setProperty ("data", mb, nullptr);
        irTree.appendChild (t, nullptr);
    }
    root.appendChild (irTree, nullptr);

    juce::MemoryOutputStream mos (destData, false);
    root.writeToStream (mos);
}

void IRConsoleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto root = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (! root.hasType ("IRConsoleState")) return;

    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid()) apvts.replaceState (params);

    std::array<ChannelIRPtr, numChannels> loaded {};
    for (auto t : root.getChildWithName ("IRs"))
    {
        const int ch = t["channel"];
        const int chans = t["numChannels"], len = t["numSamples"];
        auto* mb = t["data"].getBinaryData();
        if (! juce::isPositiveAndBelow (ch, numChannels) || mb == nullptr || chans < 1 || chans > 2 || len < 1
            || mb->getSize() != sizeof (float) * (size_t) chans * (size_t) len)
            continue;
        auto ir = std::make_shared<ChannelIR>();
        ir->data.setSize (chans, len);
        for (int i = 0; i < chans; ++i)
            std::memcpy (ir->data.getWritePointer (i), static_cast<const float*> (mb->getData()) + (size_t) i * (size_t) len, sizeof (float) * (size_t) len);
        ir->sampleRate = t["sampleRate"];
        ir->name = t["name"].toString();
        ir->path = t["path"].toString();
        loaded[(size_t) ch] = ir;
    }
    {
        const juce::ScopedLock sl (irLock);
        irs = loaded;
    }
    ++irVersion;
    dirty = true;
}

juce::AudioProcessorEditor* IRConsoleProcessor::createEditor() { return new IRConsoleEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IRConsoleProcessor(); }
