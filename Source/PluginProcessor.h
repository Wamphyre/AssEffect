#pragma once

#include <JuceHeader.h>
#include "FactoryPresets.h"
#include "LoFiEngine.h"

#include <atomic>

namespace ParameterIDs
{
inline constexpr auto machine = "machine";
inline constexpr auto drive = "drive";
inline constexpr auto age = "age";
inline constexpr auto wear = "wear";
inline constexpr auto wow = "wow";
inline constexpr auto noise = "noise";
inline constexpr auto grit = "grit";
inline constexpr auto tone = "tone";
inline constexpr auto width = "width";
inline constexpr auto mix = "mix";
inline constexpr auto output = "output";
inline constexpr auto bypass = "bypass";
}

class AssEffectAudioProcessor final : public juce::AudioProcessor
{
public:
    using FactoryPreset = AssEffectFactoryPreset;

    AssEffectAudioProcessor();
    ~AssEffectAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.12; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }
    static const std::array<FactoryPreset, 13>& getFactoryPresets();
    void loadFactoryPreset(int index);
    float consumeInputPeak() noexcept { return inputPeak.exchange(0.0f); }
    float consumeOutputPeak() noexcept { return outputPeak.exchange(0.0f); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static float findMagnitude(const juce::AudioBuffer<float>& buffer);
    void updatePeak(std::atomic<float>& destination, float value) noexcept;

    juce::AudioProcessorValueTreeState parameters;
    LoFiEngine engine;
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AssEffectAudioProcessor)
};
