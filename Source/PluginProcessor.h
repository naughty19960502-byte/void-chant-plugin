#pragma once

// CMake/FetchContent builds do not generate JuceHeader.h.
// Include each required JUCE module header directly.
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "VoidChantVoice.h"

/**
 * PluginProcessor.h  —  VOID CHANT AudioProcessor  (Refined v2)
 * ==============================================================
 *
 * Responsibilities:
 *   - Owns AudioProcessorValueTreeState (APVTS) with 5 parameters
 *   - Manages a juce::Synthesiser with up to 8 VoidChantVoice instances
 *   - Applies juce::dsp::Reverb (DEPTH) at the output bus level
 *   - Exposes getGlowLevel() — velocity-weighted envelope sum for glow animation
 */
class VoidChantAudioProcessor : public juce::AudioProcessor
{
public:
    // -----------------------------------------------------------------------
    // Parameter IDs
    // -----------------------------------------------------------------------
    static const juce::String kParamMurmur;
    static const juce::String kParamRitual;
    static const juce::String kParamPossess;
    static const juce::String kParamSacrifice;
    static const juce::String kParamDepth;

    static constexpr int kMaxVoices = 8;

    // -----------------------------------------------------------------------
    VoidChantAudioProcessor();
    ~VoidChantAudioProcessor() override = default;

    // -----------------------------------------------------------------------
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName()          const override { return "VOID CHANT"; }
    bool   acceptsMidi()                  const override { return true; }
    bool   producesMidi()                 const override { return false; }
    bool   isMidiEffect()                 const override { return false; }
    double getTailLengthSeconds()         const override { return 2.5; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // -----------------------------------------------------------------------
    // Public accessors for PluginEditor
    // -----------------------------------------------------------------------
    AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    /**
     * Returns the velocity-weighted envelope sum across all active voices.
     * Range: 0.0–1.0.  Read on the message thread (atomic).
     *
     * Formula:  sum(envelope_i * velocity_i) / kMaxVoices
     *
     * This ensures that a fortissimo note produces a brighter glow than a
     * pianissimo note, matching the MIDI Velocity → Opacity requirement.
     */
    float getGlowLevel() const noexcept;

    // -----------------------------------------------------------------------
    static AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    AudioProcessorValueTreeState apvts;
    juce::Synthesiser            synth;

    // Cathedral reverb (DEPTH)
    juce::dsp::Reverb             reverb;
    juce::dsp::Reverb::Parameters reverbParams;

    // Raw parameter pointers (atomic floats, obtained once in constructor)
    std::atomic<float>* rawMurmur    = nullptr;
    std::atomic<float>* rawRitual    = nullptr;
    std::atomic<float>* rawPossess   = nullptr;
    std::atomic<float>* rawSacrifice = nullptr;
    std::atomic<float>* rawDepth     = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoidChantAudioProcessor)
};
