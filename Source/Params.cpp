// Mz303 - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Params.h"

namespace params
{

using S = tb303::Settings;

const std::vector<Spec>& specs()
{
    //
    // CCs follow General MIDI 2's sound controllers where one fits - cutoff 74, resonance
    // 71, decay 75, sound variation 70 for the waveform, volume 7. The rest use CCs MIDI
    // leaves undefined, from 14 up.
    //
    static const std::vector<Spec> table = {
        { "tune",    "Tuning",       14, "Fine tuning, a semitone either way. Double-click for A = 440 Hz.",
          &S::tune, nullptr, {}, -100.0f, 100.0f },
        { "cutoff",  "Cut Off Freq", 74, "Filter cutoff. Lower is darker; the envelope and accents sweep up from here.",
          &S::cutoff },
        { "res",     "Resonance",    71, "Filter resonance: the squelch. Also makes accents swell rather than spike, and climb on a run of them.",
          &S::res },
        { "envMod",  "Env Mod",      15, "How far the envelope sweeps the filter on each note. Turning it up also lowers where the filter rests.",
          &S::envMod },
        { "decay",   "Decay",        75, "How long the filter's envelope takes to close, 0.2 to 2.5 s. Accented notes always close at the shortest.",
          &S::decay },
        { "accent",  "Accent",       16, "How much an accent lifts the filter and the volume. A note is accented when its velocity is 100 or more.",
          &S::accent },
        { "wave",    "Waveform",     70, "SAW, or SQUARE - the saw through a waveshaper, so its halves still lean.",
          nullptr, &S::wave, { "Saw", "Square" } },
        { "volume",  "Volume",        7, "Output level.",
          &S::volume },
    };

    return table;
}

int indexOf(const juce::String& id)
{
    const auto& t = specs();
    for (size_t i = 0; i < t.size(); ++i)
        if (id == t[i].id) return (int) i;
    return -1;
}

juce::String hintFor(int index, int cc, bool learning)
{
    if (index < 0 || index >= (int) specs().size()) return {};
    const auto& p = specs()[(size_t) index];

    juce::String midi;
    if (learning)          midi = "MIDI LEARN: move a knob on your controller. Right-click to cancel.";
    else if (cc < 0)       midi = "No MIDI CC (default " + juce::String(p.cc) + "). Right-click to learn one.";
    else if (cc != p.cc)   midi = "MIDI CC " + juce::String(cc) + ", learned (default " + juce::String(p.cc) + "). Right-click to change.";
    else                   midi = "MIDI CC " + juce::String(cc) + ". Right-click to learn another.";

    return juce::String(p.name).toUpperCase() + "\n" + p.hint + "\n" + midi;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const S defaults;

    for (const auto& p : specs())
    {
        const juce::ParameterID id { p.id, 1 };

        if (p.f != nullptr)
        {
            const bool cents = p.f == &S::tune;
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                id, p.name, juce::NormalisableRange<float> (p.min, p.max, 0.0f), defaults.*(p.f),
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [cents] (float v, int)
                    {
                        if (cents) return (v > 0.5f ? "+" : "") + juce::String(juce::roundToInt(v)) + " cents";
                        return juce::String (v, 1);
                    })));
        }
        else
        {
            layout.add(std::make_unique<juce::AudioParameterChoice>(id, p.name, p.choices, defaults.*(p.i)));
        }
    }

    return layout;
}

Reader::Reader(juce::AudioProcessorValueTreeState& state)
{
    for (const auto& p : specs())
    {
        auto* v = state.getRawParameterValue(p.id);
        jassert(v != nullptr);
        values.push_back(v);
    }
}

tb303::Settings Reader::read() const
{
    S s;
    const auto& table = specs();

    for (size_t n = 0; n < table.size(); ++n)
    {
        const float v = values[n]->load(std::memory_order_relaxed);
        const auto& p = table[n];

        if (p.f != nullptr) s.*(p.f) = v;
        else                s.*(p.i) = juce::roundToInt(v);
    }

    return s;
}

} // namespace params
