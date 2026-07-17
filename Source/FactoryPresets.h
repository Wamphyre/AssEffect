#pragma once

#include <array>

struct AssEffectFactoryPreset
{
    const char* category;
    const char* name;
    // Machine, Drive, Age, Wear, Wow, Noise, Grit, Tone, Width, Mix, Output, Bypass.
    std::array<float, 12> values;
};

inline constexpr std::array<AssEffectFactoryPreset, 13> assEffectFactoryPresets
{{
    { "MEDIA", "90s Walkman",       { 0,  5.5f, 32, 18, 14, 10,  8,   3, 100, 100,  0.0f, 0 } },
    { "MEDIA", "Worn Pressing",     { 1,  3.0f, 54, 55, 28, 34, 12,   4,  92, 100, -0.5f, 0 } },

    { "INSTRUMENTS", "4-Track Guitar", { 2, 10.5f, 52, 34, 16, 12, 31,  -8, 108,  86, -1.5f, 0 } },
    { "INSTRUMENTS", "Buried Bass",    { 2,  8.0f, 45, 25,  7,  6, 18, -18,  60,  72,  0.0f, 0 } },
    { "INSTRUMENTS", "Cellar Drums",   { 3, 11.0f, 42, 38,  5,  9, 44, -12,  72,  64, -2.0f, 0 } },
    { "INSTRUMENTS", "Crypt Vocals",   { 3,  9.5f, 61, 46, 12, 16, 36, -21,  45,  75, -1.5f, 0 } },
    { "INSTRUMENTS", "Rehearsal Mic",  { 0,  8.0f, 58, 52, 22, 23, 21, -16,  62,  84, -1.0f, 0 } },
    { "INSTRUMENTS", "Frozen Sampler", { 4,  6.0f, 32, 28,  6,  4, 62,   2, 112,  78, -1.0f, 0 } },

    { "BUS / MASTER", "Tape Glue",    { 0,  2.5f, 14,  7,  3,  2,  3,   5, 102,  20,  0.0f, 0 } },
    // Bedroom 4-track constriction: narrow, dry, mid-forward and visibly worn.
    { "BUS / MASTER", "Necro Master", { 2,  6.8f, 58, 40,  7,  8, 26, -12,  55,  58, -1.5f, 0 } },
    // Cheap stereo/fuzz haze in parallel: abrasive mids while the dry path keeps the low end.
    { "BUS / MASTER", "Raw Master",   { 3,  7.5f, 48, 30,  4,  4, 34,  18,  76,  48, -1.0f, 0 } },

    { "EXTREME", "Dust Dub",       { 1,  6.0f, 80, 92, 64, 72, 28, -14, 128, 100, -2.5f, 0 } },
    { "EXTREME", "Destroyed Room", { 3, 13.0f, 68, 72, 19, 28, 66, -28,  38,  82, -3.0f, 0 } }
}};
