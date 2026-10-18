// Mz303 - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

#include <memory>
#include <vector>

//
// The look of the panel: the 303's black knobs on brushed silver, with a scale round each,
// and a slide switch for the waveform whose positions are named beside it.
//
class PanelLook final : public juce::LookAndFeel_V4
{
public:
    PanelLook();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;

    int getSliderThumbRadius(juce::Slider&) override { return thumbRadius; }

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool highlighted, bool down) override;

    juce::Font getTextButtonFont(juce::TextButton&, int) override
    {
        return juce::FontOptions(11.5f, juce::Font::bold);
    }

    static constexpr int thumbRadius = 9;
};

//
// MIDI learn, from the panel: right-click a control for its menu, and its hover hint names
// the CC it answers to now.
//
namespace midiLearn
{
    void showMenu(Mz303Processor&, int index);
    juce::String hint(const Mz303Processor&, int index);
}

// A slider whose right button opens the MIDI learn menu instead of moving it, and whose hint
// is worked out when shown, so it names the CC as it is now.
class LearnSlider final : public juce::Slider
{
public:
    std::function<void()> onRightClick;
    std::function<juce::String()> hint;

    juce::String getTooltip() override { return hint ? hint() : juce::String(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        Slider::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseDrag(e); }
    void mouseUp(const juce::MouseEvent& e) override   { if (! e.mods.isPopupMenu()) Slider::mouseUp(e); }
};

//
// One control: its legend above, and the knob, or the switch with the name of each position
// beside it.
//
class PanelControl final : public juce::Component, public juce::TooltipClient
{
public:
    PanelControl(Mz303Processor&, const juce::String& paramId,
                 const juce::String& legend, juce::StringArray positionNames = {});

    bool isSwitch() const { return ! positions.isEmpty(); }
    int preferredWidth() const { return isSwitch() ? 84 : 96; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;   // right-click on the legend, too

    juce::String getTooltip() override { return midiLearn::hint(processor, index); }

private:
    Mz303Processor& processor;
    int index;
    LearnSlider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::String legend;
    juce::StringArray positions;
};

//
// ACCENT and SLIDE: lit while the note sounding has one - so it can be seen which notes
// the velocities and overlaps are making accents and slides of.
//
class Lights final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit Lights(Mz303Processor&);

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    Mz303Processor& processor;
    bool accent = false, slide = false;
};

//
// The whole front panel, at one fixed design size. The editor scales it to whatever size
// the window is, so the layout is written once in panel coordinates.
//
class Panel final : public juce::Component, private juce::Timer
{
public:
    explicit Panel(Mz303Processor&);
    ~Panel() override;

    static constexpr int designWidth = 1040;
    static constexpr int designHeight = 430;

    void paint(juce::Graphics&) override;
    void resized() override;

    PanelLook& getLook() { return look; }

private:
    // A control learning, or a CC changing - which may come from the audio thread - repaints.
    void timerCallback() override;
    int seenLearning = -1, seenVersion = -1;

    PanelControl* add(const char* id, const char* legend, juce::StringArray positions = {});

    Mz303Processor& processor;
    PanelLook look;

    std::vector<std::unique_ptr<PanelControl>> controls;
    Lights lights;

    juce::ComboBox presetBox;

    // This version, and where to find the next: the project's page, beside the presets.
    juce::HyperlinkButton projectLink;
    juce::TextButton accentKeys { "ACCENT" };
    juce::MidiKeyboardComponent keyboard;

    juce::Rectangle<int> knobSection, keyboardSection;
};

class Mz303Editor final : public juce::AudioProcessorEditor
{
public:
    explicit Mz303Editor(Mz303Processor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    Panel panel;

    // Hover hints. A child of the editor rather than the desktop, as hosts prefer, and
    // declared after the panel so it goes before the look it borrows from it.
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mz303Editor)
};
