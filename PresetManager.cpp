#include "PresetManager.h"
#include "../PluginProcessor.h"

namespace drawsynth
{
    namespace
    {
        constexpr const char* kFileExtension = ".dsynthpreset";
    }

    const juce::StringArray& PresetManager::getPresetParameterIds()
    {
        static const juce::StringArray ids {
            ParamIDs::root, ParamIDs::scale, ParamIDs::octaveRange, ParamIDs::octaveOffset,
            ParamIDs::notesPerBeat, ParamIDs::loopBars, ParamIDs::oscType,
            ParamIDs::attack, ParamIDs::decay, ParamIDs::sustain, ParamIDs::release, ParamIDs::masterGain
        };
        return ids;
    }

    PresetManager::PresetManager (DrawSynthAudioProcessor& processor) : processorRef (processor)
    {
        createFactoryPresetsIfNeeded();
    }

    juce::File PresetManager::getPresetFolder() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("DrawSynth")
                 .getChildFile ("Presets");
    }

    juce::String PresetManager::sanitizeFileName (const juce::String& name)
    {
        auto legal = juce::File::createLegalFileName (name.trim());
        return legal.isNotEmpty() ? legal : juce::String ("Preset");
    }

    juce::StringArray PresetManager::getAvailablePresetNames() const
    {
        juce::StringArray names;
        auto folder = getPresetFolder();
        if (! folder.isDirectory())
            return names;

        for (const auto& file : folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kFileExtension))
            names.add (file.getFileNameWithoutExtension());

        names.sort (true);
        return names;
    }

    bool PresetManager::writePresetFile (const juce::String& presetName, const std::map<juce::String, float>& values) const
    {
        auto folder = getPresetFolder();
        if (! folder.exists())
            folder.createDirectory();

        juce::XmlElement root ("DrawSynthPreset");
        root.setAttribute ("name", presetName);

        for (const auto& entry : values)
        {
            auto* param = root.createNewChildElement ("Param");
            param->setAttribute ("id", entry.first);
            param->setAttribute ("value", static_cast<double> (entry.second));
        }

        auto destFile = folder.getChildFile (sanitizeFileName (presetName) + kFileExtension);
        return root.writeTo (destFile);
    }

    bool PresetManager::saveCurrentAsPreset (const juce::String& presetName)
    {
        if (presetName.trim().isEmpty())
            return false;

        std::map<juce::String, float> values;
        for (const auto& id : getPresetParameterIds())
            if (auto* raw = processorRef.parameters.getRawParameterValue (id))
                values[id] = raw->load();

        return writePresetFile (presetName.trim(), values);
    }

    bool PresetManager::loadPreset (const juce::String& presetName)
    {
        auto file = getPresetFolder().getChildFile (sanitizeFileName (presetName) + kFileExtension);
        auto root = juce::XmlDocument::parse (file);
        if (root == nullptr || ! root->hasTagName ("DrawSynthPreset"))
            return false;

        for (int i = 0; i < root->getNumChildElements(); ++i)
        {
            auto* child = root->getChildElement (i);
            if (child == nullptr || ! child->hasTagName ("Param"))
                continue;

            const auto id = child->getStringAttribute ("id");
            const float value = static_cast<float> (child->getDoubleAttribute ("value"));

            if (auto* param = processorRef.parameters.getParameter (id))
                param->setValueNotifyingHost (param->convertTo0to1 (value));
        }

        return true;
    }

    bool PresetManager::deletePreset (const juce::String& presetName)
    {
        auto file = getPresetFolder().getChildFile (sanitizeFileName (presetName) + kFileExtension);
        return file.existsAsFile() && file.deleteFile();
    }

    void PresetManager::createFactoryPresetsIfNeeded()
    {
        auto folder = getPresetFolder();
        if (folder.isDirectory() && ! folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kFileExtension).isEmpty())
            return; // user already has presets (or previously-generated factory ones) - never overwrite

        namespace P = ParamIDs;

        writePresetFile ("Init", {
            { P::root, 0.0f }, { P::scale, 0.0f }, { P::octaveRange, 2.0f }, { P::octaveOffset, 0.0f },
            { P::notesPerBeat, 1.0f }, { P::loopBars, 2.0f }, { P::oscType, 1.0f },
            { P::attack, 0.02f }, { P::decay, 0.1f }, { P::sustain, 0.8f }, { P::release, 0.2f }, { P::masterGain, 0.8f }
        });

        writePresetFile ("Soft Pad", {
            { P::root, 0.0f }, { P::scale, 1.0f }, { P::octaveRange, 3.0f }, { P::octaveOffset, -1.0f },
            { P::notesPerBeat, 1.0f }, { P::loopBars, 2.0f }, { P::oscType, 2.0f },
            { P::attack, 0.6f }, { P::decay, 0.5f }, { P::sustain, 0.9f }, { P::release, 1.4f }, { P::masterGain, 0.7f }
        });

        writePresetFile ("Pluck Lead", {
            { P::root, 0.0f }, { P::scale, 0.0f }, { P::octaveRange, 2.0f }, { P::octaveOffset, 1.0f },
            { P::notesPerBeat, 1.0f }, { P::loopBars, 2.0f }, { P::oscType, 1.0f },
            { P::attack, 0.005f }, { P::decay, 0.15f }, { P::sustain, 0.15f }, { P::release, 0.12f }, { P::masterGain, 0.75f }
        });

        writePresetFile ("Deep Bass", {
            { P::root, 0.0f }, { P::scale, 1.0f }, { P::octaveRange, 1.0f }, { P::octaveOffset, -2.0f },
            { P::notesPerBeat, 1.0f }, { P::loopBars, 2.0f }, { P::oscType, 1.0f },
            { P::attack, 0.01f }, { P::decay, 0.2f }, { P::sustain, 0.7f }, { P::release, 0.15f }, { P::masterGain, 0.85f }
        });
    }
}
