#include "PluginEditor.h"

#include <cmath>

namespace ThemeColours
{
const auto ink = juce::Colour::fromRGB(25, 24, 21);
const auto paper = juce::Colour::fromRGB(220, 211, 187);
const auto panel = juce::Colour::fromRGB(199, 188, 159);
const auto shadow = juce::Colour::fromRGB(73, 67, 56);
const auto acid = juce::Colour::fromRGB(218, 74, 42);
const auto amber = juce::Colour::fromRGB(232, 156, 48);
const auto dim = juce::Colour::fromRGB(136, 127, 107);
}

AssEffectLookAndFeel::AssEffectLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, ThemeColours::ink);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::textColourId, ThemeColours::paper);
    setColour(juce::ComboBox::backgroundColourId, ThemeColours::ink);
    setColour(juce::ComboBox::outlineColourId, ThemeColours::shadow);
    setColour(juce::PopupMenu::backgroundColourId, ThemeColours::ink);
    setColour(juce::PopupMenu::textColourId, ThemeColours::paper);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, ThemeColours::acid);
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void AssEffectLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width,
                                             int height, float sliderPosition,
                                             float rotaryStartAngle, float rotaryEndAngle,
                                             juce::Slider& slider)
{
    const auto diameter = static_cast<float>(juce::jmin(width, height)) - 15.0f;
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                static_cast<float>(width), static_cast<float>(height))
                            .withSizeKeepingCentre(diameter, diameter)
                            .translated(0.0f, -1.0f);
    const auto radius = bounds.getWidth() * 0.5f;
    const auto angle = rotaryStartAngle + sliderPosition * (rotaryEndAngle - rotaryStartAngle);
    const auto centre = bounds.getCentre();
    const auto enabledAlpha = slider.isEnabled() ? 1.0f : 0.4f;

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f,
                        rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(ThemeColours::shadow.withAlpha(0.32f * enabledAlpha));
    g.strokePath(track, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f,
                           rotaryStartAngle, angle, true);
    g.setColour(ThemeColours::acid.withAlpha(enabledAlpha));
    g.strokePath(valueArc, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    g.setColour(ThemeColours::ink.withAlpha(enabledAlpha));
    g.fillEllipse(bounds.reduced(7.0f));
    g.setColour(ThemeColours::panel.withAlpha(0.16f * enabledAlpha));
    g.drawEllipse(bounds.reduced(9.0f), 1.2f);

    const auto pointerLength = radius * 0.55f;
    const auto pointerStart = centre + juce::Point<float>(0.0f, -radius * 0.18f).rotatedAboutOrigin(angle);
    const auto pointerEnd = centre + juce::Point<float>(0.0f, -pointerLength).rotatedAboutOrigin(angle);
    g.setColour(ThemeColours::paper.withAlpha(enabledAlpha));
    g.drawLine({ pointerStart, pointerEnd }, 3.0f);
    g.setColour(ThemeColours::acid.withAlpha(enabledAlpha));
    g.fillEllipse(juce::Rectangle<float>(5.0f, 5.0f).withCentre(centre));
}

void AssEffectLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                        bool isButtonDown, int, int, int, int,
                                        juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width),
                                                static_cast<float>(height)).reduced(0.5f);
    g.setColour(isButtonDown ? ThemeColours::shadow : ThemeColours::ink);
    g.fillRoundedRectangle(bounds, 7.0f);
    g.setColour(box.hasKeyboardFocus(true) ? ThemeColours::amber : ThemeColours::shadow);
    g.drawRoundedRectangle(bounds, 7.0f, 1.0f);

    juce::Path arrow;
    const auto cx = static_cast<float>(width - 17);
    const auto cy = static_cast<float>(height) * 0.5f;
    arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
    arrow.lineTo(cx, cy + 2.0f);
    arrow.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(ThemeColours::amber);
    g.strokePath(arrow, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
}

void AssEffectLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                            bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    auto fill = button.getToggleState() ? ThemeColours::acid : ThemeColours::ink;
    if (highlighted || down)
        fill = fill.brighter(0.12f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 7.0f);
    g.setColour(button.getToggleState() ? juce::Colours::white : ThemeColours::paper);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
}

AssEffectParameterKnob::AssEffectParameterKnob(juce::AudioProcessorValueTreeState& state,
                                               const juce::String& parameterID,
                                               const juce::String& caption,
                                               const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 20);
    slider.setTextValueSuffix(suffix);
    if (suffix.containsIgnoreCase("dB"))
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
    slider.setMouseDragSensitivity(180);
    slider.setScrollWheelEnabled(true);
    slider.setName(caption);
    slider.setTitle(caption);
    addAndMakeVisible(slider);

    label.setText(caption.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, ThemeColours::ink);
    label.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    label.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, parameterID, slider);
}

void AssEffectParameterKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(22));
    slider.setBounds(area);
}

AssEffectAudioProcessorEditor::AssEffectAudioProcessorEditor(AssEffectAudioProcessor& owner)
    : AudioProcessorEditor(&owner), processor(owner)
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(760, 500, 1280, 800);
    getConstrainer()->setFixedAspectRatio(1.6);

    presetLabel.setText("SOURCE / PRESET", juce::dontSendNotification);
    machineLabel.setText("MACHINE", juce::dontSendNotification);
    for (auto* label : { &presetLabel, &machineLabel })
    {
        label->setColour(juce::Label::textColourId, ThemeColours::ink);
        label->setFont(juce::FontOptions(11.0f, juce::Font::bold));
        label->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(*label);
    }

    presets.setTextWhenNothingSelected("CUSTOM / MODIFIED");
    int presetID = 1;
    juce::String currentCategory;
    for (const auto& preset : AssEffectAudioProcessor::getFactoryPresets())
    {
        if (currentCategory != preset.category)
        {
            currentCategory = preset.category;
            presets.addSectionHeading(currentCategory);
        }
        presets.addItem(preset.name, presetID++);
    }
    presets.onChange = [this]
    {
        const auto selectedPresetID = presets.getSelectedId();
        if (selectedPresetID > 0)
            processor.loadFactoryPreset(selectedPresetID - 1);
    };
    presets.setSelectedId(processor.getCurrentFactoryPresetIndex() + 1,
                          juce::dontSendNotification);
    lastPresetRevision = processor.getPresetRevision();
    presets.setTooltip("Factory starting points for tracks, instruments and masters");
    addAndMakeVisible(presets);

    machine.addItemList({ "90s CASSETTE", "WORN VINYL", "4-TRACK DEMO",
                          "CELLAR SPEAKER", "BITROT SAMPLER" }, 1);
    machine.setTooltip("Select the physical or digital lo-fi machine");
    addAndMakeVisible(machine);

    bypass.setClickingTogglesState(true);
    bypass.setTooltip("Hard bypass");
    addAndMakeVisible(bypass);

    auto& state = processor.getValueTreeState();
    machineAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, ParameterIDs::machine, machine);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, ParameterIDs::bypass, bypass);

    const auto addKnob = [this, &state](const char* id, const char* name, const char* suffix)
    {
        auto knob = std::make_unique<AssEffectParameterKnob>(state, id, name, suffix);
        addAndMakeVisible(*knob);
        knobs.push_back(std::move(knob));
    };

    addKnob(ParameterIDs::drive, "Drive", " dB");
    addKnob(ParameterIDs::age, "Age", " %");
    addKnob(ParameterIDs::wear, "Wear", " %");
    addKnob(ParameterIDs::wow, "Wow / Flutter", " %");
    addKnob(ParameterIDs::noise, "Noise", " %");
    addKnob(ParameterIDs::grit, "Grit", " %");
    addKnob(ParameterIDs::tone, "Tone", "");
    addKnob(ParameterIDs::width, "Width", " %");
    addKnob(ParameterIDs::mix, "Mix", " %");
    addKnob(ParameterIDs::output, "Output", " dB");

    setSize(960, 600);
    startTimerHz(30);
}

AssEffectAudioProcessorEditor::~AssEffectAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void AssEffectAudioProcessorEditor::drawBrand(juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto mark = area.removeFromLeft(area.getHeight()).reduced(7.0f);
    g.setColour(ThemeColours::acid);
    g.fillEllipse(mark);

    const auto tape = mark.reduced(mark.getWidth() * 0.20f);
    g.setColour(ThemeColours::ink);
    g.fillRoundedRectangle(tape, 5.0f);
    g.setColour(ThemeColours::paper);
    g.drawEllipse(tape.withSizeKeepingCentre(tape.getWidth() * 0.28f,
                                             tape.getWidth() * 0.28f).translated(-tape.getWidth() * 0.21f, 0.0f), 2.2f);
    g.drawEllipse(tape.withSizeKeepingCentre(tape.getWidth() * 0.28f,
                                             tape.getWidth() * 0.28f).translated(tape.getWidth() * 0.21f, 0.0f), 2.2f);
    g.drawLine(tape.getX() + 8.0f, tape.getBottom() - 7.0f,
               tape.getRight() - 8.0f, tape.getBottom() - 7.0f, 2.0f);

    area.removeFromLeft(13.0f);
    auto titleArea = area.removeFromTop(area.getHeight() * 0.67f);
    g.setColour(ThemeColours::paper);
    g.setFont(juce::FontOptions(titleArea.getHeight() * 0.72f, juce::Font::bold));
    g.drawFittedText("ASS EFFECT", titleArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
    g.setColour(ThemeColours::amber);
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawFittedText("UNDERGROUND LO-FI MACHINE", area.toNearestInt(),
                     juce::Justification::centredLeft, 1);
}

void AssEffectAudioProcessorEditor::drawMeter(juce::Graphics& g, juce::Rectangle<float> area,
                                              float level, const juce::String& label)
{
    g.setColour(ThemeColours::paper.withAlpha(0.16f));
    g.fillRoundedRectangle(area, 2.0f);
    const auto normalized = juce::jlimit(0.0f, 1.0f,
        juce::jmap(juce::Decibels::gainToDecibels(level, -60.0f), -60.0f, 3.0f, 0.0f, 1.0f));
    auto lit = area;
    lit.removeFromTop(area.getHeight() * (1.0f - normalized));
    g.setColour(normalized > 0.88f ? ThemeColours::acid : ThemeColours::amber);
    g.fillRoundedRectangle(lit, 2.0f);
    g.setColour(ThemeColours::paper);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText(label, area.translated(0.0f, area.getHeight() + 2.0f).withHeight(10.0f).toNearestInt(),
               juce::Justification::centred);
}

void AssEffectAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(ThemeColours::paper);

    // A deterministic screen-print texture: visual grit without a runtime image dependency.
    juce::Random texture(0x41535345);
    g.setColour(ThemeColours::ink.withAlpha(0.045f));
    const auto specks = juce::jmax(90, getWidth() * getHeight() / 1800);
    for (int i = 0; i < specks; ++i)
    {
        const auto x = texture.nextFloat() * static_cast<float>(getWidth());
        const auto y = texture.nextFloat() * static_cast<float>(getHeight());
        const auto size = 0.5f + texture.nextFloat() * 1.4f;
        g.fillEllipse(x, y, size, size);
    }

    const auto scale = static_cast<float>(getWidth()) / 960.0f;
    const auto margin = 18.0f * scale;
    auto header = juce::Rectangle<float>(margin, margin,
        static_cast<float>(getWidth()) - 2.0f * margin, 78.0f * scale);
    g.setColour(ThemeColours::ink);
    g.fillRoundedRectangle(header, 12.0f * scale);
    drawBrand(g, header.reduced(12.0f * scale).removeFromLeft(475.0f * scale));

    drawMeter(g, { header.getRight() - 67.0f * scale, header.getY() + 13.0f * scale,
                   10.0f * scale, 40.0f * scale }, displayedInput, "IN");
    drawMeter(g, { header.getRight() - 43.0f * scale, header.getY() + 13.0f * scale,
                   10.0f * scale, 40.0f * scale }, displayedOutput, "OUT");

    const auto controlsTop = 171.0f * scale;
    const auto controlsBottom = static_cast<float>(getHeight()) - 35.0f * scale;
    const auto controls = juce::Rectangle<float>(margin, controlsTop,
        static_cast<float>(getWidth()) - 2.0f * margin, controlsBottom - controlsTop);
    const auto halfHeight = controls.getHeight() * 0.5f;
    g.setColour(ThemeColours::panel.withAlpha(0.55f));
    g.fillRoundedRectangle(controls.withHeight(halfHeight).reduced(0.0f, 4.0f * scale), 10.0f * scale);
    g.setColour(ThemeColours::panel.withAlpha(0.27f));
    g.fillRoundedRectangle(controls.withTop(controls.getY() + halfHeight).reduced(0.0f, 4.0f * scale), 10.0f * scale);

    g.setColour(ThemeColours::dim);
    g.setFont(juce::FontOptions(10.0f * scale, juce::Font::bold));
    g.drawText("DEGRADE / MOTION", static_cast<int>(margin + 10.0f * scale),
               static_cast<int>(controlsTop + 5.0f * scale), 170, 14, juce::Justification::centredLeft);
    g.drawText("DIRT / FINISH", static_cast<int>(margin + 10.0f * scale),
               static_cast<int>(controlsTop + halfHeight + 5.0f * scale), 170, 14,
               juce::Justification::centredLeft);

    g.setColour(ThemeColours::shadow);
    g.setFont(juce::FontOptions(9.5f * scale, juce::Font::bold));
    g.drawFittedText("NO POLISH. JUST CHARACTER.  /  v1.0.0",
                     juce::Rectangle<int>(static_cast<int>(margin), getHeight() - static_cast<int>(24.0f * scale),
                                          getWidth() - static_cast<int>(2.0f * margin), 13),
                     juce::Justification::centredRight, 1);
}

void AssEffectAudioProcessorEditor::resized()
{
    const auto scale = static_cast<float>(getWidth()) / 960.0f;
    const auto margin = static_cast<int>(18.0f * scale);
    const auto top = static_cast<int>(105.0f * scale);
    const auto toolbarHeight = static_cast<int>(55.0f * scale);
    auto toolbar = juce::Rectangle<int>(margin, top, getWidth() - 2 * margin, toolbarHeight);

    auto bypassArea = toolbar.removeFromRight(static_cast<int>(88.0f * scale));
    bypass.setBounds(bypassArea.reduced(2, static_cast<int>(12.0f * scale)));
    toolbar.removeFromRight(static_cast<int>(12.0f * scale));

    auto machineArea = toolbar.removeFromRight(static_cast<int>(280.0f * scale));
    machineLabel.setBounds(machineArea.removeFromTop(static_cast<int>(17.0f * scale)));
    machine.setBounds(machineArea.reduced(0, 1));
    toolbar.removeFromRight(static_cast<int>(12.0f * scale));

    presetLabel.setBounds(toolbar.removeFromTop(static_cast<int>(17.0f * scale)));
    presets.setBounds(toolbar.reduced(0, 1));

    const auto controlsY = static_cast<int>(171.0f * scale);
    const auto controlsBottom = getHeight() - static_cast<int>(35.0f * scale);
    const auto controlsHeight = controlsBottom - controlsY;
    const auto rowHeight = controlsHeight / 2;
    const auto columnWidth = (getWidth() - 2 * margin) / 5;

    for (std::size_t index = 0; index < knobs.size(); ++index)
    {
        const auto row = static_cast<int>(index / 5u);
        const auto column = static_cast<int>(index % 5u);
        knobs[index]->setBounds(margin + column * columnWidth,
                                controlsY + row * rowHeight + static_cast<int>(18.0f * scale),
                                columnWidth,
                                rowHeight - static_cast<int>(22.0f * scale));
    }
}

void AssEffectAudioProcessorEditor::timerCallback()
{
    const auto presetRevision = processor.getPresetRevision();
    if (presetRevision != lastPresetRevision)
    {
        lastPresetRevision = presetRevision;
        presetSyncCountdown = 2;
    }

    // ComboBox click notifications are asynchronous in JUCE. Waiting until the
    // processor state is stable prevents the timer from restoring the previous
    // preset between the click and its pending onChange callback.
    if (presetSyncCountdown > 0)
    {
        if (presets.isPopupActive())
        {
            presetSyncCountdown = 2;
        }
        else if (--presetSyncCountdown == 0)
        {
            const auto latestRevision = processor.getPresetRevision();
            if (latestRevision != lastPresetRevision)
            {
                lastPresetRevision = latestRevision;
                presetSyncCountdown = 2;
            }
            else
            {
                const auto currentPresetID = processor.getCurrentFactoryPresetIndex() + 1;
                if (presets.getSelectedId() != currentPresetID)
                    presets.setSelectedId(currentPresetID, juce::dontSendNotification);
            }
        }
    }

    const auto input = processor.consumeInputPeak();
    const auto output = processor.consumeOutputPeak();
    displayedInput = input >= displayedInput ? input : displayedInput * 0.84f;
    displayedOutput = output >= displayedOutput ? output : displayedOutput * 0.84f;
    repaint();
}
