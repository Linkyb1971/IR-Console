#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>

namespace irc
{
constexpr int numChannels  = 8;
constexpr int numEqBands   = 10;
// Band centres of the classic 10-band console graphic EQ ("560-style").
constexpr std::array<float, numEqBands> eqBandFreqs { 31.0f, 63.0f, 125.0f, 250.0f, 500.0f,
                                                       1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
constexpr float maxLiveSeconds = 2.0f;

/** One impulse response loaded onto a console channel. Immutable once created. */
struct ChannelIR
{
    juce::AudioBuffer<float> data;   // 1 or 2 channels
    double sampleRate = 48000.0;
    juce::String name;
    juce::String path;
};
using ChannelIRPtr = std::shared_ptr<const ChannelIR>;

struct ChannelSettings
{
    float gainDb   = 0.0f;
    float pan      = 0.0f;     // -1 .. +1
    bool  mute     = false;
    bool  solo     = false;
    bool  invert   = false;
    float delayMs  = 0.0f;     // mic/time alignment
    float hpfHz    = 20.0f;    // <= 20 Hz  -> off
    float lpfHz    = 20000.0f; // >= 20 kHz -> off
    float lowGain  = 0.0f, lowFreq  = 120.0f;
    float midGain  = 0.0f, midFreq  = 1200.0f, midQ = 1.0f;
    float highGain = 0.0f, highFreq = 6000.0f;
};

struct BlendSettings
{
    std::array<ChannelSettings, numChannels> channels;
    bool   stereo          = true;
    bool   autoLevel       = true;   // energy-normalise the blend so loudness stays consistent
    bool   includeMasterEq = false;  // bake the master graphic EQ into the IR (export)
    bool   masterEqOn      = true;
    std::array<float, numEqBands> masterEq {};
    double sampleRate      = 48000.0;
    int    maxLength       = 0;      // 0 = automatic (trimmed, capped at maxLiveSeconds)
    bool   peakNormalise   = false;  // normalise peak to -0.1 dBFS (export)
};

/** Renders all channel IRs into one blended (mono or stereo) IR, applying per-channel
    delay, polarity, EQ, gain and pan. This single function drives both real-time playback
    and file export, so what you hear is exactly what you export. */
juce::AudioBuffer<float> renderBlend (const std::array<ChannelIRPtr, numChannels>& irs,
                                      const BlendSettings& settings);

/** High-quality windowed-sinc sample-rate conversion of an IR. */
juce::AudioBuffer<float> resampleIR (const juce::AudioBuffer<float>& in, double inRate, double outRate);

/** Coefficients for one band of the master graphic EQ (proportional-Q peaking bell). */
juce::dsp::IIR::Coefficients<float>::Ptr makeMasterEqBand (double sampleRate, int band, float gainDb);

/** Magnitude response (dB) of the full master EQ at a frequency. */
float masterEqResponseDb (const std::array<float, numEqBands>& gains, double sampleRate, double freq);

/** Smoothed magnitude response (dB) of an IR channel at log-spaced points between 20 Hz and 20 kHz. */
std::vector<float> computeResponseDb (const float* ir, int length, double sampleRate, int numPoints);

/** Loads an audio file into a ChannelIR (any format JUCE can read: WAV, AIFF, FLAC...). */
ChannelIRPtr loadIRFile (const juce::File& file, juce::String& error);

/** All IR files (WAV/AIFF/FLAC) in a folder, in natural order ("2in" before "10in"). */
juce::Array<juce::File> findIRsInFolder (const juce::File& folder);

/** The IR a step of `delta` away from `current` within its folder (wraps around).
    Returns an invalid File if `current` no longer exists on disk. */
juce::File stepIR (const juce::File& current, int delta, int* positionOut = nullptr, int* countOut = nullptr);

/** Writes an IR as a WAV file. bitDepth: 16, 24 or 32 (float). */
bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& ir, double sampleRate, int bitDepth, juce::String& error);
} // namespace irc
