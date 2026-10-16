// Mz303 - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Presets.h"

#include "Params.h"

namespace presets
{

const std::vector<Preset>& all()
{
    static const std::vector<Preset> list = {
        { "Init", {} },

        { "Acid Squelch", {
            { "cutoff", 2.5f }, { "res", 8.5f }, { "envMod", 6.5f }, { "decay", 4 }, { "accent", 8 } } },

        { "Rubber Bass", {
            { "cutoff", 3 }, { "res", 3 }, { "envMod", 4 }, { "decay", 3 }, { "accent", 4 } } },

        { "Square Reed", {
            { "wave", 1 }, { "cutoff", 4 }, { "res", 5 }, { "envMod", 4 }, { "decay", 5 }, { "accent", 6 } } },

        { "Accent Wow", {
            { "cutoff", 1.5f }, { "res", 10 }, { "envMod", 3 }, { "decay", 3 }, { "accent", 10 } } },

        { "Long Sweep", {
            { "cutoff", 1.5f }, { "res", 7 }, { "envMod", 9 }, { "decay", 10 }, { "accent", 5 } } },

        { "Deep Sub", {
            { "wave", 1 }, { "cutoff", 1 }, { "res", 1 }, { "envMod", 2 }, { "decay", 2 }, { "accent", 3 } } },

        { "Bright Pluck", {
            { "cutoff", 6 }, { "res", 4 }, { "envMod", 7 }, { "decay", 0 }, { "accent", 7 } } },
    };

    return list;
}

void apply(const Preset& p, juce::AudioProcessorValueTreeState& state)
{
    for (const auto& spec : params::specs())
        if (auto* param = state.getParameter(spec.id))
            param->setValueNotifyingHost(param->getDefaultValue());

    for (const auto& [id, value] : p.values)
        if (auto* param = state.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1((float) value));
}

} // namespace presets
