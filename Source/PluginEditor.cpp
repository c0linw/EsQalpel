/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
static juce::String getUIHtml()
{
    return juce::String (R"ESQALPELUI(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }

  body {
    background: #111111;
    color: #cccccc;
    font-family: 'Consolas', 'Courier New', monospace;
    width: 900px;
    height: 600px;
    overflow: hidden;
    display: flex;
    flex-direction: column;
    user-select: none;
  }

  /* ── Spectrum canvas ──────────────────────────────────────────── */
  #spectrumCanvas {
    display: block;
    flex-shrink: 0;
    width: 900px;
    height: 340px;
    background: #0d0d0d;
  }

  /* ── Mode button bar ──────────────────────────────────────────── */
  #modeBar {
    display: flex;
    gap: 4px;
    padding: 6px 10px;
    background: #1a1a1a;
    border-top: 1px solid #2a2a2a;
    border-bottom: 1px solid #2a2a2a;
    flex-shrink: 0;
  }

  .mode-btn {
    flex: 1;
    padding: 5px 0;
    background: #252525;
    border: 1px solid #3a3a3a;
    color: #888888;
    cursor: pointer;
    font-family: inherit;
    font-size: 11px;
    letter-spacing: 0.06em;
    text-transform: uppercase;
    transition: background 0.12s, color 0.12s;
  }
  .mode-btn.active {
    background: #1a3d70;
    border-color: #3d6eb5;
    color: #e0e8ff;
  }
  .mode-btn:hover:not(.active) { background: #2f2f2f; color: #aaa; }

  /* ── Parameter area ───────────────────────────────────────────── */
  #paramArea {
    flex: 1;
    padding: 10px 12px 8px;
    overflow: hidden;
  }

  .mode-panel { display: none; }
  .mode-panel.active {
    display: flex;
    flex-wrap: nowrap;
    gap: 10px;
    height: 100%;
    align-items: flex-start;
  }

  .param {
    display: flex;
    flex-direction: column;
    gap: 5px;
    flex: 1;
    min-width: 0;
  }
  .param label {
    font-size: 10px;
    color: #666;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    white-space: nowrap;
  }
  .param.disabled label { opacity: 0.3; }

  .param input[type=range] {
    width: 100%;
    accent-color: #3d6eb5;
    cursor: pointer;
    height: 18px;
  }
  .param input[type=range]:disabled {
    opacity: 0.25;
    cursor: not-allowed;
  }

  .param .value {
    font-size: 12px;
    color: #c0c8d0;
    text-align: right;
    min-height: 16px;
  }
  .param.disabled .value { opacity: 0.3; }
</style>
</head>
<body>

<canvas id="spectrumCanvas" width="900" height="340"></canvas>

<div id="modeBar">
  <button class="mode-btn active" data-mode="autosc">Auto SC</button>
  <button class="mode-btn"        data-mode="midisc">MIDI SC</button>
  <button class="mode-btn"        data-mode="naivesc">Naive SC</button>
  <button class="mode-btn"        data-mode="midi">MIDI</button>
</div>

<div id="paramArea">

  <!-- ── Auto SC ─────────────────────────────────────────────────── -->
  <div class="mode-panel active" id="panel-autosc">
    <div class="param">
      <label>Harmonics</label>
      <input type="range" min="1" max="8" step="1" value="4"
             oninput="this.nextElementSibling.textContent = this.value">
      <span class="value">4</span>
    </div>
    <div class="param">
      <label>Max Depth</label>
      <input type="range" min="-48" max="0" step="0.5" value="-18"
             oninput="this.nextElementSibling.textContent = parseFloat(this.value).toFixed(1) + ' dB'">
      <span class="value">-18.0 dB</span>
    </div>
    <div class="param">
      <label>Threshold</label>
      <input type="range" min="-80" max="0" step="1" value="-40"
             oninput="this.nextElementSibling.textContent = this.value + ' dBFS'">
      <span class="value">-40 dBFS</span>
    </div>
    <div class="param">
      <label>Attack</label>
      <input type="range" min="1" max="200" step="1" value="10"
             oninput="this.nextElementSibling.textContent = this.value + ' ms'">
      <span class="value">10 ms</span>
    </div>
    <div class="param">
      <label>Release</label>
      <input type="range" min="10" max="2000" step="10" value="100"
             oninput="this.nextElementSibling.textContent = this.value + ' ms'">
      <span class="value">100 ms</span>
    </div>
  </div>

  <!-- ── MIDI SC ─────────────────────────────────────────────────── -->
  <div class="mode-panel" id="panel-midisc">
    <div class="param">
      <label>Harmonics</label>
      <input type="range" min="1" max="8" step="1" value="4"
             oninput="this.nextElementSibling.textContent = this.value">
      <span class="value">4</span>
    </div>
    <div class="param">
      <label>Max Depth</label>
      <input type="range" min="-48" max="0" step="0.5" value="-18"
             oninput="this.nextElementSibling.textContent = parseFloat(this.value).toFixed(1) + ' dB'">
      <span class="value">-18.0 dB</span>
    </div>
    <div class="param">
      <label>Threshold</label>
      <input type="range" min="-80" max="0" step="1" value="-40"
             oninput="this.nextElementSibling.textContent = this.value + ' dBFS'">
      <span class="value">-40 dBFS</span>
    </div>
    <div class="param">
      <label>Attack</label>
      <input type="range" min="1" max="200" step="1" value="10"
             oninput="this.nextElementSibling.textContent = this.value + ' ms'">
      <span class="value">10 ms</span>
    </div>
    <div class="param">
      <label>Release</label>
      <input type="range" min="10" max="2000" step="10" value="100"
             oninput="this.nextElementSibling.textContent = this.value + ' ms'">
      <span class="value">100 ms</span>
    </div>
  </div>

  <!-- ── Naive SC ────────────────────────────────────────────────── -->
  <div class="mode-panel" id="panel-naivesc">
    <div class="param">
      <label>Max Depth</label>
      <input type="range" min="-48" max="0" step="0.5" value="-18"
             oninput="this.nextElementSibling.textContent = parseFloat(this.value).toFixed(1) + ' dB'">
      <span class="value">-18.0 dB</span>
    </div>
    <div class="param">
      <label>Threshold</label>
      <input type="range" min="-80" max="0" step="1" value="-40"
             oninput="this.nextElementSibling.textContent = this.value + ' dBFS'">
      <span class="value">-40 dBFS</span>
    </div>
    <div class="param">
      <label>Attack</label>
      <input type="range" min="1" max="200" step="1" value="10"
             oninput="this.nextElementSibling.textContent = this.value + ' ms'">
      <span class="value">10 ms</span>
    </div>
    <div class="param">
      <label>Release</label>
      <input type="range" min="10" max="2000" step="10" value="100"
             oninput="this.nextElementSibling.textContent = this.value + ' ms'">
      <span class="value">100 ms</span>
    </div>
  </div>

  <!-- ── MIDI (threshold disabled) ───────────────────────────────── -->
  <div class="mode-panel" id="panel-midi">
    <div class="param">
      <label>Harmonics</label>
      <input type="range" min="1" max="8" step="1" value="4"
             oninput="this.nextElementSibling.textContent = this.value">
      <span class="value">4</span>
    </div>
    <div class="param">
      <label>Max Depth</label>
      <input type="range" min="-48" max="0" step="0.5" value="-18"
             oninput="this.nextElementSibling.textContent = parseFloat(this.value).toFixed(1) + ' dB'">
      <span class="value">-18.0 dB</span>
    </div>
    <div class="param disabled">
      <label>Threshold</label>
      <input type="range" min="-80" max="0" step="1" value="-40" disabled>
      <span class="value">—</span>
    </div>
    <div class="param">
      <label>Attack</label>
      <input type="range" min="1" max="200" step="1" value="10"
             oninput="this.nextElementSibling.textContent = this.value + ' ms'">
      <span class="value">10 ms</span>
    </div>
    <div class="param">
      <label>Release</label>
      <input type="range" min="10" max="2000" step="10" value="100"
             oninput="this.nextElementSibling.textContent = this.value + ' ms'">
      <span class="value">100 ms</span>
    </div>
  </div>

</div><!-- #paramArea -->

<script>
'use strict';

// ── Constants ──────────────────────────────────────────────────────────────
const NUM_BINS    = 1024;
const SAMPLE_RATE = 44100;   // scaffold default; will be passed dynamically later
const DB_TOP      = 0;
const DB_BOTTOM   = -90;

// ── State ──────────────────────────────────────────────────────────────────
const inputMags = new Float32Array(NUM_BINS).fill(DB_BOTTOM);

// ── Canvas setup ───────────────────────────────────────────────────────────
const canvas = document.getElementById('spectrumCanvas');
const ctx    = canvas.getContext('2d');
const W      = canvas.width;    // 900
const H      = canvas.height;   // 340

// ── Frequency / amplitude mapping ─────────────────────────────────────────
const LOG20   = Math.log10(20);
const LOG20K  = Math.log10(20000);
const LOGSPAN = LOG20K - LOG20;

function freqToX (freq) {
    return (Math.log10(Math.max(freq, 1.0)) - LOG20) / LOGSPAN * W;
}

function dbToY (db) {
    return (DB_TOP - db) / (DB_TOP - DB_BOTTOM) * H;
}

// ── Spectrum render loop ───────────────────────────────────────────────────
const FREQ_GRID   = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
const FREQ_LABELS = ['20', '50', '100', '200', '500', '1k', '2k', '5k', '10k', '20k'];
const DB_GRID     = [0, -10, -20, -40, -60, -80];

function drawSpectrum () {
    // Background
    ctx.fillStyle = '#0d0d0d';
    ctx.fillRect(0, 0, W, H);

    ctx.save();
    ctx.font = '10px Consolas, monospace';

    // dB horizontal grid lines
    ctx.strokeStyle = 'rgba(255,255,255,0.05)';
    ctx.lineWidth   = 1;
    ctx.fillStyle   = 'rgba(255,255,255,0.22)';
    for (const db of DB_GRID) {
        const y = dbToY(db);
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(W, y);
        ctx.stroke();
        ctx.fillText(db + ' dB', 4, y - 2);
    }

    // Frequency vertical grid lines
    ctx.strokeStyle = 'rgba(255,255,255,0.05)';
    ctx.fillStyle   = 'rgba(255,255,255,0.22)';
    for (let i = 0; i < FREQ_GRID.length; ++i) {
        const x = freqToX(FREQ_GRID[i]);
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, H);
        ctx.stroke();
        ctx.fillText(FREQ_LABELS[i], x + 3, H - 4);
    }

    // Input spectrum line
    ctx.strokeStyle = 'rgba(100,200,255,0.85)';
    ctx.lineWidth   = 1.5;
    ctx.beginPath();
    let first = true;
    for (let i = 1; i < NUM_BINS; ++i) {
        const freq = i * (SAMPLE_RATE / 2) / NUM_BINS;
        if (freq < 20 || freq > 20000) continue;
        const x = freqToX(freq);
        const y = dbToY(Math.max(inputMags[i], DB_BOTTOM));
        if (first) { ctx.moveTo(x, y); first = false; }
        else        { ctx.lineTo(x, y); }
    }
    ctx.stroke();

    ctx.restore();

    requestAnimationFrame(drawSpectrum);
}

// ── Called by C++ timer at ~30 Hz via evaluateJavascript ──────────────────
function updateSpectra (data) {
    if (data && data.input && data.input.length === NUM_BINS) {
        for (let i = 0; i < NUM_BINS; ++i)
            inputMags[i] = data.input[i];
    }
}

// ── Mode switching ─────────────────────────────────────────────────────────
document.querySelectorAll('.mode-btn').forEach(function (btn) {
    btn.addEventListener('click', function () {
        const mode = btn.dataset.mode;
        document.querySelectorAll('.mode-btn').forEach(function (b) {
            b.classList.toggle('active', b === btn);
        });
        document.querySelectorAll('.mode-panel').forEach(function (p) {
            p.classList.toggle('active', p.id === 'panel-' + mode);
        });
    });
});

// ── Start render loop ──────────────────────────────────────────────────────
requestAnimationFrame(drawSpectrum);
</script>
</body>
</html>
)ESQALPELUI");
}

//==============================================================================
EsQalpelAudioProcessorEditor::EsQalpelAudioProcessorEditor (EsQalpelAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    using WBC = juce::WebBrowserComponent;

    auto options = WBC::Options{}
        .withBackend (WBC::Options::Backend::webview2)
        .withWinWebView2Options (
            WBC::Options::WinWebView2{}
                .withUserDataFolder (
                    juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("EsQalpel_WebView")))
        .withResourceProvider (
            [] (const juce::String& url) -> std::optional<WBC::Resource>
            {
                if (url.isEmpty() || url == "/")
                {
                    auto html = getUIHtml();
                    std::vector<std::byte> bytes;
                    bytes.reserve ((size_t) html.getNumBytesAsUTF8());
                    for (const char c : html.toStdString())
                        bytes.push_back (static_cast<std::byte> (c));
                    return WBC::Resource { std::move (bytes), "text/html" };
                }
                return std::nullopt;
            });

    webView = std::make_unique<WBC> (options);
    addAndMakeVisible (*webView);
    setSize (900, 600);
    webView->goToURL (WBC::getResourceProviderRoot());
    startTimerHz (30);
}

EsQalpelAudioProcessorEditor::~EsQalpelAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void EsQalpelAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Visible only during WebView2 init / if WebView2 is unavailable.
    g.fillAll (juce::Colour (0xff111111));
}

void EsQalpelAudioProcessorEditor::resized()
{
    if (webView)
        webView->setBounds (getLocalBounds());
}

//==============================================================================
void EsQalpelAudioProcessorEditor::timerCallback()
{
    if (!webView) return;

    audioProcessor.getInputAnalyser().processIfAvailable();

    const auto js = "updateSpectra(" + buildSpectraJson() + ");";
    webView->evaluateJavascript (js,
        [] (juce::WebBrowserComponent::EvaluationResult) {});
}

juce::String EsQalpelAudioProcessorEditor::buildSpectraJson() const
{
    const auto& mags = audioProcessor.getInputAnalyser().getMagnitudes();
    juce::String s;
    s.preallocateBytes (8192);
    s += "{\"input\":[";
    for (int i = 0; i < SpectrumAnalyser::numBins; ++i)
    {
        s += juce::String (mags[i], 2);
        if (i < SpectrumAnalyser::numBins - 1)
            s += ',';
    }
    s += "]}";
    return s;
}
