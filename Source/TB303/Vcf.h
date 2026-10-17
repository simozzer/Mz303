// Mz303 - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Dsp.h"

namespace tb303
{

//
// THE VCF: the 303's diode ladder
//
// Where a Moog ladder is four transistor stages each buffered from the next, the 303's is a
// stack of diode pairs with a capacitor across each rung and a differential pair at the
// bottom driving it. A conducting diode is a resistor whose value the cutoff current sets,
// so the ladder is four RC sections in a chain - and nothing buffers one from the next: each
// capacitor is charged by the rung below and drained by the rung above. That loading is
// what gives the 303 its sound. The poles spread out instead of sitting together, so the
// slope is gentler than a 24 dB filter's just past cutoff, and the resonant peak is broader
// and sits lower than the cutoff frequency.
//
// The top capacitor is half the size of the others, which is the usual reading of the
// circuit (and the reason it is often mistaken for an 18 dB filter). Each rung's current is
// tanh of the voltage across it, the bottom pair's most of all - the filter saturates on the
// difference between what comes in and what the ladder has got to.
//
// Resonance feeds the top of the ladder back to the other side of the bottom pair through a
// coupling capacitor, so the feedback is highpassed: the lowest notes get little of it, and
// keep their level as resonance rises where a plain ladder would lose it. Together with the
// spread poles, it is also why the 303 squelches but barely whistles on its own.
//
// Solved as one zero-delay system, trapezoidally: the rungs are a tridiagonal system, solved
// for the input and the feedback separately so the loop can be closed exactly - no unit
// delay anywhere, so cutoff and resonance stay where they are put to the top of the range.
// The nonlinearities are linearised around the previous sample's operating point, which at
// 4x oversampling is indistinguishable from iterating to convergence.
//
class Vcf
{
public:
    void setSampleRate(float rate)
    {
        hpG = std::tan((float) pi * feedbackHighpassHz / rate);
    }

    void reset()
    {
        for (auto& v : s) v = 0;
        for (auto& v : diff) v = 0;
        hpS = 0;
    }

    // g = tan(pi * fc / fs) for the rungs' RC; k = feedback.
    float process(float x, float g, float k)
    {
        // each rung's conductance, at the point it is working
        float c[4];
        c[0] = g * softClipGain(diff[0] * bottomDrive);
        for (int i = 1; i < 4; ++i)
            c[i] = g * softClipGain(diff[i] * rungDrive);
        const float c4 = c[3] * topCap;

        // the tridiagonal system: lower, main, upper, and the right-hand side as a part
        // fixed by the state plus a part per unit of the bottom pair's input u
        const float lower[4] = { 0, -c[1], -c[2], -c4 };
        const float main[4]  = { 1 + c[0] + c[1], 1 + c[1] + c[2], 1 + c[2] + c[3], 1 + c4 };
        const float upper[4] = { -c[1], -c[2], -c[3], 0 };
        float rs[4] = { s[0], s[1], s[2], s[3] };
        float ru[4] = { c[0], 0, 0, 0 };

        // Thomas, forward
        float up[4];
        up[0] = upper[0] / main[0];
        rs[0] /= main[0];
        ru[0] /= main[0];
        for (int i = 1; i < 4; ++i)
        {
            const float m = 1.0f / (main[i] - lower[i] * up[i - 1]);
            up[i] = upper[i] * m;
            rs[i] = (rs[i] - lower[i] * rs[i - 1]) * m;
            ru[i] = (ru[i] - lower[i] * ru[i - 1]) * m;
        }

        // and back
        for (int i = 2; i >= 0; --i)
        {
            rs[i] -= up[i] * rs[i + 1];
            ru[i] -= up[i] * ru[i + 1];
        }

        // y4 = rs[3] + ru[3] * u, and u = x - k * highpass(y4): close the loop
        const float hpA = 1.0f / (1.0f + hpG);
        const float hpB = -hpS / (1.0f + hpG);
        const float u = (x - k * (hpA * rs[3] + hpB)) / (1.0f + k * hpA * ru[3]);

        float y[4];
        for (int i = 0; i < 4; ++i)
            y[i] = rs[i] + ru[i] * u;

        diff[0] = u - y[0];
        for (int i = 1; i < 4; ++i)
            diff[i] = y[i - 1] - y[i];

        for (int i = 0; i < 4; ++i)
            s[i] = 2.0f * y[i] - s[i];

        // the feedback's coupling capacitor: a one-pole highpass, as the lowpass it leaves
        const float hp = hpA * y[3] + hpB;
        const float lp = y[3] - hp;
        hpS = 2.0f * lp - hpS;

        return y[3];
    }

private:
    // How hard the bottom pair and the diode rungs saturate, for a signal at the level the
    // engine drives them; the top capacitor as a fraction of the others, inverted (its RC
    // runs twice as fast); the feedback's coupling corner. Reasoned from the circuit, not
    // measured.
    static constexpr float bottomDrive = 1.0f;
    static constexpr float rungDrive = 0.5f;
    static constexpr float topCap = 2.0f;
    static constexpr float feedbackHighpassHz = 90.0f;

    float s[4] {};
    float diff[4] {};
    float hpS = 0, hpG = 0.01f;
};

} // namespace tb303
