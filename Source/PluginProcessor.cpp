#include "PluginProcessor.h"
#include "PluginEditor.h"

// ---------------------------------------------------------------------------
// Parameter ID constants
// ---------------------------------------------------------------------------
const juce::String VoidChantAudioProcessor::kParamMurmur    = "murmur";
const juce::String VoidChantAudioProcessor::kParamRitual    = "ritual";
const juce::String VoidChantAudioProcessor::kParamPossess   = "possess";
const juce::String VoidChantAudioProcessor::kParamSacrifice = "sacrifice";
const juce::String VoidChantAudioProcessor::kParamDepth     = "depth";

// ---------------------------------------------------------------------------
// Parameter layout
// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout
VoidChantAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // MURMUR — vowel morph (A=0 → U=1)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kParamMurmur, 1 }, "Murmur",
        juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("vowel")
            .withStringFromValueFunction ([](float v, int)
            {
                static const char* names[] = { "A", "E", "I", "O", "U" };
                return juce::String (names[juce::jlimit (0, 4,
                    static_cast<int> (v * 5.f))]);
            })));

    // RITUAL — unison density (0=mono, 1=8-voice)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kParamRitual, 1 }, "Ritual",
        juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // POSSESS — pitch drift intensity
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kParamPossess, 1 }, "Possess",
        juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // SACRIFICE — master volume
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kParamSacrifice, 1 }, "Sacrifice",
        juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.8f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // DEPTH — reverb wet mix
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kParamDepth, 1 }, "Depth",
        juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.3f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    return { params.begin(), params.end() };
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
VoidChantAudioProcessor::VoidChantAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VoidChantState", createParameterLayout())
{
    rawMurmur    = apvts.getRawParameterValue (kParamMurmur);
    rawRitual    = apvts.getRawParameterValue (kParamRitual);
    rawPossess   = apvts.getRawParameterValue (kParamPossess);
    rawSacrifice = apvts.getRawParameterValue (kParamSacrifice);
    rawDepth     = apvts.getRawParameterValue (kParamDepth);

    synth.addSound (new VoidChantSound());

    for (int i = 0; i < kMaxVoices; ++i)
    {
        auto* voice      = new VoidChantVoice();
        voice->pMurmur    = rawMurmur;
        voice->pRitual    = rawRitual;
        voice->pPossess   = rawPossess;
        voice->pSacrifice = rawSacrifice;
        synth.addVoice (voice);
    }
}

// ---------------------------------------------------------------------------
// prepareToPlay
// ---------------------------------------------------------------------------
void VoidChantAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<VoidChantVoice*> (synth.getVoice (i)))
            v->prepareToPlay (sampleRate, samplesPerBlock);

    // Cathedral reverb preset
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 2;
    reverb.prepare (spec);

    reverbParams.roomSize   = 0.97f;   // very large room
    reverbParams.damping    = 0.25f;   // bright, airy tail
    reverbParams.width      = 1.0f;    // full stereo
    reverbParams.freezeMode = 0.0f;
    reverbParams.wetLevel   = 0.3f;
    reverbParams.dryLevel   = 0.85f;
    reverb.setParameters (reverbParams);
}

void VoidChantAudioProcessor::releaseResources() {}

// ---------------------------------------------------------------------------
// processBlock
// ---------------------------------------------------------------------------
void VoidChantAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // Update DEPTH reverb mix dynamically
    const float depthVal      = rawDepth ? rawDepth->load() : 0.3f;
    reverbParams.wetLevel     = depthVal;
    reverbParams.dryLevel     = 1.f - depthVal * 0.45f;
    reverb.setParameters (reverbParams);

    juce::dsp::AudioBlock<float>              block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx   (block);
    reverb.process (ctx);
}

// ---------------------------------------------------------------------------
// getGlowLevel  —  velocity-weighted envelope sum
// ---------------------------------------------------------------------------
float VoidChantAudioProcessor::getGlowLevel() const noexcept
{
    float sum = 0.f;
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* v = dynamic_cast<const VoidChantVoice*> (synth.getVoice (i)))
        {
            // Weight by velocity so forte notes glow brighter
            sum += v->getEnvelopeLevel() * v->getLastVelocity();
        }
    }
    return juce::jlimit (0.f, 1.f, sum / static_cast<float> (kMaxVoices));
}

// ---------------------------------------------------------------------------
// State persistence
// ---------------------------------------------------------------------------
void VoidChantAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void VoidChantAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// ---------------------------------------------------------------------------
// Plugin entry point
// ---------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoidChantAudioProcessor();
}
