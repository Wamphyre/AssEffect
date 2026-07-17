#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <vector>

class LoFiEngine
{
public:
    struct Parameters
    {
        int machine = 0;
        float driveDb = 7.0f;
        float age = 35.0f;
        float wear = 22.0f;
        float wow = 18.0f;
        float noise = 18.0f;
        float grit = 12.0f;
        float tone = 0.0f;
        float width = 100.0f;
        float mix = 100.0f;
        float outputDb = 0.0f;
    };

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const Parameters& parameters);

private:
    struct ChannelState
    {
        float inputLow = 0.0f;
        float toneLow = 0.0f;
        float headLow = 0.0f;
        float noiseLow = 0.0f;
        float rumble = 0.0f;
        float hysteresis = 0.0f;
        float dcInput = 0.0f;
        float dcOutput = 0.0f;
        float heldSample = 0.0f;
        float dropoutGain = 1.0f;
        float crackleEnvelope = 0.0f;
        float cracklePolarity = 1.0f;
        int holdCounter = 0;
        int dropoutSamples = 0;
        double wowPhase = 0.0;
        double flutterPhase = 0.0;
        std::uint32_t randomState = 0x6d2b79f5u;
    };

    static constexpr int maxChannels = 2;

    float nextRandom(ChannelState& state) noexcept;
    float processChannel(float input, int channel, const Parameters& p,
                         float driveGain, float lowPassCoefficient,
                         float highPassCoefficient, float headCoefficient);
    float readModulatedDelay(int channel, float input, float depthMs,
                             float wowAmount, int machine);

    std::array<ChannelState, maxChannels> states;
    std::array<std::vector<float>, maxChannels> delayLines;
    std::array<int, maxChannels> delayWritePositions {};

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputSmoother;

    double sampleRate = 44100.0;
};

