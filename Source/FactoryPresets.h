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
    { "MEDIA", "90s Walkman",       { 0,  5.8f, 36, 20, 16, 11, 11,   2, 100, 100, -3.0f, 0 } },
    { "MEDIA", "Worn Pressing",     { 1,  3.0f, 54, 55, 28, 34, 12,   4,  92, 100, -0.5f, 0 } },

    { "INSTRUMENTS", "4-Track Guitar", { 2,  9.8f, 56, 36, 17, 12, 34,  -6, 106,  88, -4.8f, 0 } },
    { "INSTRUMENTS", "Buried Bass",    { 2,  7.4f, 48, 26,  8,  6, 20, -14,  60,  74, -3.8f, 0 } },
    { "INSTRUMENTS", "Cellar Drums",   { 3,  9.5f, 44, 42,  5,  8, 40,  -4,  72,  68, -2.0f, 0 } },
    { "INSTRUMENTS", "Crypt Vocals",   { 3,  8.5f, 60, 50, 12, 14, 34, -12,  45,  78, -1.8f, 0 } },
    { "INSTRUMENTS", "Rehearsal Mic",  { 0,  7.5f, 62, 55, 25, 23, 24, -12,  62,  86, -3.5f, 0 } },
    { "INSTRUMENTS", "Frozen Sampler", { 4,  6.0f, 32, 28,  6,  4, 62,   2, 112,  78, -1.0f, 0 } },

    { "BUS / MASTER", "Tape Glue",    { 0,  2.8f, 18,  7,  4,  2,  5,   4, 102,  24, -0.3f, 0 } },
    // Bedroom 4-track constriction: narrow, dry, mid-forward and visibly worn.
    { "BUS / MASTER", "Necro Master", { 2,  6.4f, 62, 42,  8,  8, 28,  -8,  55,  60, -3.2f, 0 } },
    // Cheap stereo/fuzz haze in parallel: abrasive mids while the dry path keeps the low end.
    { "BUS / MASTER", "Raw Master",   { 3,  6.8f, 50, 34,  4,  4, 30,  12,  76,  50, -1.2f, 0 } },

    { "EXTREME", "Dust Dub",       { 1,  6.0f, 80, 92, 64, 72, 28, -14, 128, 100, -2.5f, 0 } },
    { "EXTREME", "Destroyed Room", { 3, 11.5f, 70, 76, 19, 25, 60, -18,  38,  84, -3.0f, 0 } }
}};
