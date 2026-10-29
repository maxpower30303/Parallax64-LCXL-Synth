// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Flix
#pragma once
#include <array>
#include <atomic>
#include <bitset>
#include <unordered_map>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SynthParams.h"
#include "SynthEngine.h"

//==============================================================================
class Parallax64AudioProcessor : public juce::AudioProcessor
{
public:
    Parallax64AudioProcessor();
    ~Parallax64AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Parallax64-LCXL-Synth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::String getOledText() const;

    void touchParameter(int specIndex) { lastTouched.store(specIndex); }

    // ---------- Preset-API (Message-Thread) ----------
    static constexpr int kUserBase = 1000;

    int          getNumFactory() const;
    juce::String getFactoryName(int idx) const;
    void         applyFactoryPreset(int idx);

    int          refreshAndGetUserPresets();
    juce::String getUserPresetName(int idx) const;
    void         applyUserPreset(int idx);

    juce::String nextAutoPresetName() const;
    int          saveUserPresetAsNew(const juce::String& name);

    // ---------- MIDI Learn (Message-Thread-API) ----------
    void startMidiLearn(int specIdx);
    void cancelMidiLearn();
    bool isLearning(int specIdx) const;
    int  getMappedCCFor(int specIdx) const;
    void removeMappingForParam(int specIdx);
    void clearAllMidiMappings();
    int  getMappingCount() const;
    void syncMidiMapIfDirty();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String sanitizeFileName(const juce::String& in);

    float valAt(int specIdx) const;   // allokationsfrei, siehe P64_VAL in PluginProcessor.cpp
    float dbToGain(float db) const
    {
        return db <= -59.5f ? 0.0f : juce::Decibels::decibelsToGain(db);
    }

    double arpStepQuarters() const;
    void   arpAdvance(int slot);
    void   buildArpLadder();

    void addHeldNote(int note, float vel);
    void removeHeldNote(int note);

    static bool isLearnableCC(int cc);
    void ensureMidiMapNode();
    void reloadMidiMapRuntime();
    void syncMidiMapToState();

    struct MidiMapEntry
    {
        std::atomic<int> cc{ -1 };
        std::atomic<int> paramIdx{ -1 };
    };

    static constexpr int kMaxMaps = 64;
    MidiMapEntry      midiMap[kMaxMaps];
    std::atomic<int>  midiMapCount{ 0 };
    std::atomic<bool> mapDirty{ false };
    std::atomic<int>  learnTarget{ -1 };

    struct CCRoute { juce::RangedAudioParameter* p = nullptr; int idx = -1; };

    // Pro Spec-Index gecachte Zeiger (Audio-Thread-tauglich, keine String-Lookups)
    std::vector<std::atomic<float>*>        rawByIdx;
    std::vector<juce::RangedAudioParameter*> paramByIdx;
    std::array<CCRoute, 128> ccRoutes{};

    // Letzter Rohwert pro CC (Audio-Thread) – Flankenerkennung Button-Toggles
    std::array<int, 128> lastCCRaw{};

    std::atomic<int> lastTouched{ -1 };
    juce::String lastPresetName;

    bool isStandalone = false;
    juce::File presetDir, lastStateFile;
    juce::Array<juce::File> userFiles;

    // Distortion HP-Filter-Zustand mit Denormal-Schutz
    struct DistStage
    {
        float hpL = 0.0f, hpLx = 0.0f;
        float hpR = 0.0f, hpRx = 0.0f;
        float hpA = 0.995f;   // Koeffizient des 40-Hz-DC-Blockers

        void prepare(double sr)
        {
            // Fester DC-Blocker statt amount-abhaengigem Hochpass (frueher bis 500 Hz -> Bass wurde duenn)
            hpA = (float)std::exp(-6.283185307179586 * 40.0 / (sr > 1000.0 ? sr : 44100.0));
            reset();
        }

        void reset()
        {
            hpL = hpR = 0.0f;
            hpLx = hpRx = 0.0f;
        }

        static inline float shape(float x, float amount)
        {
            x = xl::xclamp(x, -5.0f, 5.0f);
            float drive = 1.0f + amount * 9.0f;
            float pre = x * drive;
            float bias = 0.20f * amount;
            float sat = std::tanh(pre + bias) - std::tanh(bias);
            return xl::xclamp(sat, -2.5f, 2.5f);
        }

        inline float toneL(float x)
        {
            if (!std::isfinite(hpL)) hpL = 0.0f;
            float y = x - hpLx + hpA * hpL;
            hpLx = x; hpL = y;
            return y;
        }

        inline float toneR(float x)
        {
            if (!std::isfinite(hpR)) hpR = 0.0f;
            float y = x - hpRx + hpA * hpR;
            hpRx = x; hpR = y;
            return y;
        }

        static inline float limit(float x)
        {
            return x / (1.0f + std::fabs(x) * 0.8f);
        }
    } dist;

    float bendSemis = 0, modVal = 0;
    double lastSR = 48000;

    xl::Engine engine;

    juce::SmoothedValue<float> masterSm, dlySm, dlyMixSm, fbSm, driveSm, revSm;
    bool delayIdle = false, reverbIdle = false;   // Effekt ist ausgeschaltet + Zustand bereits geleert

    // Power-of-Two Ringbuffer fuer schnelles Bitmask-Wrapping mit 5 Stereo-Modi
    struct SimpleDelay
    {
        std::vector<float> bufL, bufR;
        int bufMask = 0;
        int bufSize = 0;
        int wpos = 0;

        // Feedback filter states
        float lpL = 0.0f, lpR = 0.0f;
        float hpL = 0.0f, hpR = 0.0f;
        float prevInL = 0.0f, prevInR = 0.0f;

        // Tape drift / LFO modulation (0.75 Hz)
        double modPhaseL = 0.0;
        double modPhaseR = 0.25; // 90 Grad Quadratur fuer breites Stereo
        double modInc = 0.0;

        // Diffuser allpass filter states
        float apBufL[512] = {};
        float apBufR[512] = {};
        int apPos = 0;

        void prepare(double sr, float maxSec)
        {
            int req = (int)(sr * maxSec) + 64;
            bufSize = 1;
            while (bufSize < req) bufSize <<= 1;
            bufMask = bufSize - 1;

            bufL.assign((size_t)bufSize, 0.0f);
            bufR.assign((size_t)bufSize, 0.0f);
            wpos = 0;
            lpL = lpR = 0.0f;
            hpL = hpR = 0.0f;
            prevInL = prevInR = 0.0f;

            modPhaseL = 0.0;
            modPhaseR = 0.25;
            modInc = 0.75 / (sr > 1000.0 ? sr : 44100.0);

            std::fill(std::begin(apBufL), std::end(apBufL), 0.0f);
            std::fill(std::begin(apBufR), std::end(apBufR), 0.0f);
            apPos = 0;
        }

        void reset()
        {
            std::fill(bufL.begin(), bufL.end(), 0.0f);
            std::fill(bufR.begin(), bufR.end(), 0.0f);
            wpos = 0;
            lpL = lpR = 0.0f;
            hpL = hpR = 0.0f;
            prevInL = prevInR = 0.0f;
            std::fill(std::begin(apBufL), std::end(apBufL), 0.0f);
            std::fill(std::begin(apBufR), std::end(apBufR), 0.0f);
            apPos = 0;
        }

        inline float readAt(const std::vector<float>& b, float delaySamples) const
        {
            float rp = (float)wpos - delaySamples;
            int i0 = (int)std::floor(rp);
            float fr = rp - (float)i0;
            int idx0 = i0 & bufMask;
            int idx1 = (i0 + 1) & bufMask;
            return b[(size_t)idx0] * (1.0f - fr) + b[(size_t)idx1] * fr;
        }

        inline void processSample(float& l, float& r, float dS, float fb, int mode, float mix)
        {
            if (bufSize == 0) return;

            dS = xl::xclamp(dS, 2.0f, (float)(bufSize - 64));
            fb = xl::xclamp(fb, 0.0f, 0.95f);

            // Tape drift LFO
            modPhaseL += modInc;
            if (modPhaseL >= 1.0) modPhaseL -= 1.0;
            modPhaseR += modInc;
            if (modPhaseR >= 1.0) modPhaseR -= 1.0;

            float dSL = dS;
            float dSR = dS;

            if (mode == 0) // STEREO WIDE (Haas Micro-Spread)
            {
                dSL = dS;
                dSR = dS * 1.02f + 20.0f;
            }
            else if (mode == 1) // PING-PONG
            {
                dSL = dS;
                dSR = dS;
            }
            else if (mode == 2) // TAPE DRIFT (Vintage Wow/Flutter Modulation)
            {
                float modL = (float)std::sin(modPhaseL * 6.283185307179586) * 32.0f;
                float modR = (float)std::cos(modPhaseR * 6.283185307179586) * 32.0f;
                dSL = xl::xclamp(dS + modL, 2.0f, (float)(bufSize - 64));
                dSR = xl::xclamp(dS + modR, 2.0f, (float)(bufSize - 64));
            }
            else if (mode == 3) // CROSS 3:4 (Polyrhythmus Dotted 8th gegen Straight)
            {
                dSL = dS;
                dSR = dS * 0.75f;
            }
            else if (mode == 4) // DIFFUSE AMBIENT
            {
                dSL = dS;
                dSR = dS * 1.01f + 12.0f;
            }

            dSL = xl::xclamp(dSL, 2.0f, (float)(bufSize - 64));
            dSR = xl::xclamp(dSR, 2.0f, (float)(bufSize - 64));

            float wl = readAt(bufL, dSL);
            float wr = readAt(bufR, dSR);

            if (!std::isfinite(wl)) wl = 0.0f;
            if (!std::isfinite(wr)) wr = 0.0f;

            // Allpass-Diffusion fuer Diffuse Mode
            if (mode == 4)
            {
                int apIdx = apPos & 511;
                float apInL = wl, apInR = wr;
                float apOutL = -0.5f * apInL + apBufL[apIdx];
                apBufL[apIdx] = apInL + 0.5f * apOutL;
                float apOutR = -0.5f * apInR + apBufR[apIdx];
                apBufR[apIdx] = apInR + 0.5f * apOutR;
                wl = apOutL;
                wr = apOutR;
                apPos = (apPos + 1) & 511;
            }

            // Cross-Feedback je nach Modus
            float fbInL = 0.0f, fbInR = 0.0f;
            if (mode == 1) // Ping-Pong: Left feeds Right, Right feeds Left
            {
                fbInL = wr;
                fbInR = wl;
            }
            else if (mode == 3) // Cross 3:4: 70% eigen, 30% kreuz
            {
                fbInL = wl * 0.70f + wr * 0.30f;
                fbInR = wr * 0.70f + wl * 0.30f;
            }
            else
            {
                fbInL = wl;
                fbInR = wr;
            }

            // Analoge Tanh-Softsaturation im Feedback-Pfad: 100% warm & explosionssicher
            float satL = std::tanh(fb * fbInL * 1.05f);
            float satR = std::tanh(fb * fbInR * 1.05f);

            // Dual-Filter im Feedback:
            // 1) High-Pass (~160 Hz) schneidet Mulm und Subbass ab
            float hpCoeff = 0.978f;
            hpL = hpCoeff * (hpL + satL - prevInL);
            hpR = hpCoeff * (hpR + satR - prevInR);
            prevInL = satL;
            prevInR = satR;

            // 2) Low-Pass (~4.5 kHz, im Tape-Modus noch waermer)
            float lpDamp = (mode == 2) ? 0.44f : 0.35f;
            lpL += lpDamp * (hpL - lpL);
            lpR += lpDamp * (hpR - lpR);

            if (!std::isfinite(lpL)) lpL = 0.0f;
            if (!std::isfinite(lpR)) lpR = 0.0f;

            // Ping-Pong: Eingang nur in den linken Puffer (Mono-Summe), sonst kommen beide
            // Kanaele gleichzeitig zurueck und das Kreuz-Feedback bewirkt nichts Hoerbares.
            float inL = l, inR = r;
            if (mode == 1) { inL = 0.5f * (l + r); inR = 0.0f; }

            bufL[(size_t)wpos] = xl::xclamp(inL + lpL, -4.0f, 4.0f);
            bufR[(size_t)wpos] = xl::xclamp(inR + lpR, -4.0f, 4.0f);

            l = l * (1.0f - mix) + wl * mix;
            r = r * (1.0f - mix) + wr * mix;

            wpos = (wpos + 1) & bufMask;
        }
    } delay;

    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbP;
    float dampA = 0.3f;
    std::vector<float> tmpL, tmpR;

    // ---------- Arp-Zustand ----------
    std::vector<std::pair<int, float>> arpHeld;
    // UP/DOWN schreibt bis zu 2*64-2 = 126 Eintraege (vorher int[64] -> Ueberlauf ab >33 Eintraegen)
    static constexpr int kArpLadderMax = 128;
    int  arpLadder[kArpLadderMax] = {};
    int  arpLadderLen = 0;
    int  arpPos = 0;
    int  arpLadPos = 0;
    int  arpNote = -1;
    bool gateOffDone = false;
    long long lastArpStep = -1;
    double arpFrac = 0.0;
    juce::uint32 arpRng = 0x51ED2701u;
    bool lastArpOn = false;
    bool lastMono = false, lastUnison = false;   // Spielmodus-Wechsel erkennen (siehe processBlock)

    // Sustain-Pedal (CC 64): Note-Offs werden gemerkt und beim Loslassen des Pedals nachgeholt
    bool sustainDown = false;
    std::bitset<128> sustained;
    void releaseAllNotes(bool hard);    // CC 123 / CC 120 / Moduswechsel: alles beenden (inkl. Arp, Sustain-Merker)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Parallax64AudioProcessor)
};
