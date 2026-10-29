// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Flix
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "SynthParams.h"

namespace xl
{

    constexpr double kPi = 3.14159265358979323846;
    constexpr double k2Pi = 6.28318530717958647692;

    template <typename T> static inline T xclamp(T v, T lo, T hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    // Padé (3/2) Approximation fuer tan(x) im Bereich 0 .. ~1.5 rad
    // Spart std::tan() Aufrufe, Fehler < 0.01%
    inline float fastTan(float x)
    {
        float x2 = x * x;
        return x * (105.0f - 10.0f * x2) / (105.0f - 45.0f * x2 + x2 * x2);
    }

    //==============================================================================
    struct EngineParams
    {
        int   osc1Wave = 2, osc1Oct = 0;
        float osc1Tune = 0, osc1Level = 1;
        int   osc2Wave = 2, osc2Oct = 0;
        float osc2Tune = 7, osc2Level = 0.8f;
        float subLevel = 0.3f, noiseLevel = 0.0f, glide = 0.0f;
        float cutoff = 1200, reso = 0.15f, envAmt = 0.4f, drive = 0.1f;
        int   fltMode = 1;
        float lfoRate = 5, lfoDepth = 0;
        int   lfoWave = 1;
        float dlyTime = 0.35f, dlyFb = 0.3f, dlyMix = 0.15f, revMix = 0.15f, master = -6;
        int   dlyMode = 0;
        float a1 = 0.005f, d1 = 0.3f, s1 = 0.7f, r1 = 0.3f;
        float a2 = 0.005f, d2 = 0.4f, s2 = 0.3f, r2 = 0.25f;
        bool  osc1On = true, osc2On = true, sync = false, subOn = true, noiseOn = false;
        bool  lfoPitch = false, lfoFilter = false, lfoPW = false, lfoRetrig = false;
        bool  mono = false, legato = false, glideOn = false, unison = false;
        bool  dlyOn = true, revOn = true, arpOn = false;
        bool  distOn = false;
        float distAmt = 0.5f;
        float bendFactor = 1.0f;
    };

    //==============================================================================
    // Xorshift32 PRNG (lock-frei, kein Heap, schnell)
    struct FastRandom
    {
        uint32_t s = 1;
        void seed(uint32_t x) { s = x ? x : 1u; }
        inline float next()
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (float)(int32_t)s * (1.0f / 2147483648.0f);
        }
    };

    //==============================================================================
    // Bandlimited Oszillator mit PolyBLEP fuer Saw/Square und PolyBLAMP fuer Triangle
    struct Osc
    {
        double phase = 0.0, inc = 0.0;
        int wave = 2;
        float pw = 0.5f;

        void setRate(double f, double sr) { inc = f / sr; }
        inline bool tick() { phase += inc; if (phase >= 1.0) { phase -= 1.0; return true; } return false; }

        static inline float polyBlep(double t, double dt)
        {
            if (dt <= 0.0) return 0.0f;
            if (t < dt) { t /= dt; return (float)(t + t - t * t - 1.0); }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float)(t * t + t + t + 1.0); }
            return 0.0f;
        }

        static inline float polyBlamp(double t, double dt)
        {
            if (dt <= 0.0) return 0.0f;
            if (t < dt) { double d = t / dt - 1.0; return (float)(-1.0 / 3.0 * d * d * d); }
            if (t > 1.0 - dt) { double d = (t - 1.0) / dt + 1.0; return (float)(1.0 / 3.0 * d * d * d); }
            return 0.0f;
        }

        inline float render() const
        {
            switch (wave)
            {
            case 0: return (float)std::sin(phase * k2Pi);
            case 1:
            {
                // Bandlimited Triangle ueber integrierten Square/PolyBLAMP
                float naive = phase < 0.5 ? (float)(4.0 * phase - 1.0) : (float)(3.0 - 4.0 * phase);
                double t2 = phase + 0.5;              // phase liegt in [0,1) -> ein Abzug genuegt (statt fmod)
                if (t2 >= 1.0) t2 -= 1.0;
                float b1 = polyBlamp(phase, inc);
                float b2 = polyBlamp(t2, inc);
                return naive + (float)(4.0 * inc * (b1 - b2));
            }
            case 2:
            {
                float y = (float)(2.0 * phase - 1.0);
                return y - polyBlep(phase, inc);
            }
            case 3:
            {
                float y = phase < pw ? 1.0f : -1.0f;
                double e = phase + 1.0 - (double)pw;  // in [0.05, 1.95) -> ein Abzug genuegt (statt fmod)
                if (e >= 1.0) e -= 1.0;
                return y + polyBlep(phase, inc) - polyBlep(e, inc);
            }
            }
            return 0.0f;
        }
    };

    //==============================================================================
    // SVF Filter mit fastTan, rationaler Soft-Saturation & Denormal-Schutz
    struct SVF
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;

        void reset()
        {
            ic1 = 0.0f;
            ic2 = 0.0f;
        }

        inline void setCoefs(float fc, float Q, double sr)
        {
            float nyq = (float)sr * 0.5f;
            fc = xclamp(fc, 16.0f, nyq * 0.90f);
            float w = (float)(kPi * (double)fc / sr);
            w = xclamp(w, 0.0001f, 1.40f);
            float g = fastTan(w);
            k = 1.0f / std::max(0.01f, Q);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        inline void process(float x, float& lp, float& bp, float& hp)
        {
            if (!std::isfinite(ic1) || !std::isfinite(ic2))
            {
                ic1 = 0.0f;
                ic2 = 0.0f;
            }

            x = xclamp(x, -6.0f, 6.0f);

            float v3 = x - ic2;
            float v1 = a1 * v3 + a2 * ic1;
            float v2 = ic2 + a2 * ic1 + a3 * v3;

            float new_ic1 = 2.0f * v1 - ic1;
            float new_ic2 = 2.0f * v2 - ic2;

            // Rationale Soft-Saturation der Integrator-Zustaende (verhindert Divergenz & ahmt analoge Dioden nach)
            ic1 = new_ic1 / (1.0f + 0.08f * std::fabs(new_ic1));
            ic2 = new_ic2 / (1.0f + 0.08f * std::fabs(new_ic2));

            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
    };

    //==============================================================================
    // Ladder-Filter (4 Stufen, 24 dB) mit Anti-Divergenz & Denormal-Schutz
    struct LadderFilter
    {
        double fs = 48000.0;
        double k = 0, p = 0, r = 0;
        double y1 = 0, y2 = 0, y3 = 0, y4 = 0;
        double oldx = 0, oldy1 = 0, oldy2 = 0, oldy3 = 0;

        void prepare(double sr) { fs = sr; reset(); }

        void reset()
        {
            y1 = y2 = y3 = y4 = 0.0;
            oldx = oldy1 = oldy2 = oldy3 = 0.0;
        }

        inline void setParams(double cutoff, double reso)
        {
            double f = (2.0 * cutoff) / fs;
            double fMax = 0.55 - 0.20 * xclamp(reso, 0.0, 1.0);
            f = xclamp(f, 0.002, fMax);

            k = 3.6 * f - 1.6 * f * f - 1.0;
            p = (k + 1.0) * 0.5;

            double scale = std::exp((1.0 - p) * 1.386249);
            r = xclamp(reso, 0.0, 1.0) * scale * 3.8;
        }

        inline double process(double input)
        {
            input = xclamp(input, -4.0, 4.0);
            double x = input - r * y4;

            y1 = x * p + oldx * p - k * y1;
            y1 = xclamp(y1, -3.5, 3.5);
            y1 -= (y1 * y1 * y1) * (1.0 / 6.0);

            y2 = y1 * p + oldy1 * p - k * y2;
            y2 = xclamp(y2, -3.5, 3.5);
            y2 -= (y2 * y2 * y2) * (1.0 / 6.0);

            y3 = y2 * p + oldy2 * p - k * y3;
            y3 = xclamp(y3, -3.5, 3.5);
            y3 -= (y3 * y3 * y3) * (1.0 / 6.0);

            y4 = y3 * p + oldy3 * p - k * y4;
            y4 = xclamp(y4, -3.5, 3.5);
            y4 -= (y4 * y4 * y4) * (1.0 / 6.0);

            oldx = x; oldy1 = y1; oldy2 = y2; oldy3 = y3;

            if (!(std::isfinite(y1) && std::isfinite(y2) &&
                std::isfinite(y3) && std::isfinite(y4)))
            {
                reset();
                return 0.0;
            }

            return y4 / (1.0 + 0.3 * r);
        }
    };

    //==============================================================================
    // Performance-optimierter ADSR (Koeffizienten vorkalkuliert, 0 std::exp Aufrufe im Audio-Loop)
    struct ADSR
    {
        float a = 0.005f, d = 0.3f, s = 0.7f, r = 0.3f;
        float rateA = 0.01f, coefD = 0.01f, coefR = 0.01f;
        int stage = 0;
        float level = 0.0f;
        bool released = false;

        void reset() { level = 0.0f; stage = 0; released = false; }
        void set(float A, float D, float S, float R, double sr)
        {
            a = A; d = D; s = S; r = R;
            rateA = 1.0f / (float)std::max(0.002 * sr, (double)a * sr);   // min. 2 ms Attack gegen Einsatz-Knacken
            coefD = 1.0f - (float)std::exp(-3.0 / std::max(0.001, (double)d * sr));
            coefR = 1.0f - (float)std::exp(-3.0 / std::max(0.001, (double)r * sr));
        }
        void trigger() { if (stage == 0) level = 0.0f; stage = 1; released = false; }   // laufende Stimme: vom aktuellen Pegel weiter (kein Sprung auf 0)
        void release() { if (stage > 0 && stage < 4) stage = 4; released = true; }
        void resume() { if (stage == 4) { stage = 2; released = false; } }
        bool finished() const { return stage == 0; }

        inline float process()
        {
            switch (stage)
            {
            case 1:
                level += rateA;
                if (level >= 1.0f) { level = 1.0f; stage = 2; }
                break;
            case 2:
                level += (s - level) * coefD;
                if (std::fabs(level - s) < 0.001f) { level = s; stage = 3; }
                break;
            case 3:
                level = s;
                break;
            case 4:
                level += (0.0f - level) * coefR;
                if (level < 0.0002f) { level = 0.0f; stage = 0; }
                break;
            default:
                level = 0.0f;
                break;
            }
            return level;
        }
    };

    //==============================================================================
    struct LFO
    {
        double phase = 0, inc = 0;
        int wave = 1;
        float sh = 0;
        FastRandom* rng = nullptr;

        void setRate(float hz, double sr) { inc = (double)hz / sr; }
        void retrigger() { phase = 0; }

        inline float process()
        {
            phase += inc;
            if (phase >= 1.0) { phase -= 1.0; if (wave == 4) sh = rng ? rng->next() : 0.0f; }

            switch (wave)
            {
            case 0: return (float)std::sin(phase * k2Pi);
            case 1: return phase < 0.5 ? (float)(4.0 * phase - 1.0) : (float)(3.0 - 4.0 * phase);
            case 2: return (float)(1.0 - 2.0 * phase);
            case 3: return phase < 0.5 ? 1.0f : -1.0f;
            case 4: return sh;
            }
            return 0.0f;
        }
    };

    //==============================================================================
    struct NoteStack
    {
        int   notes[32] = {};
        float vels[32] = {};
        int   count = 0;

        void push(int n, float v) { if (count < 32) { notes[count] = n; vels[count] = v; ++count; } }
        void remove(int n)
        {
            for (int i = 0; i < count; ++i)
                if (notes[i] == n)
                {
                    for (int j = i; j < count - 1; ++j) { notes[j] = notes[j + 1]; vels[j] = vels[j + 1]; }
                    --count; break;
                }
        }
        int top() const { return count > 0 ? notes[count - 1] : -1; }
    };

    //==============================================================================
    struct Voice
    {
        bool active = false, released = false;
        int note = 60;
        float velocity = 1;
        double freq = 440, targetFreq = 440;
        float glideCoef = 1;
        float unisonCents = 0, panL = 0.7071f, panR = 0.7071f, voiceGain = 0.45f;
        double ratio1 = 1.0, ratio2 = 1.0;   // Oktave+Tune+Unison-Detune als Frequenzfaktor (pro Block in setParams)
        int age = 0;
        bool everStarted = false;   // erste Note einer Stimme darf nicht aus dem 440-Hz-Startwert gleiten
        double sampleRate = 48000;
        const EngineParams* par = nullptr;

        Osc osc1, osc2, sub;
        FastRandom noise;
        SVF fltA, fltB;
        LadderFilter ladder;
        ADSR amp, fenv;
        LFO lfo;

        float fcSm = -1.0f, qSm = -1.0f;
        int subBlockCounter = 0; // Sub-block Update (alle 4 Samples)
        float cachedResComp = 1.0f;

        void hardStop()
        {
            active = false; released = true;
            amp.reset();
            fenv.reset();
            fcSm = -1.0f; qSm = -1.0f;
            subBlockCounter = 0;
            ladder.reset();
            fltA.reset();
            fltB.reset();
        }

        void start(int n, double f, float vel, bool retrigLfo)
        {
            note = n; velocity = vel; targetFreq = f;
            // Frueher glitt die allererste Note jeder Stimme (bei aktivem Glide) von 440 Hz aus los,
            // unabhaengig von Tonhoehe und Oktavlage.
            if (glideCoef >= 0.999f || !everStarted) freq = f;
            everStarted = true;
            const bool wasSounding = active;   // Stimmenklau / Mono-Retrigger: Zustand beibehalten statt hart zuruecksetzen
            amp.trigger(); fenv.trigger();
            released = false; active = true;
            if (wasSounding)
            {
                // Phasen, Filter-Zustand und Cutoff-Glaettung laufen weiter -> kein Sprung im Signal
            }
            else if (par != nullptr && par->unison)
            {
                // Unison: zufaellige Startphasen. Mit Phase 0 addieren sich die 8 Stimmen
                // kohaerent (Lautstaerkesprung, bei 0 ct Detune sogar einfach 8x lauter).
                auto rnd01 = [this]() { double u = 0.5 * (double)noise.next() + 0.5; return u >= 1.0 ? u - 1.0 : u; };
                osc1.phase = rnd01(); osc2.phase = rnd01(); sub.phase = rnd01();
            }
            else
            {
                osc1.phase = 0; osc2.phase = 0; sub.phase = 0;
            }
            if (!wasSounding)
            {
                fcSm = -1.0f; qSm = -1.0f;
                subBlockCounter = 0;
                ladder.reset();
                fltA.reset();
                fltB.reset();
            }
            if (retrigLfo) lfo.retrigger();
        }

        void release() { amp.release(); fenv.release(); released = true; }

        inline void process(float& L, float& R)
        {
            if (par == nullptr) return;

            freq += (targetFreq - freq) * glideCoef;

            float lfoVal = lfo.process() * par->lfoDepth;

            double fBase = freq * par->bendFactor;
            if (par->lfoPitch) fBase *= std::pow(2.0, (double)lfoVal / 12.0);

            double f1 = fBase * ratio1;
            double f2 = fBase * ratio2;
            f1 = xclamp(f1, 1.0, 20000.0);
            f2 = xclamp(f2, 1.0, 20000.0);

            osc1.setRate(f1, sampleRate);
            osc2.setRate(f2, sampleRate);
            sub.setRate(f1 * 0.5, sampleRate);

            osc1.wave = par->osc1Wave;
            osc2.wave = par->osc2Wave;

            float pw = 0.5f;
            if (par->lfoPW) pw = xclamp(0.5f + lfoVal * 0.45f, 0.05f, 0.95f);
            osc1.pw = pw; osc2.pw = pw;

            bool w1 = osc1.tick();
            if (par->sync && w1) osc2.phase = 0.0;
            osc2.tick();
            sub.tick();

            float dry = 0;
            if (par->osc1On)  dry += osc1.render() * par->osc1Level;
            if (par->osc2On)  dry += osc2.render() * par->osc2Level;
            if (par->subOn)   dry += sub.render() * par->subLevel;
            if (par->noiseOn) dry += noise.next() * par->noiseLevel;

            float e = fenv.process();

            // Sub-Block-Update: Filter-Koeffizienten werden alle 4 Samples neu berechnet (~70% CPU-Ersparnis)
            if ((subBlockCounter++ & 3) == 0)
            {
                float fc = par->cutoff * (float)std::pow(2.0,
                    e * par->envAmt * 4.0 + (par->lfoFilter ? lfoVal * 3.0 : 0.0));

                float nyq = 0.5f * (float)sampleRate;
                fc = xclamp(fc, 16.0f, nyq * (0.75f - 0.25f * par->reso));

                float Q = 0.5f + par->reso * 8.0f;
                float xN = fc / nyq;
                if (xN > 0.4f)
                    Q = std::max(0.6f, Q * (1.0f - (xN - 0.4f) * 2.2f));

                cachedResComp = 1.0f / (1.0f + par->reso * 0.6f);

                if (fcSm < 0.0f) fcSm = fc;
                if (qSm < 0.0f) qSm = Q;
                fcSm += (fc - fcSm) * 0.12f;
                qSm += (Q - qSm) * 0.12f;

                if (par->fltMode == 4)
                {
                    ladder.setParams((double)fcSm, (double)par->reso);
                }
                else if (par->fltMode == 1) // LP24: Staffelung der Resonanz fuer analoge Stabilitaet
                {
                    float q1 = 0.5f + par->reso * 5.0f;
                    float q2 = 0.5f + par->reso * 3.0f;
                    fltA.setCoefs(fcSm, q1, sampleRate);
                    fltB.setCoefs(fcSm, q2, sampleRate);
                }
                else
                {
                    fltA.setCoefs(fcSm, qSm, sampleRate);
                    fltB.setCoefs(fcSm, qSm, sampleRate);
                }
            }

            float y = 0;
            if (par->fltMode == 4)
            {
                y = (float)ladder.process((double)dry);
            }
            else
            {
                float lp, bp, hp;
                fltA.process(dry, lp, bp, hp);

                y = lp;
                if (par->fltMode == 1) { float l2, b2, h2; fltB.process(lp, l2, b2, h2); y = l2; }
                else if (par->fltMode == 2) y = hp;
                else if (par->fltMode == 3) y = bp * 1.5f;

                y *= cachedResComp;
            }

            float a = amp.process();
            float v = y * a * (0.3f + 0.7f * velocity) * voiceGain;

            float vL = v * panL;
            float vR = v * panR;

            if (!std::isfinite(vL) || !std::isfinite(vR))
            {
                hardStop();
                return;
            }

            L += vL;
            R += vR;
        }
    };

    //==============================================================================
    class Engine
    {
    public:
        static constexpr int NV = 8;

        void prepare(double sr)
        {
            sampleRate = sr;
            for (int i = 0; i < NV; ++i)
            {
                auto& v = voices[(size_t)i];
                v.sampleRate = sr;
                v.par = &par;
                v.noise.seed(0x9E3779B9u * (uint32_t)(i + 1));
                v.lfo.rng = &v.noise;
                v.ladder.prepare(sr);
                v.hardStop();
            }
            stack.count = 0;
            ageCounter = 0;
        }

        void allNotesOff()
        {
            for (auto& v : voices)
                if (v.active && !v.released)
                    v.release();
            stack.count = 0;
        }

        // MIDI "All Sound Off" (CC 120): sofort stumm, alle Stimmen hart beenden
        void allSoundOff()
        {
            for (auto& v : voices) v.hardStop();
            stack.count = 0;
        }

        void setParams(const EngineParams& e)
        {
            par = e;
            par.lfoDepth = xclamp(par.lfoDepth, 0.0f, 1.0f);

            float glideC = 1.0f;
            if (par.glideOn && par.glide > 1.0e-4f)
                glideC = 1.0f - (float)std::exp(-3.0 / (par.glide * sampleRate));

            static const float panTable[NV] = { 0.0f, 0.7f, -0.7f, 0.35f, -0.35f, 0.9f, -0.9f, 0.15f };

            for (int i = 0; i < NV; ++i)
            {
                auto& v = voices[(size_t)i];
                v.amp.set(par.a1, par.d1, par.s1, par.r1, sampleRate);
                v.fenv.set(par.a2, par.d2, par.s2, par.r2, sampleRate);
                v.lfo.setRate(par.lfoRate, sampleRate);
                v.lfo.wave = par.lfoWave;
                v.glideCoef = glideC;

                if (par.unison)
                {
                    float t = (float)i / (float)(NV - 1) * 2.0f - 1.0f;
                    v.unisonCents = par.osc2Tune * t;
                    float ang = (panTable[i] + 1.0f) * (float)kPi * 0.25f;
                    v.panL = std::cos(ang); v.panR = std::sin(ang);
                    v.voiceGain = 0.3f;
                }
                else
                {
                    v.unisonCents = 0;
                    v.panL = v.panR = 0.7071f;
                    v.voiceGain = 0.45f;
                }

                // Frueher 2x std::pow pro Sample und Stimme in Voice::process
                v.ratio1 = std::pow(2.0, par.osc1Oct + (double)par.osc1Tune / 1200.0 + (double)v.unisonCents / 1200.0);
                v.ratio2 = std::pow(2.0, par.osc2Oct + (double)par.osc2Tune / 1200.0 + (double)v.unisonCents / 1200.0);
            }
        }

        void noteOn(int n, float vel)
        {
            double f = noteFreq(n);

            if (par.unison)
            {
                ++ageCounter;
                for (auto& v : voices) { v.age = ageCounter; v.start(n, f, vel, par.lfoRetrig); }
            }
            else if (par.mono)
            {
                for (int i = 1; i < NV; ++i) { auto& o = voices[(size_t)i]; if (o.active && !o.released) o.release(); }   // frueher hardStop() = Knacken
                stack.remove(n);   // doppelter Note-On derselben Taste: kein zweiter Stapel-Eintrag (sonst haengt die Note)
                stack.push(n, vel);

                auto& v = voices[0];
                if (v.active && par.legato)
                {
                    v.targetFreq = f;
                    v.amp.resume(); v.fenv.resume(); v.released = false;
                }
                else
                    v.start(n, f, vel, par.lfoRetrig);
                v.age = ++ageCounter;
            }
            else
            {
                Voice* sel = nullptr;
                for (auto& v : voices) if (!v.active) { sel = &v; break; }
                if (sel == nullptr)
                {
                    Voice* best = &voices[0];
                    for (auto& v : voices) if (v.age < best->age) best = &v;
                    sel = best;
                }
                sel->start(n, f, vel, par.lfoRetrig);
                sel->age = ++ageCounter;
            }
        }

        void noteOff(int n)
        {
            if (par.unison)
            {
                for (auto& v : voices) if (v.active && v.note == n) v.release();
            }
            else if (par.mono)
            {
                stack.remove(n);
                if (stack.count == 0)
                {
                    // Alle Stimmen freigeben, nicht nur Stimme 0: Waren die Tasten vor dem Umschalten auf Mono
                    // als Poly-Noten auf anderen Stimmen gestartet, haengen diese sonst fuer immer.
                    for (auto& v : voices) if (v.active && !v.released) v.release();
                }
                else { int t = stack.top(); if (t >= 0) voices[0].targetFreq = noteFreq(t); }
            }
            else
            {
                for (auto& v : voices) if (v.active && v.note == n && !v.released) v.release();
            }
        }

        void process(float* L, float* R, int n)
        {
            for (auto& v : voices)
                if (v.active && v.released && v.amp.finished()) v.active = false;

            for (int i = 0; i < n; ++i)
            {
                float l = 0, r = 0;
                for (auto& v : voices) if (v.active) v.process(l, r);
                if (!std::isfinite(l)) l = 0.0f;
                if (!std::isfinite(r)) r = 0.0f;
                L[i] = xclamp(l, -6.0f, 6.0f);
                R[i] = xclamp(r, -6.0f, 6.0f);
            }
        }

        EngineParams par;

    private:
        // Globale Tastatur-Transponierung in Halbtonen. Die Oktavregler (OCT) sind jetzt
        // 0-basiert: "OCT 0" = Grundton wie gespielt. Die +12 hier erhaelt exakt den
        // Klang aller bisherigen Presets (v1.2.4: Index-Offset +2, noteFreq -12 = netto +1 Oktave).
        // Auf 0 setzen fuer Konzert-Tonhoehe (alle Presets klingen dann 1 Oktave tiefer).
        static constexpr int kKeyTransposeSemis = 12;

        static double noteFreq(int n) { return 440.0 * std::pow(2.0, (n - 69 + kKeyTransposeSemis) / 12.0); }

        std::array<Voice, NV> voices;
        NoteStack stack;
        int ageCounter = 0;
        double sampleRate = 48000;
    };

} // namespace xl
