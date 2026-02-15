/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
EsQalpelAudioProcessor::EsQalpelAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

EsQalpelAudioProcessor::~EsQalpelAudioProcessor()
{
}

//==============================================================================
const juce::String EsQalpelAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EsQalpelAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EsQalpelAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EsQalpelAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EsQalpelAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EsQalpelAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EsQalpelAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EsQalpelAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EsQalpelAudioProcessor::getProgramName (int index)
{
    return {};
}

void EsQalpelAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EsQalpelAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate);
    monoMixBuffer.setSize (1, samplesPerBlock, false, true, false);
    inputAnalyser.reset();
}

void EsQalpelAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EsQalpelAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void EsQalpelAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    for (int i = numIn; i < numOut; ++i)
        buffer.clear (i, 0, n);

    if (numIn > 0)
    {
        // Mono-sum all input channels into monoMixBuffer and push to analyser.
        monoMixBuffer.clear();
        const float scale = 1.0f / (float) numIn;
        for (int ch = 0; ch < numIn; ++ch)
            monoMixBuffer.addFrom (0, 0, buffer, ch, 0, n, scale);
        inputAnalyser.pushSamples (monoMixBuffer.getReadPointer (0), n);
    }
}

//==============================================================================
bool EsQalpelAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EsQalpelAudioProcessor::createEditor()
{
    return new EsQalpelAudioProcessorEditor (*this);
}

//==============================================================================
void EsQalpelAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void EsQalpelAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EsQalpelAudioProcessor();
}
