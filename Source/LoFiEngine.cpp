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

float processTptBandPass(float input, float g, float k, float& ic1, float& ic2) noexcept
{
    const auto denominator = 1.0f + g * (g + k);
    const auto v1 = (ic1 + g * (input - ic2)) / denominator;
    const auto v2 = ic2 + g * v1;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    return v1;
}

}

void LoFiEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    hostSampleRate = juce::jmax(1.0, spec.sampleRate);
    maximumBlockSize = juce::jmax(1, static_cast<int>(spec.maximumBlockSize));

    // Keep the non-linear stages close to 192 kHz without wasting CPU when the
    // host is already running at a high sample rate. The IIR half-band filters
    // add very little latency and their integer-latency mode lets the dry path
    // remain exactly aligned for parallel processing.
    const auto factorPower = hostSampleRate <= 50000.0 ? 2u
                           : hostSampleRate <= 100000.0 ? 1u : 0u;
    oversamplingFactor = 1 << factorPower;
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        maxChannels, factorPower,
        juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR,
        true, true);
    oversampling->initProcessing(static_cast<std::size_t>(maximumBlockSize));
    oversamplingLatencySamples = juce::roundToInt(oversampling->getLatencyInSamples());

    processingSampleRate = hostSampleRate * static_cast<double>(oversamplingFactor);
    noiseOversamplingCompensation = std::sqrt(static_cast<float>(oversamplingFactor));

    // The deepest modulation is just over 9 ms. 12 ms leaves a small safety
    // margin for interpolation and transport drift.
    const auto delaySize = static_cast<std::size_t>(std::ceil(processingSampleRate * 0.012)) + 8u;

    for (auto& line : delayLines)
        line.assign(delaySize, 0.0f);
    for (auto& line : dryDelayLines)
        line.assign(static_cast<std::size_t>(oversamplingLatencySamples + 1), 0.0f);
    for (auto& scratch : dryScratch)
        scratch.assign(static_cast<std::size_t>(maximumBlockSize), 0.0f);

    driveSmoother.reset(processingSampleRate, 0.025);
    ageSmoother.reset(processingSampleRate, 0.040);
    wearSmoother.reset(processingSampleRate, 0.040);
    wowSmoother.reset(processingSampleRate, 0.040);
    noiseSmoother.reset(processingSampleRate, 0.040);
    gritSmoother.reset(processingSampleRate, 0.040);
    widthSmoother.reset(processingSampleRate, 0.040);
    mixSmoother.reset(hostSampleRate, 0.025);
    outputSmoother.reset(hostSampleRate, 0.025);
    coefficientSmoothing = 1.0f - std::exp(-1.0f
        / static_cast<float>(processingSampleRate * 0.020));
    rumbleCoefficient = onePoleCoefficient(32.0f, processingSampleRate);
    hissColourCoefficient = onePoleCoefficient(250.0f, processingSampleRate);
    surfaceColourCoefficient = onePoleCoefficient(680.0f, processingSampleRate);
    dropoutAttackCoefficient = 1.0f - std::exp(-1.0f
        / static_cast<float>(processingSampleRate * 0.00075));
    dropoutReleaseCoefficient = 1.0f - std::exp(-1.0f
        / static_cast<float>(processingSampleRate * 0.0052));
    dcBlockCoefficient = std::exp(-twoPi * 12.0f
        / static_cast<float>(processingSampleRate));
    magneticYoungCoefficient = onePoleCoefficient(5200.0f, processingSampleRate);
    magneticOldCoefficient = onePoleCoefficient(2100.0f, processingSampleRate);
    magneticEnvelopeAttackCoefficient = onePoleCoefficient(140.0f, processingSampleRate);
    magneticEnvelopeReleaseCoefficient = onePoleCoefficient(18.0f, processingSampleRate);
    speakerEnvelopeAttackCoefficient = onePoleCoefficient(180.0f, processingSampleRate);
    speakerEnvelopeReleaseCoefficient = onePoleCoefficient(14.0f, processingSampleRate);
    transportDriftCoefficient = onePoleCoefficient(0.55f, processingSampleRate);
    flutterJitterCoefficient = onePoleCoefficient(11.0f, processingSampleRate);

    for (std::size_t index = 0; index < standardHissGains.size(); ++index)
    {
        const auto amount = static_cast<float>(index) * 0.01f;
        standardHissGains[index] = juce::Decibels::decibelsToGain(-76.0f + amount * 43.0f);
        fourTrackHissGains[index] = juce::Decibels::decibelsToGain(-72.0f + amount * 43.0f);
        cellarHissGains[index] = juce::Decibels::decibelsToGain(-78.0f + amount * 43.0f);
        vinylRumbleGains[index] = juce::Decibels::decibelsToGain(-52.0f + amount * 22.0f);
        vinylCrackleGains[index] = juce::Decibels::decibelsToGain(-22.0f + amount * 12.0f);
        vinylSurfaceGains[index] = juce::Decibels::decibelsToGain(-58.0f + amount * 32.0f);
    }
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
        std::fill(dryDelayLines[static_cast<std::size_t>(channel)].begin(),
                  dryDelayLines[static_cast<std::size_t>(channel)].end(), 0.0f);
        dryDelayWritePositions[static_cast<std::size_t>(channel)] = 0;
        std::fill(dryScratch[static_cast<std::size_t>(channel)].begin(),
                  dryScratch[static_cast<std::size_t>(channel)].end(), 0.0f);
    }

    if (oversampling != nullptr)
        oversampling->reset();

    parametersInitialised = false;
    motionRandomState = 0xa341316cu;
    transportDrift = 0.0f;
    transportDriftTarget = 0.0f;
    flutterJitter = 0.0f;
    transportDriftSamples = 0;
    wowPhase = 0.0;
    flutterPhase = 0.0;
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

float LoFiEngine::nextMotionRandom() noexcept
{
    auto x = motionRandomState;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    motionRandomState = x;
    return static_cast<float>(x) * (1.0f / 4294967295.0f) * 2.0f - 1.0f;
}

float LoFiEngine::lookupGain(const std::array<float, 101>& table, float percent) noexcept
{
    const auto position = juce::jlimit(0.0f, 100.0f, percent);
    const auto index = static_cast<std::size_t>(position);
    if (index >= table.size() - 1u)
        return table.back();
    const auto fraction = position - static_cast<float>(index);
    return table[index] + fraction * (table[index + 1u] - table[index]);
}

LoFiEngine::ArtifactGains LoFiEngine::calculateArtifactGains(int machine,
                                                              float noisePercent) const noexcept
{
    ArtifactGains gains;
    if (machine == 1)
    {
        gains.rumble = lookupGain(vinylRumbleGains, noisePercent);
        gains.crackle = lookupGain(vinylCrackleGains, noisePercent);
        gains.surface = lookupGain(vinylSurfaceGains, noisePercent);
    }
    else
    {
        const auto& table = machine == 2 ? fourTrackHissGains
                          : machine == 3 ? cellarHissGains : standardHissGains;
        gains.hiss = lookupGain(table, noisePercent);
    }
    return gains;
}

float LoFiEngine::calculateMotionDelaySamples(int machine, float wowAmount) noexcept
{
    const auto slowRate = machine == 1 ? 0.31 : (machine == 2 ? 0.46 : 0.23);
    const auto fastRate = machine == 1 ? 2.1 : (machine == 2 ? 6.7 : 5.2);

    if (transportDriftSamples <= 0)
    {
        transportDriftTarget = nextMotionRandom();
        const auto intervalSeconds = 0.35f + 1.1f * (nextMotionRandom() * 0.5f + 0.5f);
        transportDriftSamples = juce::jmax(1, static_cast<int>(processingSampleRate
                                                               * intervalSeconds));
    }
    --transportDriftSamples;

    transportDrift += transportDriftCoefficient * (transportDriftTarget - transportDrift);
    flutterJitter += flutterJitterCoefficient * (nextMotionRandom() - flutterJitter);

    const auto driftedSlowRate = slowRate * (1.0f + 0.075f * transportDrift);
    const auto driftedFastRate = fastRate * (1.0f + 0.035f * transportDrift);
    wowPhase += twoPi * driftedSlowRate / processingSampleRate;
    flutterPhase += twoPi * driftedFastRate / processingSampleRate;

    if (wowPhase >= twoPi)
        wowPhase -= twoPi;
    if (flutterPhase >= twoPi)
        flutterPhase -= twoPi;

    if (wowAmount < 0.0001f)
        return 0.0f;

    const auto jitter = juce::jlimit(-1.0f, 1.0f, flutterJitter * 9.0f);
    const auto slowWeight = machine == 1 ? 0.79f : 0.66f;
    const auto modulation = slowWeight * std::sin(static_cast<float>(wowPhase))
                          + (1.0f - slowWeight - 0.07f)
                              * std::sin(static_cast<float>(flutterPhase))
                          + 0.045f * transportDrift + 0.025f * jitter;
    const auto depthMs = machine == 1 ? 4.4f : machine == 2 ? 3.8f
                       : machine == 3 ? 1.1f : machine == 4 ? 0.8f : 2.6f;
    const auto depthSamples = depthMs * 0.001f
                            * static_cast<float>(processingSampleRate) * wowAmount;
    // Three samples of fixed delay keep all four points of the cubic reader in
    // valid history. At the oversampled rate this is well below audibility.
    return 3.0f + depthSamples * (1.05f + modulation);
}

float LoFiEngine::readModulatedDelay(int channel, float input, float delaySamples)
{
    auto& line = delayLines[static_cast<std::size_t>(channel)];
    auto& writePosition = delayWritePositions[static_cast<std::size_t>(channel)];
    const auto lineSize = static_cast<int>(line.size());

    line[static_cast<std::size_t>(writePosition)] = input;

    if (delaySamples <= 0.0f)
    {
        if (++writePosition >= lineSize)
            writePosition = 0;
        return input;
    }

    auto readPosition = static_cast<float>(writePosition) - delaySamples;
    if (readPosition < 0.0f)
        readPosition += static_cast<float>(lineSize);

    const auto index1 = static_cast<int>(readPosition);
    const auto index0 = index1 > 0 ? index1 - 1 : lineSize - 1;
    const auto index2 = index1 + 1 < lineSize ? index1 + 1 : 0;
    const auto index3 = index2 + 1 < lineSize ? index2 + 1 : 0;
    const auto fraction = readPosition - static_cast<float>(index1);
    const auto sample0 = line[static_cast<std::size_t>(index0)];
    const auto sample1 = line[static_cast<std::size_t>(index1)];
    const auto sample2 = line[static_cast<std::size_t>(index2)];
    const auto sample3 = line[static_cast<std::size_t>(index3)];
    const auto delayed = sample1 + 0.5f * fraction
        * (sample2 - sample0 + fraction
            * (2.0f * sample0 - 5.0f * sample1 + 4.0f * sample2 - sample3
                + fraction * (3.0f * (sample1 - sample2) + sample3 - sample0)));

    if (++writePosition >= lineSize)
        writePosition = 0;
    return delayed;
}

float LoFiEngine::delayDrySample(int channel, float input) noexcept
{
    if (oversamplingLatencySamples <= 0)
        return input;

    auto& line = dryDelayLines[static_cast<std::size_t>(channel)];
    auto& writePosition = dryDelayWritePositions[static_cast<std::size_t>(channel)];
    const auto lineSize = static_cast<int>(line.size());
    const auto readPosition = (writePosition + lineSize - oversamplingLatencySamples) % lineSize;
    const auto delayed = line[static_cast<std::size_t>(readPosition)];
    line[static_cast<std::size_t>(writePosition)] = input;
    if (++writePosition >= lineSize)
        writePosition = 0;
    return delayed;
}

float LoFiEngine::applySafetyCeiling(float sample) noexcept
{
    constexpr float threshold = 0.74f;
    constexpr float ceiling = 0.86f;
    const auto magnitude = std::abs(sample);
    if (magnitude <= threshold)
        return sample;
    const auto limited = threshold + (ceiling - threshold)
                                  * std::tanh((magnitude - threshold) / (ceiling - threshold));
    return std::copysign(limited, sample);
}

float LoFiEngine::applyReconstructionCeiling(float sample) noexcept
{
    constexpr float threshold = 0.94f;
    constexpr float ceiling = 0.985f;
    const auto magnitude = std::abs(sample);
    if (magnitude <= threshold)
        return sample;
    const auto limited = threshold + (ceiling - threshold)
        * std::tanh((magnitude - threshold) / (ceiling - threshold));
    return std::copysign(limited, sample);
}

float LoFiEngine::processChannel(float input, int channel, const Parameters& p,
                                 float lowPassCoefficient, float highPassCoefficient,
                                 float headCoefficient, float magneticCoefficient,
                                 float motionDelaySamples, const ArtifactGains& artifactGains,
                                 const SaturationSettings& saturation,
                                 const DigitalSettings& digital,
                                 const SpeakerSettings& speaker)
{
    auto& state = states[static_cast<std::size_t>(channel)];
    const auto age = juce::jlimit(0.0f, 1.0f, p.age * 0.01f);
    const auto wear = juce::jlimit(0.0f, 1.0f, p.wear * 0.01f);
    const auto grit = juce::jlimit(0.0f, 1.0f, p.grit * 0.01f);

    state.inputLow += highPassCoefficient * (input - state.inputLow);
    auto value = input - state.inputLow;

    auto saturationInput = value;
    if (p.machine == 0 || p.machine == 2)
    {
        const auto magnitude = std::abs(value);
        const auto envelopeCoefficient = magnitude > state.magneticEnvelope
                                       ? magneticEnvelopeAttackCoefficient
                                       : magneticEnvelopeReleaseCoefficient;
        state.magneticEnvelope += envelopeCoefficient
                                * (magnitude - state.magneticEnvelope);

        const auto normalisedSlew = std::abs(value - state.magneticPreviousInput)
            * static_cast<float>(processingSampleRate / 48000.0);
        state.magneticPreviousInput = value;
        const auto boundedSlew = juce::jmin(8.0f, normalisedSlew);
        const auto rateLoss = 1.0f / (1.0f + boundedSlew
            * (0.018f + 0.055f * age + (p.machine == 2 ? 0.015f : 0.0f)));
        const auto tracking = juce::jmin(1.0f, magneticCoefficient
            * (p.machine == 2 ? 0.78f : 1.0f) / (1.0f + 0.10f * boundedSlew));
        const auto excitation = 1.05f + 0.55f * age + 0.22f * wear
                              + (p.machine == 2 ? 0.18f : 0.0f);
        const auto magneticTarget = std::tanh(value * rateLoss * excitation
            + state.hysteresis * (0.10f + 0.12f * age));
        state.hysteresis += tracking * (magneticTarget - state.hysteresis);

        const auto compression = 1.0f / (1.0f + state.magneticEnvelope
            * (0.10f + 0.28f * age + (p.machine == 2 ? 0.12f : 0.0f)));
        const auto memoryAmount = 0.06f + 0.16f * age
                                + (p.machine == 2 ? 0.05f : 0.0f);
        saturationInput = value * rateLoss * compression
                        + memoryAmount * state.hysteresis;
    }
    else
    {
        state.magneticPreviousInput = value;
        state.magneticEnvelope += magneticEnvelopeReleaseCoefficient
                                * (0.0f - state.magneticEnvelope);
        state.hysteresis += magneticCoefficient * (0.0f - state.hysteresis);
    }

    const auto biased = saturationInput * saturation.drive
                      + saturation.asymmetry;
    value = (std::tanh(biased) - saturation.biasCorrection) * saturation.normalisation;

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
            && (nextRandom(state) * 0.5f + 0.5f)
                < eventsPerSecond / static_cast<float>(processingSampleRate))
        {
            state.dropoutSamples = static_cast<int>(processingSampleRate
                * (0.006 + 0.045 * wear * (nextRandom(state) * 0.5f + 0.5f)));
        }

        const auto dropoutTarget = state.dropoutSamples > 0 ? 1.0f - (0.18f + 0.68f * wear) : 1.0f;
        state.dropoutGain += (state.dropoutSamples > 0 ? dropoutAttackCoefficient
                                                       : dropoutReleaseCoefficient)
                           * (dropoutTarget - state.dropoutGain);
        value *= state.dropoutGain;
        if (state.dropoutSamples > 0)
            --state.dropoutSamples;
    }

    auto artefact = 0.0f;
    const auto white = nextRandom(state) * noiseOversamplingCompensation;

    if (p.machine == 1)
    {
        state.rumble += rumbleCoefficient * (white - state.rumble);
        artefact += state.rumble * artifactGains.rumble;

        const auto cracklesPerSecond = 0.18f + wear * wear * 24.0f;
        if ((nextRandom(state) * 0.5f + 0.5f)
            < cracklesPerSecond / static_cast<float>(processingSampleRate))
        {
            const auto brightness = nextRandom(state) * 0.5f + 0.5f;
            const auto frequency = 1700.0f + 5600.0f * brightness;
            const auto angularFrequency = twoPi * frequency
                                        / static_cast<float>(processingSampleRate);
            const auto decaySeconds = 0.00022f + 0.0013f
                * (nextRandom(state) * 0.5f + 0.5f);
            const auto pole = std::exp(-1.0f
                / (decaySeconds * static_cast<float>(processingSampleRate)));
            state.crackleCoefficient = 2.0f * pole
                                      * std::cos(angularFrequency);
            state.cracklePoleSquared = pole * pole;
            const auto amplitude = 0.25f + 0.75f
                * (nextRandom(state) * 0.5f + 0.5f);
            state.crackleCurrent += (nextRandom(state) >= 0.0f ? 1.0f : -1.0f)
                                  * amplitude * std::sin(angularFrequency)
                                  * noiseOversamplingCompensation;
        }
        artefact += state.crackleCurrent * artifactGains.crackle;
        const auto nextCrackle = state.crackleCoefficient * state.crackleCurrent
                               - state.cracklePoleSquared * state.cracklePrevious;
        state.cracklePrevious = state.crackleCurrent;
        state.crackleCurrent = nextCrackle;

        state.noiseLow += surfaceColourCoefficient * (white - state.noiseLow);
        const auto dustScratch = (white - state.noiseLow) * (0.25f + 0.75f * wear);
        const auto surfaceTexture = 0.72f * state.noiseLow + 0.28f * dustScratch;
        artefact += surfaceTexture * artifactGains.surface;
    }
    else
    {
        state.noiseLow += hissColourCoefficient * (white - state.noiseLow);
        const auto hiss = white - state.noiseLow * 0.78f;
        artefact += hiss * artifactGains.hiss;
    }

    value += artefact;

    if (p.machine == 3)
    {
        const auto body = processTptBandPass(value, speaker.bodyG, speaker.bodyK,
                                             state.speakerBodyIc1, state.speakerBodyIc2);
        const auto presence = processTptBandPass(value, speaker.presenceG, speaker.presenceK,
                                                 state.speakerPresenceIc1,
                                                 state.speakerPresenceIc2);
        const auto magnitude = std::abs(value);
        const auto envelopeCoefficient = magnitude > state.speakerEnvelope
                                       ? speakerEnvelopeAttackCoefficient
                                       : speakerEnvelopeReleaseCoefficient;
        state.speakerEnvelope += envelopeCoefficient
                               * (magnitude - state.speakerEnvelope);

        const auto bodyMix = 0.12f + 0.12f * age + 0.18f * wear;
        const auto presenceMix = 0.07f + 0.12f * grit;
        auto resonant = 0.72f * value + bodyMix * body + presenceMix * presence;
        const auto excursion = juce::jlimit(0.0f, 1.0f,
            state.speakerEnvelope * (0.65f + 0.65f * wear)
                + 0.12f * std::abs(state.speakerBodyIc2));
        const auto coneCoefficient = speaker.coneOpenCoefficient
            + excursion * (speaker.coneClosedCoefficient - speaker.coneOpenCoefficient);
        state.speakerConeLow += coneCoefficient * (resonant - state.speakerConeLow);
        resonant = state.speakerConeLow
                 / (1.0f + state.speakerEnvelope * (0.10f + 0.24f * wear));

        const auto positiveDrive = 1.0f + 4.0f * grit + 1.2f * wear;
        const auto negativeDrive = 1.0f + 2.7f * grit + 0.8f * wear;
        auto shaped = std::tanh(resonant * (resonant >= 0.0f ? positiveDrive
                                                             : negativeDrive));
        shaped /= 1.0f + 0.30f * grit + 0.10f * wear;
        const auto nonlinearMix = juce::jlimit(0.0f, 1.0f,
                                               0.22f + 0.62f * grit + 0.16f * wear);
        value = resonant + nonlinearMix * (shaped - resonant);
    }
    else
    {
        const auto decay = 1.0f - speakerEnvelopeReleaseCoefficient;
        state.speakerBodyIc1 *= decay;
        state.speakerBodyIc2 *= decay;
        state.speakerPresenceIc1 *= decay;
        state.speakerPresenceIc2 *= decay;
        state.speakerConeLow *= decay;
        state.speakerEnvelope *= decay;
    }

    // Grit remains a continuous, machine-specific non-linearity on the physical
    // models. Sample-rate and bit-depth damage belong exclusively to Bitrot.
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

    if (digital.holdFactor > 1)
    {
        if (state.holdCounter <= 0)
        {
            state.heldSample = value;
            state.holdCounter = digital.holdFactor;
        }
        --state.holdCounter;
        value = state.heldSample;
    }
    else
    {
        state.heldSample = value;
        state.holdCounter = 0;
    }

    if (digital.quantisationLevels > 0.0f)
        value = std::round(value * digital.quantisationLevels) / digital.quantisationLevels;

    value = readModulatedDelay(channel, value, motionDelaySamples);

    const auto dcBlocked = value - state.dcInput + dcBlockCoefficient * state.dcOutput;
    state.dcInput = value;
    state.dcOutput = dcBlocked;
    return dcBlocked;
}

void LoFiEngine::process(juce::AudioBuffer<float>& buffer, const Parameters& p, bool bypassed)
{
    const auto numChannels = juce::jmin(maxChannels, buffer.getNumChannels());
    const auto numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0 || oversampling == nullptr)
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
    const auto cutoff = juce::jlimit(1800.0f,
                                     static_cast<float>(processingSampleRate * 0.45),
                                     baseCutoff * std::pow(2.0f, toneOctaves));
    targetLowPassCoefficient = onePoleCoefficient(cutoff, processingSampleRate);
    targetHighPassCoefficient = onePoleCoefficient(highPass, processingSampleRate);
    targetHeadCoefficient = onePoleCoefficient(targetParameters.machine == 2 ? 125.0f : 92.0f,
                                                processingSampleRate);

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

    auto fullBlock = juce::dsp::AudioBlock<float>(buffer)
        .getSubsetChannelBlock(0, static_cast<std::size_t>(numChannels));

    for (int blockStart = 0; blockStart < numSamples; blockStart += maximumBlockSize)
    {
        const auto blockSamples = juce::jmin(maximumBlockSize, numSamples - blockStart);
        auto baseBlock = fullBlock.getSubBlock(static_cast<std::size_t>(blockStart),
                                               static_cast<std::size_t>(blockSamples));

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto* input = baseBlock.getChannelPointer(static_cast<std::size_t>(channel));
            auto& scratch = dryScratch[static_cast<std::size_t>(channel)];
            for (int sample = 0; sample < blockSamples; ++sample)
                scratch[static_cast<std::size_t>(sample)] = delayDrySample(channel, input[sample]);
        }

        auto oversampledBlock = oversampling->processSamplesUp(
            juce::dsp::AudioBlock<const float>(baseBlock));
        processOversampledBlock(oversampledBlock, targetParameters);
        oversampling->processSamplesDown(baseBlock);

        auto* wetLeft = baseBlock.getChannelPointer(0);
        auto* wetRight = numChannels > 1 ? baseBlock.getChannelPointer(1) : nullptr;
        const auto& dryLeft = dryScratch[0];
        const auto& dryRight = dryScratch[1];

        for (int sample = 0; sample < blockSamples; ++sample)
        {
            const auto mix = mixSmoother.getNextValue();
            const auto output = outputSmoother.getNextValue();
            if (bypassed)
            {
                wetLeft[sample] = dryLeft[static_cast<std::size_t>(sample)];
                if (wetRight != nullptr)
                    wetRight[sample] = dryRight[static_cast<std::size_t>(sample)];
                continue;
            }

            const auto protectedWetLeft = applyReconstructionCeiling(wetLeft[sample]);
            const auto mixedLeft = dryLeft[static_cast<std::size_t>(sample)]
                + mix * (protectedWetLeft - dryLeft[static_cast<std::size_t>(sample)]);
            wetLeft[sample] = mixedLeft * output;

            if (wetRight != nullptr)
            {
                const auto protectedWetRight = applyReconstructionCeiling(wetRight[sample]);
                const auto mixedRight = dryRight[static_cast<std::size_t>(sample)]
                    + mix * (protectedWetRight - dryRight[static_cast<std::size_t>(sample)]);
                wetRight[sample] = mixedRight * output;
            }
        }
    }
}

void LoFiEngine::processOversampledBlock(juce::dsp::AudioBlock<float>& block,
                                         const Parameters& targetParameters)
{
    const auto numChannels = static_cast<int>(block.getNumChannels());
    const auto numSamples = static_cast<int>(block.getNumSamples());
    auto* left = block.getChannelPointer(0);
    auto* right = numChannels > 1 ? block.getChannelPointer(1) : nullptr;

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

        const auto normalisedAge = sampleParameters.age * 0.01f;
        const auto normalisedWear = sampleParameters.wear * 0.01f;
        const auto normalisedGrit = sampleParameters.grit * 0.01f;
        const auto magneticCoefficient = magneticYoungCoefficient
            + normalisedAge * (magneticOldCoefficient - magneticYoungCoefficient);
        const auto artifactGains = calculateArtifactGains(sampleParameters.machine,
                                                          sampleParameters.noise);
        const auto wowAmount = std::pow(sampleParameters.wow * 0.01f, 1.35f);
        const auto motionDelaySamples = calculateMotionDelaySamples(sampleParameters.machine,
                                                                    wowAmount);

        const auto dryLeft = left[sample];
        const auto dryRight = right != nullptr ? right[sample] : 0.0f;
        const auto drive = driveSmoother.getNextValue();

        SaturationSettings saturation;
        saturation.drive = drive;
        switch (sampleParameters.machine)
        {
            case 1: saturation.drive *= 0.72f; break;
            case 2:
                saturation.drive *= 1.22f;
                saturation.asymmetry = 0.035f + 0.04f * normalisedAge;
                break;
            case 3:
                saturation.drive *= 1.35f;
                saturation.asymmetry = 0.055f;
                break;
            case 4: saturation.drive *= 0.86f; break;
            default: saturation.asymmetry = 0.018f + 0.025f * normalisedAge; break;
        }
        saturation.biasCorrection = std::tanh(saturation.asymmetry);
        saturation.normalisation = 1.0f
            / juce::jmax(1.0f, std::tanh(saturation.drive) * 1.12f);

        DigitalSettings digital;
        if (sampleParameters.machine == 4)
        {
            const auto baseHoldFactor = 1 + static_cast<int>(normalisedGrit * normalisedGrit
                * 38.0f + normalisedWear * normalisedWear * 4.0f);
            digital.holdFactor = 1 + (baseHoldFactor - 1) * oversamplingFactor;
            const auto bitDepth = 15.0f - normalisedGrit * 11.0f
                                         - normalisedWear * 1.5f;
            digital.quantisationLevels = std::exp2(juce::jlimit(3.0f, 16.0f, bitDepth));
        }

        SpeakerSettings speaker;
        if (sampleParameters.machine == 3)
        {
            const auto bodyFrequency = 145.0f + 55.0f * normalisedWear
                                      - 12.0f * normalisedAge;
            const auto bodyQ = 1.15f + 1.20f * normalisedWear;
            const auto presenceFrequency = 1450.0f - 250.0f * normalisedAge
                                          + 350.0f * normalisedGrit;
            const auto presenceQ = 0.75f + 0.80f * normalisedGrit;
            speaker.bodyG = std::tan(0.5f * twoPi * bodyFrequency
                                     / static_cast<float>(processingSampleRate));
            speaker.bodyK = 1.0f / bodyQ;
            speaker.presenceG = std::tan(0.5f * twoPi * presenceFrequency
                                         / static_cast<float>(processingSampleRate));
            speaker.presenceK = 1.0f / presenceQ;
            speaker.coneOpenCoefficient = onePoleCoefficient(5200.0f
                - 1400.0f * normalisedAge, processingSampleRate);
            speaker.coneClosedCoefficient = onePoleCoefficient(2300.0f
                - 650.0f * normalisedAge, processingSampleRate);
        }

        auto wetLeft = processChannel(dryLeft, 0, sampleParameters,
                                      currentLowPassCoefficient,
                                      currentHighPassCoefficient,
                                      currentHeadCoefficient,
                                      magneticCoefficient,
                                      motionDelaySamples,
                                      artifactGains,
                                      saturation,
                                      digital,
                                      speaker);
        auto wetRight = right != nullptr
                      ? processChannel(dryRight, 1, sampleParameters,
                                       currentLowPassCoefficient,
                                       currentHighPassCoefficient,
                                       currentHeadCoefficient,
                                       magneticCoefficient,
                                       motionDelaySamples,
                                       artifactGains,
                                       saturation,
                                       digital,
                                       speaker)
                      : 0.0f;

        if (right != nullptr)
        {
            const auto mid = 0.5f * (wetLeft + wetRight);
            const auto side = 0.5f * (wetLeft - wetRight)
                            * juce::jlimit(0.0f, 1.5f, sampleParameters.width * 0.01f);
            wetLeft = mid + side;
            wetRight = mid - side;
        }

        left[sample] = applySafetyCeiling(wetLeft);
        if (right != nullptr)
            right[sample] = applySafetyCeiling(wetRight);
    }
}
