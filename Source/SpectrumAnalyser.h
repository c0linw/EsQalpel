#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

/**
 * Lock-free single-producer / single-consumer FFT helper.
 *
 * Audio thread calls pushSamples().
 * Message thread calls processIfAvailable() then reads getMagnitudes().
 *
 * Constants:
 *   fftOrder = 11  →  fftSize = 2048  →  numBins = 1024
 *   fifoSize = 4096  (2× fftSize to handle large host block sizes)
 */
class SpectrumAnalyser
{
public:
    static constexpr int   fftOrder = 11;
    static constexpr int   fftSize  = 1 << fftOrder;   // 2048
    static constexpr int   numBins  = fftSize / 2;      // 1024
    static constexpr int   fifoSize = fftSize * 2;      // 4096
    static constexpr float kFloorDB = -90.0f;

    SpectrumAnalyser()
    {
        const auto twoPi = juce::MathConstants<double>::twoPi;
        for (int i = 0; i < fftSize; ++i)
            hannWindow[i] = 0.5f - 0.5f * (float) std::cos (twoPi * (double) i
                                                              / (double) (fftSize - 1));
        reset();
    }

    //===========================================================================
    // Audio thread — never blocks.
    void pushSamples (const float* data, int numSamples) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (numSamples, s1, n1, s2, n2);
        if (n1 > 0) juce::FloatVectorOperations::copy (fifoBuffer.data() + s1, data,      n1);
        if (n2 > 0) juce::FloatVectorOperations::copy (fifoBuffer.data() + s2, data + n1, n2);
        fifo.finishedWrite (n1 + n2);
    }

    //===========================================================================
    // Message thread only. Returns true if a new FFT frame was computed.
    bool processIfAvailable()
    {
        if (fifo.getNumReady() < fftSize)
            return false;

        // Drain exactly fftSize samples from the FIFO into fftBuffer[0..fftSize-1].
        int s1, n1, s2, n2;
        fifo.prepareToRead (fftSize, s1, n1, s2, n2);
        if (n1 > 0) juce::FloatVectorOperations::copy (fftBuffer.data(),      fifoBuffer.data() + s1, n1);
        if (n2 > 0) juce::FloatVectorOperations::copy (fftBuffer.data() + n1, fifoBuffer.data() + s2, n2);
        fifo.finishedRead (n1 + n2);

        // Apply Hann window to the real samples.
        juce::FloatVectorOperations::multiply (fftBuffer.data(), hannWindow.data(), fftSize);

        // Zero the imaginary half (fftBuffer[fftSize .. 2*fftSize-1]).
        juce::FloatVectorOperations::fill (fftBuffer.data() + fftSize, 0.0f, fftSize);

        // In-place forward FFT.
        // Output layout: fftBuffer[2*k] = Re(bin k), fftBuffer[2*k+1] = Im(bin k).
        forwardFFT.performRealOnlyForwardTransform (fftBuffer.data(), true);

        // Convert to dBFS magnitudes.
        for (int i = 0; i < numBins; ++i)
        {
            const float re  = fftBuffer[2 * i];
            const float im  = fftBuffer[2 * i + 1];
            const float mag = std::sqrt (re * re + im * im) / (float) fftSize;
            magnitudes[i]   = juce::Decibels::gainToDecibels (mag, kFloorDB);
        }

        return true;
    }

    const std::array<float, numBins>& getMagnitudes() const noexcept { return magnitudes; }

    // Call from prepareToPlay() to reinitialise all state.
    void reset()
    {
        fifo.reset();
        fifoBuffer.fill (0.0f);
        fftBuffer.fill  (0.0f);
        magnitudes.fill (kFloorDB);
    }

private:
    juce::dsp::FFT                 forwardFFT { fftOrder };
    juce::AbstractFifo             fifo       { fifoSize };
    std::array<float, fifoSize>    fifoBuffer {};
    std::array<float, fftSize * 2> fftBuffer  {};   // 4096 floats: real input + workspace
    std::array<float, fftSize>     hannWindow {};
    std::array<float, numBins>     magnitudes {};
};
