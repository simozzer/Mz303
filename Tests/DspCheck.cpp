// Mz303 - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

//
// The engine, checked on its own: pitch, the ladder's resonance stopping short of
// self-oscillation, the two envelopes, accent and its "wow", slide, and that no setting of
// any control makes it blow up. Links no JUCE.
//
//     DspCheck                 run the checks
//     DspCheck --wav <dir>     and also write a few demo phrases there to listen to
//

#define _CRT_SECURE_NO_WARNINGS   // fopen, for the demo renders

#include "Engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace tb303;

namespace
{

constexpr double rate = 48000.0;
constexpr int soft = 80, hard = 127;   // velocities: plain, accented
int failures = 0;

void check(bool ok, const char* what, const std::string& detail)
{
    std::printf("  %s  %-48s %s\n", ok ? "ok  " : "FAIL", what, detail.c_str());
    if (! ok) ++failures;
}

std::string fmt(const char* f, double a, double b = 0, double c = 0)
{
    char buf[160];
    std::snprintf(buf, sizeof buf, f, a, b, c);
    return buf;
}

std::vector<float> render(Engine& e, double seconds)
{
    std::vector<float> v((size_t) (seconds * rate));
    e.render(v.data(), (int) v.size());
    return v;
}

// Frequency from rising zero crossings, interpolated, between two fractions of the buffer.
double frequency(const std::vector<float>& v, double from = 0.5, double to = 1.0)
{
    double first = -1, last = -1;
    int count = 0;

    for (size_t i = (size_t) (v.size() * from) + 1; i < (size_t) (v.size() * to); ++i)
    {
        if (v[i - 1] < 0 && v[i] >= 0)
        {
            const double t = (double) (i - 1) + v[i - 1] / (double) (v[i - 1] - v[i]);
            if (first < 0) first = t; else ++count;
            last = t;
        }
    }

    return count > 0 ? count * rate / (last - first) : 0.0;
}

double peak(const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    double p = 0;
    for (size_t i = from; i < (to ? to : v.size()); ++i) p = std::max(p, (double) std::fabs(v[i]));
    return p;
}

double rms(const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    to = to ? to : v.size();
    double s = 0;
    for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return std::sqrt(s / (double) std::max<size_t>(1, to - from));
}

double db(double ratio) { return 20.0 * std::log10(ratio); }

// The level of one frequency over the second half of the buffer: a Goertzel, windowed.
double levelAt(const std::vector<float>& v, double hz)
{
    const size_t from = v.size() / 2, n = v.size() - from;
    const double w = 2 * pi * hz / rate, c = 2 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double hann = 0.5 - 0.5 * std::cos(2 * pi * (double) i / (double) (n - 1));
        const double s0 = v[from + i] * hann + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / (double) n;
}

// An open, plain patch: filter wide open, no envelope or accent on it, no resonance.
Settings plain()
{
    Settings s;
    s.cutoff = 10; s.res = 0; s.envMod = 0; s.accent = 0; s.decay = 5;
    return s;
}

Engine* make(const Settings& s)
{
    static Engine e;
    e.prepare(rate);
    e.setSettings(s);
    return &e;
}

double pitchOf(const Settings& s, int note, float bend = 0)
{
    Engine& e = *make(s);
    e.setPitchBend(bend);
    e.noteOn(note, soft);
    return frequency(render(e, 1.0));
}

void pitchChecks()
{
    std::printf("\nthe VCO\n");

    struct Case { const char* what; double expect; Settings s; int note; float bend; };
    std::vector<Case> cases;

    auto add = [&] (const char* what, double expect, int note, float bend, auto tweak)
    {
        Settings s = plain();
        tweak(s);
        cases.push_back({ what, expect, s, note, bend });
    };

    add("saw, A4",                 440.0, 69, 0, [] (Settings&) {});
    add("saw, A1",                  55.0, 33, 0, [] (Settings&) {});
    add("square, A4",              440.0, 69, 0, [] (Settings& s) { s.wave = Wave::Square; });
    add("square, A1",               55.0, 33, 0, [] (Settings& s) { s.wave = Wave::Square; });
    add("TUNE +100 cents: a semitone up", 440.0 * std::pow(2.0, 1.0 / 12.0), 69, 0, [] (Settings& s) { s.tune = 100; });
    add("TUNE -100 cents: a semitone down", 440.0 * std::pow(2.0, -1.0 / 12.0), 69, 0, [] (Settings& s) { s.tune = -100; });
    add("bend fully up: a tone",   440.0 * std::pow(2.0, 2.0 / 12.0), 69, 1, [] (Settings&) {});
    add("saw, C7 (2.1 kHz)",      2093.0, 96, 0, [] (Settings&) {});

    for (auto& c : cases)
    {
        const double f = pitchOf(c.s, c.note, c.bend);
        const double cents = 1200.0 * std::log2(f / c.expect);
        check(std::fabs(cents) < 6.0, c.what, fmt("%.2f Hz (%+.1f cents)", f, cents));
    }
}

void filterChecks()
{
    std::printf("\nthe VCF\n");

    // The ladder alone: at the top of RESONANCE it rings, and dies away - the 303 squelches
    // but does not whistle on its own. A little past it, it would.
    auto ringing = [] (float fc, float k)
    {
        const float fs = 192000;
        Vcf v;
        v.setSampleRate(fs);
        v.reset();
        const float g = std::tan((float) pi * fc / fs);
        double early = 0, late = 0;
        const int n = (int) (fs * 0.5f);
        for (int i = 0; i < n; ++i)
        {
            const float y = v.process(i < 4 ? 1.0e-3f : 0.0f, g, k);
            if (i > n / 8 && i < n / 4) early += y * y;
            if (i > 3 * n / 4) late += y * y;
        }
        return 10 * std::log10((late + 1e-300) / (early + 1e-300));
    };

    double worst = -1e9;
    for (float fc : { 150.0f, 500.0f, 1500.0f, 4000.0f, 10000.0f })
        worst = std::max(worst, ringing(fc, 17.0f));
    check(worst < -3.0, "full resonance rings, then dies away", fmt("slowest decay %.1f dB over 0.3 s", worst));
    check(ringing(2000.0f, 20.0f) > -1.0, "and a little more would self-oscillate", fmt("%.1f dB at k = 20", ringing(2000.0f, 20.0f)));

    // Closing the filter takes the level down.
    auto level = [] (float cutoff, float res, int note, bool fundamental)
    {
        Settings s = plain();
        s.cutoff = cutoff;
        s.res = res;
        Engine& e = *make(s);
        e.noteOn(note, soft);
        const auto v = render(e, 1.0);
        return fundamental ? levelAt(v, 440.0 * std::pow(2.0, (note - 69) / 12.0)) : rms(v, v.size() / 2);
    };

    const double open = level(10, 0, 57, false), shut = level(0, 0, 57, false);
    check(shut < open * 0.15, "closing the filter takes the saw away", fmt("rms %.3f open, %.4f shut", open, shut));

    // Resonance takes level from below the peak - but the highpassed feedback spares the
    // lowest notes. Measured on each note's fundamental, well under the cutoff.
    const double midLoss = db(level(5, 10, 45, true) / level(5, 0, 45, true));
    const double lowLoss = db(level(5, 10, 28, true) / level(5, 0, 28, true));
    const double whole = db(level(5, 10, 45, false) / level(5, 0, 45, false));
    check(midLoss < -3.0, "resonance thins the bass", fmt("fundamental %.1f dB at A2", midLoss));
    check(lowLoss > midLoss + 2.0, "but spares the lowest notes", fmt("fundamental %.1f dB at E1", lowLoss));
    check(whole > -9.0 && whole < -3.0, "and is not a volume control", fmt("overall %+.1f dB at A2", whole));

    // ENV MOD sweeps up from a lower resting point.
    auto cutoffAt = [] (float envMod, double seconds)
    {
        Settings s = plain();
        s.cutoff = 5; s.envMod = envMod; s.decay = 0;
        Engine& e = *make(s);
        e.noteOn(45, soft);
        render(e, seconds);
        return e.cutoffHz();
    };
    const double rest0 = cutoffAt(0, 1.5), rest10 = cutoffAt(10, 1.5), peak10 = cutoffAt(10, 0.002);
    check(rest10 < rest0 * 0.6 && peak10 > rest0 * 4, "ENV MOD lowers the rest and raises the peak",
          fmt("rest %.0f Hz -> %.0f Hz, peak %.0f Hz", rest0, rest10, peak10));
}

void envelopeChecks()
{
    std::printf("\nthe envelopes and VCA\n");

    Settings s = plain();
    Engine& e = *make(s);
    e.noteOn(45, soft);
    const auto held = render(e, 6.0);
    e.noteOff(45);
    const auto tail = render(e, 0.1);
    const size_t ms20 = (size_t) (0.02 * rate);

    check(peak(tail, ms20) < 1.0e-3, "the gate closing lets go within 20 ms", fmt("peak after 20 ms %.2e", peak(tail, ms20)));

    const double early = rms(held, (size_t) (0.2 * rate), (size_t) (0.4 * rate));
    const double late = rms(held, (size_t) (5.6 * rate), (size_t) (5.8 * rate));
    check(db(late / early) < -10.0, "a held note fades away", fmt("%.1f dB after 5.5 s", db(late / early)));

    // DECAY: half a second in, a short MEG has closed the filter and a long one has not.
    auto cutoffAfter = [] (float decay, int velocity)
    {
        Settings s = plain();
        s.cutoff = 3; s.envMod = 10; s.decay = decay; s.accent = 5;
        Engine& e = *make(s);
        e.noteOn(45, velocity);
        render(e, 0.5);
        return e.cutoffHz();
    };
    const double shortD = cutoffAfter(0, soft), longD = cutoffAfter(10, soft), accented = cutoffAfter(10, hard);
    check(longD > shortD * 3, "DECAY sets how long the filter stays open", fmt("%.0f Hz at 0, %.0f Hz at 10", shortD, longD));
    check(accented < shortD * 1.2, "an accent decays at the shortest, whatever DECAY",
          fmt("%.0f Hz accented at 10", accented));

    // the default patch should sit at a sensible level, accented or not
    for (int vel : { soft, hard })
    {
        Engine& d = *make(Settings());
        d.noteOn(36, vel);
        const double p = peak(render(d, 1.0));
        check(p > 0.1 && p < 1.0, vel == hard ? "default patch, accented, below full scale" : "default patch peaks below full scale",
              fmt("peak %.2f (%.1f dBFS)", p, db(p)));
    }
}

void accentChecks()
{
    std::printf("\naccent\n");

    auto hit = [] (int velocity, double* bright)
    {
        Settings s;
        s.cutoff = 3; s.res = 3; s.envMod = 5; s.accent = 10; s.decay = 5;
        Engine& e = *make(s);
        e.noteOn(45, velocity);
        const auto v = render(e, 0.3);
        if (bright)
        {
            double top = 0;
            Engine& f = *make(s);
            f.noteOn(45, velocity);
            float buf[64];
            for (int i = 0; i < 100; ++i) { f.render(buf, 64); top = std::max(top, (double) f.cutoffHz()); }
            *bright = top;
        }
        return peak(v);
    };

    double plainTop = 0, accTop = 0;
    const double plainPeak = hit(soft, &plainTop), accPeak = hit(hard, &accTop);
    check(db(accPeak / plainPeak) > 3.0, "an accent is louder", fmt("%+.1f dB", db(accPeak / plainPeak)));
    check(accTop > plainTop * 1.5, "and brighter", fmt("cutoff peak %.0f Hz, plain %.0f Hz", accTop, plainTop));

    std::vector<int> velocities;
    for (int v = 1; v <= 127; ++v) velocities.push_back(v);
    int first = 0;
    for (int v : velocities)
    {
        Engine& e = *make(plain());
        e.noteOn(45, v);
        if (e.accented()) { first = v; break; }
    }
    check(first == Engine::accentVelocity, "velocity 100 and up is an accent", fmt("first accented velocity %.0f", first));

    // The wow: four accented sixteenths at 120 BPM. At full resonance the accent capacitor
    // climbs from one to the next, and so does each note's peak cutoff; at none, every
    // accent is the same blip.
    auto run = [] (float res, std::vector<double>& peaks)
    {
        Settings s;
        s.cutoff = 2; s.res = res; s.envMod = 3; s.accent = 10; s.decay = 3;
        Engine& e = *make(s);
        float buf[48];
        for (int n = 0; n < 4; ++n)
        {
            e.noteOn(45, hard);
            double top = 0;
            for (int i = 0; i < 125; ++i)   // 125 ms
            {
                if (i == 62) e.noteOff(45);
                e.render(buf, 48);
                top = std::max(top, (double) e.cutoffHz());
            }
            peaks.push_back(top);
        }
    };

    std::vector<double> wow, flat;
    run(10, wow);
    run(0, flat);
    const double climb = std::log2(wow[3] / wow[0]), level = std::log2(flat[3] / flat[0]);
    check(climb > 0.3, "accents at full resonance climb", fmt("%.0f Hz -> %.0f Hz over four (%+.2f oct)", wow[0], wow[3], climb));
    check(std::fabs(level) < 0.1, "accents at no resonance do not", fmt("%.0f Hz -> %.0f Hz (%+.2f oct)", flat[0], flat[3], level));
}

void slideChecks()
{
    std::printf("\nslide\n");

    // A2 held, then A3 over it: a glide, not a jump.
    {
        Settings s = plain();
        Engine& e = *make(s);
        e.noteOn(45, soft);
        render(e, 0.3);
        e.noteOn(57, soft);
        const auto into = render(e, 0.4);
        const double during = frequency(into, 0.0, 0.05);   // the first 20 ms
        const double after = frequency(into, 0.75, 1.0);
        check(during > 115 && during < 210, "overlapping notes slide", fmt("%.1f Hz in the first 20 ms, heading for 220", during));
        check(std::fabs(1200 * std::log2(after / 220.0)) < 6, "and arrive", fmt("%.2f Hz after 300 ms", after));

        // letting go of A3 slides back down to the A2 still held
        e.noteOff(57);
        const auto back = render(e, 0.4);
        check(e.gateIsOn() && std::fabs(1200 * std::log2(frequency(back, 0.75, 1.0) / 110.0)) < 6,
              "letting go slides back to the held note", fmt("%.2f Hz", frequency(back, 0.75, 1.0)));
    }

    // Detached notes jump.
    {
        Settings s = plain();
        Engine& e = *make(s);
        e.noteOn(45, soft);
        render(e, 0.3);
        e.noteOff(45);
        e.noteOn(57, soft);
        const auto v = render(e, 0.4);
        const double f = frequency(v, 0.0, 0.05);
        check(std::fabs(1200 * std::log2(f / 220.0)) < 15, "detached notes do not", fmt("%.1f Hz in the first 20 ms", f));
    }

    // A slide restarts neither envelope: the filter keeps closing through it.
    {
        Settings s = plain();
        s.cutoff = 3; s.envMod = 10; s.decay = 0;
        Engine& e = *make(s);
        e.noteOn(45, soft);
        render(e, 0.3);
        const double before = e.cutoffHz();
        e.noteOn(57, soft);
        render(e, 0.01);
        check(e.cutoffHz() <= before * 1.01, "a slide does not restart the envelopes", fmt("cutoff %.0f Hz -> %.0f Hz", before, e.cutoffHz()));
    }
}

void fuzz()
{
    std::printf("\nevery control, anywhere\n");

    Random r(1234);
    double worst = 0;
    bool finite = true;

    for (int run = 0; run < 200; ++run)
    {
        Settings s;
        auto pick = [&] { return r.uniform() < 0.2f ? (r.uniform() < 0.5f ? 0.0f : 10.0f) : r.uniform() * 10.0f; };

        s.tune = pick() * 20 - 100; s.cutoff = pick(); s.res = pick(); s.envMod = pick();
        s.decay = pick(); s.accent = pick(); s.wave = (int) (r.uniform() * 2); s.volume = 10;

        Engine e;
        e.prepare(r.uniform() < 0.5f ? 44100.0 : 96000.0);
        e.setSettings(s);
        e.setPitchBend(r.bipolar());

        std::vector<float> v(2048);
        for (int k = 0; k < 8; ++k)
        {
            const int note = 12 + (int) (r.uniform() * 100);
            e.noteOn(note, r.uniform() < 0.5f ? hard : soft);
            e.render(v.data(), (int) v.size());
            for (float x : v) { if (! std::isfinite(x)) finite = false; worst = std::max(worst, (double) std::fabs(x)); }
            if (r.uniform() < 0.5f) e.noteOff(note);
        }
    }

    check(finite, "200 random patches, always finite", "");
    check(worst < 2.0, "and never runs away", fmt("worst peak %.2f at full volume", worst));
}

void speed()
{
    std::printf("\ncost\n");

    Settings s;
    s.res = 9; s.envMod = 7;
    Engine e;
    e.prepare(44100.0);
    e.setSettings(s);
    e.noteOn(36, hard);

    std::vector<float> v(44100 * 10);
    const auto t0 = std::chrono::steady_clock::now();
    e.render(v.data(), (int) v.size());
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    check(secs < 10.0, "10 s of audio at 44.1 kHz (4x inside)", fmt("%.0f ms - %.0fx realtime", secs * 1000, 10.0 / secs));
}

//------------------------------------------------------------------------------ demo renders

void writeWav(const std::string& path, const std::vector<float>& v)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (! f) { std::printf("  could not write %s\n", path.c_str()); return; }

    auto u32 = [&] (uint32_t x) { std::fwrite(&x, 4, 1, f); };
    auto u16 = [&] (uint16_t x) { std::fwrite(&x, 2, 1, f); };

    const uint32_t bytes = (uint32_t) v.size() * 2;
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(1); u32((uint32_t) rate); u32((uint32_t) rate * 2); u16(2); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    for (float x : v) { const int16_t s = (int16_t) std::lround(clamp(x, -1.0f, 1.0f) * 32767.0f); std::fwrite(&s, 2, 1, f); }
    std::fclose(f);
    std::printf("  wrote %s\n", path.c_str());
}

// A pattern as the 303 writes one: note (-1 a rest), accent, slide into the next step.
struct Step { int note; bool accent; bool slide; };

void pattern(const std::string& path, const Settings& s, const std::vector<Step>& steps, int repeats, double bpm,
             float sweepFrom = -1, float sweepTo = -1)
{
    Engine e;
    e.prepare(rate);
    Settings t = s;
    e.setSettings(t);

    const double step = 60.0 / bpm / 4.0;
    std::vector<float> out;
    int sounding = -1;
    const int total = (int) steps.size() * repeats;

    for (int i = 0; i < total; ++i)
    {
        if (sweepFrom >= 0)
        {
            t.cutoff = sweepFrom + (sweepTo - sweepFrom) * (float) i / (float) total;
            e.setSettings(t);
        }

        const auto& st = steps[(size_t) i % steps.size()];
        const bool tiedIn = i > 0 && steps[(size_t) (i - 1) % steps.size()].slide && sounding >= 0;

        if (st.note >= 0)
        {
            e.noteOn(st.note, st.accent ? hard : soft);
            if (tiedIn && sounding != st.note) e.noteOff(sounding);
            sounding = st.note;
        }
        else if (sounding >= 0)
        {
            e.noteOff(sounding);
            sounding = -1;
        }

        // the gate is half a step, unless this step slides into the next
        auto on = render(e, step * 0.5);
        out.insert(out.end(), on.begin(), on.end());
        if (! st.slide && sounding >= 0) { e.noteOff(sounding); sounding = -1; }
        auto off = render(e, step * 0.5);
        out.insert(out.end(), off.begin(), off.end());
    }

    if (sounding >= 0) e.noteOff(sounding);
    auto tail = render(e, 0.5);
    out.insert(out.end(), tail.begin(), tail.end());
    writeWav(path, out);
}

void demos(const std::string& dir)
{
    std::printf("\ndemos\n");

    const std::vector<Step> line = {
        { 36, true, false }, { 36, false, false }, { 48, false, true }, { 36, false, false },
        { 39, true, false }, { -1, false, false }, { 36, false, true }, { 43, false, false },
        { 36, true, false }, { 36, false, false }, { 46, true, true }, { 48, false, false },
        { 41, false, false }, { 39, true, true }, { 34, false, false }, { 36, true, false } };

    Settings acid;
    acid.cutoff = 3; acid.res = 8; acid.envMod = 6; acid.decay = 4; acid.accent = 8;
    pattern(dir + "/acid-saw.wav", acid, line, 4, 128, 1.5f, 6.0f);

    Settings sq = acid;
    sq.wave = Wave::Square; sq.res = 6; sq.envMod = 5;
    pattern(dir + "/acid-square.wav", sq, line, 2, 128);

    // every step accented at full resonance: the wow
    std::vector<Step> wow(16, Step { 36, true, false });
    Settings w;
    w.cutoff = 2; w.res = 10; w.envMod = 3; w.accent = 10; w.decay = 3;
    pattern(dir + "/accent-wow.wav", w, wow, 2, 128);
}

} // namespace

int main(int argc, char** argv)
{
    std::printf("Mz303 engine check\n");

    pitchChecks();
    filterChecks();
    envelopeChecks();
    accentChecks();
    slideChecks();
    fuzz();
    speed();

    if (argc > 2 && std::strcmp(argv[1], "--wav") == 0)
        demos(argv[2]);

    std::printf("\n%s\n", failures == 0 ? "all checks passed" : (std::to_string(failures) + " FAILED").c_str());
    return failures == 0 ? 0 : 1;
}
