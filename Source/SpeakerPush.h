#pragma once
#include <juce_dsp/juce_dsp.h>

namespace irc
{
/** "Speaker Push": models a guitar speaker being driven hard, before the cab IR.
    - voice-coil power compression (program-dependent gain reduction)
    - cone resonance bump that grows with drive (Reso sets the frequency)
    - upper-mid cone breakup emphasis
    - asymmetric soft saturation (cone excursion), 2x oversampled */
class SpeakerPush
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void setParameters (float push01, float resoHz);
    void process (juce::AudioBuffer<float>& buffer, int numChannels);
    int getLatencySamples() const;

private:
    void updateFilters();

    double sampleRate = 48000.0;
    float push = 0.0f, reso = 100.0f;
    float lastPush = -1.0f, lastReso = -1.0f;
    juce::dsp::IIR::Filter<float> resoFilter[2], breakupFilter[2], dcBlock[2];
    float env[2] { 0.0f, 0.0f };
    float attackCoef = 0.0f, releaseCoef = 0.0f;
    juce::SmoothedValue<float> pushSmoothed;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
};
}
