/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// GainReductionDisplay — shows the per-frequency gain change applied by the EQ.
//
// 0 dB (centre) = no processing. Negative = cut. Positive = boost.
// Updated from the message thread at 30 Hz via timerCallback().
//==============================================================================
class GainReductionDisplay : public juce::Component
{
public:
    static constexpr int numBins = SpectrumAnalyser::numBins;

    GainReductionDisplay()
    {
        setOpaque (true);
        eqMags.fill (0.f);
    }

    // Called from the message thread at 30 Hz.
    // EMA smoothing (α ≈ 0.35) damps frame-to-frame variation in notch depth
    // without adding noticeable lag at display rate.
    void update (const float* eq, int count) noexcept
    {
        jassert (count <= numBins);
        static constexpr float kAlpha = 0.35f;
        for (int i = 0; i < count; ++i)
            eqMags[i] = kAlpha * eq[i] + (1.0f - kAlpha) * eqMags[i];
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const float W = (float) getWidth();
        const float H = (float) getHeight();

        g.fillAll (juce::Colour (0xff0d0d0d));
        g.setFont (juce::Font (10.f));

        // ── dB horizontal grid lines ──────────────────────────────────────
        const int dbLevels[] = { 24, 18, 12, 6, 0, -6, -12, -18, -24 };
        for (int db : dbLevels)
        {
            const float y = dbToY ((float) db, H);
            // 0 dB reference is slightly brighter — the "no processing" baseline.
            g.setColour (db == 0 ? juce::Colour (0x40ffffff) : juce::Colour (0x0dffffff));
            g.drawHorizontalLine ((int) y, 0.f, W);
            g.setColour (juce::Colour (0x38ffffff));
            const int textY = (y < 12.f) ? (int) y + 2 : std::max (0, (int) y - 12);
            g.drawText (juce::String (db) + " dB",
                        4, textY, 55, 12,
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

        // ── Gain reduction curve ───────────────────────────────────────────
        // White line: sits flat at 0 dB when idle, dips below on cuts,
        // rises above on boosts.
        juce::Path path;
        bool started = false;
        const double sr = audioProcessor != nullptr
                              ? audioProcessor->getSampleRate()
                              : 44100.0;

        for (int i = 1; i < numBins; ++i)
        {
            const float freq = (float) (i * (sr / 2.0) / numBins);
            if (freq < 20.f || freq > 20000.f) continue;

            const float x = freqToX (freq, W);
            const float y = dbToY (juce::jlimit (kDisplayFloor, kDisplayTop, eqMags[i]), H);

            if (! started) { path.startNewSubPath (x, y); started = true; }
            else             path.lineTo (x, y);
        }

        g.setColour (juce::Colour (0xd9ffffff));
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }

    // Processor pointer — needed only to read sample rate for bin→Hz conversion.
    EsQalpelAudioProcessor* audioProcessor = nullptr;

private:
    static constexpr float kDisplayTop   =  24.f;
    static constexpr float kDisplayFloor = -24.f;

    std::array<float, numBins> eqMags;

    static float freqToX (float freq, float W) noexcept
    {
        static const float log20   = std::log10 (20.f);
        static const float logSpan = std::log10 (20000.f) - log20; // = 3.0
        return (std::log10 (std::max (freq, 1.f)) - log20) / logSpan * W;
    }

    static float dbToY (float db, float H) noexcept
    {
        // +24 dB → top (y=0),  0 dB → centre,  −24 dB → bottom (y=H).
        return (kDisplayTop - db) / (kDisplayTop - kDisplayFloor) * H;
    }
};

//==============================================================================
static const char* const kModeNames[4] = { "Auto SC", "MIDI SC", "Naive SC", "MIDI" };

EsQalpelAudioProcessorEditor::EsQalpelAudioProcessorEditor (EsQalpelAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // ── Gain reduction display ────────────────────────────────────────────────
    grDisplay = std::make_unique<GainReductionDisplay>();
    grDisplay->audioProcessor = &p;
    addAndMakeVisible (*grDisplay);

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

    // ── CPU label ─────────────────────────────────────────────────────────────
    cpuLabel.setFont (juce::Font (10.f));
    cpuLabel.setColour (juce::Label::textColourId, juce::Colour (0xff555555));
    cpuLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (cpuLabel);

    // ── Initial state — restore persisted mode from APVTS ────────────────────
    const int savedMode = static_cast<int> (*audioProcessor.getAPVTS().getRawParameterValue ("mode"));
    setActiveMode (savedMode);
    setSize (900, 440);
    startTimerHz (30);
}

EsQalpelAudioProcessorEditor::~EsQalpelAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void EsQalpelAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111111));

    // Mode bar background and separator lines.
    constexpr int kSpecH = 300, kBarH = 36;
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRect (0, kSpecH, getWidth(), kBarH);
    g.setColour (juce::Colour (0xff2a2a2a));
    g.drawHorizontalLine (kSpecH,         0.f, (float) getWidth());
    g.drawHorizontalLine (kSpecH + kBarH, 0.f, (float) getWidth());
}

void EsQalpelAudioProcessorEditor::resized()
{
    constexpr int kSpecH    = 300;
    constexpr int kBarH     = 36;
    constexpr int kBtnPad   = 10;
    constexpr int kBtnGap   = 4;
    constexpr int kParamPad = 12;
    constexpr int kStripGap = 10;
    constexpr int kLabelH   = 14;
    constexpr int kSliderH  = 24;

    grDisplay->setBounds (0, 0, getWidth(), kSpecH);

    // Mode buttons — evenly spaced inside the mode bar.
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
            strips[i].label .setBounds (x, paramTop,               sw, kLabelH);
            strips[i].slider.setBounds (x, paramTop + kLabelH + 4, sw, kSliderH);
        }
    };

    layoutStrips (autoSC,   5);
    layoutStrips (midiSC,   5);
    layoutStrips (naiveSC,  4);
    layoutStrips (midiOnly, 4);

    // CPU label — bottom-right corner.
    cpuLabel.setBounds (getWidth() - 80, getHeight() - 16, 76, 14);
}

//==============================================================================
void EsQalpelAudioProcessorEditor::timerCallback()
{
    std::array<float, SpectrumAnalyser::numBins> eqMags;
    audioProcessor.getEQMagnitudes (eqMags.data(), SpectrumAnalyser::numBins,
                                    audioProcessor.getSampleRate());
    grDisplay->update (eqMags.data(), SpectrumAnalyser::numBins);

    const float load = audioProcessor.getCpuLoad();
    cpuLabel.setText ("CPU: " + juce::String (load * 100.0f, 1) + "%",
                      juce::dontSendNotification);
}

//==============================================================================
void EsQalpelAudioProcessorEditor::setActiveMode (int index)
{
    activeMode = index;

    for (int i = 0; i < 4; ++i)
        modeButtons[i].setToggleState (i == index, juce::dontSendNotification);

    if (auto* p = dynamic_cast<juce::AudioParameterInt*> (
            audioProcessor.getAPVTS().getParameter ("mode")))
        p->setValueNotifyingHost (p->convertTo0to1 (index));

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
