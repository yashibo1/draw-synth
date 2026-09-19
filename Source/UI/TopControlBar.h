#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace drawsynth
{
    // Top control strip. Row 1 shapes the note grid itself (key, scale,
    // octave window, quantize resolution, loop length) - changing any of
    // these retroactively re-quantizes whatever is already drawn. Row 2
    // shapes the sound (tempo, oscillator, ADSR, output level) and never
    // touches the note grid.
    class TopControlBar : public juce::Component
    {
    public:
        explicit TopControlBar (DrawSynthAudioProcessor& processor);

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
        using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

        void configureLabel (juce::Label& label, const juce::String& text);
        void configureRotary (juce::Slider& slider);
        void populateFromParameterChoices (juce::ComboBox& box, const juce::String& paramID);

        DrawSynthAudioProcessor& processorRef;

        // Row 1: grid shape
        juce::Label keyLabel, scaleLabel, octaveRangeLabel, octaveOffsetLabel, quantizeLabel, loopLabel;
        juce::ComboBox keyBox, scaleBox, quantizeBox, loopBox;
        juce::Slider octaveRangeSlider, octaveOffsetSlider;

        // Row 2: sound shape
        juce::ToggleButton syncButton { "Sync To Host" };
        juce::Label bpmLabel, oscLabel, levelLabel, attackLabel, decayLabel, sustainLabel, releaseLabel;
        juce::Slider bpmSlider, levelSlider, attackSlider, decaySlider, sustainSlider, releaseSlider;
        juce::ComboBox oscBox;

        std::unique_ptr<ComboAttachment> keyAttachment, scaleAttachment, quantizeAttachment, loopAttachment, oscAttachment;
        std::unique_ptr<SliderAttachment> octaveRangeAttachment, octaveOffsetAttachment, bpmAttachment, levelAttachment;
        std::unique_ptr<SliderAttachment> attackAttachment, decayAttachment, sustainAttachment, releaseAttachment;
        std::unique_ptr<ButtonAttachment> syncAttachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopControlBar)
    };
}
