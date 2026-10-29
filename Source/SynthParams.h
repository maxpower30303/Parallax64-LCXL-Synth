// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Flix
#pragma once
#include <cmath>
#include <cstring>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

namespace xl
{
    constexpr const char* kPluginVersion = "1.0.11";

    enum Kind { kFloat = 0, kChoice = 1, kBool = 2 };

    struct Spec
    {
        const char* id;
        int cc;
        const char* name;
        const char* label;
        int kind;
        float min = 0.0f, max = 1.0f, def = 0.0f, centre = 0.0f;
        const char* suffix = "";
        bool bipolar = false;
        const char* const* choices = nullptr;
        int nChoices = 0;
        int defChoice = 0;
    };

    // CC-Mapping = LaunchControl XL Standard-Template:
    //   Enc R1: 13-20 | Enc R2: 21-28 | Enc R3: 29-36
    //   Fader:  5-12  | Btn oben: 37-44 | Btn unten: 45-52
    // Arp-Konfiguration + Sync-Buttons + DIST: reine GUI-Parameter (cc = -1)
    inline const std::vector<Spec>& getSpecs()
    {
        static const char* const waves[] = { "Sin", "Tri", "Saw", "Sqr" };
        static const char* const octs[] = { "-2", "-1", "0", "+1", "+2" };
        static const char* const modes[] = { "LP12", "LP24", "HP12", "BP12", "LAD" };
        static const char* const lwaves[] = { "Sin", "Tri", "Saw", "Sqr", "S&H" };
        static const char* const arprate[] = { "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
        static const char* const arpdir[] = { "UP", "DOWN", "UP/DOWN", "RANDOM" };
        static const char* const arpoct[] = { "1 OCT", "2 OCT", "3 OCT", "4 OCT" };
        static const char* const dlyModes[] = { "STEREO", "PING-PONG", "TAPE DRIFT", "CROSS 3:4", "DIFFUSE" };

        static const std::vector<Spec> s =
        {
            // ---------- Reihe 1 (CC 13-20): OSZILLATOREN ----------
            {.id = "osc1Wave",  .cc = 13, .name = "OSC1 Wave",   .label = "WAVE",  .kind = kChoice, .choices = waves, .nChoices = 4, .defChoice = 2 },
            {.id = "osc1Oct",   .cc = 14, .name = "OSC1 Octave", .label = "OCT",   .kind = kChoice, .choices = octs,  .nChoices = 5, .defChoice = 2 },
            {.id = "osc1Tune",  .cc = 15, .name = "OSC1 Tune",   .label = "TUNE",  .kind = kFloat,  .min = -50.0f, .max = 50.0f, .def = 0.0f, .suffix = "ct", .bipolar = true },
            {.id = "osc1Level", .cc = 16, .name = "OSC1 Level",  .label = "LVL",   .kind = kFloat,  .min = 0.0f, .max = 1.0f, .def = 1.0f, .suffix = "%" },
            {.id = "osc2Wave",  .cc = 17, .name = "OSC2 Wave",   .label = "WAVE",  .kind = kChoice, .choices = waves, .nChoices = 4, .defChoice = 2 },
            {.id = "osc2Oct",   .cc = 18, .name = "OSC2 Octave", .label = "OCT",   .kind = kChoice, .choices = octs,  .nChoices = 5, .defChoice = 2 },
            {.id = "osc2Tune",  .cc = 19, .name = "OSC2 Tune",   .label = "TUNE",  .kind = kFloat,  .min = -50.0f, .max = 50.0f, .def = 7.0f, .suffix = "ct", .bipolar = true },
            {.id = "osc2Level", .cc = 20, .name = "OSC2 Level",  .label = "LVL",   .kind = kFloat,  .min = 0.0f, .max = 1.0f, .def = 0.8f, .suffix = "%" },

            // ---------- Reihe 2 (CC 21-28): MIX & FILTER ----------
            {.id = "subLevel",  .cc = 21, .name = "Sub Level",   .label = "SUB",    .kind = kFloat, .min = 0.0f, .max = 1.0f, .def = 0.3f, .suffix = "%" },
            {.id = "noiseLevel",.cc = 22, .name = "Noise Level", .label = "NOISE",  .kind = kFloat, .min = 0.0f, .max = 1.0f, .def = 0.0f, .suffix = "%" },
            {.id = "glide",     .cc = 23, .name = "Glide Time",  .label = "GLIDE",  .kind = kFloat, .min = 0.0f, .max = 2.0f, .def = 0.0f, .centre = 0.1f, .suffix = "s" },
            {.id = "cutoff",    .cc = 24, .name = "Cutoff",      .label = "CUT",    .kind = kFloat, .min = 16.0f, .max = 16000.0f, .def = 1200.0f, .centre = 1100.0f, .suffix = "Hz" },
            {.id = "reso",      .cc = 25, .name = "Resonance",   .label = "RES",    .kind = kFloat, .min = 0.0f, .max = 1.0f, .def = 0.15f, .suffix = "%" },
            {.id = "envAmt",    .cc = 26, .name = "Env Amount",  .label = "ENV",    .kind = kFloat, .min = -1.0f, .max = 1.0f, .def = 0.4f, .suffix = "%", .bipolar = true },
            {.id = "drive",     .cc = 27, .name = "Drive",       .label = "DRIVE",  .kind = kFloat, .min = 0.0f, .max = 1.0f, .def = 0.1f, .suffix = "%" },
            // MODE: 0=LP12 1=LP24 2=HP12 3=BP12 4=LAD (Ladder, 24 dB LP)
            {.id = "fltMode",   .cc = 28, .name = "Filter Mode", .label = "MODE",   .kind = kChoice, .choices = modes, .nChoices = 5, .defChoice = 1 },

            // ---------- Reihe 3 (CC 29-36): LFO, FX, MASTER ----------
            {.id = "lfoRate",   .cc = 29, .name = "LFO Rate",    .label = "RATE",    .kind = kFloat, .min = 0.05f, .max = 30.0f, .def = 5.0f, .centre = 1.5f, .suffix = "Hz" },
            {.id = "lfoWave",   .cc = 30, .name = "LFO Wave",    .label = "WAVE",    .kind = kChoice, .choices = lwaves, .nChoices = 5, .defChoice = 1 },
            {.id = "lfoDepth",  .cc = 31, .name = "LFO Depth",   .label = "DEPTH",   .kind = kFloat, .min = 0.0f, .max = 1.0f, .def = 0.0f, .suffix = "%" },
            {.id = "dlyTime",   .cc = 32, .name = "Delay Time",  .label = "TIME",    .kind = kFloat, .min = 0.01f, .max = 1.0f, .def = 0.35f, .centre = 0.25f, .suffix = "s" },
            {.id = "dlyFb",     .cc = 33, .name = "Delay Feedback", .label = "FB",   .kind = kFloat, .min = 0.0f, .max = 0.95f, .def = 0.3f, .suffix = "%" },
            {.id = "dlyMix",    .cc = 34, .name = "Delay Mix",   .label = "DLY",     .kind = kFloat, .min = 0.0f, .max = 1.0f, .def = 0.15f, .suffix = "%" },
            {.id = "revMix",    .cc = 35, .name = "Reverb Mix",  .label = "REV",     .kind = kFloat, .min = 0.0f, .max = 1.0f, .def = 0.15f, .suffix = "%" },
            {.id = "master",    .cc = 36, .name = "Master Gain", .label = "MASTER",  .kind = kFloat, .min = -60.0f, .max = 0.0f, .def = -6.0f, .suffix = "dB" },

            // ---------- Fader (CC 5-12): ENVELOPES ----------
            {.id = "ampA", .cc = 5,  .name = "Amp Attack",  .label = "A", .kind = kFloat, .min = 0.0005f, .max = 10.0f, .def = 0.005f, .centre = 0.05f, .suffix = "s" },
            {.id = "ampD", .cc = 6,  .name = "Amp Decay",   .label = "D", .kind = kFloat, .min = 0.005f,  .max = 15.0f, .def = 0.3f,   .centre = 0.3f,  .suffix = "s" },
            {.id = "ampS", .cc = 7,  .name = "Amp Sustain", .label = "S", .kind = kFloat, .min = 0.0f, .max = 1.0f, .def = 0.7f, .suffix = "%" },
            {.id = "ampR", .cc = 8,  .name = "Amp Release", .label = "R", .kind = kFloat, .min = 0.005f, .max = 20.0f, .def = 0.3f, .centre = 0.5f, .suffix = "s" },
            {.id = "fltA", .cc = 9,  .name = "Flt Attack",  .label = "A", .kind = kFloat, .min = 0.0005f, .max = 10.0f, .def = 0.005f, .centre = 0.05f, .suffix = "s" },
            {.id = "fltD", .cc = 10, .name = "Flt Decay",   .label = "D", .kind = kFloat, .min = 0.005f,  .max = 15.0f, .def = 0.4f,   .centre = 0.3f,  .suffix = "s" },
            {.id = "fltS", .cc = 11, .name = "Flt Sustain", .label = "S", .kind = kFloat, .min = 0.0f, .max = 1.0f, .def = 0.3f, .suffix = "%" },
            {.id = "fltR", .cc = 12, .name = "Flt Release", .label = "R", .kind = kFloat, .min = 0.005f, .max = 20.0f, .def = 0.25f, .centre = 0.5f, .suffix = "s" },

            // ---------- Buttons oben (CC 37-44): QUELLEN & LFO-ROUTING ----------
            {.id = "osc1On",   .cc = 37, .name = "OSC1 On",      .label = "OSC1",  .kind = kBool, .def = 1.0f },
            {.id = "osc2On",   .cc = 38, .name = "OSC2 On",      .label = "OSC2",  .kind = kBool, .def = 1.0f },
            {.id = "sync",     .cc = 39, .name = "Osc Sync",     .label = "SYNC",  .kind = kBool, .def = 0.0f },
            {.id = "subOn",    .cc = 40, .name = "Sub On",       .label = "SUB",   .kind = kBool, .def = 1.0f },
            {.id = "noiseOn",  .cc = 41, .name = "Noise On",     .label = "NOISE", .kind = kBool, .def = 0.0f },
            {.id = "lfoPitch", .cc = 42, .name = "LFO to Pitch", .label = "LFO>P", .kind = kBool, .def = 0.0f },
            {.id = "lfoFilter",.cc = 43, .name = "LFO to Filter",.label = "LFO>F", .kind = kBool, .def = 0.0f },
            {.id = "lfoPW",    .cc = 44, .name = "LFO to PWM",   .label = "LFO>PW",.kind = kBool, .def = 0.0f },

            // ---------- Buttons unten (CC 45-52): PLAY / GLOBAL ----------
            {.id = "mono",      .cc = 45, .name = "Mono",        .label = "MONO", .kind = kBool, .def = 0.0f },
            {.id = "legato",    .cc = 46, .name = "Legato",      .label = "LEGA", .kind = kBool, .def = 0.0f },
            {.id = "glideOn",   .cc = 47, .name = "Glide On",    .label = "GLIDE",.kind = kBool, .def = 0.0f },
            {.id = "unison",    .cc = 48, .name = "Unison",      .label = "UNI",  .kind = kBool, .def = 0.0f },
            {.id = "dlyOn",     .cc = 49, .name = "Delay On",    .label = "DLY",  .kind = kBool, .def = 1.0f },
            {.id = "revOn",     .cc = 50, .name = "Reverb On",   .label = "REV",  .kind = kBool, .def = 1.0f },
            {.id = "lfoRetrig", .cc = 51, .name = "LFO Retrig",  .label = "RTRG", .kind = kBool, .def = 0.0f },
            {.id = "arpOn",     .cc = 52, .name = "Arp On",      .label = "ARP",  .kind = kBool, .def = 0.0f },

            // ---------- ARP-KONFIGURATION (nur GUI, keine CCs) ----------
            {.id = "arpRate",  .cc = -1, .name = "Arp Rate",      .label = "RATE",  .kind = kChoice, .choices = arprate, .nChoices = 8, .defChoice = 5 },
            {.id = "arpDir",   .cc = -1, .name = "Arp Direction", .label = "DIR",   .kind = kChoice, .choices = arpdir,  .nChoices = 4, .defChoice = 0 },
            {.id = "arpOct",   .cc = -1, .name = "Arp Octaves",   .label = "OCT",   .kind = kChoice, .choices = arpoct,  .nChoices = 4, .defChoice = 0 },
            {.id = "arpGate",  .cc = -1, .name = "Arp Gate",      .label = "GATE",  .kind = kFloat,  .min = 0.05f, .max = 1.0f, .def = 0.85f, .suffix = "%" },
            {.id = "arpSync",  .cc = -1, .name = "Arp Sync",      .label = "SYNC",  .kind = kBool,   .def = 1.0f },

            // ---------- ARP STEPS 1-8 (nur GUI) ----------
            {.id = "arpStep1", .cc = -1, .name = "Arp Step 1", .label = "1", .kind = kBool, .def = 1.0f },
            {.id = "arpStep2", .cc = -1, .name = "Arp Step 2", .label = "2", .kind = kBool, .def = 1.0f },
            {.id = "arpStep3", .cc = -1, .name = "Arp Step 3", .label = "3", .kind = kBool, .def = 1.0f },
            {.id = "arpStep4", .cc = -1, .name = "Arp Step 4", .label = "4", .kind = kBool, .def = 1.0f },
            {.id = "arpStep5", .cc = -1, .name = "Arp Step 5", .label = "5", .kind = kBool, .def = 1.0f },
            {.id = "arpStep6", .cc = -1, .name = "Arp Step 6", .label = "6", .kind = kBool, .def = 1.0f },
            {.id = "arpStep7", .cc = -1, .name = "Arp Step 7", .label = "7", .kind = kBool, .def = 1.0f },
            {.id = "arpStep8", .cc = -1, .name = "Arp Step 8", .label = "8", .kind = kBool, .def = 1.0f },

            // ---------- LFO / DELAY SYNC & MODI (nur GUI) ----------
            {.id = "lfoSync", .cc = -1, .name = "LFO Sync",   .label = "SYNC", .kind = kBool,   .def = 0.0f },
            {.id = "dlySync", .cc = -1, .name = "Delay Sync", .label = "SYNC", .kind = kBool,   .def = 0.0f },
            {.id = "dlyMode", .cc = -1, .name = "Delay Mode", .label = "MODE", .kind = kChoice, .choices = dlyModes, .nChoices = 5, .defChoice = 0 },

            // ---------- DIST (nur GUI) ----------
            {.id = "distOn",   .cc = -1, .name = "Distortion On",    .label = "ON",   .kind = kBool,  .def = 0.0f },
            {.id = "distAmt",  .cc = -1, .name = "Distortion Amount",.label = "DIST", .kind = kFloat, .min = 0.0f, .max = 1.0f, .def = 0.5f, .suffix = "%" },

            // ---------- PRESET-PEGEL (nur Preset-Abgleich, kein Regler/CC; wirkt hinter allen Effekten) ----------
            {.id = "presetLevel", .cc = -1, .name = "Preset Level", .label = "LEVEL", .kind = kFloat, .min = -40.0f, .max = 36.0f, .def = 0.0f, .suffix = "dB", .bipolar = true },
        };
        return s;
    }

    // Spec-Index zu einer Parameter-ID (allokationsfrei). Im Audio-Thread nur ueber das
    // P64_IDX-Makro in PluginProcessor.cpp benutzen, das den Index pro Aufrufstelle cached.
    inline int indexOf(const char* id)
    {
        const auto& s = getSpecs();
        for (size_t i = 0; i < s.size(); ++i)
            if (std::strcmp(s[i].id, id) == 0)
                return (int)i;
        jassertfalse;
        return -1;
    }

    inline constexpr const char* const dlyModes[5] =
    { "STEREO", "PING-PONG", "TAPE DRIFT", "CROSS 3:4", "DIFFUSE" };

    inline constexpr const char* tempoStepNames[8] =
    { "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };

    inline constexpr double tempoStepQuarters[8] =
    { 4.0, 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

    inline constexpr int kDelaySyncSteps = 16;

    inline constexpr const char* delaySyncNames[16] =
    {
        "1/1",
        "1/2D",
        "1/2",
        "1/2T",
        "1/4D",
        "1/4",
        "1/4T",
        "1/8D",
        "1/8",
        "1/8T",
        "1/16D",
        "1/16",
        "1/16T",
        "1/32D",
        "1/32",
        "1/64"
    };

    inline constexpr double delaySyncQuarters[16] =
    {
        4.0,           // 1/1
        3.0,           // 1/2D (dotted half)
        2.0,           // 1/2
        4.0 / 3.0,     // 1/2T
        1.5,           // 1/4D (dotted quarter)
        1.0,           // 1/4
        2.0 / 3.0,     // 1/4T
        0.75,          // 1/8D (dotted eighth = 3/16) - THE GROOVE KING
        0.5,           // 1/8
        1.0 / 3.0,     // 1/8T
        0.375,         // 1/16D (dotted sixteenth)
        0.25,          // 1/16
        1.0 / 6.0,     // 1/16T
        0.1875,        // 1/32D
        0.125,         // 1/32
        0.0625         // 1/64
    };

    inline constexpr const char* oscWaveNames[4] = { "SIN", "TRI", "SAW", "SQR" };

    inline juce::String numStr(float v, int maxDec = 2)
    {
        if (std::fabs(v - std::round(v)) < 0.005f)
            return juce::String((int)std::round(v));
        return juce::String(v, maxDec);
    }

    inline juce::String fmtVal(float v, const juce::String& suf, bool bipolar)
    {
        juce::String sign = (bipolar && v > 0.0f) ? "+" : "";
        if (suf == "Hz") { if (v >= 1000.0f) return sign + numStr(v / 1000.0f) + " kHz"; return sign + numStr(v) + " Hz"; }
        if (suf == "s") { if (v < 1.0f) return sign + numStr(v * 1000.0f) + " ms"; return sign + numStr(v) + " s"; }
        if (suf == "%")  return sign + numStr(v * 100.0f) + " %";
        if (suf == "dB") { if (v <= -59.5f) return "-inf dB"; return sign + numStr(v, 1) + " dB"; }
        if (suf == "ct") return sign + numStr(v, 1) + " ct";
        return sign + numStr(v);
    }

    inline std::unique_ptr<juce::RangedAudioParameter> makeParam(const Spec& s)
    {
        auto pid = juce::ParameterID{ s.id, 1 };

        if (s.kind == kBool)
            return std::make_unique<juce::AudioParameterBool>(pid, s.name, s.def > 0.5f);

        if (s.kind == kChoice)
        {
            juce::StringArray arr;
            for (int c = 0; c < s.nChoices; ++c) arr.add(s.choices[c]);
            return std::make_unique<juce::AudioParameterChoice>(pid, s.name, arr, s.defChoice);
        }

        juce::NormalisableRange<float> r(s.min, s.max);
        if (s.centre > s.min && s.centre < s.max)
            r.setSkewForCentre(s.centre);

        juce::String suf(s.suffix);
        bool bi = s.bipolar;
        auto attrs = juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([suf, bi](float v, int) { return fmtVal(v, suf, bi); });

        return std::make_unique<juce::AudioParameterFloat>(pid, s.name, r, s.def, attrs);
    }

} // namespace xl
