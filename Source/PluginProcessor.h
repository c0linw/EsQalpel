/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SpectrumAnalyser.h"
#include "AutoSCProcessor.h"
#include <bitset>

//==============================================================================
/**
*/
class EsQalpelAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    EsQalpelAudioProcessor();
    ~EsQalpelAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    SpectrumAnalyser& getInputAnalyser()    noexcept { return inputAnalyser; }
    SpectrumAnalyser& getSidechainAnalyser() noexcept { return sidechainAnalyser; }
    SpectrumAnalyser& getOutputAnalyser()   noexcept { return outputAnalyser; }

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    void getEQMagnitudes (float* output, int numBins, double sampleRate) const noexcept;

    // Fraction of the audio buffer period spent in processBlock (0.0–1.0+).
    // Exponentially smoothed; safe to read from any thread.
    float getCpuLoad() const noexcept { return cpuLoad.load (std::memory_order_relaxed); }

private:
    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    SpectrumAnalyser         inputAnalyser;
    SpectrumAnalyser         sidechainAnalyser;
    SpectrumAnalyser         outputAnalyser;
    juce::AudioBuffer<float> monoMixBuffer;

    AutoSCProcessor          autoSCProc;

    std::bitset<128>         activeNotes;
    double                   currentSampleRate { 44100.0 };
    std::atomic<float>       cpuLoad          { 0.0f };

    juce::AudioProcessorValueTreeState apvts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EsQalpelAudioProcessor)
};
