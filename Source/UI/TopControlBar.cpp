#include "TopControlBar.h"
#include "../Params.h"

namespace drawsynth
{
    namespace
    {
        constexpr int kLabelTextHeight = 13;
    }

    void TopControlBar::configureLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (juce::Font (juce::FontOptions ((float) kLabelTextHeight)));
        label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
        addAndMakeVisible (label);
    }

    void TopControlBar::configureRotary (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (slider);
    }

    void TopControlBar::populateFromParameterChoices (juce::ComboBox& box, const juce::String& paramID)
    {
        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (processorRef.parameters.getParameter (paramID)))
        {
            int itemId = 1;
            for (const auto& choice : choiceParam->choices)
                box.addItem (choice, itemId++);
        }

        addAndMakeVisible (box);
    }

    TopControlBar::TopControlBar (DrawSynthAudioProcessor& processor) : processorRef (processor)
    {
        auto& apvts = processor.parameters;

        configureLabel (keyLabel, "Key");
        configureLabel (scaleLabel, "Scale");
        configureLabel (octaveRangeLabel, "Octaves");
        configureLabel (octaveOffsetLabel, "Oct. Shift");
        configureLabel (quantizeLabel, "Quantize");
        configureLabel (loopLabel, "Loop");
        configureLabel (bpmLabel, "BPM");
        configureLabel (oscLabel, "Osc");
        configureLabel (levelLabel, "Level");
        configureLabel (attackLabel, "A");
        configureLabel (decayLabel, "D");
        configureLabel (sustainLabel, "S");
        configureLabel (releaseLabel, "R");

        populateFromParameterChoices (keyBox, ParamIDs::root);
        populateFromParameterChoices (scaleBox, ParamIDs::scale);
        populateFromParameterChoices (quantizeBox, ParamIDs::notesPerBeat);
        populateFromParameterChoices (loopBox, ParamIDs::loopBars);
        populateFromParameterChoices (oscBox, ParamIDs::oscType);

        keyAttachment      = std::make_unique<ComboAttachment> (apvts, ParamIDs::root, keyBox);
        scaleAttachment    = std::make_unique<ComboAttachment> (apvts, ParamIDs::scale, scaleBox);
        quantizeAttachment = std::make_unique<ComboAttachment> (apvts, ParamIDs::notesPerBeat, quantizeBox);
        loopAttachment     = std::make_unique<ComboAttachment> (apvts, ParamIDs::loopBars, loopBox);
        oscAttachment      = std::make_unique<ComboAttachment> (apvts, ParamIDs::oscType, oscBox);

        octaveRangeSlider.setSliderStyle (juce::Slider::IncDecButtons);
        octaveRangeSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 26, 20);
        addAndMakeVisible (octaveRangeSlider);
        octaveRangeAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::octaveRange, octaveRangeSlider);

        octaveOffsetSlider.setSliderStyle (juce::Slider::IncDecButtons);
        octaveOffsetSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 26, 20);
        addAndMakeVisible (octaveOffsetSlider);
        octaveOffsetAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::octaveOffset, octaveOffsetSlider);

        addAndMakeVisible (syncButton);
        syncAttachment = std::make_unique<ButtonAttachment> (apvts, ParamIDs::tempoSync, syncButton);

        bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
        addAndMakeVisible (bpmSlider);
        bpmAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::manualBpm, bpmSlider);

        levelSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        levelSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 20);
        addAndMakeVisible (levelSlider);
        levelAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::masterGain, levelSlider);

        configureRotary (attackSlider);
        configureRotary (decaySlider);
        configureRotary (sustainSlider);
        configureRotary (releaseSlider);

        attackAttachment  = std::make_unique<SliderAttachment> (apvts, ParamIDs::attack, attackSlider);
        decayAttachment   = std::make_unique<SliderAttachment> (apvts, ParamIDs::decay, decaySlider);
        sustainAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::sustain, sustainSlider);
        releaseAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::release, releaseSlider);

        auto updateBpmEnablement = [this] { bpmSlider.setEnabled (! syncButton.getToggleState()); };
        syncButton.onClick = updateBpmEnablement;
        updateBpmEnablement();
    }

    void TopControlBar::paint (juce::Graphics& g)
    {
        g.setColour (juce::Colour (0xff23262e));
        g.fillRect (getLocalBounds());
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawLine (0.0f, (float) getHeight() - 1.0f, (float) getWidth(), (float) getHeight() - 1.0f, 1.0f);
    }

    void TopControlBar::resized()
    {
        auto area = getLocalBounds().reduced (10, 4);
        auto row1 = area.removeFromTop (area.getHeight() / 2);
        auto row2 = area;

        auto place = [] (juce::Rectangle<int>& row, juce::Label& label, juce::Component& control, int labelWidth, int controlWidth)
        {
            label.setBounds (row.removeFromLeft (labelWidth));
            control.setBounds (row.removeFromLeft (controlWidth));
            row.removeFromLeft (12);
        };

        place (row1, keyLabel, keyBox, 26, 46);
        place (row1, scaleLabel, scaleBox, 40, 116);
        place (row1, octaveRangeLabel, octaveRangeSlider, 54, 70);
        place (row1, octaveOffsetLabel, octaveOffsetSlider, 62, 70);
        place (row1, quantizeLabel, quantizeBox, 58, 130);
        place (row1, loopLabel, loopBox, 32, 80);

        place (row2, bpmLabel, bpmSlider, 30, 96);
        row2.removeFromLeft (4);
        syncButton.setBounds (row2.removeFromLeft (110));
        row2.removeFromLeft (12);
        place (row2, oscLabel, oscBox, 24, 80);
        place (row2, attackLabel, attackSlider, 12, 40);
        place (row2, decayLabel, decaySlider, 12, 40);
        place (row2, sustainLabel, sustainSlider, 12, 40);
        place (row2, releaseLabel, releaseSlider, 12, 40);
        place (row2, levelLabel, levelSlider, 32, 90);
    }
}
