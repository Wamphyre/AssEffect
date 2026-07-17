#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr std::array<const char*, 12> parameterOrder
{
    ParameterIDs::machine, ParameterIDs::drive, ParameterIDs::age, ParameterIDs::wear,
    ParameterIDs::wow, ParameterIDs::noise, ParameterIDs::grit, ParameterIDs::tone,
    ParameterIDs::width, ParameterIDs::mix, ParameterIDs::output, ParameterIDs::bypass
};
}

AssEffectAudioProcessor::AssEffectAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "ASS_EFFECT_STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AssEffectAudioProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;

    result.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParameterIDs::machine, 1), "Machine",
        juce::StringArray { "90s Cassette", "Worn Vinyl", "4-Track Demo", "Cellar Speaker", "Bitrot Sampler" }, 0));
    result.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::drive, 1), "Drive", Range(0.0f, 24.0f, 0.1f, 0.62f), 7.0f, " dB"));
    result.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::age, 1), "Age", Range(0.0f, 100.0f, 0.1f), 38.0f, " %"));
    result.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::wear, 1), "Wear", Range(0.0f, 100.0f, 0.1f), 24.0f, " %"));
    result.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::wow, 1), "Wow / Flutter", Range(0.0f, 100.0f, 0.1f, 0.75f), 21.0f, " %"));
    result.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::noise, 1), "Noise", Range(0.0f, 100.0f, 0.1f, 0.65f), 18.0f, " %"));
    result.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::grit, 1), "Grit", Range(0.0f, 100.0f, 0.1f, 0.7f), 10.0f, " %"));
    result.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::tone, 1), "Tone", Range(-100.0f, 100.0f, 0.1f), -5.0f, ""));
    result.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::width, 1), "Width", Range(0.0f, 150.0f, 0.1f), 104.0f, " %"));
    result.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::mix, 1), "Mix", Range(0.0f, 100.0f, 0.1f), 100.0f, " %"));
    result.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::output, 1), "Output", Range(-18.0f, 12.0f, 0.1f), -0.5f, " dB"));
    result.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParameterIDs::bypass, 1), "Bypass", false));

    return { result.begin(), result.end() };
}

void AssEffectAudioProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    engine.prepare({ newSampleRate, static_cast<juce::uint32>(samplesPerBlock),
                     static_cast<juce::uint32>(getTotalNumOutputChannels()) });
}

void AssEffectAudioProcessor::releaseResources()
{
    engine.reset();
}

bool AssEffectAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();
    return input == output && (output == juce::AudioChannelSet::mono()
                            || output == juce::AudioChannelSet::stereo());
}

juce::AudioProcessorEditor* AssEffectAudioProcessor::createEditor()
{
    return new AssEffectAudioProcessorEditor(*this);
}

float AssEffectAudioProcessor::findMagnitude(const juce::AudioBuffer<float>& buffer)
{
    auto magnitude = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        magnitude = juce::jmax(magnitude, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
    return magnitude;
}

void AssEffectAudioProcessor::updatePeak(std::atomic<float>& destination, float value) noexcept
{
    auto previous = destination.load(std::memory_order_relaxed);
    while (previous < value
           && !destination.compare_exchange_weak(previous, value, std::memory_order_relaxed))
    {
    }
}

void AssEffectAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    updatePeak(inputPeak, findMagnitude(buffer));

    if (parameters.getRawParameterValue(ParameterIDs::bypass)->load() < 0.5f)
    {
        LoFiEngine::Parameters p;
        p.machine = juce::roundToInt(parameters.getRawParameterValue(ParameterIDs::machine)->load());
        p.driveDb = parameters.getRawParameterValue(ParameterIDs::drive)->load();
        p.age = parameters.getRawParameterValue(ParameterIDs::age)->load();
        p.wear = parameters.getRawParameterValue(ParameterIDs::wear)->load();
        p.wow = parameters.getRawParameterValue(ParameterIDs::wow)->load();
        p.noise = parameters.getRawParameterValue(ParameterIDs::noise)->load();
        p.grit = parameters.getRawParameterValue(ParameterIDs::grit)->load();
        p.tone = parameters.getRawParameterValue(ParameterIDs::tone)->load();
        p.width = parameters.getRawParameterValue(ParameterIDs::width)->load();
        p.mix = parameters.getRawParameterValue(ParameterIDs::mix)->load();
        p.outputDb = parameters.getRawParameterValue(ParameterIDs::output)->load();
        engine.process(buffer, p);
    }

    updatePeak(outputPeak, findMagnitude(buffer));
}

void AssEffectAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    updatePeak(inputPeak, findMagnitude(buffer));
    updatePeak(outputPeak, findMagnitude(buffer));
}

void AssEffectAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destination);
}

void AssEffectAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

const std::array<AssEffectAudioProcessor::FactoryPreset, 13>& AssEffectAudioProcessor::getFactoryPresets()
{
    return assEffectFactoryPresets;
}

void AssEffectAudioProcessor::loadFactoryPreset(int index)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(assEffectFactoryPresets.size())))
        return;

    const auto& preset = assEffectFactoryPresets[static_cast<std::size_t>(index)];
    for (std::size_t i = 0; i < parameterOrder.size(); ++i)
    {
        if (auto* parameter = parameters.getParameter(parameterOrder[i]))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(preset.values[i]));
            parameter->endChangeGesture();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AssEffectAudioProcessor();
}
