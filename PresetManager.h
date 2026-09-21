#pragma once

#include <juce_core/juce_core.h>
#include <map>

namespace drawsynth
{
    class DrawSynthAudioProcessor; // avoids a circular include with PluginProcessor.h

    // A preset browser/save/load system for DrawSynth's own sound and grid
    // parameters (key/scale/octave/quantize/loop length, oscillator, ADSR,
    // level). This deliberately does NOT store the drawn pattern - a preset
    // here is an "instrument setting", the same idea as a preset in any
    // other synth; the drawing itself is closer to "a song" than "a patch".
    //
    // It cannot import another plugin's preset format (e.g. Serum's): those
    // encode a completely different synthesis engine - wavetables, filters,
    // a modulation matrix - with no meaningful equivalent in DrawSynth's
    // simple oscillator+ADSR voice, so there's nothing sensible to map the
    // data onto even if the file could be parsed.
    class PresetManager
    {
    public:
        explicit PresetManager (DrawSynthAudioProcessor& processor);

        juce::File getPresetFolder() const;
        juce::StringArray getAvailablePresetNames() const; // sorted, rescanned from disk each call

        bool saveCurrentAsPreset (const juce::String& presetName);
        bool loadPreset (const juce::String& presetName);
        bool deletePreset (const juce::String& presetName);

        // The APVTS parameter IDs a preset captures - everything that shapes
        // the sound/grid, nothing that's session or transport state.
        static const juce::StringArray& getPresetParameterIds();

    private:
        void createFactoryPresetsIfNeeded();
        bool writePresetFile (const juce::String& presetName, const std::map<juce::String, float>& values) const;
        static juce::String sanitizeFileName (const juce::String& name);

        DrawSynthAudioProcessor& processorRef;
    };
}
