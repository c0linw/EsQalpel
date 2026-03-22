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
                       .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)
                       .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

EsQalpelAudioProcessor::~EsQalpelAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout EsQalpelAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ── Active mode ───────────────────────────────────────────────────────────
    // 0 = Auto Sidechain, 1 = MIDI Sidechain, 2 = Naive Sidechain, 3 = MIDI
    layout.add (std::make_unique<juce::AudioParameterInt> ("mode", "Mode", 0, 3, 0));

    // ── Auto Sidechain ────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt>   ("auto_harmonic_count", "Auto SC Harmonics",
                                                              1, 8, 4));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("auto_max_depth", "Auto SC Max Depth",
                                                              juce::NormalisableRange<float> (-48.0f, 0.0f), -18.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("auto_threshold", "Auto SC Threshold",
                                                              juce::NormalisableRange<float> (-80.0f, 0.0f), -40.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("auto_attack", "Auto SC Attack",
                                                              juce::NormalisableRange<float> (1.0f, 200.0f), 10.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("auto_release", "Auto SC Release",
                                                              juce::NormalisableRange<float> (10.0f, 2000.0f), 100.0f));

    // ── MIDI Sidechain ────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt>   ("midi_sc_harmonic_count", "MIDI SC Harmonics",
                                                              1, 8, 4));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("midi_sc_max_depth", "MIDI SC Max Depth",
                                                              juce::NormalisableRange<float> (-48.0f, 0.0f), -18.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("midi_sc_threshold", "MIDI SC Threshold",
                                                              juce::NormalisableRange<float> (-80.0f, 0.0f), -40.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("midi_sc_attack", "MIDI SC Attack",
                                                              juce::NormalisableRange<float> (1.0f, 200.0f), 10.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("midi_sc_release", "MIDI SC Release",
                                                              juce::NormalisableRange<float> (10.0f, 2000.0f), 100.0f));

    // ── Naive Sidechain ───────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> ("naive_sc_max_depth", "Naive SC Max Depth",
                                                              juce::NormalisableRange<float> (-48.0f, 0.0f), -18.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("naive_sc_threshold", "Naive SC Threshold",
                                                              juce::NormalisableRange<float> (-80.0f, 0.0f), -40.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("naive_sc_attack", "Naive SC Attack",
                                                              juce::NormalisableRange<float> (1.0f, 200.0f), 10.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("naive_sc_release", "Naive SC Release",
                                                              juce::NormalisableRange<float> (10.0f, 2000.0f), 100.0f));

    // ── MIDI only ─────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterInt>   ("midi_harmonic_count", "MIDI Harmonics",
                                                              1, 8, 4));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("midi_max_depth", "MIDI Max Depth",
                                                              juce::NormalisableRange<float> (-48.0f, 0.0f), -18.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("midi_attack", "MIDI Attack",
                                                              juce::NormalisableRange<float> (1.0f, 200.0f), 10.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("midi_release", "MIDI Release",
                                                              juce::NormalisableRange<float> (10.0f, 2000.0f), 100.0f));

    return layout;
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
    juce::ignoreUnused (index);
}

const juce::String EsQalpelAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void EsQalpelAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void EsQalpelAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    monoMixBuffer.setSize (1, samplesPerBlock, false, true, false);
    inputAnalyser.reset();
    sidechainAnalyser.reset();
    outputAnalyser.reset();
    autoSCProc.prepare (sampleRate, samplesPerBlock);
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
    // Main output must be mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Main input must match main output.
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    // Sidechain bus may be disabled, mono, or stereo — nothing else.
    const auto& sc = layouts.getChannelSet (true, 1);
    if (!sc.isDisabled()
        && sc != juce::AudioChannelSet::mono()
        && sc != juce::AudioChannelSet::stereo())
        return false;

    return true;
  #endif
}
#endif

void EsQalpelAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const double blockStartMs = juce::Time::getMillisecondCounterHiRes();
    juce::ScopedNoDenormals noDenormals;

    // ── MIDI note tracking ────────────────────────────────────────────────────
    for (const auto meta : midiMessages)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())  activeNotes.set   ((size_t) msg.getNoteNumber());
        if (msg.isNoteOff()) activeNotes.reset ((size_t) msg.getNoteNumber());
    }

    const int numTotalOut = getTotalNumOutputChannels();
    const int n           = buffer.getNumSamples();

    // ── (1) Pre-EQ main input capture ─────────────────────────────────────────
    auto mainBus   = getBusBuffer (buffer, true, 0);
    const int numMainIn = mainBus.getNumChannels();

    // Clear any extra output channels (mono-in → stereo-out, etc.)
    for (int i = numMainIn; i < numTotalOut; ++i)
        buffer.clear (i, 0, n);

    if (numMainIn > 0)
    {
        monoMixBuffer.clear();
        const float mainScale = 1.0f / (float) numMainIn;
        for (int ch = 0; ch < numMainIn; ++ch)
            monoMixBuffer.addFrom (0, 0, mainBus, ch, 0, n, mainScale);
        inputAnalyser.pushSamples (monoMixBuffer.getReadPointer (0), n);
    }

    // ── (2) Sidechain capture ──────────────────────────────────────────────────
    auto scBuffer      = getBusBuffer (buffer, true, 1);
    const int scChannels = scBuffer.getNumChannels();
    if (scChannels > 0)
    {
        monoMixBuffer.clear();
        const float scScale = 1.0f / (float) scChannels;
        for (int ch = 0; ch < scChannels; ++ch)
            monoMixBuffer.addFrom (0, 0, scBuffer, ch, 0, n, scScale);
        sidechainAnalyser.pushSamples (monoMixBuffer.getReadPointer (0), n);
    }

    // ── (2b) Auto Sidechain processing ────────────────────────────────────────
    // monoMixBuffer still holds the sidechain mono mix from step (2).
    const int mode = static_cast<int> (*apvts.getRawParameterValue ("mode"));
    if (mode == 0 && scChannels > 0)
    {
        autoSCProc.process (
            mainBus,
            monoMixBuffer.getReadPointer (0),
            n,
            static_cast<int> (*apvts.getRawParameterValue ("auto_harmonic_count")),
            *apvts.getRawParameterValue ("auto_max_depth"),
            *apvts.getRawParameterValue ("auto_threshold"),
            *apvts.getRawParameterValue ("auto_attack"),
            *apvts.getRawParameterValue ("auto_release"));
    }

    // ── (3) Post-EQ output capture ────────────────────────────────────────────
    if (numMainIn > 0)
    {
        monoMixBuffer.clear();
        const float mainScale = 1.0f / (float) numMainIn;
        for (int ch = 0; ch < numMainIn; ++ch)
            monoMixBuffer.addFrom (0, 0, mainBus, ch, 0, n, mainScale);
        outputAnalyser.pushSamples (monoMixBuffer.getReadPointer (0), n);
    }

    // ── CPU load measurement ──────────────────────────────────────────────────
    const double bufferMs  = (double) buffer.getNumSamples() / currentSampleRate * 1000.0;
    const double elapsedMs = juce::Time::getMillisecondCounterHiRes() - blockStartMs;
    const float  loadRaw   = (float) (elapsedMs / bufferMs);
    // Exponential moving average — weight new reading at ~10 %.
    cpuLoad.store (0.1f * loadRaw + 0.9f * cpuLoad.load (std::memory_order_relaxed),
                   std::memory_order_relaxed);
}

//==============================================================================
void EsQalpelAudioProcessor::getEQMagnitudes (float* output, int numBins, double sampleRate) const noexcept
{
    const int mode = static_cast<int> (*apvts.getRawParameterValue ("mode"));

    if (mode == 0)
    {
        autoSCProc.getEQMagnitudes (output, numBins, sampleRate);
        return;
    }

    // Other modes not yet implemented — flat curve.
    juce::FloatVectorOperations::fill (output, 0.0f, numBins);
}

//==============================================================================
bool EsQalpelAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* EsQalpelAudioProcessor::createEditor()
{
    return new EsQalpelAudioProcessorEditor (*this);
}

//==============================================================================
void EsQalpelAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void EsQalpelAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EsQalpelAudioProcessor();
}
