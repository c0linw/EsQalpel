/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// SpectrumDisplay — pure JUCE Graphics, no web layer.
//
// Audio thread writes to the SpectrumAnalyser FIFOs in the processor.
// Message thread calls update() from timerCallback(), which stores the
// magnitude arrays and calls repaint(). paint() draws everything synchronously
// on the message thread with no cross-thread data hazards.
//==============================================================================
class SpectrumDisplay : public juce::Component
{
public:
    static constexpr int numBins = SpectrumAnalyser::numBins;

    SpectrumDisplay()
    {
        setOpaque (true);
        inputMags.fill (kFloorDB);
        scMags   .fill (kFloorDB);
        eqMags   .fill (0.f);
        outMags  .fill (kFloorDB);
    }

    // Called from the message thread — copies magnitude data and triggers repaint.
    void update (const float* input,  const float* sc,
                 const float* eq,     const float* output,
                 double sr) noexcept
    {
        std::copy (input,  input  + numBins, inputMags.data());
        std::copy (sc,     sc     + numBins, scMags.data());
        std::copy (eq,     eq     + numBins, eqMags.data());
        std::copy (output, output + numBins, outMags.data());
        sampleRate = sr;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const float W = (float) getWidth();
        const float H = (float) getHeight();

        g.fillAll (juce::Colour (0xff0d0d0d));

        g.setFont (juce::Font (10.f));

        // ── dB horizontal grid lines ──────────────────────────────────────
        const int dbLevels[] = { 0, -10, -20, -40, -60, -80 };
        for (int db : dbLevels)
        {
            const float y = dbToY ((float) db, H);
            g.setColour (juce::Colour (0x0dffffff));
            g.drawHorizontalLine ((int) y, 0.f, W);
            g.setColour (juce::Colour (0x38ffffff));
            g.drawText (juce::String (db) + " dB",
                        4, std::max (0, (int) y - 12), 55, 12,
                        juce::Justification::left, false);
        }

        // ── Frequency vertical grid lines ─────────────────────────────────
        const float       freqGrid[]   = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        const char* const freqLabels[] = { "20","50","100","200","500","1k","2k","5k","10k","20k" };
        for (int i = 0; i < 10; ++i)
        {
            const float x = freqToX (freqGrid[i], W);
            g.setColour (juce::Colour (0x0dffffff));
            g.drawVerticalLine ((int) x, 0.f, H);
            g.setColour (juce::Colour (0x38ffffff));
            g.drawText (freqLabels[i], (int) x + 3, (int) H - 14, 40, 12,
                        juce::Justification::left, false);
        }

        // ── Spectra (back → front) ────────────────────────────────────────
        // EQ curve (green, thicker) — behind audio spectra so they read on top
        drawSpecLine (g, eqMags.data(),    juce::Colour (0xe564ff78), 2.0f);
        // Sidechain (orange)
        drawSpecLine (g, scMags.data(),    juce::Colour (0xbfffa03c), 1.5f);
        // Post-EQ output (pink)
        drawSpecLine (g, outMags.data(),   juce::Colour (0xbfff64c8), 1.5f);
        // Pre-EQ input (cyan) — most prominent, on top
        drawSpecLine (g, inputMags.data(), juce::Colour (0xd964c8ff), 1.5f);
    }

private:
    static constexpr float kFloorDB = SpectrumAnalyser::kFloorDB;

    std::array<float, numBins> inputMags, scMags, eqMags, outMags;
    double sampleRate = 44100.0;

    void drawSpecLine (juce::Graphics& g, const float* mags,
                       juce::Colour colour, float thickness) const
    {
        juce::Path path;
        const float W = (float) getWidth();
        const float H = (float) getHeight();
        bool started = false;

        for (int i = 1; i < numBins; ++i)
        {
            const auto freq = (float) (i * (sampleRate / 2.0) / numBins);
            if (freq < 20.f || freq > 20000.f) continue;

            const float x = freqToX (freq, W);
            const float y = dbToY (std::max (mags[i], kFloorDB), H);

            if (! started) { path.startNewSubPath (x, y); started = true; }
            else             path.lineTo (x, y);
        }

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness));
    }

    static float freqToX (float freq, float W) noexcept
    {
        // Log-scale: 20 Hz → x=0, 20 kHz → x=W.
        static const float log20   = std::log10 (20.f);
        static const float logSpan = std::log10 (20000.f) - log20; // = 3.0
        return (std::log10 (std::max (freq, 1.f)) - log20) / logSpan * W;
    }

    static float dbToY (float db, float H) noexcept
    {
        // 0 dBFS → top (y=0), −90 dBFS → bottom (y=H).
        return (0.f - db) / (0.f - kFloorDB) * H;
    }
};

//==============================================================================
static const char* const kModeNames[4] = { "Auto SC", "MIDI SC", "Naive SC", "MIDI" };

EsQalpelAudioProcessorEditor::EsQalpelAudioProcessorEditor (EsQalpelAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // ── Spectrum display ──────────────────────────────────────────────────────
    spectrumDisplay = std::make_unique<SpectrumDisplay>();
    addAndMakeVisible (*spectrumDisplay);

    // ── Mode buttons ──────────────────────────────────────────────────────────
    for (int i = 0; i < 4; ++i)
    {
        auto& btn = modeButtons[i];
        btn.setButtonText (kModeNames[i]);
        btn.setClickingTogglesState (false);
        btn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff252525));
        btn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff1a3d70));
        btn.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff888888));
        btn.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffe0e8ff));
        btn.onClick = [this, i] { setActiveMode (i); };
        addAndMakeVisible (btn);
    }

    // ── Parameter strips ──────────────────────────────────────────────────────
    auto& apvts = audioProcessor.getAPVTS();

    auto initStrip = [&] (Strip& s, const char* labelText, const juce::String& paramId)
    {
        s.label.setText (labelText, juce::dontSendNotification);
        s.label.setFont (juce::Font (10.f));
        s.label.setColour (juce::Label::textColourId, juce::Colour (0xff666666));
        s.label.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (s.label);

        s.slider.setSliderStyle (juce::Slider::LinearHorizontal);
        s.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
        s.slider.setColour (juce::Slider::thumbColourId,             juce::Colour (0xff3d6eb5));
        s.slider.setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xffc0c8d0));
        s.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff111111));
        s.slider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colour (0x00000000));
        addAndMakeVisible (s.slider);

        s.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, paramId, s.slider);
    };

    // Auto SC
    initStrip (autoSC[0], "HARMONICS", "auto_harmonic_count");
    initStrip (autoSC[1], "MAX DEPTH", "auto_max_depth");
    initStrip (autoSC[2], "THRESHOLD", "auto_threshold");
    initStrip (autoSC[3], "ATTACK",    "auto_attack");
    initStrip (autoSC[4], "RELEASE",   "auto_release");

    // MIDI SC
    initStrip (midiSC[0], "HARMONICS", "midi_sc_harmonic_count");
    initStrip (midiSC[1], "MAX DEPTH", "midi_sc_max_depth");
    initStrip (midiSC[2], "THRESHOLD", "midi_sc_threshold");
    initStrip (midiSC[3], "ATTACK",    "midi_sc_attack");
    initStrip (midiSC[4], "RELEASE",   "midi_sc_release");

    // Naive SC
    initStrip (naiveSC[0], "MAX DEPTH", "naive_sc_max_depth");
    initStrip (naiveSC[1], "THRESHOLD", "naive_sc_threshold");
    initStrip (naiveSC[2], "ATTACK",    "naive_sc_attack");
    initStrip (naiveSC[3], "RELEASE",   "naive_sc_release");

    // MIDI only (no threshold parameter)
    initStrip (midiOnly[0], "HARMONICS", "midi_harmonic_count");
    initStrip (midiOnly[1], "MAX DEPTH", "midi_max_depth");
    initStrip (midiOnly[2], "ATTACK",    "midi_attack");
    initStrip (midiOnly[3], "RELEASE",   "midi_release");

    // ── Initial state ─────────────────────────────────────────────────────────
    setActiveMode (0);
    setSize (900, 600);
    startTimerHz (30);
}

EsQalpelAudioProcessorEditor::~EsQalpelAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void EsQalpelAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Main background.
    g.fillAll (juce::Colour (0xff111111));

    // Mode bar background and separator lines.
    constexpr int kBarY = 340, kBarH = 36;
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRect (0, kBarY, getWidth(), kBarH);
    g.setColour (juce::Colour (0xff2a2a2a));
    g.drawHorizontalLine (kBarY,         0.f, (float) getWidth());
    g.drawHorizontalLine (kBarY + kBarH, 0.f, (float) getWidth());
}

void EsQalpelAudioProcessorEditor::resized()
{
    constexpr int kSpecH    = 340;
    constexpr int kBarH     = 36;
    constexpr int kBtnPad   = 10;
    constexpr int kBtnGap   = 4;
    constexpr int kParamPad = 12;
    constexpr int kStripGap = 10;
    constexpr int kLabelH   = 14;
    constexpr int kSliderH  = 24;

    spectrumDisplay->setBounds (0, 0, getWidth(), kSpecH);

    // Mode buttons — evenly spaced with small padding and gaps.
    const int btnW = (getWidth() - 2 * kBtnPad - 3 * kBtnGap) / 4;
    const int btnY = kSpecH + (kBarH - 24) / 2;
    for (int i = 0; i < 4; ++i)
        modeButtons[i].setBounds (kBtnPad + i * (btnW + kBtnGap), btnY, btnW, 24);

    // Parameter strips — all laid out regardless of visibility.
    const int paramTop   = kSpecH + kBarH + 10;
    const int stripAreaW = getWidth() - 2 * kParamPad;

    auto layoutStrips = [&] (Strip* strips, int count)
    {
        const int sw = (stripAreaW - (count - 1) * kStripGap) / count;
        for (int i = 0; i < count; ++i)
        {
            const int x = kParamPad + i * (sw + kStripGap);
            strips[i].label .setBounds (x, paramTop,                sw, kLabelH);
            strips[i].slider.setBounds (x, paramTop + kLabelH + 4,  sw, kSliderH);
        }
    };

    layoutStrips (autoSC,   5);
    layoutStrips (midiSC,   5);
    layoutStrips (naiveSC,  4);
    layoutStrips (midiOnly, 4);
}

//==============================================================================
void EsQalpelAudioProcessorEditor::timerCallback()
{
    audioProcessor.getInputAnalyser()   .processIfAvailable();
    audioProcessor.getSidechainAnalyser().processIfAvailable();
    audioProcessor.getOutputAnalyser()  .processIfAvailable();

    std::array<float, SpectrumAnalyser::numBins> eqMags;
    audioProcessor.getEQMagnitudes (eqMags.data(), SpectrumAnalyser::numBins,
                                    audioProcessor.getSampleRate());

    spectrumDisplay->update (
        audioProcessor.getInputAnalyser()   .getMagnitudes().data(),
        audioProcessor.getSidechainAnalyser().getMagnitudes().data(),
        eqMags.data(),
        audioProcessor.getOutputAnalyser()  .getMagnitudes().data(),
        audioProcessor.getSampleRate());
}

//==============================================================================
void EsQalpelAudioProcessorEditor::setActiveMode (int index)
{
    activeMode = index;

    for (int i = 0; i < 4; ++i)
        modeButtons[i].setToggleState (i == index, juce::dontSendNotification);

    for (int i = 0; i < 5; ++i)
    {
        autoSC[i].label .setVisible (index == 0);
        autoSC[i].slider.setVisible (index == 0);
        midiSC[i].label .setVisible (index == 1);
        midiSC[i].slider.setVisible (index == 1);
    }
    for (int i = 0; i < 4; ++i)
    {
        naiveSC[i] .label .setVisible (index == 2);
        naiveSC[i] .slider.setVisible (index == 2);
        midiOnly[i].label .setVisible (index == 3);
        midiOnly[i].slider.setVisible (index == 3);
    }
}
