#include "BlendEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace irc
{
namespace
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    double clampFreq (double f, double sr) { return juce::jlimit (10.0, sr * 0.45, f); }

    void runFilter (Coeffs::Ptr c, float* data, int n)
    {
        juce::dsp::IIR::Filter<float> f (c);
        f.reset();
        for (int i = 0; i < n; ++i)
            data[i] = f.processSample (data[i]);
    }

    double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 50; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < 1e-12 * sum) break;
        }
        return sum;
    }

    bool isActive (const BlendSettings& s, int ch, bool anySolo)
    {
        const auto& c = s.channels[(size_t) ch];
        if (c.mute) return false;
        if (anySolo && ! c.solo) return false;
        return true;
    }
}

juce::AudioBuffer<float> resampleIR (const juce::AudioBuffer<float>& in, double inRate, double outRate)
{
    if (std::abs (inRate - outRate) < 0.5 || in.getNumSamples() == 0)
        return in;

    const double ratio  = outRate / inRate;
    const double cutoff = juce::jmin (1.0, ratio) * 0.97;        // anti-alias when downsampling
    const int    halfTaps = (int) std::ceil (32.0 / cutoff);
    const double beta = 8.6;
    const double i0b  = besselI0 (beta);
    const int inLen  = in.getNumSamples();
    const int outLen = (int) std::ceil (inLen * ratio);

    juce::AudioBuffer<float> out (in.getNumChannels(), outLen);
    for (int ch = 0; ch < in.getNumChannels(); ++ch)
    {
        const float* src = in.getReadPointer (ch);
        float* dst = out.getWritePointer (ch);
        for (int n = 0; n < outLen; ++n)
        {
            const double t = n / ratio;
            const int centre = (int) std::floor (t);
            double acc = 0.0;
            for (int k = centre - halfTaps + 1; k <= centre + halfTaps; ++k)
            {
                if (k < 0 || k >= inLen) continue;
                const double x = t - k;
                const double wPos = x / (halfTaps + 1);
                if (std::abs (wPos) >= 1.0) continue;
                const double arg = juce::MathConstants<double>::pi * cutoff * x;
                const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (arg) / arg;
                const double w = besselI0 (beta * std::sqrt (1.0 - wPos * wPos)) / i0b;
                acc += src[k] * cutoff * sinc * w;
            }
            dst[n] = (float) acc;
        }
    }
    return out;
}

Coeffs::Ptr makeMasterEqBand (double sampleRate, int band, float gainDb)
{
    // Proportional Q: broad and gentle at small settings, tighter as you push the band harder.
    const float q = 0.8f + 0.12f * std::abs (gainDb);
    return Coeffs::makePeakFilter (sampleRate, clampFreq (eqBandFreqs[(size_t) band], sampleRate), q,
                                   juce::Decibels::decibelsToGain (gainDb));
}

float masterEqResponseDb (const std::array<float, numEqBands>& gains, double sampleRate, double freq)
{
    double mag = 1.0;
    for (int b = 0; b < numEqBands; ++b)
        if (std::abs (gains[(size_t) b]) > 0.01f)
            mag *= makeMasterEqBand (sampleRate, b, gains[(size_t) b])->getMagnitudeForFrequency (freq, sampleRate);
    return (float) juce::Decibels::gainToDecibels (mag, -120.0);
}

juce::AudioBuffer<float> renderBlend (const std::array<ChannelIRPtr, numChannels>& irs, const BlendSettings& s)
{
    const double sr = s.sampleRate;
    const int outChans = s.stereo ? 2 : 1;

    bool anySolo = false;
    for (int ch = 0; ch < numChannels; ++ch)
        if (irs[(size_t) ch] != nullptr && s.channels[(size_t) ch].solo) anySolo = true;

    // 1. Resample each active channel and work out the blend length.
    std::array<juce::AudioBuffer<float>, numChannels> src;
    std::array<int, numChannels> delaySamples {};
    int needed = 0;
    const int filterTail = (int) std::ceil (0.05 * sr);   // room for EQ ringing

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto& ir = irs[(size_t) ch];
        if (ir == nullptr || ! isActive (s, ch, anySolo)) continue;
        src[(size_t) ch] = resampleIR (ir->data, ir->sampleRate, sr);
        delaySamples[(size_t) ch] = (int) std::round (s.channels[(size_t) ch].delayMs * 0.001 * sr);
        needed = juce::jmax (needed, src[(size_t) ch].getNumSamples() + delaySamples[(size_t) ch] + filterTail);
    }

    const int liveCap = (int) (maxLiveSeconds * sr);
    needed = juce::jmin (needed, liveCap);
    const int workLen = juce::jmax (needed, s.maxLength, 1);

    juce::AudioBuffer<float> mix (outChans, workLen);
    mix.clear();
    juce::AudioBuffer<float> tmp (2, workLen);

    // 2. Process each channel: delay, polarity, EQ, gain, pan -> sum.
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& in = src[(size_t) ch];
        if (in.getNumSamples() == 0) continue;
        const auto& c = s.channels[(size_t) ch];
        const int srcChans = in.getNumChannels() > 1 ? 2 : 1;

        tmp.clear();
        const int d = delaySamples[(size_t) ch];
        const int copyLen = juce::jmin (in.getNumSamples(), workLen - d);
        for (int i = 0; i < srcChans && copyLen > 0; ++i)
            tmp.copyFrom (i, d, in, i, 0, copyLen);

        const float polarity = c.invert ? -1.0f : 1.0f;
        for (int i = 0; i < srcChans; ++i)
        {
            float* data = tmp.getWritePointer (i);
            if (c.hpfHz > 20.5f)     runFilter (Coeffs::makeHighPass (sr, clampFreq (c.hpfHz, sr), 0.707f), data, workLen);
            if (c.lpfHz < 19900.0f && c.lpfHz < sr * 0.45)
                                     runFilter (Coeffs::makeLowPass (sr, clampFreq (c.lpfHz, sr), 0.707f), data, workLen);
            if (std::abs (c.lowGain) > 0.01f)
                runFilter (Coeffs::makeLowShelf (sr, clampFreq (c.lowFreq, sr), 0.707f, juce::Decibels::decibelsToGain (c.lowGain)), data, workLen);
            if (std::abs (c.midGain) > 0.01f)
                runFilter (Coeffs::makePeakFilter (sr, clampFreq (c.midFreq, sr), c.midQ, juce::Decibels::decibelsToGain (c.midGain)), data, workLen);
            if (std::abs (c.highGain) > 0.01f)
                runFilter (Coeffs::makeHighShelf (sr, clampFreq (c.highFreq, sr), 0.707f, juce::Decibels::decibelsToGain (c.highGain)), data, workLen);
        }

        const float gain = polarity * juce::Decibels::decibelsToGain (c.gainDb, -96.0f);
        if (! s.stereo)
        {
            const float g = srcChans == 2 ? gain * 0.5f : gain;
            for (int i = 0; i < srcChans; ++i)
                mix.addFrom (0, 0, tmp, i, 0, workLen, g);
        }
        else if (srcChans == 1)
        {
            // Constant-power pan, unity at centre.
            const float theta = (c.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            const float gl = std::sqrt (2.0f) * std::cos (theta);
            const float gr = std::sqrt (2.0f) * std::sin (theta);
            mix.addFrom (0, 0, tmp, 0, 0, workLen, gain * juce::jmin (gl, 1.4142f));
            mix.addFrom (1, 0, tmp, 0, 0, workLen, gain * juce::jmin (gr, 1.4142f));
        }
        else
        {
            // Stereo IR: pan acts as a balance control.
            const float gl = juce::jmin (1.0f, 1.0f - c.pan);
            const float gr = juce::jmin (1.0f, 1.0f + c.pan);
            mix.addFrom (0, 0, tmp, 0, 0, workLen, gain * gl);
            mix.addFrom (1, 0, tmp, 1, 0, workLen, gain * gr);
        }
    }

    // 3. Auto level: normalise energy so the blend sits at a consistent loudness.
    if (s.autoLevel)
    {
        double energy = 0.0;
        for (int i = 0; i < outChans; ++i)
        {
            const float* p = mix.getReadPointer (i);
            for (int n = 0; n < workLen; ++n) energy += (double) p[n] * p[n];
        }
        energy /= outChans;
        if (energy > 1e-12)
            mix.applyGain ((float) (0.5 / std::sqrt (energy)));   // ~ -6 dB reference
    }

    // 4. Optional master EQ (export).
    if (s.includeMasterEq && s.masterEqOn)
        for (int b = 0; b < numEqBands; ++b)
            if (std::abs (s.masterEq[(size_t) b]) > 0.01f)
                for (int i = 0; i < outChans; ++i)
                    runFilter (makeMasterEqBand (sr, b, s.masterEq[(size_t) b]), mix.getWritePointer (i), workLen);

    // 5. Length: fixed (export) or trimmed to where the tail decays below -100 dB.
    int finalLen = workLen;
    if (s.maxLength > 0)
        finalLen = s.maxLength;
    else
    {
        const float peak = mix.getMagnitude (0, workLen);
        const float thresh = peak * 1.0e-5f;
        finalLen = 1;
        for (int i = 0; i < outChans; ++i)
        {
            const float* p = mix.getReadPointer (i);
            for (int n = workLen - 1; n >= 0; --n)
                if (std::abs (p[n]) > thresh) { finalLen = juce::jmax (finalLen, n + 1); break; }
        }
    }

    juce::AudioBuffer<float> out (outChans, finalLen);
    for (int i = 0; i < outChans; ++i)
        out.copyFrom (i, 0, mix, i, 0, finalLen);

    // Gentle fade on the last 10% to avoid a click at the truncation point.
    const int fade = juce::jlimit (1, 2048, finalLen / 10);
    if (finalLen > 64)
        for (int i = 0; i < outChans; ++i)
        {
            float* p = out.getWritePointer (i);
            for (int n = 0; n < fade; ++n)
            {
                const float w = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * (float) (n + 1) / (float) fade));
                p[finalLen - fade + n] *= w;
            }
        }

    if (s.peakNormalise)
    {
        const float peak = out.getMagnitude (0, finalLen);
        if (peak > 1e-9f)
            out.applyGain (juce::Decibels::decibelsToGain (-0.1f) / peak);
    }
    return out;
}

std::vector<float> computeResponseDb (const float* ir, int length, double sampleRate, int numPoints)
{
    std::vector<float> result ((size_t) numPoints, -120.0f);
    if (ir == nullptr || length <= 0) return result;

    int order = 12;
    while ((1 << order) < length && order < 16) ++order;
    const int size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf ((size_t) size * 2, 0.0f);
    std::copy (ir, ir + juce::jmin (length, size), buf.begin());
    fft.performFrequencyOnlyForwardTransform (buf.data());

    const double binHz = sampleRate / size;
    for (int p = 0; p < numPoints; ++p)
    {
        const double f = 20.0 * std::pow (1000.0, (double) p / (numPoints - 1));
        // 1/6-octave smoothing (power average)
        const int lo = juce::jlimit (1, size / 2, (int) std::floor (f * std::pow (2.0, -1.0 / 12.0) / binHz));
        const int hi = juce::jlimit (lo, size / 2, (int) std::ceil (f * std::pow (2.0, 1.0 / 12.0) / binHz));
        double acc = 0.0;
        for (int b = lo; b <= hi; ++b) acc += (double) buf[(size_t) b] * buf[(size_t) b];
        acc /= (hi - lo + 1);
        result[(size_t) p] = (float) (10.0 * std::log10 (acc + 1e-20));
    }
    return result;
}

ChannelIRPtr loadIRFile (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { error = "Could not read " + file.getFileName(); return nullptr; }

    const auto maxSamples = (juce::int64) (10.0 * reader->sampleRate);
    if (reader->lengthInSamples <= 0) { error = file.getFileName() + " is empty"; return nullptr; }
    if (reader->lengthInSamples > maxSamples) { error = file.getFileName() + " is longer than 10 seconds - not an IR?"; return nullptr; }

    auto ir = std::make_shared<ChannelIR>();
    const int chans = juce::jmin (2, (int) reader->numChannels);
    ir->data.setSize (chans, (int) reader->lengthInSamples);
    reader->read (&ir->data, 0, (int) reader->lengthInSamples, 0, true, chans > 1);
    ir->sampleRate = reader->sampleRate;
    ir->name = file.getFileNameWithoutExtension();
    ir->path = file.getFullPathName();
    return ir;
}

bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& ir, double sampleRate, int bitDepth, juce::String& error)
{
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk()) { error = "Cannot write to " + file.getFullPathName(); return false; }

    juce::WavAudioFormat wav;
    auto options = juce::AudioFormatWriterOptions().withSampleRate (sampleRate)
                                                   .withNumChannels (ir.getNumChannels())
                                                   .withBitsPerSample (bitDepth)
                                                   .withSampleFormat (bitDepth == 32 ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                                                                     : juce::AudioFormatWriterOptions::SampleFormat::integral);
    auto writer = wav.createWriterFor (stream, options);
    if (writer == nullptr) { error = "Unsupported WAV format"; return false; }
    if (! writer->writeFromAudioSampleBuffer (ir, 0, ir.getNumSamples())) { error = "Write failed"; return false; }
    return true;
}
} // namespace irc
