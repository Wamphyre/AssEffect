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
    std::size_t getWorkingMemoryBytes() const noexcept;

private:
    struct ArtifactGains
    {
        float hiss = 0.0f;
        float rumble = 0.0f;
        float crackle = 0.0f;
        float surface = 0.0f;
    };

    struct SaturationSettings
    {
        float drive = 1.0f;
        float asymmetry = 0.0f;
        float biasCorrection = 0.0f;
        float normalisation = 1.0f;
    };

    struct DigitalSettings
    {
        int holdFactor = 1;
        float quantisationLevels = 0.0f;
    };

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
        std::uint32_t randomState = 0x6d2b79f5u;
    };

    static constexpr int maxChannels = 2;

    float nextRandom(ChannelState& state) noexcept;
    float processChannel(float input, int channel, const Parameters& p,
                         float lowPassCoefficient, float highPassCoefficient,
                         float headCoefficient, float magneticCoefficient,
                         float motionDelaySamples, const ArtifactGains& artifactGains,
                         const SaturationSettings& saturation,
                         const DigitalSettings& digital);
    float readModulatedDelay(int channel, float input, float delaySamples);
    float calculateMotionDelaySamples(int machine, float wowAmount) noexcept;
    ArtifactGains calculateArtifactGains(int machine, float noisePercent) const noexcept;
    static float lookupGain(const std::array<float, 101>& table, float percent) noexcept;
    static float applySafetyCeiling(float sample) noexcept;

    std::array<ChannelState, maxChannels> states;
    std::array<std::vector<float>, maxChannels> delayLines;
    std::array<int, maxChannels> delayWritePositions {};
    std::array<float, 101> standardHissGains {};
    std::array<float, 101> fourTrackHissGains {};
    std::array<float, 101> cellarHissGains {};
    std::array<float, 101> vinylRumbleGains {};
    std::array<float, 101> vinylCrackleGains {};
    std::array<float, 101> vinylSurfaceGains {};

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> ageSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> wearSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> wowSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> noiseSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gritSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputSmoother;

    float currentLowPassCoefficient = 1.0f;
    float currentHighPassCoefficient = 1.0f;
    float currentHeadCoefficient = 1.0f;
    float targetLowPassCoefficient = 1.0f;
    float targetHighPassCoefficient = 1.0f;
    float targetHeadCoefficient = 1.0f;
    float coefficientSmoothing = 1.0f;
    float rumbleCoefficient = 0.0f;
    float crackleDecay = 0.0f;
    float magneticYoungCoefficient = 0.0f;
    float magneticOldCoefficient = 0.0f;
    double wowPhase = 0.0;
    double flutterPhase = 0.0;
    bool parametersInitialised = false;

    double sampleRate = 44100.0;
};
