#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "BlendEngine.h"
#include "SpeakerPush.h"

class IRConsoleProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    IRConsoleProcessor();
    ~IRConsoleProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "IR Console"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return irc::maxLiveSeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    static juce::String chId (int ch, const char* name) { return "ch" + juce::String (ch + 1) + "_" + name; }
    static juce::String eqId (int band) { return "eq" + juce::String (band); }

    juce::String loadIR (int channel, const juce::File& file);   // returns error or empty
    void setIR (int channel, irc::ChannelIRPtr ir);
    void clearIR (int channel) { setIR (channel, nullptr); }
    irc::ChannelIRPtr getIR (int channel) const;
    std::array<irc::ChannelIRPtr, irc::numChannels> getIRs() const;
    int getIRVersion() const { return irVersion.load(); }

    irc::BlendSettings getCurrentSettings() const;
    double getCurrentSampleRate() const { return currentRate.load(); }

    struct Display
    {
        std::vector<float> responseL, responseR;   // dB at log points 20 Hz..20 kHz
        int lengthSamples = 0;
        double sampleRate = 48000.0;
        bool stereo = true;
        bool empty = true;
    };
    Display getDisplay() const;
    int getDisplayVersion() const { return displayVersion.load(); }
    static constexpr int displayPoints = 240;

    float getOutputPeak (int ch) const { return outPeak[ch].load(); }
    float getOutputRms (int ch) const { return outRms[ch].load(); }
    float getInputPeak() const { return inPeak.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float) override;
    void rebuildBlend();
    void updateMasterEq();

    struct BlendThread : juce::Thread
    {
        BlendThread (IRConsoleProcessor& p) : juce::Thread ("IR blend"), owner (p) {}
        void run() override;
        IRConsoleProcessor& owner;
    } blendThread { *this };

    mutable juce::CriticalSection irLock, displayLock, buildLock;
    std::array<irc::ChannelIRPtr, irc::numChannels> irs;
    std::atomic<int> irVersion { 0 }, displayVersion { 0 };
    std::atomic<bool> dirty { true };
    std::atomic<double> currentRate { 48000.0 };
    std::atomic<bool> prepared { false };
    Display display;

    juce::dsp::Convolution convolution { juce::dsp::Convolution::NonUniform { 256 } };
    irc::SpeakerPush push;
    juce::dsp::IIR::Filter<float> masterEq[irc::numEqBands][2];
    std::array<float, irc::numEqBands> lastEqGains;
    juce::AudioBuffer<float> work;
    juce::SmoothedValue<float> inGain, outGain;
    std::atomic<float> outPeak[2] { 0.0f, 0.0f };
    std::atomic<float> outRms[2] { 0.0f, 0.0f };
    std::atomic<float> inPeak { 0.0f };

    // cached raw parameter pointers
    std::atomic<float>* pStereo; std::atomic<float>* pAutoLevel; std::atomic<float>* pPush; std::atomic<float>* pReso;
    std::atomic<float>* pEqOn; std::atomic<float>* pIn; std::atomic<float>* pOut;
    std::array<std::atomic<float>*, irc::numEqBands> pEq;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IRConsoleProcessor)
};
