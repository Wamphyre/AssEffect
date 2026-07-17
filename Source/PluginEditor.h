#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <memory>
#include <vector>

class AssEffectLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    AssEffectLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosition, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted,
                          bool down) override;
};

class AssEffectParameterKnob final : public juce::Component
{
public:
    AssEffectParameterKnob(juce::AudioProcessorValueTreeState&, const juce::String& parameterID,
                           const juce::String& caption, const juce::String& suffix);
    void resized() override;

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class AssEffectAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit AssEffectAudioProcessorEditor(AssEffectAudioProcessor&);
    ~AssEffectAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawBrand(juce::Graphics&, juce::Rectangle<float> area);
    void drawMeter(juce::Graphics&, juce::Rectangle<float> area, float level,
                   const juce::String& label);

    AssEffectAudioProcessor& processor;
    AssEffectLookAndFeel lookAndFeel;
    juce::ComboBox presets;
    juce::ComboBox machine;
    juce::ToggleButton bypass { "BYPASS" };
    juce::Label presetLabel;
    juce::Label machineLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> machineAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::vector<std::unique_ptr<AssEffectParameterKnob>> knobs;
    float displayedInput = 0.0f;
    float displayedOutput = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AssEffectAudioProcessorEditor)
};

