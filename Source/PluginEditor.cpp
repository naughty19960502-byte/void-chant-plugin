#include "PluginEditor.h"

// ============================================================================
// VoidKnobLookAndFeel
// ============================================================================

VoidKnobLookAndFeel::VoidKnobLookAndFeel()
{
    // Load knob image via ImageCache — decoded once, shared globally
    knobImage = juce::ImageCache::getFromMemory (
        BinaryData::void_chant_knob_png,
        BinaryData::void_chant_knob_pngSize);
}

// ----------------------------------------------------------------------------
void VoidKnobLookAndFeel::drawRotarySlider (
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos,
    float startAngle, float endAngle,
    juce::Slider& /*slider*/)
{
    const float cx    = static_cast<float> (x) + width  * 0.5f;
    const float cy    = static_cast<float> (y) + height * 0.5f;
    const float r     = juce::jmin (width, height) * 0.5f - 2.f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // -----------------------------------------------------------------------
    // Layer 0: outer glow ring (radial gradient)
    // -----------------------------------------------------------------------
    {
        const float glowAlpha = 0.25f + sliderPos * 0.75f;
        const float glowR     = r * (1.05f + sliderPos * 0.12f);

        juce::ColourGradient grad (
            juce::Colour (kGlowInner).withAlpha (glowAlpha),
            cx, cy,
            juce::Colour (kGlowOuter).withAlpha (0.f),
            cx + glowR, cy,
            true /* radial */);

        g.setGradientFill (grad);
        g.fillEllipse (cx - glowR, cy - glowR, glowR * 2.f, glowR * 2.f);
    }

    // -----------------------------------------------------------------------
    // Layer 1: stone sphere — ImageCache + clip + rotate
    // -----------------------------------------------------------------------
    {
        const float sphereR = r * 0.60f;

        juce::Graphics::ScopedSaveState saved (g);

        // Circular clip region
        juce::Path clip;
        clip.addEllipse (cx - sphereR, cy - sphereR, sphereR * 2.f, sphereR * 2.f);
        g.reduceClipRegion (clip);

        // Rotate the image around the knob centre
        g.addTransform (juce::AffineTransform::rotation (angle, cx, cy));

        if (knobImage.isValid())
        {
            const float imgSize = sphereR * 2.f;
            g.drawImage (knobImage,
                cx - sphereR, cy - sphereR, imgSize, imgSize,
                0, 0, knobImage.getWidth(), knobImage.getHeight());
        }
        else
        {
            // Fallback: dark stone-like circle
            g.setColour (juce::Colour (0xFF1A1A2E));
            g.fillEllipse (cx - sphereR, cy - sphereR, sphereR * 2.f, sphereR * 2.f);
        }
    }

    // -----------------------------------------------------------------------
    // Layer 2: arc meter — DropShadow + two-pass stroke
    // -----------------------------------------------------------------------
    {
        const float arcAlpha = 0.45f + sliderPos * 0.55f;
        const float arcWidth = 2.5f + sliderPos * 3.5f;

        juce::Path arc;
        arc.addCentredArc (cx, cy, r * 0.95f, r * 0.95f,
                           0.f, startAngle, angle, true);

        // DropShadow to simulate neon glow on dark stone surface
        juce::DropShadow shadow;
        shadow.colour = juce::Colour (kArcGlow).withAlpha (arcAlpha * 0.8f);
        shadow.radius = static_cast<int> (6.f + sliderPos * 8.f);
        shadow.offset = { 0, 0 };

        // Render shadow onto a temporary image, then composite
        {
            const juce::Rectangle<float> arcBounds =
                arc.getBounds().expanded (static_cast<float> (shadow.radius) + 2.f);
            const juce::Rectangle<int> arcBoundsInt = arcBounds.toNearestInt();

            if (arcBoundsInt.getWidth() > 0 && arcBoundsInt.getHeight() > 0)
            {
                juce::Image shadowImg (juce::Image::ARGB,
                    arcBoundsInt.getWidth(), arcBoundsInt.getHeight(), true);
                {
                    juce::Graphics sg (shadowImg);
                    sg.setColour (juce::Colours::white);
                    sg.strokePath (arc,
                        juce::PathStrokeType (arcWidth + 2.f,
                            juce::PathStrokeType::curved,
                            juce::PathStrokeType::rounded),
                        juce::AffineTransform::translation (
                            -arcBoundsInt.getX(), -arcBoundsInt.getY()));
                }
                shadow.drawForImage (g, shadowImg,
                    juce::AffineTransform::translation (
                        static_cast<float> (arcBoundsInt.getX()),
                        static_cast<float> (arcBoundsInt.getY())));
            }
        }

        // Core arc stroke
        g.setColour (juce::Colour (kArcCore).withAlpha (arcAlpha));
        g.strokePath (arc,
            juce::PathStrokeType (arcWidth,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));

        // Bright highlight pass (thinner, fully opaque)
        g.setColour (juce::Colours::white.withAlpha (arcAlpha * 0.4f));
        g.strokePath (arc,
            juce::PathStrokeType (arcWidth * 0.35f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
    }
}

// ----------------------------------------------------------------------------
void VoidKnobLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    const juce::Colour textCol = juce::Colour (0xFFA0E8FF);
    const juce::Colour glowCol = juce::Colour (0xFF00C8FF);
    const juce::Rectangle<int> bounds = label.getLocalBounds();
    const juce::String text = label.getText();
    const juce::Font   font = label.getFont();

    // Glow pass
    g.setColour (glowCol.withAlpha (0.4f));
    g.setFont (font);
    g.drawText (text, bounds.expanded (1), label.getJustificationType(), false);

    // Core text
    g.setColour (textCol);
    g.drawText (text, bounds, label.getJustificationType(), false);
}

// ============================================================================
// MagicCircleComponent::paint
// ============================================================================
void MagicCircleComponent::paint (juce::Graphics& g)
{
    if (currentGlow < 0.004f) return;

    const float w  = static_cast<float> (getWidth());
    const float h  = static_cast<float> (getHeight());
    const float cx = w * 0.5f;
    const float cy = h * 0.45f;

    // Outer diffuse halo
    {
        const float r = w * 0.28f;
        juce::ColourGradient grad (
            juce::Colour (80, 120, 255).withAlpha (currentGlow * 0.50f),
            cx, cy,
            juce::Colour (40, 60, 200).withAlpha (0.f),
            cx + r * 1.5f, cy,
            true);
        g.setGradientFill (grad);
        g.fillEllipse (cx - r * 1.5f, cy - r * 1.5f, r * 3.f, r * 3.f);
    }

    // Inner bright core
    {
        const float r = w * 0.17f;
        juce::ColourGradient grad (
            juce::Colour (160, 210, 255).withAlpha (currentGlow * 0.38f),
            cx, cy,
            juce::Colour (80, 140, 255).withAlpha (0.f),
            cx + r, cy,
            true);
        g.setGradientFill (grad);
        g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
    }

    // Specular ring at the circle edge (thin bright ring)
    {
        const float r = w * 0.265f;
        juce::Path ring;
        ring.addEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
        g.setColour (juce::Colour (120, 180, 255).withAlpha (currentGlow * 0.25f));
        g.strokePath (ring, juce::PathStrokeType (1.5f));
    }
}

// ============================================================================
// PluginEditor
// ============================================================================

VoidChantAudioProcessorEditor::VoidChantAudioProcessorEditor (
    VoidChantAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (kWidth, kHeight);
    setResizable (false, false);

    // Background image via ImageCache
    backgroundImage = juce::ImageCache::getFromMemory (
        BinaryData::void_chant_bg_png,
        BinaryData::void_chant_bg_pngSize);

    addAndMakeVisible (magicCircle);

    setupKnob (murmurKnob,    murmurLabel,    murmurPct,
               VoidChantAudioProcessor::kParamMurmur,
               "MURMUR",    "Whisper Depth",   murmurAtt);

    setupKnob (ritualKnob,    ritualLabel,    ritualPct,
               VoidChantAudioProcessor::kParamRitual,
               "RITUAL",    "Chant Intensity", ritualAtt);

    setupKnob (possessKnob,   possessLabel,   possessPct,
               VoidChantAudioProcessor::kParamPossess,
               "POSSESS",   "Soul Resonance",  possessAtt);

    setupKnob (sacrificeKnob, sacrificeLabel, sacrificePct,
               VoidChantAudioProcessor::kParamSacrifice,
               "SACRIFICE", "Void Offering",   sacrificeAtt);

    setupKnob (depthKnob,     depthLabel,     depthPct,
               VoidChantAudioProcessor::kParamDepth,
               "DEPTH",     "Abyss Level",     depthAtt);

    startTimerHz (60);
}

VoidChantAudioProcessorEditor::~VoidChantAudioProcessorEditor()
{
    stopTimer();
}

// ----------------------------------------------------------------------------
void VoidChantAudioProcessorEditor::setupKnob (
    juce::Slider& knob,
    juce::Label&  nameLabel,
    juce::Label&  pctLabel,
    const juce::String& paramId,
    const juce::String& displayName,
    const juce::String& description,
    std::unique_ptr<Attachment>& attachment)
{
    knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setLookAndFeel (&knobLAF);
    addAndMakeVisible (knob);

    attachment = std::make_unique<Attachment> (
        processor.getAPVTS(), paramId, knob);

    nameLabel.setText (displayName + "\n" + description, juce::dontSendNotification);
    nameLabel.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(),
                                   10.f, juce::Font::bold));
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFA0E8FF));
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    pctLabel.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(),
                                   11.f, juce::Font::bold));
    pctLabel.setJustificationType (juce::Justification::centred);
    pctLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF7DF4FF));
    pctLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (pctLabel);

    knob.onValueChange = [&knob, &pctLabel]
    {
        pctLabel.setText (
            juce::String (static_cast<int> (knob.getValue() * 100.f)) + "%",
            juce::dontSendNotification);
    };
    knob.onValueChange();
}

// ----------------------------------------------------------------------------
void VoidChantAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (backgroundImage.isValid())
        g.drawImageAt (backgroundImage, 0, 0);
    else
        g.fillAll (juce::Colour (0xFF0A0A14));
}

// ----------------------------------------------------------------------------
void VoidChantAudioProcessorEditor::resized()
{
    magicCircle.setBounds (0, 0, kWidth, kHeight);

    const int knobCentreY = static_cast<int> (kHeight * kKnobY);
    const int pctH        = 16;
    const int nameH       = 32;
    const int topY        = knobCentreY - (pctH + 4 + kKnobSize / 2);

    juce::Slider* knobs[] = { &murmurKnob, &ritualKnob, &possessKnob,
                               &sacrificeKnob, &depthKnob };
    juce::Label*  names[] = { &murmurLabel, &ritualLabel, &possessLabel,
                               &sacrificeLabel, &depthLabel };
    juce::Label*  pcts[]  = { &murmurPct, &ritualPct, &possessPct,
                               &sacrificePct, &depthPct };

    for (int i = 0; i < 5; ++i)
    {
        const int cx = static_cast<int> (kXPos[i] * kWidth);
        const int lx = cx - kKnobSize / 2;

        pcts[i]->setBounds  (lx,            topY,                         kKnobSize,      pctH);
        knobs[i]->setBounds (lx,            topY + pctH + 4,              kKnobSize,      kKnobSize);
        names[i]->setBounds (lx - 12,       topY + pctH + 4 + kKnobSize + 6,
                             kKnobSize + 24, nameH);
    }
}

// ----------------------------------------------------------------------------
/**
 * timerCallback — 60 fps glow animation
 *
 * 1. Poll getGlowLevel() (atomic float, safe from message thread)
 * 2. Pass to MagicCircleComponent::setGlowLevel()
 * 3. Call tick() which applies smoothing and triggers repaint()
 */
void VoidChantAudioProcessorEditor::timerCallback()
{
    magicCircle.setGlowLevel (processor.getGlowLevel());
    magicCircle.tick();
}

// ----------------------------------------------------------------------------
juce::AudioProcessorEditor* VoidChantAudioProcessor::createEditor()
{
    return new VoidChantAudioProcessorEditor (*this);
}
