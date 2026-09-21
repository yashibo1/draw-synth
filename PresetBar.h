#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace drawsynth
{
    // A slim preset browser: pick a saved sound from the dropdown to load it
    // instantly, or hit Save, type a name, and press Enter to store the
    // current sound + grid settings (not the drawn pattern - see
    // PresetManager for why).
    class PresetBar : public juce::Component
    {
    public:
        explicit PresetBar (DrawSynthAudioProcessor& processor);

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        void refreshPresetList (const juce::String& selectName = {});
        void beginSave();
        void commitSave();
        void deleteSelected();

        DrawSynthAudioProcessor& processorRef;

        juce::Label titleLabel;
        juce::ComboBox presetBox;
        juce::TextButton saveButton { "Save" };
        juce::TextButton deleteButton { "Delete" };
        juce::TextEditor nameEntry;

        bool loadingProgrammatically = false; // guards presetBox.onChange while we repopulate it

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
    };
}
