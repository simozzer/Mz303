// Mz303 - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Engine.h"

#include <algorithm>

namespace tb303
{

namespace
{
    constexpr float a4 = 440.0f;

    float panel(float v) { return clamp(v * 0.1f, 0.0f, 1.0f); }   // 0..10 -> 0..1

    //
    // The voice's ranges. None is printed in the owner's manual the way the SH-101's are in
    // its spec, so these are reasoned from the circuit and from how the instrument is played,
    // and each is a single number to fit to a real unit.
    //

    // CUTOFF: the rungs' own RC frequency across the knob. The resonant peak sits a little
    // above it - the loaded ladder's poles spread both ways - so this is about 200 Hz to
    // 5 kHz of peak.
    constexpr float cutoffLo = 150.0f, cutoffHi = 3500.0f;

    // ENV MOD: how far the MEG sweeps the cutoff at full; and how far turning it up pulls
    // the resting cutoff DOWN, since on the 303 the knob shifts the envelope's whole range
    // rather than only adding to it.
    constexpr float envOctaves = 4.0f, envDip = 1.0f;

    // ACCENT: how far an accent sweeps the cutoff, and how much it lifts the VCA, at full.
    constexpr float accentOctaves = 2.5f, accentVca = 1.0f;

    // DECAY: the MEG's time to fall to 5%, across the knob; an accent's, whatever the knob.
    constexpr float decayShortest = 0.2f, decayLongest = 2.5f;

    // RESONANCE: the loop gain at full, just short of where the ladder whistles on its own
    // (about 18; more for low cutoffs, where the coupling capacitor takes the feedback away).
    constexpr float kMax = 17.0f;

    // Feedback costs the ladder 1 / (1 + k) of its passband. The output makes up part of
    // that, so RESONANCE thins the sound rather than doubling as a volume control: a
    // departure from the strict circuit. Without it full resonance takes 17 dB off a bass
    // line, far more than a 303 is heard to lose; with it, about 6.
    constexpr float makeup = 0.15f;

    // SLIDE: an RC on the pitch CV - most of the way in about a sixteenth at 120 BPM.
    constexpr float slideTau = 0.04f;

    // how hard the oscillator drives the filter's bottom pair
    constexpr float drive = 0.6f;
}

void Engine::prepare(double hostSampleRate)
{
    hostRate = (float) hostSampleRate;
    factor = hostRate <= 50000.0f ? 4 : (hostRate <= 100000.0f ? 2 : 1);
    rateInt = hostRate * (float) factor;

    decimator.prepare(factor, hostRate);
    vcf.setSampleRate(rateInt);
    meg.setSampleRate(rateInt);
    veg.setSampleRate(rateInt);
    sweep.setSampleRate(rateInt);

    vco.reset();
    vcf.reset();
    meg.reset();
    veg.reset();
    sweep.reset();

    smoothCoef  = onePoleCoef(0.004f, rateInt);   // a knob cannot move faster than a hand
    slideCoef   = onePoleCoef(slideTau, rateInt);
    accentDecayCoef = onePoleCoef(decayShortest / 3.0f, rateInt);
    driftCoef   = onePoleCoef(1.5f, rateInt);
    jitterCoef  = onePoleCoef(1.0f / (2.0f * (float) pi * 25.0f), rateInt);
    volumeCoef  = onePoleCoef(0.01f, hostRate);

    // the coupling capacitor into the filter, and the output's
    inCoef = 1.0f - (float) (2.0 * pi * 5.0 / rateInt);
    dcCoef = 1.0f - (float) (2.0 * pi * 8.0 / hostRate);

    maxFc = std::min(0.3f * rateInt, 30000.0f);

    inX1 = inY1 = dcX1 = dcY1 = 0;
    gate = accent = sliding = false;
    accentLight = slideLight = false;
    numHeld = 0;

    // The knobs jump to the first settings given after this rather than gliding there from
    // whatever was set before.
    updateTargets();
    primed = false;
}

void Engine::setSettings(const Settings& s)
{
    settings = s;
    updateTargets();
}

void Engine::updateTargets()
{
    const auto& s = settings;

    const float env = panel(s.envMod);
    tgt.cutoffLog  = std::log2(cutoffLo) + panel(s.cutoff) * std::log2(cutoffHi / cutoffLo) - envDip * env;
    tgt.envOct     = envOctaves * env;
    tgt.k          = kMax * panel(s.res);
    tgt.resPot     = panel(s.res);
    tgt.accentOct  = accentOctaves * panel(s.accent);
    tgt.accentGain = accentVca * panel(s.accent);

    const float v = panel(s.volume);
    tgt.volume = 1.4f * v * v;

    meg.setDecay(expLaw(panel(s.decay), decayShortest, decayLongest));

    tuneOct = s.tune / 1200.0f;

    if (! primed)
    {
        cur = tgt;
        volume = tgt.volume;
        primed = true;
    }
}

//------------------------------------------------------------------------------ keyboard

void Engine::noteOn(int note, int velocity)
{
    if (note < 0 || note > 127) return;
    if (velocity <= 0) { noteOff(note); return; }

    const bool slide = numHeld > 0 && gate;
    removeHeld(note);
    held[numHeld++] = note;

    accent = velocity >= accentVelocity;
    play(note, slide);
}

void Engine::noteOff(int note)
{
    removeHeld(note);

    if (numHeld == 0)
    {
        gate = false;
        veg.release();   // the MEG goes on decaying as it was: it never looks at the gate
        accentLight = slideLight = false;
    }
    else if (note == key)
    {
        play(held[numHeld - 1], true);   // back to the note still held
    }
}

void Engine::allNotesOff()
{
    numHeld = 0;
    gate = false;
    veg.release();
    accentLight = slideLight = false;
}

void Engine::play(int note, bool slide)
{
    key = note;
    keyTarget = (float) (note - 69) / 12.0f;
    sliding = slide;

    if (! slide)
    {
        keyCv = keyTarget;
        meg.trigger();
        veg.trigger();
    }

    gate = true;
    accentLight = accent;
    slideLight = slide;
}

void Engine::removeHeld(int note)
{
    int j = 0;
    for (int i = 0; i < numHeld; ++i)
        if (held[i] != note) held[j++] = held[i];
    numHeld = j;
}

//------------------------------------------------------------------------------ audio

float Engine::tick()
{
    auto smooth = [this] (float& v, float t) { v += (t - v) * smoothCoef; };
    smooth(cur.cutoffLog, tgt.cutoffLog);
    smooth(cur.envOct, tgt.envOct);
    smooth(cur.k, tgt.k);
    smooth(cur.resPot, tgt.resPot);
    smooth(cur.accentOct, tgt.accentOct);
    smooth(cur.accentGain, tgt.accentGain);

    // slide: an RC on the pitch CV - fast at first, slowing as it arrives
    if (sliding) keyCv += (keyTarget - keyCv) * slideCoef;
    else         keyCv = keyTarget;

    //---------------------------------------------------------------- the analog
    // Thermal drift: a couple of cents, wandering over seconds; and the expo converter's own
    // noise, a fraction of a cent.
    if (--driftCountdown <= 0)
    {
        driftCountdown = (int) (rateInt * (0.3f + 0.9f * rng.uniform()));
        driftTarget = rng.noise() * 0.0012f;
    }
    drift += (driftTarget - drift) * driftCoef;
    jitter += (rng.noise() * 0.004f - jitter) * jitterCoef;

    //---------------------------------------------------------------- VCO
    const float pitch = keyCv + tuneOct + bend * (2.0f / 12.0f) + drift + jitter;
    const float dt = std::min(a4 * std::exp2(pitch) / rateInt, 0.45f);

    const float osc = vco.process(dt, settings.wave);

    const float coupled = osc - inX1 + inCoef * inY1;
    inX1 = osc;
    inY1 = coupled;

    //---------------------------------------------------------------- envelopes and accent
    const float m = meg.process(accentDecayCoef, accent);
    const float accentIn = accent ? m : 0.0f;
    const float swept = sweep.process(accentIn, cur.resPot);

    //---------------------------------------------------------------- VCF
    const float fLog = cur.cutoffLog + m * cur.envOct + swept * cur.accentOct;
    const float fc = clamp(std::exp2(fLog), 10.0f, maxFc);
    lastFc = fc;
    const float g = std::tan((float) pi * fc / rateInt);

    // A trace of thermal noise at the input, so the ladder rings from silence as the real
    // one does.
    const float filtered = vcf.process(drive * coupled + 1.0e-5f * rng.noise(), g, cur.k)
                         * (1.0f + makeup * cur.k) / drive;

    //---------------------------------------------------------------- VCA
    const float amp = veg.process() * (1.0f + cur.accentGain * accentIn);
    return softClip(filtered * amp);
}

void Engine::render(float* out, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        for (int r = 0; r < factor; ++r)
            decimator.push(tick());

        const float y = decimator.output();

        const float hp = y - dcX1 + dcCoef * dcY1;
        dcX1 = y;
        dcY1 = hp;

        volume += (tgt.volume - volume) * volumeCoef;
        out[i] = hp * volume;
    }
}

} // namespace tb303
