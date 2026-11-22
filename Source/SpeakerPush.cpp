#include "SpeakerPush.h"

namespace irc
{
void SpeakerPush::prepare (const juce::dsp::ProcessSpec& spec)
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing (spec.maximumBlockSize);
    sampleRate = spec.sampleRate * 2.0;

    attackCoef  = (float) std::exp (-1.0 / (0.004 * sampleRate));
    releaseCoef = (float) std::exp (-1.0 / (0.150 * sampleRate));
    pushSmoothed.reset (sampleRate, 0.05);
    lastPush = lastReso = -1.0f;
    updateFilters();
    reset();
}

void SpeakerPush::reset()
{
    for (int i = 0; i < 2; ++i)
    {
        resoFilter[i].reset(); breakupFilter[i].reset(); dcBlock[i].reset();
        env[i] = 0.0f;
    }
    if (oversampling) oversampling->reset();
}

void SpeakerPush::setParameters (float push01, float resoHz)
{
    push = juce::jlimit (0.0f, 1.0f, push01);
    reso = juce::jlimit (40.0f, 250.0f, resoHz);
    pushSmoothed.setTargetValue (push);
}

int SpeakerPush::getLatencySamples() const
{
    return oversampling ? (int) std::round (oversampling->getLatencyInSamples()) : 0;  // integer with useIntegerLatency
}

void SpeakerPush::updateFilters()
{
    if (std::abs (push - lastPush) < 0.002f && std::abs (reso - lastReso) < 0.5f) return;
    lastPush = push; lastReso = reso;
    using C = juce::dsp::IIR::Coefficients<float>;
    auto r  = C::makePeakFilter (sampleRate, reso, 1.6f, juce::Decibels::decibelsToGain (push * 6.0f));
    auto b  = C::makePeakFilter (sampleRate, 2600.0, 0.8f, juce::Decibels::decibelsToGain (push * 3.5f));
    auto dc = C::makeHighPass (sampleRate, 12.0);
    for (int i = 0; i < 2; ++i)
    {
        *resoFilter[i].coefficients = *r;
        *breakupFilter[i].coefficients = *b;
        *dcBlock[i].coefficients = *dc;
    }
}

void SpeakerPush::process (juce::AudioBuffer<float>& buffer, int numChannels)
{
    updateFilters();

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) buffer.getNumSamples());
    auto up = oversampling->processSamplesUp (block);
    const int n = (int) up.getNumSamples();

    for (int s = 0; s < n; ++s)
    {
        const float p = pushSmoothed.getNextValue();
        const float drive = juce::Decibels::decibelsToGain (p * 20.0f);
        const float bias = 0.18f * p;
        const float biasOffset = std::tanh (bias);
        const float makeup = 1.0f / std::pow (drive, 0.8f);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* d = up.getChannelPointer ((size_t) ch);
            const float dry = d[s];
            float x = breakupFilter[ch].processSample (resoFilter[ch].processSample (dry));

            // Voice-coil heating: slow program-dependent compression.
            const float a = std::abs (x);
            env[ch] = a > env[ch] ? attackCoef * env[ch] + (1.0f - attackCoef) * a
                                  : releaseCoef * env[ch] + (1.0f - releaseCoef) * a;
            const float comp = 1.0f / (1.0f + 2.5f * p * env[ch]);

            // Asymmetric cone excursion saturation.
            const float y = (std::tanh (x * drive * comp + bias) - biasOffset) * makeup;
            const float wet = dcBlock[ch].processSample (y);
            d[s] = dry + juce::jlimit (0.0f, 1.0f, p * 4.0f) * (wet - dry);  // smooth fade in near zero
        }
    }

    oversampling->processSamplesDown (block);
}
}
