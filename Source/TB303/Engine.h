// Mz303 - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Decimator.h"
#include "Dsp.h"
#include "Envelopes.h"
#include "Settings.h"
#include "Vcf.h"
#include "Vco.h"

#include <atomic>

namespace tb303
{

//
// One TB-303 voice, played from MIDI: VCO, diode-ladder VCF, VCA, the two envelopes and the
// accent circuit, oversampled.
//
// On the original, ACCENT and SLIDE are written into each step of a pattern, along with the
// note. Here they come with each note instead:
//
//   accent   a note whose velocity is at least accentVelocity
//   slide    a note played while the one before is still held. The pitch glides to it,
//            and neither envelope restarts - the 303's slide is a legato, as much as a
//            portamento. Letting go of a slid-to note while the first is still held slides
//            back.
//
// Monophonic with last-note priority. Everything is called from the audio thread, except the
// read-side accessors, which are safe from any thread.
//
class Engine
{
public:
    static constexpr int accentVelocity = 100;

    void prepare(double hostSampleRate);
    void setSettings(const Settings& s);

    void noteOn(int note, int velocity);
    void noteOff(int note);
    void allNotesOff();

    void setPitchBend(float v) { bend = clamp(v, -1.0f, 1.0f); }   // two semitones either way

    void render(float* out, int numSamples);

    // any thread: the panel's ACCENT and SLIDE lights
    bool accentLit() const  { return accentLight.load(std::memory_order_relaxed); }
    bool slideLit() const   { return slideLight.load(std::memory_order_relaxed); }

    int  currentKey() const    { return key; }
    bool gateIsOn() const      { return gate; }
    bool accented() const      { return accent; }
    int  oversampling() const  { return factor; }

    // For the checks: what the filter and accent circuit are doing now.
    float cutoffHz() const      { return lastFc; }
    float accentCharge() const  { return sweep.charge(); }

private:
    struct Targets
    {
        float cutoffLog = 0, k = 0, envOct = 0, accentOct = 0, accentGain = 0, resPot = 0;
        float volume = 0;
    };

    void updateTargets();
    float tick();

    void play(int note, bool slide);
    void removeHeld(int note);

    Settings settings;
    Targets tgt, cur;
    bool primed = false;

    float hostRate = 44100, rateInt = 176400;
    int   factor = 4;

    Vco vco;
    Vcf vcf;
    Meg meg;
    Veg veg;
    AccentSweep sweep;
    Decimator decimator;
    Random rng;

    // keyboard
    int  held[128] {};
    int  numHeld = 0;
    int  key = 48;
    bool gate = false;
    bool accent = false;
    bool sliding = false;
    float keyTarget = 0, keyCv = 0;

    std::atomic<bool> accentLight { false }, slideLight { false };

    float bend = 0;

    // derived per block
    float slideCoef = 1, smoothCoef = 1, accentDecayCoef = 1;
    float tuneOct = 0, maxFc = 20000;
    float lastFc = 0;

    // the analog around the edges
    float drift = 0, driftTarget = 0, driftCoef = 0;
    int   driftCountdown = 0;
    float jitter = 0, jitterCoef = 0;

    float inX1 = 0, inY1 = 0, inCoef = 0.9995f;
    float dcX1 = 0, dcY1 = 0, dcCoef = 0.999f;
    float volume = 0, volumeCoef = 0.01f;
};

} // namespace tb303
