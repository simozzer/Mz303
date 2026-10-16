// Mz303 - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Dsp.h"
#include "Settings.h"

namespace tb303
{

//
// THE VCO
//
// One sawtooth core: an exponential current charging a capacitor, dumped at the top of each
// cycle. The 303 has no second waveform generator - its square is the saw put through a
// one-transistor waveshaper, so it is never quite square. The transistor switches as the
// ramp crosses its threshold, near the middle, but does not switch hard: the ramp still
// shows through, so each half of the wave leans the way the saw does. That lean is a good
// part of why the 303's square sounds hollow and reedy rather than like a clean pulse.
//
// Every edge is band-limited with a polyBLEP, placed at its exact fractional position. The
// correction straddles the edge - half before it, half after - so the output runs one sample
// late: `pending` holds the previous sample until the next one says whether an edge landed
// between them.
//
class Vco
{
public:
    void reset()
    {
        phase = 0;
        high = true;
        pending = 0;
    }

    // dt is frequency / sample rate.
    float process(float dt, int wave)
    {
        before = after = 0;

        const bool square = wave == Wave::Square;
        const float sawLevel = square ? lean : 1.0f;
        const float pulseLevel = square ? squareLevel : 0.0f;

        float p = phase + dt;

        // the waveshaper switching low, partway up the ramp
        if (high && phase < threshold && p >= threshold)
        {
            high = false;
            edge(-2.0f * pulseLevel, (p - threshold) / dt);
        }

        if (p >= 1.0f)
        {
            p -= 1.0f;
            const float d = p / dt;   // samples since the reset

            edge(-2.0f * sawLevel, d);

            if (! high)
            {
                high = true;
                edge(2.0f * pulseLevel, d);
            }

            // and low again in the same sample, at very high pitches
            if (p >= threshold)
            {
                high = false;
                edge(-2.0f * pulseLevel, (p - threshold) / dt);
            }
        }

        phase = p;

        const float naive = sawLevel * (2.0f * phase - 1.0f) + pulseLevel * (high ? 1.0f : -1.0f);

        const float out = pending + before;
        pending = naive + after;
        return out;
    }

private:
    // A step of height h, d samples (0..1) before now: the polyBLEP residual, split between
    // the previous sample and this one.
    void edge(float h, float d)
    {
        before += 0.5f * h * d * d;
        after  -= 0.5f * h * (1.0f - d) * (1.0f - d);
    }

    // Where on the ramp the waveshaper switches - a little under half way, so the square's
    // high half is the shorter - and how much of the ramp it lets through. Reasoned from the
    // circuit, not measured: each is the single number to fit to a real unit.
    static constexpr float threshold   = 0.46f;
    static constexpr float lean        = 0.3f;
    static constexpr float squareLevel = 0.8f;

    float phase = 0;
    bool  high = true;
    float pending = 0;
    float before = 0, after = 0;
};

} // namespace tb303
