#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// ============================================================================
// VoidKnobLookAndFeel
// ============================================================================
/**
 * Custom LookAndFeel for VOID CHANT rotary sliders.
 *
 * Paint layers (drawRotarySlider):
 *   0. Outer glow ring    — radial gradient, radius/opacity driven by sliderPos
 *   1. Stone sphere image — juce::ImageCache, clipped circle, rotated by angle
 *   2. Arc meter          — two-pass stroke (glow pass + core pass)
 *                           juce::DropShadow applied to the arc path
 *
 * The knob image is retrieved via juce::ImageCache::getFromMemory() which
 * caches decoded images globally, eliminating redundant PNG decoding across
 * the 5 knob instances.
 */
class VoidKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VoidKnobLookAndFeel();

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float startAngle, float endAngle,
                           juce::Slider& slider) override;

    void drawLabel (juce::Graphics& g, juce::Label& label) override;

private:
    // Cached knob image (shared across all instances via ImageCache)
    juce::Image knobImage;

    // Neon blue palette
    static constexpr juce::uint32 kGlowInner = 0xFF00D4FF;
    static constexpr juce::uint32 kGlowOuter = 0x0000A0FF;
    static constexpr juce::uint32 kArcCore   = 0xFF00C8FF;
    static constexpr juce::uint32 kArcGlow   = 0x8800A8E0;
};

// ============================================================================
// MagicCircleComponent
// ============================================================================
/**
 * Transparent overlay that renders the magic circle glow.
 *
 * The glow is driven by glowLevel (0–1), which encodes both the ADSR envelope
 * state and the MIDI velocity of the most recent noteOn.
 *
 * Animation model (called from PluginEditor::timerCallback at 60 fps):
 *   - Attack  : instant snap-up when targetGlow > currentGlow
 *   - Decay   : exponential smoothing  currentGlow += (target - current) * kDecay
 *               kDecay = 0.04 → ~95% decay in ~1.2 s at 60 fps
 *
 * Two concentric radial gradients are painted:
 *   1. Outer diffuse halo  (radius = 28% of component width)
 *   2. Inner bright core   (radius = 17% of component width)
 */
class MagicCircleComponent : public juce::Component
{
public:
    MagicCircleComponent() { setInterceptsMouseClicks (false, false); }

    void setGlowLevel (float level) noexcept
    {
        targetGlow = juce::jlimit (0.f, 1.f, level);
    }

    void tick() noexcept
    {
        constexpr float kDecay = 0.04f;
        if (targetGlow > currentGlow)
            currentGlow = targetGlow;
        else
            currentGlow += (targetGlow - currentGlow) * kDecay;

        repaint();
    }

    void paint (juce::Graphics& g) override;

private:
    float currentGlow = 0.f;
    float targetGlow  = 0.f;
};

// ============================================================================
// PluginEditor
// ============================================================================
class VoidChantAudioProcessorEditor
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit VoidChantAudioProcessorEditor (VoidChantAudioProcessor&);
    ~VoidChantAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    // -----------------------------------------------------------------------
    void timerCallback() override;

    // -----------------------------------------------------------------------
    VoidChantAudioProcessor& processor;

    juce::Image backgroundImage;
    MagicCircleComponent magicCircle;

    VoidKnobLookAndFeel knobLAF; // single shared instance

    // Knobs
    juce::Slider murmurKnob, ritualKnob, possessKnob, sacrificeKnob, depthKnob;

    // Labels: name + description (below knob)
    juce::Label murmurLabel,    ritualLabel,    possessLabel,
                sacrificeLabel, depthLabel;

    // Percentage labels (above knob)
    juce::Label murmurPct,    ritualPct,    possessPct,
                sacrificePct, depthPct;

    // APVTS attachments
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> murmurAtt, ritualAtt, possessAtt,
                                sacrificeAtt, depthAtt;

    // Canvas dimensions (fixed, matching background image)
    static constexpr int   kWidth    = 1109;
    static constexpr int   kHeight   = 960;
    static constexpr int   kKnobSize = 110;
    static constexpr float kKnobY    = 0.875f;
    static constexpr float kXPos[5]  = { 0.15f, 0.325f, 0.50f, 0.675f, 0.85f };

    void setupKnob (juce::Slider& knob,
                    juce::Label&  nameLabel,
                    juce::Label&  pctLabel,
                    const juce::String& paramId,
                    const juce::String& displayName,
                    const juce::String& description,
                    std::unique_ptr<Attachment>& attachment);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoidChantAudioProcessorEditor)
};
