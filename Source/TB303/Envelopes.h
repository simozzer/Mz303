// Mz303 - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Dsp.h"

namespace tb303
{

//
// THE ENVELOPES
//
// The 303 has two, and neither is an ADSR.
//
// The MEG, the filter's envelope, is a decay and nothing else: a note charges its capacitor
// almost at once and it drains away through the DECAY pot, whatever the gate does. An
// accented note drains it through a fixed resistor instead, at the pot's fastest - so
// accents are always short, sharp blips, however long DECAY is set.
//
// The VEG, the VCA's, has no knob at all. It rises in a few milliseconds, decays slowly
// over seconds while the gate is held - a long held note fades away - and when the gate
// closes it falls in a few milliseconds more. Both its times are fixed in the circuit.
//
// Both are RC: every segment an exponential approach, and a new note restarts them from
// wherever they are.
//
class Meg
{
public:
    void setSampleRate(float r)
    {
        rate = r;
        attackCoef = onePoleCoef(attackTau, r);
    }

    // seconds for the envelope to fall to 5% - about three time constants
    void setDecay(float seconds) { decayCoef = onePoleCoef(seconds / 3.0f, rate); }

    void trigger() { attacking = true; }
    void reset()   { attacking = false; level = 0; }

    float process(float accentDecayCoef, bool accented)
    {
        if (attacking)
        {
            level += (overshoot - level) * attackCoef;
            if (level >= 1.0f) { level = 1.0f; attacking = false; }
        }
        else
        {
            level -= level * (accented ? accentDecayCoef : decayCoef);
        }
        return level;
    }

    float value() const { return level; }

private:
    // a fraction of a millisecond to the top, heading for a little past it
    static constexpr float attackTau = 0.0003f;
    static constexpr float overshoot = 1.2f;

    float rate = 44100;
    bool  attacking = false;
    float level = 0;
    float attackCoef = 1, decayCoef = 1;
};

class Veg
{
public:
    void setSampleRate(float r)
    {
        attackCoef  = onePoleCoef(attackTau, r);
        decayCoef   = onePoleCoef(decayTau, r);
        releaseCoef = onePoleCoef(releaseTau, r);
    }

    void trigger() { stage = Attack; }
    void release() { if (stage != Idle) stage = Release; }
    void reset()   { stage = Idle; level = 0; }

    float process()
    {
        switch (stage)
        {
            case Attack:
                level += (overshoot - level) * attackCoef;
                if (level >= 1.0f) { level = 1.0f; stage = Decay; }
                break;

            case Decay:
                level -= level * decayCoef;
                break;

            case Release:
                level -= level * releaseCoef;
                if (level < 1.0e-6f) { level = 0; stage = Idle; }
                break;

            default:
                break;
        }
        return level;
    }

    bool idle() const { return stage == Idle; }

private:
    enum Stage { Idle, Attack, Decay, Release };

    // Reasoned from the circuit rather than measured: a few milliseconds up, an e-fold every
    // couple of seconds held, a few milliseconds down.
    static constexpr float attackTau  = 0.0015f;
    static constexpr float overshoot  = 1.3f;
    static constexpr float decayTau   = 2.0f;
    static constexpr float releaseTau = 0.0025f;

    Stage stage = Idle;
    float level = 0;
    float attackCoef = 1, decayCoef = 1, releaseCoef = 1;
};

//
// THE ACCENT CIRCUIT
//
// An accented note sends the MEG, scaled by the ACCENT knob, to the filter's cutoff and to
// the VCA - on top of what ENV MOD sends. But on its way to the filter it passes a capacitor,
// and the second half of the RESONANCE pot, ganged with the first, decides how much of it
// goes through the capacitor rather than round it.
//
// At low resonance it goes round: each accent is a sharp blip on the cutoff, the same every
// time. At high resonance most of it is smoothed by the capacitor, which charges more slowly
// than the blip and drains more slowly still. So an accent swells rather than spikes, its
// peak arriving late; and accents close together find the capacitor still charged from the
// last one and climb higher each time. That climbing, resonant "wow" on a run of accents is
// the most recognisable sound the 303 makes.
//
class AccentSweep
{
public:
    void setSampleRate(float r)
    {
        chargeCoef = onePoleCoef(chargeTau, r);
        drainCoef  = onePoleCoef(drainTau, r);
    }

    void reset() { cap = 0; }

    // in: the MEG times ACCENT, on accented notes, else 0; res: the RESONANCE knob, 0..1.
    float process(float in, float res)
    {
        cap += (in - cap) * (in > cap ? chargeCoef : drainCoef);
        return (1.0f - res) * in + res * capGain * cap;
    }

    float charge() const { return cap; }

private:
    // The capacitor charges in tens of milliseconds and drains over a few hundred, so a
    // run of sixteenths at 120 BPM (125 ms apart) stacks up. capGain makes up the level the
    // smoothing loses, so a lone accent sweeps about as far either way.
    static constexpr float chargeTau = 0.04f;
    static constexpr float drainTau  = 0.3f;
    static constexpr float capGain   = 1.6f;

    float cap = 0;
    float chargeCoef = 1, drainCoef = 1;
};

} // namespace tb303
