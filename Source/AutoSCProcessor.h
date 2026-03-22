#pragma once

#include <JuceHeader.h>
#include <array>
#include <algorithm>
#include <cmath>

/**
 * Auto Sidechain processor for EsQalpel.
 *
 * Runs entirely on the audio thread. Detects the fundamental frequency of the
 * sidechain input (YIN algorithm), measures per-harmonic energy (Goertzel),
 * and applies dynamic IIR notch filters to the main bus.
 *
 * Call prepare()         from prepareToPlay().
 * Call process()         from processBlock() after the sidechain mono mix is ready.
 * Call getEQMagnitudes() from the message thread (30 Hz timer) for display.
 */
class AutoSCProcessor
{
public:
    AutoSCProcessor()  = default;
    ~AutoSCProcessor() = default;

    //==========================================================================
    // Internal constants — adjust here for tuning without touching the algorithm.
    static constexpr int   kYinWindowSize  = 2048;     // sets pitch resolution
    static constexpr int   kMaxHarmonics   = 8;
    static constexpr int   kMaxChannels    = 2;
    static constexpr float kYinThreshold   = 0.15f;    // CMNDF threshold; below = voiced
    static constexpr float kF0SmoothMs     = 30.0f;    // one-pole f0 smoother time constant
    static constexpr float kNotchQ         = 8.0f;     // fixed notch bandwidth (Q factor)
    static constexpr float kDepthNormRange = 20.0f;    // dB range for linear depth ramp above threshold
    static constexpr float kMinF0Hz        = 50.0f;
    static constexpr float kMaxF0Hz        = 1500.0f;
    // YIN is expensive (O(tauMax × halfWindow)). Run it at most every kYinHopSize
    // new samples so the cost is bounded regardless of host block size.
    static constexpr int   kYinHopSize     = kYinWindowSize / 2;   // ≈ 23 ms @ 44.1 kHz

private:
    //==========================================================================
    // Biquad filter state — two delay elements per filter instance.
    struct BiquadState
    {
        float v1 = 0.0f, v2 = 0.0f;
        void reset() noexcept { v1 = v2 = 0.0f; }
    };

    // Normalised (a0 = 1) second-order peaking EQ coefficients.
    // `active` is false when the filter is at unity gain and can be skipped.
    struct BiquadCoeffs
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float                         a1 = 0.0f, a2 = 0.0f;
        bool  active = false;
    };

public:
    //==========================================================================
    /** Called from prepareToPlay(). Resets all state. */
    void prepare (double sr, int /*maxBlockSize*/)
    {
        sampleRate    = sr;
        // Coefficient is applied once per YIN hop, not once per sample.
        f0SmoothCoeff = std::exp (-(float) kYinHopSize / (kF0SmoothMs * 0.001f * (float) sr));

        yinBuf.fill  (0.0f);
        yinCmndf.fill (0.0f);
        yinWritePos         = 0;
        yinBufFull          = false;
        yinSamplesSinceRun  = 0;
        lastAperiodicity    = 1.0f;
        smoothedF0          = 0.0f;
        harmEnv.fill (-90.0f);   // dBFS floor — avoids spurious notches at startup

        for (auto& row : filterState)
            for (auto& s : row)
                s.reset();

        filterCoeffs.fill ({});

        {
            const juce::SpinLock::ScopedLockType lock (displayLock);
            displayCoeffs.fill ({});
        }
    }

    //==========================================================================
    /**
     * Process one audio block.
     *
     *  mainBus       — main audio buffer, modified in-place (all channels).
     *  scMono        — pre-mixed mono sidechain, numSamples long (read-only).
     *  harmonicCount — number of harmonic notches to apply (1–kMaxHarmonics).
     *  maxDepth_dB   — ceiling notch depth in dB (negative, e.g. –18.0f).
     *  threshold_dBFS — per-harmonic energy below this → no notch.
     *  attack_ms / release_ms — per-harmonic envelope follower times.
     */
    void process (juce::AudioBuffer<float>& mainBus,
                  const float*              scMono,
                  int                       numSamples,
                  int                       harmonicCount,
                  float                     maxDepth_dB,
                  float                     threshold_dBFS,
                  float                     attack_ms,
                  float                     release_ms)
    {
        jassert (numSamples > 0);
        harmonicCount = juce::jlimit (1, kMaxHarmonics, harmonicCount);
        const int numCh = juce::jmin (mainBus.getNumChannels(), kMaxChannels);

        //── Push sidechain into the YIN ring buffer ───────────────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            yinBuf[yinWritePos] = scMono[i];
            if (++yinWritePos >= kYinWindowSize)
            {
                yinWritePos = 0;
                yinBufFull  = true;
            }
        }

        //── Pitch detection — throttled to at most once per kYinHopSize samples ─
        // smoothedF0 and lastAperiodicity are persistent members; they hold their
        // last values between YIN runs so voiced state and notch position remain
        // stable across all blocks, not just the one where YIN happened to fire.
        yinSamplesSinceRun += numSamples;
        if (yinBufFull && yinSamplesSinceRun >= kYinHopSize)
        {
            float rawF0 = 0.0f;
            runYIN (rawF0, lastAperiodicity);
            yinSamplesSinceRun = 0;
            if (rawF0 > 0.0f)
                smoothedF0 = f0SmoothCoeff * smoothedF0 + (1.0f - f0SmoothCoeff) * rawF0;
        }

        const float voiced  = (smoothedF0 > 0.0f && lastAperiodicity < kYinThreshold) ? 1.0f : 0.0f;
        const float nyquist = (float) (sampleRate * 0.5);

        //── Envelope follower coefficients (precomputed once per block) ───────
        // Applied once per block, so multiply by numSamples to get the correct
        // wall-clock time constant regardless of host block size.
        const float atkCoeff = std::exp (-(float) numSamples / (attack_ms  * 0.001f * (float) sampleRate));
        const float relCoeff = std::exp (-(float) numSamples / (release_ms * 0.001f * (float) sampleRate));

        //── Per-harmonic computation ──────────────────────────────────────────
        std::array<BiquadCoeffs, kMaxHarmonics> localCoeffs {};

        for (int k = 0; k < kMaxHarmonics; ++k)
        {
            if (k >= harmonicCount || smoothedF0 <= 0.0f)
                continue;

            const float f_k = smoothedF0 * (float) (k + 1);
            if (f_k >= nyquist || f_k > 20000.0f)
                continue;

            // Goertzel: measure energy at f_k from the sidechain block.
            const float mag = goertzelMagnitude (scMono, numSamples, f_k);
            const float db  = juce::Decibels::gainToDecibels (mag, -90.0f);

            // Envelope follower.
            float& env = harmEnv[k];
            env = (db > env) ? atkCoeff * env + (1.0f - atkCoeff) * db
                             : relCoeff * env + (1.0f - relCoeff) * db;

            // Notch depth.
            const float factor     = voiced * juce::jlimit (0.0f, 1.0f,
                                         (env - threshold_dBFS) / kDepthNormRange);
            const float depth_dB   = maxDepth_dB * factor;
            const float gainFactor = juce::Decibels::decibelsToGain (depth_dB);

            localCoeffs[k] = makePeakCoeffs (f_k, (float) sampleRate, kNotchQ, gainFactor);
        }

        //── Commit coefficients (audio thread owns filterCoeffs, no lock) ────
        filterCoeffs = localCoeffs;

        //── Snapshot for display (brief lock) ────────────────────────────────
        {
            const juce::SpinLock::ScopedLockType lock (displayLock);
            displayCoeffs = localCoeffs;
        }

        //── Apply filters to main bus ─────────────────────────────────────────
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data = mainBus.getWritePointer (ch);
            for (int k = 0; k < harmonicCount; ++k)
            {
                const auto& c = filterCoeffs[k];
                if (! c.active) continue;
                applyBiquad (data, numSamples, filterState[k][ch], c);
            }
        }
    }

    //==========================================================================
    /**
     * Writes the composite magnitude response (dB) of all active notch filters
     * into output[0..numBins-1]. Safe to call from the message thread.
     */
    void getEQMagnitudes (float* output, int numBins, double sr) const
    {
        // Snapshot under a brief lock, then compute outside it.
        // This prevents the audio thread from spin-waiting while we do trig.
        std::array<BiquadCoeffs, kMaxHarmonics> snap;
        {
            const juce::SpinLock::ScopedLockType lock (displayLock);
            snap = displayCoeffs;
        }

        juce::FloatVectorOperations::fill (output, 0.0f, numBins);

        for (int k = 0; k < kMaxHarmonics; ++k)
        {
            const auto& c = snap[k];
            if (! c.active) continue;

            const double b0 = c.b0, b1 = c.b1, b2 = c.b2;
            const double a1 = c.a1, a2 = c.a2;

            for (int bin = 0; bin < numBins; ++bin)
            {
                const double freq  = (double) bin / (double) (numBins * 2) * sr;
                const double omega = juce::MathConstants<double>::twoPi * freq / sr;
                const double cw    = std::cos (omega),       sw  = std::sin (omega);
                const double c2w   = std::cos (2.0 * omega), s2w = std::sin (2.0 * omega);

                const double nR = b0 + b1 * cw + b2 * c2w;
                const double nI =    - b1 * sw - b2 * s2w;
                const double dR = 1.0 + a1 * cw + a2 * c2w;
                const double dI =      - a1 * sw - a2 * s2w;

                const double magSq = (nR * nR + nI * nI) / (dR * dR + dI * dI + 1e-30);
                output[bin] += (float) (10.0 * std::log10 (std::max (magSq, 1e-30)));
            }
        }
    }

private:
    //==========================================================================
    // Audio EQ Cookbook: peaking EQ, normalised so a0 = 1.
    // gainFactor is linear amplitude gain (< 1 = cut, > 1 = boost).
    static BiquadCoeffs makePeakCoeffs (float freq, float sr, float Q, float gainFactor) noexcept
    {
        const float A     = std::sqrt (std::max (gainFactor, 0.0f));
        const float w0    = juce::MathConstants<float>::twoPi * freq / sr;
        const float alpha = std::sin (w0) / (2.0f * Q);
        const float a0inv = 1.0f / (1.0f + alpha / A);
        const float cosW  = std::cos (w0);

        BiquadCoeffs c;
        c.b0     = (1.0f + alpha * A) * a0inv;
        c.b1     = (-2.0f * cosW)     * a0inv;
        c.b2     = (1.0f - alpha * A) * a0inv;
        c.a1     = (-2.0f * cosW)     * a0inv;
        c.a2     = (1.0f - alpha / A) * a0inv;
        c.active = (gainFactor < 0.9999f);
        return c;
    }

    //==========================================================================
    // Direct-form II transposed biquad.
    static void applyBiquad (float* data, int numSamples,
                              BiquadState& s, const BiquadCoeffs& c) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = data[i];
            const float y = c.b0 * x + s.v1;
            s.v1 = c.b1 * x - c.a1 * y + s.v2;
            s.v2 = c.b2 * x - c.a2 * y;
            data[i] = y;
        }
    }

    //==========================================================================
    // YIN: linearise ring buffer, compute CMNDF, find minimum below threshold.
    void runYIN (float& rawF0, float& aperiodicity)
    {
        // Reconstruct a linear window from the ring buffer.
        const int tail = kYinWindowSize - yinWritePos;
        std::copy (yinBuf.begin() + yinWritePos, yinBuf.end(),                yinScratch.begin());
        std::copy (yinBuf.begin(),               yinBuf.begin() + yinWritePos, yinScratch.begin() + tail);

        const float* w      = yinScratch.data();
        constexpr int halfW = kYinWindowSize / 2;   // 1024

        const int tauMin = juce::jmax (1, (int) (sampleRate / kMaxF0Hz));   // ≈ 29 @ 44.1 kHz
        const int tauMax = juce::jmin (halfW - 1, (int) (sampleRate / kMinF0Hz)); // ≈ 882

        // yinCmndf is a pre-allocated member — no stack allocation, no zero-init cost.
        float cumSum = 0.0f;

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float d = 0.0f;
            for (int t = 0; t < halfW; ++t)
            {
                const float diff = w[t] - w[t + tau];
                d += diff * diff;
            }
            cumSum += d;
            yinCmndf[tau] = (cumSum > 0.0f) ? d * (float) tau / cumSum : 1.0f;
        }

        // First τ below threshold, refined to the local minimum of that dip.
        int bestTau = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (yinCmndf[tau] < kYinThreshold)
            {
                while (tau + 1 <= tauMax && yinCmndf[tau + 1] < yinCmndf[tau])
                    ++tau;
                bestTau = tau;
                break;
            }
        }

        if (bestTau < 0) { rawF0 = 0.0f; aperiodicity = 1.0f; return; }

        // Parabolic interpolation for sub-sample accuracy.
        float tauFrac = (float) bestTau;
        if (bestTau > tauMin && bestTau < tauMax)
        {
            const float y0 = yinCmndf[bestTau - 1];
            const float y1 = yinCmndf[bestTau];
            const float y2 = yinCmndf[bestTau + 1];
            const float denom = 2.0f * (y0 - 2.0f * y1 + y2);
            if (std::abs (denom) > 1e-10f)
                tauFrac += (y0 - y2) / denom;
        }

        rawF0        = (float) sampleRate / tauFrac;
        aperiodicity = yinCmndf[bestTau];
    }

    //==========================================================================
    // Goertzel: O(N) DFT magnitude at a single arbitrary frequency.
    float goertzelMagnitude (const float* buf, int N, float freq) const noexcept
    {
        const float k     = freq * (float) N / (float) sampleRate;
        const float omega = juce::MathConstants<float>::twoPi * k / (float) N;
        const float coeff = 2.0f * std::cos (omega);
        float s1 = 0.0f, s2 = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float s0 = buf[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        return std::sqrt (std::max (0.0f, power)) / ((float) N * 0.5f);
    }

    //==========================================================================
    double sampleRate    = 44100.0;
    float  f0SmoothCoeff = 0.0f;
    float  smoothedF0    = 0.0f;

    std::array<float, kYinWindowSize>      yinBuf     {};
    std::array<float, kYinWindowSize>      yinScratch {};
    std::array<float, kYinWindowSize / 2>  yinCmndf   {};   // pre-allocated; avoids per-call stack alloc
    int   yinWritePos        = 0;
    bool  yinBufFull         = false;
    int   yinSamplesSinceRun = 0;
    float lastAperiodicity   = 1.0f;   // 1.0 = unvoiced; persists between YIN runs

    std::array<float, kMaxHarmonics> harmEnv {};

    std::array<std::array<BiquadState,  kMaxChannels>, kMaxHarmonics> filterState  {};
    std::array<BiquadCoeffs,                           kMaxHarmonics> filterCoeffs {};

    mutable juce::SpinLock                             displayLock;
    mutable std::array<BiquadCoeffs, kMaxHarmonics>    displayCoeffs {};

    AutoSCProcessor (const AutoSCProcessor&) = delete;
    AutoSCProcessor& operator= (const AutoSCProcessor&) = delete;
};
