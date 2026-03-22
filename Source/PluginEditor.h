/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class GainReductionDisplay; // Defined in PluginEditor.cpp

//==============================================================================
class EsQalpelAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit EsQalpelAudioProcessorEditor (EsQalpelAudioProcessor&);
    ~EsQalpelAudioProcessorEditor() override;

    //==============================================================================
    void paint   (juce::Graphics&) override;
    void resized ()                override;

private:
    //==============================================================================
    void timerCallback() override;
    void setActiveMode (int index);

    //==============================================================================
    // One label + one slider + one APVTS attachment, grouped as a parameter strip.
    struct Strip
    {
        juce::Label  label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    //==============================================================================
    EsQalpelAudioProcessor& audioProcessor;

    std::unique_ptr<GainReductionDisplay> grDisplay;

    juce::TextButton modeButtons[4];
    int              activeMode { 0 };

    Strip autoSC[5];    // harmonics, maxDepth, threshold, attack, release
    Strip midiSC[5];    // harmonics, maxDepth, threshold, attack, release
    Strip naiveSC[4];   // maxDepth, threshold, attack, release
    Strip midiOnly[4];  // harmonics, maxDepth, attack, release

    juce::Label cpuLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EsQalpelAudioProcessorEditor)
};
