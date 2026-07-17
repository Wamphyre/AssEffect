#include "LoFiEngine.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;

float onePoleCoefficient(float frequency, double sampleRate)
{
    const auto safeFrequency = juce::jlimit(5.0f, static_cast<float>(sampleRate * 0.45), frequency);
    return 1.0f - std::exp(-twoPi * safeFrequency / static_cast<float>(sampleRate));
}

float softClip(float value, float gain, float asymmetry)
{
    const auto biased = value * gain + asymmetry;
    const auto clipped = std::tanh(biased) - std::tanh(asymmetry);
    return clipped / juce::jmax(1.0f, std::tanh(gain) * 1.12f);
}
}

void LoFiEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    const auto delaySize = static_cast<std::size_t>(std::ceil(sampleRate * 0.05)) + 8u;

    for (auto& line : delayLines)
        line.assign(delaySize, 0.0f);

    driveSmoother.reset(sampleRate, 0.025);
    mixSmoother.reset(sampleRate, 0.025);
    outputSmoother.reset(sampleRate, 0.025);
    reset();
}

void LoFiEngine::reset()
{
    for (int channel = 0; channel < maxChannels; ++channel)
    {
        states[static_cast<std::size_t>(channel)] = {};
        states[static_cast<std::size_t>(channel)].randomState = 0x6d2b79f5u
            ^ (0x9e3779b9u * static_cast<std::uint32_t>(channel + 1));
        std::fill(delayLines[static_cast<std::size_t>(channel)].begin(),
                  delayLines[static_cast<std::size_t>(channel)].end(), 0.0f);
        delayWritePositions[static_cast<std::size_t>(channel)] = 0;
    }

    driveSmoother.setCurrentAndTargetValue(1.0f);
    mixSmoother.setCurrentAndTargetValue(1.0f);
    outputSmoother.setCurrentAndTargetValue(1.0f);
}

float LoFiEngine::nextRandom(ChannelState& state) noexcept
{
    auto x = state.randomState;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    state.randomState = x;
    return static_cast<float>(x) * (1.0f / 4294967295.0f) * 2.0f - 1.0f;
}

float LoFiEngine::readModulatedDelay(int channel, float input, float depthMs,
                                     float wowAmount, int machine)
{
    if (wowAmount < 0.0001f)
        return input;

    auto& state = states[static_cast<std::size_t>(channel)];
    auto& line = delayLines[static_cast<std::size_t>(channel)];
    auto& writePosition = delayWritePositions[static_cast<std::size_t>(channel)];

    line[static_cast<std::size_t>(writePosition)] = input;

    const auto slowRate = machine == 1 ? 0.31 : (machine == 2 ? 0.46 : 0.23);
    const auto fastRate = machine == 1 ? 2.1 : (machine == 2 ? 6.7 : 5.2);
    state.wowPhase += twoPi * slowRate / sampleRate;
    state.flutterPhase += twoPi * fastRate / sampleRate;

    if (state.wowPhase >= twoPi)
        state.wowPhase -= twoPi;
    if (state.flutterPhase >= twoPi)
        state.flutterPhase -= twoPi;

    const auto modulation = 0.73f * std::sin(static_cast<float>(state.wowPhase))
                          + 0.27f * std::sin(static_cast<float>(state.flutterPhase));
    const auto depthSamples = depthMs * 0.001f * static_cast<float>(sampleRate) * wowAmount;
    const auto delaySamples = 1.0f + depthSamples * (1.05f + modulation);
    auto readPosition = static_cast<float>(writePosition) - delaySamples;

    while (readPosition < 0.0f)
        readPosition += static_cast<float>(line.size());

    const auto indexA = static_cast<int>(readPosition) % static_cast<int>(line.size());
    const auto indexB = (indexA + 1) % static_cast<int>(line.size());
    const auto fraction = readPosition - std::floor(readPosition);
    const auto delayed = line[static_cast<std::size_t>(indexA)]
                       + fraction * (line[static_cast<std::size_t>(indexB)]
                                   - line[static_cast<std::size_t>(indexA)]);

    writePosition = (writePosition + 1) % static_cast<int>(line.size());
    return delayed;
}

float LoFiEngine::processChannel(float input, int channel, const Parameters& p,
                                 float driveGain, float lowPassCoefficient,
                                 float highPassCoefficient, float headCoefficient)
{
    auto& state = states[static_cast<std::size_t>(channel)];
    const auto age = juce::jlimit(0.0f, 1.0f, p.age * 0.01f);
    const auto wear = juce::jlimit(0.0f, 1.0f, p.wear * 0.01f);
    const auto noise = juce::jlimit(0.0f, 1.0f, p.noise * 0.01f);
    const auto grit = juce::jlimit(0.0f, 1.0f, p.grit * 0.01f);

    state.inputLow += highPassCoefficient * (input - state.inputLow);
    auto value = input - state.inputLow;

    const auto magneticSpeed = 1.0f - std::exp(-twoPi * (28.0f + 34.0f * (1.0f - age))
                                                / static_cast<float>(sampleRate));
    state.hysteresis += magneticSpeed * (value - state.hysteresis);

    auto machineDrive = driveGain;
    float asymmetry = 0.0f;
    switch (p.machine)
    {
        case 1: machineDrive *= 0.72f; break;
        case 2: machineDrive *= 1.22f; asymmetry = 0.035f + 0.04f * age; break;
        case 3: machineDrive *= 1.65f; asymmetry = 0.075f; break;
        case 4: machineDrive *= 0.86f; break;
        default: asymmetry = 0.018f + 0.025f * age; break;
    }

    const auto memoryAmount = (p.machine == 1 || p.machine == 4) ? 0.025f : (0.08f + 0.13f * age);
    value = softClip(value + memoryAmount * state.hysteresis, machineDrive, asymmetry);

    state.headLow += headCoefficient * (value - state.headLow);
    const auto headBump = p.machine == 0 ? 0.13f + 0.16f * age
                        : p.machine == 2 ? 0.20f + 0.20f * age
                        : p.machine == 3 ? 0.10f : 0.0f;
    value += state.headLow * headBump;

    state.toneLow += lowPassCoefficient * (value - state.toneLow);
    value = state.toneLow;

    if (p.machine == 0 || p.machine == 2)
    {
        const auto eventsPerSecond = 0.015f + wear * wear * (p.machine == 2 ? 1.4f : 0.75f);
        if (state.dropoutSamples <= 0
            && (nextRandom(state) * 0.5f + 0.5f) < eventsPerSecond / static_cast<float>(sampleRate))
        {
            state.dropoutSamples = static_cast<int>(sampleRate * (0.006 + 0.045 * wear
                                         * (nextRandom(state) * 0.5f + 0.5f)));
        }

        const auto dropoutTarget = state.dropoutSamples > 0 ? 1.0f - (0.18f + 0.68f * wear) : 1.0f;
        state.dropoutGain += (state.dropoutSamples > 0 ? 0.028f : 0.004f)
                           * (dropoutTarget - state.dropoutGain);
        value *= state.dropoutGain;
        if (state.dropoutSamples > 0)
            --state.dropoutSamples;
    }

    auto artefact = 0.0f;
    const auto white = nextRandom(state);

    if (p.machine == 1)
    {
        const auto rumbleCoefficient = onePoleCoefficient(32.0f, sampleRate);
        state.rumble += rumbleCoefficient * (white - state.rumble);
        artefact += state.rumble * juce::Decibels::decibelsToGain(-52.0f + noise * 22.0f);

        const auto cracklesPerSecond = 0.12f + wear * wear * 15.0f;
        if ((nextRandom(state) * 0.5f + 0.5f) < cracklesPerSecond / static_cast<float>(sampleRate))
        {
            state.crackleEnvelope = 0.25f + 0.75f * (nextRandom(state) * 0.5f + 0.5f);
            state.cracklePolarity = nextRandom(state) >= 0.0f ? 1.0f : -1.0f;
        }
        artefact += state.cracklePolarity * state.crackleEnvelope
                  * juce::Decibels::decibelsToGain(-22.0f + noise * 12.0f);
        state.crackleEnvelope *= std::exp(-1.0f / static_cast<float>(sampleRate * 0.0014));

        state.noiseLow += 0.085f * (white - state.noiseLow);
        artefact += state.noiseLow * juce::Decibels::decibelsToGain(-61.0f + noise * 29.0f);
    }
    else
    {
        state.noiseLow += 0.032f * (white - state.noiseLow);
        const auto hiss = white - state.noiseLow * 0.78f;
        const auto machineNoise = p.machine == 2 ? 4.0f : (p.machine == 3 ? -2.0f : 0.0f);
        artefact += hiss * juce::Decibels::decibelsToGain(-76.0f + noise * 43.0f + machineNoise);
    }

    value += artefact;

    auto holdFactor = 1;
    auto bitDepth = 16.0f;
    if (p.machine == 4)
    {
        holdFactor = 1 + static_cast<int>(grit * grit * 38.0f);
        bitDepth = 15.0f - grit * 11.0f;
    }
    else if (grit > 0.42f)
    {
        holdFactor = 1 + static_cast<int>((grit - 0.42f) * (p.machine == 3 ? 14.0f : 6.0f));
        bitDepth = 16.0f - (grit - 0.42f) * (p.machine == 3 ? 13.0f : 8.0f);
    }

    if (state.holdCounter <= 0)
    {
        state.heldSample = value;
        state.holdCounter = holdFactor;
    }
    --state.holdCounter;
    value = state.heldSample;

    if (bitDepth < 15.9f)
    {
        const auto levels = std::pow(2.0f, juce::jlimit(3.0f, 16.0f, bitDepth));
        value = std::round(value * levels) / levels;
    }

    if (p.machine == 3)
    {
        value = std::tanh(value * (1.0f + grit * 5.5f));
        value = 0.84f * value + 0.16f * std::abs(value) * (value >= 0.0f ? 1.0f : -0.35f);
    }

    const auto depthMs = p.machine == 1 ? 4.4f : p.machine == 2 ? 3.8f
                       : p.machine == 3 ? 1.1f : p.machine == 4 ? 0.8f : 2.6f;
    value = readModulatedDelay(channel, value, depthMs,
                               std::pow(juce::jlimit(0.0f, 1.0f, p.wow * 0.01f), 1.35f),
                               p.machine);

    const auto dcBlocked = value - state.dcInput + 0.995f * state.dcOutput;
    state.dcInput = value;
    state.dcOutput = dcBlocked;
    return dcBlocked;
}

void LoFiEngine::process(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    const auto numChannels = juce::jmin(maxChannels, buffer.getNumChannels());
    const auto numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0)
        return;

    driveSmoother.setTargetValue(juce::Decibels::decibelsToGain(p.driveDb));
    mixSmoother.setTargetValue(juce::jlimit(0.0f, 1.0f, p.mix * 0.01f));
    outputSmoother.setTargetValue(juce::Decibels::decibelsToGain(p.outputDb));

    const auto age = juce::jlimit(0.0f, 1.0f, p.age * 0.01f);
    const auto toneOctaves = juce::jlimit(-1.8f, 1.8f, p.tone * 0.018f);
    float baseCutoff = 18000.0f;
    float highPass = 24.0f;

    switch (p.machine)
    {
        case 1: baseCutoff = 16600.0f - age * 7600.0f; highPass = 34.0f + age * 30.0f; break;
        case 2: baseCutoff = 12800.0f - age * 6900.0f; highPass = 48.0f + age * 38.0f; break;
        case 3: baseCutoff = 6700.0f - age * 2500.0f; highPass = 105.0f + age * 55.0f; break;
        case 4: baseCutoff = 14700.0f - age * 4300.0f; highPass = 28.0f + age * 18.0f; break;
        default: baseCutoff = 18400.0f - age * 10500.0f; highPass = 25.0f + age * 28.0f; break;
    }

    const auto cutoff = juce::jlimit(1800.0f, static_cast<float>(sampleRate * 0.45),
                                     baseCutoff * std::pow(2.0f, toneOctaves));
    const auto lowPassCoefficient = onePoleCoefficient(cutoff, sampleRate);
    const auto highPassCoefficient = onePoleCoefficient(highPass, sampleRate);
    const auto headCoefficient = onePoleCoefficient(p.machine == 2 ? 125.0f : 92.0f, sampleRate);
    const auto width = juce::jlimit(0.0f, 1.5f, p.width * 0.01f);

    auto* left = buffer.getWritePointer(0);
    auto* right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto dryLeft = left[sample];
        const auto dryRight = right != nullptr ? right[sample] : 0.0f;
        const auto drive = driveSmoother.getNextValue();
        auto wetLeft = processChannel(dryLeft, 0, p, drive, lowPassCoefficient,
                                      highPassCoefficient, headCoefficient);
        auto wetRight = right != nullptr
                      ? processChannel(dryRight, 1, p, drive, lowPassCoefficient,
                                       highPassCoefficient, headCoefficient)
                      : 0.0f;

        if (right != nullptr)
        {
            const auto mid = 0.5f * (wetLeft + wetRight);
            const auto side = 0.5f * (wetLeft - wetRight) * width;
            wetLeft = mid + side;
            wetRight = mid - side;
        }

        const auto mix = mixSmoother.getNextValue();
        const auto output = outputSmoother.getNextValue();
        left[sample] = (dryLeft + mix * (wetLeft - dryLeft)) * output;
        if (right != nullptr)
            right[sample] = (dryRight + mix * (wetRight - dryRight)) * output;
    }
}

