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
    ageSmoother.reset(sampleRate, 0.040);
    wearSmoother.reset(sampleRate, 0.040);
    wowSmoother.reset(sampleRate, 0.040);
    noiseSmoother.reset(sampleRate, 0.040);
    gritSmoother.reset(sampleRate, 0.040);
    widthSmoother.reset(sampleRate, 0.040);
    mixSmoother.reset(sampleRate, 0.025);
    outputSmoother.reset(sampleRate, 0.025);
    coefficientSmoothing = 1.0f - std::exp(-1.0f / static_cast<float>(sampleRate * 0.020));
    rumbleCoefficient = onePoleCoefficient(32.0f, sampleRate);
    crackleDecay = std::exp(-1.0f / static_cast<float>(sampleRate * 0.0014));
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

    parametersInitialised = false;
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

    if (wowAmount < 0.0001f)
    {
        writePosition = (writePosition + 1) % static_cast<int>(line.size());
        return input;
    }

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
        state.crackleEnvelope *= crackleDecay;

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

    // Grit has a continuous analogue stage on every physical machine. Higher
    // settings progressively add sample-rate and bit-depth damage below.
    if (p.machine == 0)
    {
        const auto shaped = std::tanh(value * (1.0f + grit * 1.8f))
                          / (1.0f + grit * 0.34f);
        value += grit * (shaped - value);
    }
    else if (p.machine == 1)
    {
        const auto stylusCompression = value - grit * 0.20f * value * std::abs(value);
        const auto shaped = std::tanh(stylusCompression * (1.0f + grit * 0.85f));
        value += grit * (shaped - value);
    }
    else if (p.machine == 2)
    {
        const auto shaped = std::tanh((value + 0.035f * grit * value * value)
                                     * (1.0f + grit * 2.4f))
                          / (1.0f + grit * 0.42f);
        value += grit * (shaped - value);
    }

    // Wear also represents cone fatigue and converter instability in the two
    // non-media machines, so the control never has a dead machine mode.
    if (p.machine == 3)
    {
        const auto shaped = std::tanh(value * (1.0f + wear * 1.35f))
                          / (1.0f + wear * 0.28f);
        value += wear * (shaped - value);
    }

    auto holdFactor = 1;
    auto bitDepth = 16.0f;
    if (p.machine == 4)
    {
        holdFactor = 1 + static_cast<int>(grit * grit * 38.0f + wear * wear * 4.0f);
        bitDepth = 15.0f - grit * 11.0f - wear * 1.5f;
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

    auto targetParameters = p;
    targetParameters.machine = juce::jlimit(0, 4, p.machine);
    const auto age = juce::jlimit(0.0f, 1.0f, p.age * 0.01f);
    const auto wear = juce::jlimit(0.0f, 1.0f, p.wear * 0.01f);
    const auto toneOctaves = juce::jlimit(-1.8f, 1.8f, p.tone * 0.018f);
    float baseCutoff = 18000.0f;
    float highPass = 24.0f;
    float wearDarkening = 0.10f;

    switch (targetParameters.machine)
    {
        case 1:
            baseCutoff = 16600.0f - age * 7600.0f;
            highPass = 34.0f + age * 30.0f;
            wearDarkening = 0.08f;
            break;
        case 2:
            baseCutoff = 12800.0f - age * 6900.0f;
            highPass = 48.0f + age * 38.0f;
            wearDarkening = 0.18f;
            break;
        case 3:
            baseCutoff = 6700.0f - age * 2500.0f;
            highPass = 105.0f + age * 55.0f;
            wearDarkening = 0.24f;
            break;
        case 4:
            baseCutoff = 14700.0f - age * 4300.0f;
            highPass = 28.0f + age * 18.0f;
            wearDarkening = 0.06f;
            break;
        default:
            baseCutoff = 18400.0f - age * 10500.0f;
            highPass = 25.0f + age * 28.0f;
            wearDarkening = 0.13f;
            break;
    }

    baseCutoff *= 1.0f - wear * wearDarkening;
    highPass += wear * (targetParameters.machine == 3 ? 28.0f : 9.0f);
    const auto cutoff = juce::jlimit(1800.0f, static_cast<float>(sampleRate * 0.45),
                                     baseCutoff * std::pow(2.0f, toneOctaves));
    targetLowPassCoefficient = onePoleCoefficient(cutoff, sampleRate);
    targetHighPassCoefficient = onePoleCoefficient(highPass, sampleRate);
    targetHeadCoefficient = onePoleCoefficient(targetParameters.machine == 2 ? 125.0f : 92.0f,
                                                sampleRate);

    const auto driveTarget = juce::Decibels::decibelsToGain(p.driveDb);
    const auto ageTarget = juce::jlimit(0.0f, 100.0f, p.age);
    const auto wearTarget = juce::jlimit(0.0f, 100.0f, p.wear);
    const auto wowTarget = juce::jlimit(0.0f, 100.0f, p.wow);
    const auto noiseTarget = juce::jlimit(0.0f, 100.0f, p.noise);
    const auto gritTarget = juce::jlimit(0.0f, 100.0f, p.grit);
    const auto widthTarget = juce::jlimit(0.0f, 150.0f, p.width);
    const auto mixTarget = juce::jlimit(0.0f, 1.0f, p.mix * 0.01f);
    const auto outputTarget = juce::Decibels::decibelsToGain(p.outputDb);

    if (!parametersInitialised)
    {
        driveSmoother.setCurrentAndTargetValue(driveTarget);
        ageSmoother.setCurrentAndTargetValue(ageTarget);
        wearSmoother.setCurrentAndTargetValue(wearTarget);
        wowSmoother.setCurrentAndTargetValue(wowTarget);
        noiseSmoother.setCurrentAndTargetValue(noiseTarget);
        gritSmoother.setCurrentAndTargetValue(gritTarget);
        widthSmoother.setCurrentAndTargetValue(widthTarget);
        mixSmoother.setCurrentAndTargetValue(mixTarget);
        outputSmoother.setCurrentAndTargetValue(outputTarget);
        currentLowPassCoefficient = targetLowPassCoefficient;
        currentHighPassCoefficient = targetHighPassCoefficient;
        currentHeadCoefficient = targetHeadCoefficient;
        parametersInitialised = true;
    }
    else
    {
        driveSmoother.setTargetValue(driveTarget);
        ageSmoother.setTargetValue(ageTarget);
        wearSmoother.setTargetValue(wearTarget);
        wowSmoother.setTargetValue(wowTarget);
        noiseSmoother.setTargetValue(noiseTarget);
        gritSmoother.setTargetValue(gritTarget);
        widthSmoother.setTargetValue(widthTarget);
        mixSmoother.setTargetValue(mixTarget);
        outputSmoother.setTargetValue(outputTarget);
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        currentLowPassCoefficient += coefficientSmoothing
                                   * (targetLowPassCoefficient - currentLowPassCoefficient);
        currentHighPassCoefficient += coefficientSmoothing
                                    * (targetHighPassCoefficient - currentHighPassCoefficient);
        currentHeadCoefficient += coefficientSmoothing
                                * (targetHeadCoefficient - currentHeadCoefficient);

        auto sampleParameters = targetParameters;
        sampleParameters.age = ageSmoother.getNextValue();
        sampleParameters.wear = wearSmoother.getNextValue();
        sampleParameters.wow = wowSmoother.getNextValue();
        sampleParameters.noise = noiseSmoother.getNextValue();
        sampleParameters.grit = gritSmoother.getNextValue();
        sampleParameters.width = widthSmoother.getNextValue();

        const auto dryLeft = left[sample];
        const auto dryRight = right != nullptr ? right[sample] : 0.0f;
        const auto drive = driveSmoother.getNextValue();
        auto wetLeft = processChannel(dryLeft, 0, sampleParameters, drive,
                                      currentLowPassCoefficient,
                                      currentHighPassCoefficient,
                                      currentHeadCoefficient);
        auto wetRight = right != nullptr
                      ? processChannel(dryRight, 1, sampleParameters, drive,
                                       currentLowPassCoefficient,
                                       currentHighPassCoefficient,
                                       currentHeadCoefficient)
                      : 0.0f;

        if (right != nullptr)
        {
            const auto mid = 0.5f * (wetLeft + wetRight);
            const auto side = 0.5f * (wetLeft - wetRight)
                            * juce::jlimit(0.0f, 1.5f, sampleParameters.width * 0.01f);
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
