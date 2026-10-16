// Mz303 - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "PluginEditor.h"

#include "Params.h"
#include "Presets.h"

namespace colours
{
    const juce::Colour panel    { 0xffc6c7c3 };   // the 303's brushed aluminium
    const juce::Colour section  { 0xffb7b8b4 };
    const juce::Colour edge     { 0xff8e908c };
    const juce::Colour legend   { 0xff141414 };
    const juce::Colour dim      { 0xff5a5c5f };
    const juce::Colour lower    { 0xff2a2b2d };   // the dark lower half, round the keys
    const juce::Colour light    { 0xffe9e9e6 };
    const juce::Colour slot     { 0xff1a1a1a };
    const juce::Colour accent   { 0xffd8321f };   // the LEDs
}

//============================================================================== PanelLook

PanelLook::PanelLook()
{
    setColour(juce::ComboBox::backgroundColourId, colours::section);
    setColour(juce::ComboBox::outlineColourId, colours::edge);
    setColour(juce::ComboBox::textColourId, colours::legend);
    setColour(juce::ComboBox::arrowColourId, colours::legend);
    setColour(juce::PopupMenu::backgroundColourId, colours::lower);
    setColour(juce::PopupMenu::textColourId, colours::light);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour(juce::TextButton::textColourOffId, colours::light);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::BubbleComponent::backgroundColourId, colours::lower);
    setColour(juce::BubbleComponent::outlineColourId, colours::edge);
    setColour(juce::TooltipWindow::textColourId, colours::light);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff111214));
    setColour(juce::TooltipWindow::outlineColourId, colours::accent);
}

void PanelLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                 float sliderPos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 10.0f;
    const auto c = bounds.getCentre();

    // the scale: eleven marks, 0 to 10
    g.setColour(colours::legend);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const bool major = i % 5 == 0;
        const auto dir = juce::Point<float>(std::sin(a), -std::cos(a));
        g.drawLine({ c + dir * (radius + 3.0f), c + dir * (radius + (major ? 9.0f : 6.0f)) }, major ? 1.6f : 1.0f);
    }

    // the knob: a skirt, then the cap, lit from above
    const auto skirt = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(c);
    g.setColour(juce::Colour(0xff0b0b0b));
    g.fillEllipse(skirt);

    const auto cap = skirt.reduced(radius * 0.16f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a3a3c), cap.getCentreX(), cap.getY(),
                                           juce::Colour(0xff111112), cap.getCentreX(), cap.getBottom(), false));
    g.fillEllipse(cap);
    g.setColour(juce::Colours::black);
    g.drawEllipse(cap, 1.0f);

    // the pointer
    const float a = startAngle + (endAngle - startAngle) * sliderPos;
    const auto dir = juce::Point<float>(std::sin(a), -std::cos(a));
    g.setColour(colours::light);
    g.drawLine({ c + dir * (radius * 0.25f), c + dir * (radius * 0.92f) }, 2.5f);
}

void PanelLook::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                 float sliderPos, float, float,
                                 juce::Slider::SliderStyle, juce::Slider& slider)
{
    const float cx = (float) x + (float) width * 0.5f;
    const float top = (float) y, bottom = (float) (y + height);

    // a mark per position
    g.setColour(colours::legend);
    const int marks = juce::roundToInt(slider.getMaximum() - slider.getMinimum());
    for (int i = 0; i <= marks; ++i)
    {
        const float ty = bottom - (bottom - top) * (float) i / (float) juce::jmax(1, marks);
        g.fillRect(cx - 17.0f, ty - 0.5f, 8.0f, 1.0f);
    }

    // the slot, and the cap
    g.setColour(colours::slot);
    g.fillRoundedRectangle(cx - 3.0f, top - 6.0f, 6.0f, (bottom - top) + 12.0f, 2.0f);

    const juce::Rectangle<float> cap(cx - 8.0f, sliderPos - 7.0f, 16.0f, 14.0f);
    g.setColour(juce::Colour(0xff121212));
    g.fillRoundedRectangle(cap, 2.0f);
    g.setColour(juce::Colour(0xff4a4a4a));
    g.drawRoundedRectangle(cap, 2.0f, 1.0f);
    g.setColour(colours::light);
    g.fillRect(cap.getX() + 3.0f, sliderPos - 0.75f, cap.getWidth() - 6.0f, 1.5f);
}

void PanelLook::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                     bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(down ? juce::Colour(0xff0c0c0c) : juce::Colour(0xff161616));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(highlighted ? colours::light.withAlpha(0.5f) : colours::edge.withAlpha(0.5f));
    g.drawRoundedRectangle(r, 3.0f, 1.0f);

    // the LED
    const auto led = juce::Rectangle<float>(6.0f, 6.0f).withCentre({ r.getCentreX(), r.getY() + 7.0f });
    g.setColour(b.getToggleState() ? colours::accent.brighter(0.4f) : juce::Colour(0xff3a1714));
    g.fillEllipse(led);
}

//============================================================================== MIDI learn

namespace midiLearn
{

juce::String hint(const Mz303Processor& p, int index)
{
    return params::hintFor(index, p.ccFor(index), p.learningIndex() == index);
}

void showMenu(Mz303Processor& p, int index)
{
    const int cc = p.ccFor(index);
    const int def = params::specs()[(size_t) index].cc;
    const bool learning = p.learningIndex() == index;

    juce::PopupMenu m;
    m.addSectionHeader(juce::String(params::specs()[(size_t) index].name) +
                       (cc >= 0 ? "  -  CC " + juce::String(cc) : "  -  no CC"));

    if (learning) m.addItem("Cancel MIDI Learn", [&p] { p.learn(-1); });
    else          m.addItem("MIDI Learn", [&p, index] { p.learn(index); });

    m.addItem("Forget CC", cc >= 0, false, [&p, index] { p.forgetCc(index); });
    m.addItem("Restore default (CC " + juce::String(def) + ")", cc != def, false,
              [&p, index] { p.restoreDefaultCc(index); });
    m.addSeparator();
    m.addItem("Reset all CCs to defaults", [&p] { p.resetAllCcs(); });

    m.showMenuAsync(juce::PopupMenu::Options());
}

// While a control waits for its CC: a red outline and LEARN.
void drawLearning(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(colours::accent);
    g.drawRoundedRectangle(r.reduced(1.0f), 4.0f, 2.0f);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("LEARN", r.removeFromBottom(12.0f), juce::Justification::centred);
}

} // namespace midiLearn

//============================================================================== PanelControl

PanelControl::PanelControl(Mz303Processor& p, const juce::String& paramId,
                           const juce::String& legendText, juce::StringArray positionNames)
    : processor(p), index(params::indexOf(paramId)),
      legend(legendText), positions(std::move(positionNames))
{
    auto& state = p.state;
    jassert(index >= 0);

    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setVelocityBasedMode(false);

    if (isSwitch())
    {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.onValueChange = [this] { repaint(); };   // the lit position name follows the cap
    }
    else
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity(220);
        slider.setPopupDisplayEnabled(true, true, nullptr);
    }
    addAndMakeVisible(slider);

    // the same hint and menu over the knob as over its legend
    slider.hint = [this] { return getTooltip(); };
    slider.onRightClick = [this] { midiLearn::showMenu(processor, index); };

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, paramId, slider);

    // the attachment has set the range by now
    if (! isSwitch())
        slider.setDoubleClickReturnValue(true, (double) state.getParameter(paramId)->convertFrom0to1(
                                                   state.getParameter(paramId)->getDefaultValue()));
}

void PanelControl::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop(34);

    if (isSwitch())
    {
        r.removeFromBottom(14);
        slider.setBounds(r.removeFromLeft(36).withTrimmedTop(10));
    }
    else
    {
        slider.setBounds(r.withSizeKeepingCentre(r.getWidth(), r.getWidth()).withY(r.getY()));
    }
}

void PanelControl::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) midiLearn::showMenu(processor, index);
}

void PanelControl::paint(juce::Graphics& g)
{
    if (processor.learningIndex() == index)
        midiLearn::drawLearning(g, getLocalBounds().toFloat());

    g.setColour(colours::legend);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawFittedText(legend, getLocalBounds().removeFromTop(32), juce::Justification::centredBottom, 2);

    if (! isSwitch()) return;

    // the position names, level with where the cap sits in each
    const auto track = slider.getBounds().reduced(0, PanelLook::thumbRadius);
    const int n = positions.size();

    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    for (int i = 0; i < n; ++i)
    {
        const float ty = (float) track.getBottom() - (float) track.getHeight() * (float) i / (float) juce::jmax(1, n - 1);
        g.setColour(juce::roundToInt(slider.getValue()) == i ? colours::legend : colours::dim);
        g.drawText(positions[i], juce::Rectangle<float>((float) slider.getRight() - 2.0f, ty - 7.0f,
                                                        (float) getWidth() - (float) slider.getRight() + 2.0f, 14.0f),
                   juce::Justification::centredLeft);
    }
}

//============================================================================== Lights

Lights::Lights(Mz303Processor& p) : processor(p)
{
    setTooltip("Lit while the note playing is accented (velocity 100 or more), or slid into (played over the note before).");
    startTimerHz(30);
}

void Lights::timerCallback()
{
    const auto& e = processor.engineView();
    if (e.accentLit() == accent && e.slideLit() == slide) return;
    accent = e.accentLit();
    slide = e.slideLit();
    repaint();
}

void Lights::paint(juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto one = [&g] (juce::Rectangle<int> row, const char* name, bool on)
    {
        const auto led = juce::Rectangle<float>(12.0f, 12.0f).withCentre({ (float) row.getX() + 10.0f, (float) row.getCentreY() });
        if (on)
        {
            g.setColour(colours::accent.withAlpha(0.35f));
            g.fillEllipse(led.expanded(4.0f));
        }
        g.setColour(on ? colours::accent.brighter(0.5f) : juce::Colour(0xff4a1c18));
        g.fillEllipse(led);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawEllipse(led, 1.0f);

        g.setColour(colours::legend);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(name, row.withTrimmedLeft(24), juce::Justification::centredLeft);
    };

    r.removeFromTop(40);
    one(r.removeFromTop(30), "ACCENT", accent);
    r.removeFromTop(10);
    one(r.removeFromTop(30), "SLIDE", slide);
}

//============================================================================== Panel

Panel::Panel(Mz303Processor& p)
    : processor(p),
      lights(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&look);
    startTimerHz(15);

    // the 303's own wording, left to right along its top edge
    add("tune",   "TUNING");
    add("cutoff", "CUT OFF FREQ");
    add("res",    "RESONANCE");
    add("envMod", "ENV MOD");
    add("decay",  "DECAY");
    add("accent", "ACCENT");
    add("wave",   "WAVEFORM", { "SAW", "SQUARE" });
    add("volume", "VOLUME");

    addAndMakeVisible(lights);

    presetBox.setTooltip("Factory patches. Your own edits are saved with the project.");
    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem(processor.getProgramName(i), i + 1);
    presetBox.setSelectedId(processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        processor.setCurrentProgram(presetBox.getSelectedId() - 1);
        processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withProgramChanged(true));
    };
    addAndMakeVisible(presetBox);

    // The on-screen keys play at a plain velocity, or with ACCENT latched, an accented one.
    accentKeys.setClickingTogglesState(true);
    accentKeys.setTooltip("Latch to play the on-screen keys accented. From MIDI, velocity 100 or more is an accent, "
                          "and a note played before the last is let go slides.");
    accentKeys.onClick = [this] { keyboard.setVelocity(accentKeys.getToggleState() ? 1.0f : 0.6f, false); };
    keyboard.setVelocity(0.6f, false);
    addAndMakeVisible(accentKeys);

    // three octaves from C1, where most bass lines live
    keyboard.setAvailableRange(24, 60);
    keyboard.setOctaveForMiddleC(4);   // middle C is C4
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::accent.withAlpha(0.6f));
    keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::accent.withAlpha(0.25f));
    addAndMakeVisible(keyboard);
}

void Panel::timerCallback()
{
    const int l = processor.learningIndex(), v = processor.midiMapVersion();
    if (l == seenLearning && v == seenVersion) return;

    seenLearning = l;
    seenVersion = v;
    for (auto& c : controls) c->repaint();
}

Panel::~Panel()
{
    setLookAndFeel(nullptr);
}

PanelControl* Panel::add(const char* id, const char* legend, juce::StringArray positions)
{
    controls.push_back(std::make_unique<PanelControl>(processor, id, legend, std::move(positions)));
    addAndMakeVisible(*controls.back());
    return controls.back().get();
}

void Panel::resized()
{
    constexpr int margin = 14, pad = 12;

    presetBox.setBounds(designWidth - margin - 220, 14, 220, 26);

    knobSection = { margin, 56, designWidth - 2 * margin, 150 };
    keyboardSection = { margin, 218, designWidth - 2 * margin, designHeight - 218 - margin };

    // the controls spread evenly along the strip, the lights at its end
    constexpr int lightsWidth = 100;
    int total = lightsWidth;
    for (auto& c : controls) total += c->preferredWidth();
    const int gap = (knobSection.getWidth() - 2 * pad - total) / (int) controls.size();

    int x = knobSection.getX() + pad;
    for (auto& c : controls)
    {
        c->setBounds(x, knobSection.getY() + 8, c->preferredWidth(), knobSection.getHeight() - 16);
        x += c->preferredWidth() + gap;
    }
    lights.setBounds(x, knobSection.getY() + 8, lightsWidth, knobSection.getHeight() - 16);

    auto keys = keyboardSection.reduced(12);
    accentKeys.setBounds(keys.removeFromLeft(72).withSizeKeepingCentre(64, 44));
    keys.removeFromLeft(8);
    keyboard.setBounds(keys);
    keyboard.setKeyWidth((float) keyboard.getWidth() / 22.0f);   // 22 white keys, C1 to C4
}

void Panel::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel);

    // a brushed grain, faint
    juce::Random grain(303);
    for (int y = 0; y < designHeight; y += 2)
    {
        g.setColour(juce::Colours::white.withAlpha(0.02f + 0.03f * grain.nextFloat()));
        g.fillRect(0, y, designWidth, 1);
    }

    g.setColour(colours::legend);
    g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    g.drawText("Mz303", 18, 10, 120, 36, juce::Justification::centredLeft);
    g.setColour(colours::dim);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("BASS LINE", 116, 10, 300, 38, juce::Justification::centredLeft);

    g.setColour(colours::section);
    g.fillRoundedRectangle(knobSection.toFloat(), 4.0f);
    g.setColour(colours::edge);
    g.drawRoundedRectangle(knobSection.toFloat().reduced(0.5f), 4.0f, 1.0f);

    g.setColour(colours::lower);
    g.fillRoundedRectangle(keyboardSection.toFloat(), 4.0f);
}

//============================================================================== Mz303Editor

Mz303Editor::Mz303Editor(Mz303Processor& p)
    : AudioProcessorEditor(&p), panel(p)
{
    tooltips.setLookAndFeel(&panel.getLook());
    addAndMakeVisible(panel);
    panel.setBounds(0, 0, Panel::designWidth, Panel::designHeight);

    setResizable(true, true);
    setResizeLimits(Panel::designWidth / 2, Panel::designHeight / 2,
                    Panel::designWidth * 2, Panel::designHeight * 2);
    getConstrainer()->setFixedAspectRatio((double) Panel::designWidth / Panel::designHeight);
    setSize(Panel::designWidth, Panel::designHeight);
}

void Mz303Editor::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel);
}

void Mz303Editor::resized()
{
    const float scale = (float) getWidth() / (float) Panel::designWidth;
    panel.setTransform(juce::AffineTransform::scale(scale));
}
