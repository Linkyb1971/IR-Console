// Headless tests for IR Console: blend maths, export formats, processor & state, UI snapshot.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/BlendEngine.h"
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

using namespace irc;
static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::cout << "  PASS  " << msg << "\n"; else { std::cout << "  FAIL  " << msg << "\n"; ++failures; } } while (0)

static ChannelIRPtr makeIR (double sr, int len, int seed, int chans = 1)
{
    auto ir = std::make_shared<ChannelIR>();
    ir->data.setSize (chans, len);
    juce::Random r (seed);
    for (int c = 0; c < chans; ++c)
        for (int i = 0; i < len; ++i)
            ir->data.setSample (c, i, (r.nextFloat() * 2 - 1) * std::exp (-i / (0.004f * (float) sr)));
    ir->data.setSample (0, 0, 1.0f);
    ir->sampleRate = sr;
    ir->name = "test" + juce::String (seed);
    return ir;
}

static float maxDiff (const float* a, const float* b, int n)
{
    float m = 0; for (int i = 0; i < n; ++i) m = juce::jmax (m, std::abs (a[i] - b[i])); return m;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::array<ChannelIRPtr, numChannels> irs {};
    const auto a = makeIR (48000, 2048, 1), b = makeIR (48000, 2048, 2);

    std::cout << "Blend engine\n";
    {
        irs = {}; irs[0] = a;
        BlendSettings s; s.autoLevel = false; s.stereo = false; s.maxLength = 1500;
        auto out = renderBlend (irs, s);
        CHECK (out.getNumChannels() == 1 && out.getNumSamples() == 1500, "mono, fixed length 1500");
        CHECK (maxDiff (out.getReadPointer (0), a->data.getReadPointer (0), 1300) < 1e-6f, "single unity channel reproduces the IR (before fade)");

        s.stereo = true; s.channels[0].pan = -1.0f;
        out = renderBlend (irs, s);
        CHECK (out.getMagnitude (1, 0, out.getNumSamples()) < 1e-6f, "hard-left pan: right channel silent");
        s.channels[0].pan = 0.0f;
        out = renderBlend (irs, s);
        CHECK (maxDiff (out.getReadPointer (0), out.getReadPointer (1), 1500) < 1e-6f, "centre pan: L == R");
        CHECK (std::abs (out.getSample (0, 0) - 1.0f) < 1e-4f, "centre pan is unity gain");

        irs[1] = a; s.channels[1].invert = true; s.stereo = false;
        out = renderBlend (irs, s);
        CHECK (out.getMagnitude (0, 0, 1500) < 1e-6f, "identical IRs, one polarity-flipped, cancel");
        s.channels[1].invert = false; s.channels[1].mute = true;
        out = renderBlend (irs, s);
        CHECK (maxDiff (out.getReadPointer (0), a->data.getReadPointer (0), 1300) < 1e-6f, "mute removes channel");
        s.channels[1].mute = false; s.channels[0].solo = true; irs[1] = b;
        out = renderBlend (irs, s);
        CHECK (maxDiff (out.getReadPointer (0), a->data.getReadPointer (0), 1300) < 1e-6f, "solo isolates channel");
        s.channels[0].solo = false;

        irs = {}; irs[0] = a;
        s.channels[0].delayMs = 1.0f;
        out = renderBlend (irs, s);
        CHECK (std::abs (out.getSample (0, 47)) < 1e-6f && std::abs (out.getSample (0, 48) - 1.0f) < 1e-6f, "1 ms align = 48 samples delay");
        s.channels[0].delayMs = 0.0f;

        s.channels[0].gainDb = -6.0f;
        out = renderBlend (irs, s);
        CHECK (std::abs (out.getSample (0, 0) - 0.501f) < 0.002f, "-6 dB fader");
        s.channels[0].gainDb = 0.0f;

        s.channels[0].hpfHz = 400.0f;
        out = renderBlend (irs, s);
        auto respH = computeResponseDb (out.getReadPointer (0), out.getNumSamples(), 48000, 240);
        auto respA = computeResponseDb (a->data.getReadPointer (0), 2048, 48000, 240);
        CHECK (respH[40] - respA[40] < -12.0f, "HPF 400 Hz cuts ~50 Hz by >12 dB (got " + juce::String (respH[40] - respA[40], 1) + ")");
        s.channels[0].hpfHz = 20.0f;

        s.includeMasterEq = true; s.masterEq[5] = 12.0f; s.maxLength = 8192;
        out = renderBlend (irs, s);
        auto respE = computeResponseDb (out.getReadPointer (0), out.getNumSamples(), 48000, 240);
        const int k1 = (int) std::round (std::log (1000.0 / 20.0) / std::log (1000.0) * 239);
        CHECK (std::abs ((respE[k1] - respA[k1]) - 12.0f) < 2.0f, "master EQ +12 dB @1k baked into export (got " + juce::String (respE[k1] - respA[k1], 1) + ")");
        CHECK (std::abs (masterEqResponseDb (s.masterEq, 48000, 1000.0) - 12.0f) < 0.1f, "master EQ curve +12 dB at 1 kHz");
        s.includeMasterEq = false; s.masterEq[5] = 0.0f;

        s.peakNormalise = true;
        out = renderBlend (irs, s);
        CHECK (std::abs (out.getMagnitude (0, 0, out.getNumSamples()) - juce::Decibels::decibelsToGain (-0.1f)) < 1e-4f, "peak normalise to -0.1 dBFS");
        s.peakNormalise = false; s.maxLength = 0;

        // Resampling 44.1k IR to 48k keeps response
        auto a44 = std::make_shared<ChannelIR> (*a); a44->sampleRate = 44100;
        {   // band-limit the test IR to 16 kHz so the round trip is meaningful
            auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (44100, 16000);
            for (int k = 0; k < 4; ++k) { juce::dsp::IIR::Filter<float> f (lp); auto* d = a44->data.getWritePointer (0); for (int i = 0; i < 2048; ++i) d[i] = f.processSample (d[i]); }
        }
        auto rs = resampleIR (a44->data, 44100, 48000);
        CHECK (std::abs (rs.getNumSamples() - (int) std::ceil (2048 * 48000.0 / 44100.0)) <= 1, "44.1k -> 48k length");
        auto back = resampleIR (rs, 48000, 44100);
        const float rt = maxDiff (back.getReadPointer (0) + 100, a44->data.getReadPointer (0) + 100, 1500) / a44->data.getMagnitude (0, 0, 2048);
        CHECK (rt < 0.01f, "resample round-trip error < -40 dB (got " + juce::String (juce::Decibels::gainToDecibels (rt), 1) + " dB)");

        // Stereo IR on a channel with balance
        irs = {}; irs[0] = makeIR (48000, 1024, 3, 2);
        s.stereo = true; s.channels[0].pan = 1.0f; s.maxLength = 1024;
        out = renderBlend (irs, s);
        CHECK (out.getMagnitude (0, 0, 1024) < 1e-6f && out.getMagnitude (1, 0, 1024) > 0.5f, "stereo IR balance hard right");
        s.channels[0].pan = 0;

        // Auto level holds energy constant
        irs = {}; irs[0] = a; irs[1] = b; s.autoLevel = true; s.maxLength = 0; s.stereo = false;
        auto e1 = renderBlend (irs, s);
        s.channels[1].gainDb = 12.0f;
        auto e2 = renderBlend (irs, s);
        auto energy = [] (const juce::AudioBuffer<float>& x) { double e = 0; for (int i = 0; i < x.getNumSamples(); ++i) e += x.getSample (0, i) * x.getSample (0, i); return e; };
        CHECK (std::abs (energy (e1) - energy (e2)) < 0.01 * energy (e1), "auto level keeps energy constant");
    }

    std::cout << "File I/O & hardware export\n";
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("irconsole_test");
    dir.deleteRecursively(); dir.createDirectory();
    {
        juce::String err;
        auto f = dir.getChildFile ("cabA.wav");
        CHECK (writeWav (f, a->data, 44100, 24, err), "write 24-bit 44.1k source IR");
        auto loaded = loadIRFile (f, err);
        CHECK (loaded != nullptr && loaded->sampleRate == 44100 && loaded->data.getNumSamples() == 2048, "load it back");

        irs = {}; irs[0] = loaded; irs[1] = b;
        BlendSettings s; s.stereo = false; s.sampleRate = 48000; s.maxLength = 2048; s.includeMasterEq = true; s.peakNormalise = true;
        auto blend = renderBlend (irs, s);
        auto helix = dir.getChildFile ("helix.wav");
        CHECK (writeWav (helix, blend, 48000, 16, err), "write Helix export");
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (helix));
        CHECK (r != nullptr && r->sampleRate == 48000 && r->numChannels == 1 && r->bitsPerSample == 16 && r->lengthInSamples == 2048,
               "Helix file: 48 kHz / 16-bit / mono / 2048 samples");

        s.maxLength = 1024; blend = renderBlend (irs, s);
        auto qc = dir.getChildFile ("qc.wav");
        writeWav (qc, blend, 48000, 24, err);
        r.reset (fm.createReaderFor (qc));
        CHECK (r != nullptr && r->sampleRate == 48000 && r->numChannels == 1 && r->bitsPerSample == 24 && r->lengthInSamples == 1024,
               "QC file: 48 kHz / 24-bit / mono / 1024 samples");

        s.stereo = true; s.maxLength = 0; s.channels[1].pan = 0.5f; blend = renderBlend (irs, s);
        auto st = dir.getChildFile ("stereo.wav");
        writeWav (st, blend, 48000, 32, err);
        r.reset (fm.createReaderFor (st));
        CHECK (r != nullptr && r->numChannels == 2 && r->bitsPerSample == 32 && r->usesFloatingPointData, "stereo 32-bit float export");
        r.reset();
        auto reload = loadIRFile (st, err);
        CHECK (reload != nullptr && reload->data.getNumChannels() == 2, "exported stereo blend loads back as a channel IR");
    }

    std::cout << "Processor\n";
    {
        IRConsoleProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000, 512);
        CHECK (proc.loadIR (0, dir.getChildFile ("cabA.wav")).isEmpty(), "processor loads IR file");
        proc.apvts.getParameter ("autoLevel")->setValueNotifyingHost (0.0f);
        proc.prepareToPlay (48000, 512);
        juce::Thread::sleep (300);

        // Expected response = blend at 48k
        auto s = proc.getCurrentSettings();
        auto expected = renderBlend (proc.getIRs(), s);

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        std::vector<float> outL;
        // Run silence a while so the convolution engine swaps in the new IR.
        for (int i = 0; i < 40; ++i) { buf.clear(); proc.processBlock (buf, midi); }
        const int lat = proc.getLatencySamples();
        for (int blk = 0; blk < 10; ++blk)
        {
            buf.clear();
            if (blk == 0) { buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f); }
            proc.processBlock (buf, midi);
            for (int i = 0; i < 512; ++i) outL.push_back (buf.getSample (0, i));
        }
        float md = 0;
        for (int i = 0; i < 1500; ++i) md = juce::jmax (md, std::abs (outL[(size_t) (i + lat)] - expected.getSample (0, i)));
        std::cout << "        latency " << lat << " samples, max error vs blend " << md << "\n";
        CHECK (md < 0.05f, "real-time output matches the exported blend (through oversampling filters)");

        // Speaker push changes the sound and stays finite
        proc.apvts.getParameter ("push")->setValueNotifyingHost (1.0f);
        bool finite = true; float peak = 0;
        juce::Random rnd (5);
        for (int blk = 0; blk < 100; ++blk)
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) buf.setSample (c, i, std::sin ((blk * 512 + i) * 0.02f) * 0.9f);
            proc.processBlock (buf, midi);
            for (int i = 0; i < 512; ++i) { finite &= std::isfinite (buf.getSample (0, i)); peak = juce::jmax (peak, std::abs (buf.getSample (0, i))); }
        }
        CHECK (finite && peak > 0.01f && peak < 20.0f, "Speaker Push 100% output finite and sane (peak " + juce::String (peak, 2) + ")");

        // State round-trip
        proc.apvts.getParameter (IRConsoleProcessor::chId (0, "pan"))->setValueNotifyingHost (0.2f);
        juce::MemoryBlock state;
        proc.getStateInformation (state);
        IRConsoleProcessor proc2;
        proc2.setStateInformation (state.getData(), (int) state.getSize());
        auto ir2 = proc2.getIR (0);
        CHECK (ir2 != nullptr && ir2->data.getNumSamples() == 2048 && ir2->name == "cabA", "state restores embedded IR");
        CHECK (std::abs (proc2.apvts.getParameter (IRConsoleProcessor::chId (0, "pan"))->getValue() - 0.2f) < 1e-4f, "state restores parameters");

        // Mono mode output
        proc.apvts.getParameter ("push")->setValueNotifyingHost (0.0f);
        proc.apvts.getParameter ("stereo")->setValueNotifyingHost (0.0f);
        juce::Thread::sleep (200);
        for (int i = 0; i < 40; ++i) { buf.clear(); proc.processBlock (buf, midi); }
        float diff = 0;
        for (int blk = 0; blk < 4; ++blk)
        {
            buf.clear(); buf.setSample (0, 3, 1.0f);   // left-only input
            proc.processBlock (buf, midi);
            diff = juce::jmax (diff, maxDiff (buf.getReadPointer (0), buf.getReadPointer (1), 512));
        }
        CHECK (diff < 1e-6f, "mono mode: identical L/R output");

        // UI snapshot with some IRs loaded
        proc.apvts.getParameter ("stereo")->setValueNotifyingHost (1.0f);
        proc.setIR (1, b);
        proc.setIR (2, makeIR (48000, 4096, 9, 2));
        auto setP = [&] (const juce::String& id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        setP (IRConsoleProcessor::chId (0, "pan"), -0.6f); setP (IRConsoleProcessor::chId (1, "pan"), 0.6f);
        setP (IRConsoleProcessor::chId (1, "gain"), -4.0f); setP (IRConsoleProcessor::chId (0, "highGain"), 3.0f);
        setP (IRConsoleProcessor::chId (1, "hpf"), 90.0f); setP (IRConsoleProcessor::chId (2, "mute"), 1.0f);
        setP (IRConsoleProcessor::eqId (1), 3.0f); setP (IRConsoleProcessor::eqId (4), -4.0f); setP (IRConsoleProcessor::eqId (7), 2.5f);
        setP ("push", 35.0f);
        juce::Thread::sleep (300);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (irc::ConsoleView::designW, irc::ConsoleView::designH);
        for (int i = 0; i < 20; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File png ("/tmp/claude-0/irconsole_ui.png");
        png.deleteFile();
        juce::FileOutputStream os (png);
        juce::PNGImageFormat().writeImageToStream (img, os);
        CHECK (img.isValid(), "editor renders (" + png.getFullPathName() + ")");
        ed.reset();
    }

    std::cout << (failures == 0 ? "\nALL TESTS PASSED\n" : "\nFAILURES: " + std::to_string (failures) + "\n");
    return failures == 0 ? 0 : 1;
}
