#include "PresetBar.h"

namespace drawsynth
{
    PresetBar::PresetBar (DrawSynthAudioProcessor& processor) : processorRef (processor)
    {
        titleLabel.setText ("Preset", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
        titleLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
        addAndMakeVisible (titleLabel);

        addAndMakeVisible (presetBox);
        presetBox.onChange = [this]
        {
            if (loadingProgrammatically)
                return;

            const auto name = presetBox.getText();
            if (name.isNotEmpty())
                processorRef.getPresetManager().loadPreset (name);
        };

        saveButton.onClick = [this] { beginSave(); };
        addAndMakeVisible (saveButton);

        deleteButton.onClick = [this] { deleteSelected(); };
        addAndMakeVisible (deleteButton);

        nameEntry.setTextToShowWhenEmpty ("New preset name...", juce::Colours::white.withAlpha (0.4f));
        nameEntry.setVisible (false);
        nameEntry.onReturnKey = [this] { commitSave(); };
        nameEntry.onEscapeKey = [this] { nameEntry.setVisible (false); resized(); };
        nameEntry.onFocusLost = [this] { nameEntry.setVisible (false); resized(); };
        addAndMakeVisible (nameEntry);

        refreshPresetList();
    }

    void PresetBar::refreshPresetList (const juce::String& selectName)
    {
        loadingProgrammatically = true;

        presetBox.clear (juce::dontSendNotification);
        const auto names = processorRef.getPresetManager().getAvailablePresetNames();

        int itemId = 1;
        int idToSelect = 0;
        for (const auto& name : names)
        {
            presetBox.addItem (name, itemId);
            if (name == selectName)
                idToSelect = itemId;
            ++itemId;
        }

        if (idToSelect != 0)
            presetBox.setSelectedId (idToSelect, juce::dontSendNotification);
        else
            presetBox.setText ("", juce::dontSendNotification);

        loadingProgrammatically = false;
    }

    void PresetBar::beginSave()
    {
        nameEntry.setText (presetBox.getText(), juce::dontSendNotification);
        nameEntry.setVisible (true);
        resized();
        nameEntry.grabKeyboardFocus();
        nameEntry.selectAll();
    }

    void PresetBar::commitSave()
    {
        const auto name = nameEntry.getText().trim();
        nameEntry.setVisible (false);

        if (name.isNotEmpty())
        {
            processorRef.getPresetManager().saveCurrentAsPreset (name);
            refreshPresetList (name);
        }

        resized();
    }

    void PresetBar::deleteSelected()
    {
        const auto name = presetBox.getText();
        if (name.isEmpty())
            return;

        processorRef.getPresetManager().deletePreset (name);
        refreshPresetList();
    }

    void PresetBar::paint (juce::Graphics& g)
    {
        g.setColour (juce::Colour (0xff23262e));
        g.fillRect (getLocalBounds());
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawLine (0.0f, static_cast<float> (getHeight()) - 1.0f, static_cast<float> (getWidth()), static_cast<float> (getHeight()) - 1.0f, 1.0f);
    }

    void PresetBar::resized()
    {
        auto area = getLocalBounds().reduced (10, 4);
        titleLabel.setBounds (area.removeFromLeft (48));
        area.removeFromLeft (6);

        auto rightControls = area.removeFromRight (140);
        deleteButton.setBounds (rightControls.removeFromRight (64));
        rightControls.removeFromRight (6);
        saveButton.setBounds (rightControls);

        area.removeFromRight (10);

        if (nameEntry.isVisible())
            nameEntry.setBounds (area);
        else
            presetBox.setBounds (area);
    }
}
