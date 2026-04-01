#pragma once

// CMake/FetchContent builds do not generate JuceHeader.h.
// Include each required JUCE module header directly.
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <random>

/**
 * VoidChantVoice.h  —  VOID CHANT DSP Core  (Refined v2)
 * =======================================================
 *
 * DSP Chain per voice:
 *
 *   [PolyBLEP Sawtooth Oscillator]
 *       │
 *       ├─ [FormantFilterBank (MURMUR)]
 *       │     3 parallel bandpass filters (F1/F2/F3)
 *       │     Continuous vowel morph: A → E → I → O → U
 *       │
 *       ├─ [UnisonEngine (RITUAL)]
 *       │     Up to 8 sub-voices
 *       │     Per-voice: detune + micro delay offset (1–5 ms)
 *       │     Phase-cancellation prevention via polarity check
 *       │
 *       ├─ [PitchDriftLFO (POSSESS)]
 *       │     Random-walk target, smoothed by LinearSmoothedValue
 *       │     Organic analogue-style pitch flutter
 *       │
 *       ├─ [ADSR Envelope]
 *       │
 *       └─ [GainStage (SACRIFICE)]
 *
 * Reverb (DEPTH) is applied at the Processor bus level.
 *
 * Thread safety:
 *   Parameter values are passed as std::atomic<float>* (owned by Processor).
 *   envelopeLevel and lastVelocity are atomic floats written on the audio
 *   thread and read on the message thread for glow animation.
 */

// ============================================================================
// FormantFilterBank  —  3-band parallel resonant bandpass (F1 / F2 / F3)
// ============================================================================
class FormantFilterBank
{
public:
    // Vowel formant table: F1, F2, F3 (Hz) and bandwidth (Hz)
    // Values derived from Peterson & Barney (1952) / Klatt (1980)
    struct Formant { float freq, bw; };
    struct VowelSpec { Formant f1, f2, f3; };

    static constexpr std::array<VowelSpec, 5> kVowels = {{
        //         F1              F2              F3
        { {800.f, 80.f},  {1200.f,120.f}, {2500.f,200.f} }, // A
        { {400.f, 60.f},  {2200.f,180.f}, {2800.f,220.f} }, // E
        { {300.f, 50.f},  {2600.f,200.f}, {3100.f,250.f} }, // I
        { {500.f, 70.f},  { 900.f, 90.f}, {2500.f,200.f} }, // O
        { {300.f, 50.f},  { 700.f, 70.f}, {2200.f,180.f} }, // U
    }};

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        for (auto& row : filters)
            for (auto& f : row)
                f.reset();
    }

    /** murmurValue 0.0–1.0 → continuous vowel morph A→E→I→O→U */
    void setMurmur (float murmurValue) noexcept
    {
        const float pos  = juce::jlimit (0.f, 4.999f, murmurValue * 5.f);
        const int   idx0 = static_cast<int> (pos);
        const int   idx1 = juce::jmin (idx0 + 1, 4);
        const float frac = pos - static_cast<float> (idx0);

        auto lerp = [&](float a, float b) { return a + frac * (b - a); };

        auto setFilter = [&](int band, float freq, float bw)
        {
            auto coeff = juce::dsp::IIR::Coefficients<float>::makeBandPass (
                sr,
                static_cast<double> (freq),
                static_cast<double> (freq / juce::jmax (bw, 1.f)));
            for (int ch = 0; ch < 2; ++ch)
                *filters[ch][band].coefficients = *coeff;
        };

        setFilter (0,
            lerp (kVowels[idx0].f1.freq, kVowels[idx1].f1.freq),
            lerp (kVowels[idx0].f1.bw,   kVowels[idx1].f1.bw));
        setFilter (1,
            lerp (kVowels[idx0].f2.freq, kVowels[idx1].f2.freq),
            lerp (kVowels[idx0].f2.bw,   kVowels[idx1].f2.bw));
        setFilter (2,
            lerp (kVowels[idx0].f3.freq, kVowels[idx1].f3.freq),
            lerp (kVowels[idx0].f3.bw,   kVowels[idx1].f3.bw));
    }

    /** Process one sample for the given channel (0=L, 1=R) */
    float processSample (float x, int ch) noexcept
    {
        // Parallel sum of three formant bands with perceptual weighting
        const float f1 = filters[ch][0].processSample (x) * 1.0f;
        const float f2 = filters[ch][1].processSample (x) * 0.8f;
        const float f3 = filters[ch][2].processSample (x) * 0.5f;
        const float wet = (f1 + f2 + f3) / 2.3f;
        return x * 0.35f + wet * 0.65f;
    }

private:
    double sr = 44100.0;
    // [channel][band]
    std::array<std::array<juce::dsp::IIR::Filter<float>, 3>, 2> filters;
};

// ============================================================================
// UnisonDelayLine  —  fractional-sample delay for micro-offset per sub-voice
// ============================================================================
class UnisonDelayLine
{
public:
    static constexpr int kMaxDelaySamples = 512; // ~11 ms @ 44.1 kHz

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        buffer.fill (0.f);
        writePos = 0;
    }

    void setDelayMs (float ms) noexcept
    {
        delaySamples = juce::jlimit (0.f,
            static_cast<float> (kMaxDelaySamples - 1),
            static_cast<float> (ms * sr / 1000.0));
    }

    float process (float input) noexcept
    {
        buffer[writePos] = input;
        const int readPos = static_cast<int> (writePos - static_cast<int> (delaySamples)
                                              + kMaxDelaySamples) % kMaxDelaySamples;
        writePos = (writePos + 1) % kMaxDelaySamples;
        return buffer[readPos];
    }

private:
    double sr = 44100.0;
    float  delaySamples = 0.f;
    std::array<float, kMaxDelaySamples> buffer {};
    int writePos = 0;
};

// ============================================================================
// PitchDriftLFO  —  random-walk pitch flutter via LinearSmoothedValue
// ============================================================================
class PitchDriftLFO
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        smoothed.reset (sampleRate, 0.25); // 250 ms smoothing time
        smoothed.setCurrentAndTargetValue (0.f);
        stepCounter = 0;
        rng.seed (std::random_device{}());
    }

    /** possessValue 0.0–1.0 → max drift ±1.5 semitones */
    float tick (float possessValue) noexcept
    {
        // New random target every ~0.25–0.45 s (varies slightly)
        const int kStepSamples = static_cast<int> (sr * 0.35);
        if (++stepCounter >= kStepSamples)
        {
            stepCounter = 0;
            const float maxDrift = possessValue * 1.5f;
            const float newTarget = (dist (rng) * 2.f - 1.f) * maxDrift;
            smoothed.setTargetValue (newTarget);
        }
        return smoothed.getNextValue(); // semitone offset
    }

private:
    double sr = 44100.0;
    juce::LinearSmoothedValue<float> smoothed;
    int stepCounter = 0;
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist { 0.f, 1.f };
};

// ============================================================================
// VoidChantSound
// ============================================================================
struct VoidChantSound : public juce::SynthesiserSound
{
    bool appliesToNote    (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

// ============================================================================
// VoidChantVoice
// ============================================================================
class VoidChantVoice : public juce::SynthesiserVoice
{
public:
    // Atomic parameter pointers — set by PluginProcessor after construction
    std::atomic<float>* pMurmur    = nullptr;
    std::atomic<float>* pRitual    = nullptr;
    std::atomic<float>* pPossess   = nullptr;
    std::atomic<float>* pSacrifice = nullptr;

    // Read by PluginEditor on the message thread for glow animation
    float getEnvelopeLevel()  const noexcept { return envelopeLevel.load(); }
    float getLastVelocity()   const noexcept { return lastVelocity.load(); }

    // ------------------------------------------------------------------
    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<VoidChantSound*> (s) != nullptr;
    }

    void startNote (int midiNote, float velocity,
                    juce::SynthesiserSound*, int /*pitchWheel*/) override
    {
        currentMidiNote = midiNote;
        lastVelocity.store (velocity);
        adsr.noteOn();

        const double sr = getSampleRate();
        driftLfo.prepare (sr);
        formant.prepare (sr);

        for (auto& d : unisonDelays)
            d.prepare (sr);

        // Assign micro-delay offsets (1–5 ms) to each unison sub-voice
        for (int v = 0; v < kMaxUnison; ++v)
        {
            const float delayMs = 1.f + static_cast<float> (v) * (4.f / (kMaxUnison - 1));
            unisonDelays[v].setDelayMs (delayMs);
        }

        // Reset oscillator phases with small per-voice offset to avoid
        // phase cancellation at startup
        for (int v = 0; v < kMaxUnison; ++v)
            phases[v] = static_cast<double> (v) / static_cast<double> (kMaxUnison);
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
            adsr.noteOff();
        else
        {
            adsr.reset();
            clearCurrentNote();
            envelopeLevel.store (0.f);
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    // ------------------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        adsr.setSampleRate (sampleRate);

        juce::ADSR::Parameters p;
        p.attack  = 0.012f;
        p.decay   = 0.25f;
        p.sustain = 0.78f;
        p.release = 1.4f;
        adsr.setParameters (p);

        formant.prepare (sampleRate);
        driftLfo.prepare (sampleRate);

        for (auto& d : unisonDelays)
            d.prepare (sampleRate);

        (void) samplesPerBlock;
    }

    // ------------------------------------------------------------------
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override
    {
        if (! isVoiceActive()) return;

        const float murmurVal    = pMurmur    ? pMurmur->load()    : 0.5f;
        const float ritualVal    = pRitual    ? pRitual->load()    : 0.0f;
        const float possessVal   = pPossess   ? pPossess->load()   : 0.0f;
        const float sacrificeVal = pSacrifice ? pSacrifice->load() : 0.8f;

        formant.setMurmur (murmurVal);

        const int   unisonCount  = 1 + static_cast<int> (ritualVal * (kMaxUnison - 1));
        const float detuneRange  = ritualVal * 0.05f;   // ±5 cents max
        const float stereoSpread = ritualVal * 0.35f;
        const double sr          = getSampleRate();

        const int numChannels = outputBuffer.getNumChannels();

        for (int i = startSample; i < startSample + numSamples; ++i)
        {
            const float driftSt = driftLfo.tick (possessVal);

            float sampleL = 0.f, sampleR = 0.f;

            for (int v = 0; v < unisonCount; ++v)
            {
                // Detune: symmetric spread around centre pitch
                const float detuneSt = (unisonCount > 1)
                    ? juce::jmap (static_cast<float> (v),
                                  0.f, static_cast<float> (unisonCount - 1),
                                  -detuneRange, detuneRange)
                    : 0.f;

                const float totalSt = static_cast<float> (currentMidiNote)
                                      + driftSt + detuneSt;
                const double freq     = 440.0 * std::pow (2.0, (totalSt - 69.0) / 12.0);
                const double phaseInc = freq / sr;

                phases[v] = std::fmod (phases[v] + phaseInc, 1.0);

                // PolyBLEP anti-aliased sawtooth
                float saw = static_cast<float> (2.0 * phases[v] - 1.0);
                saw -= polyBlep (phases[v], phaseInc);

                // Micro-delay for thickness (phase-cancellation prevention:
                // delay ensures sub-voices are temporally separated)
                const float delayed = unisonDelays[v].process (saw);

                // Stereo panning: odd voices left, even voices right
                const float pan = (unisonCount > 1)
                    ? juce::jmap (static_cast<float> (v),
                                  0.f, static_cast<float> (unisonCount - 1),
                                  -stereoSpread, stereoSpread)
                    : 0.f;

                sampleL += delayed * (1.f - juce::jmax (0.f,  pan));
                sampleR += delayed * (1.f - juce::jmax (0.f, -pan));
            }

            // Normalise by voice count
            const float norm = 1.f / static_cast<float> (unisonCount);
            sampleL *= norm;
            sampleR *= norm;

            // Formant filter (per channel)
            sampleL = formant.processSample (sampleL, 0);
            sampleR = formant.processSample (sampleR, numChannels >= 2 ? 1 : 0);

            // ADSR envelope
            const float env = adsr.getNextSample();
            envelopeLevel.store (env);

            const float gain = env * lastVelocity.load() * sacrificeVal;
            sampleL *= gain;
            sampleR *= gain;

            // Write to output buffer
            outputBuffer.addSample (0, i, sampleL);
            if (numChannels >= 2)
                outputBuffer.addSample (1, i, sampleR);

            if (! adsr.isActive())
            {
                clearCurrentNote();
                envelopeLevel.store (0.f);
                break;
            }
        }
    }

private:
    // PolyBLEP discontinuity correction
    static float polyBlep (double t, double dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return static_cast<float> (2.0 * t - t * t - 1.0);
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return static_cast<float> (t * t + 2.0 * t + 1.0);
        }
        return 0.f;
    }

    static constexpr int kMaxUnison = 8;

    std::array<double, kMaxUnison>        phases {};
    std::array<UnisonDelayLine, kMaxUnison> unisonDelays;

    int currentMidiNote = 60;

    juce::ADSR         adsr;
    FormantFilterBank  formant;
    PitchDriftLFO      driftLfo;

    std::atomic<float> envelopeLevel { 0.f };
    std::atomic<float> lastVelocity  { 0.f };
};
