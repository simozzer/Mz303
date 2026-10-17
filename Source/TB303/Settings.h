// Mz303 - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

namespace tb303
{

namespace Wave { enum { Saw, Square }; }

//
// The front panel: the six knobs along the top of the original, its WAVEFORM switch, and
// VOLUME. Every knob but TUNING is in panel units, 0 to 10; the switch is its position.
// The engine turns these into volts, hertz and seconds itself, so the plugin layer never has
// to know the curves.
//
// The ACCENT knob sets how strong accents are, not which notes have one. On the original,
// accent and slide are written into each step of the pattern; played from MIDI, a note's
// velocity is its accent and overlapping the note before is its slide.
//
struct Settings
{
    float tune    = 0.0f;     // cents, -100 to +100
    float cutoff  = 4.0f;
    float res     = 5.0f;
    float envMod  = 5.0f;
    float decay   = 5.0f;
    float accent  = 5.0f;
    int   wave    = Wave::Saw;
    float volume  = 7.0f;
};

} // namespace tb303
