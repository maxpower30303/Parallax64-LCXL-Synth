// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Flix
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <algorithm>
#include <map>
#include <cmath>
#include <cstring>

// Audio-Thread-sicherer Parameterzugriff: die Spec-Position einer ID wird pro Aufrufstelle
// genau einmal aufgeloest (function-local static) und danach nur noch als Index benutzt.
// Vorher: val("x") baute pro Aufruf einen juce::String (Heap-Allokation) -> ~70x pro Block.
#define P64_IDX(id) ([]() noexcept -> int { static const int i_ = xl::indexOf(id); return i_; }())
#define P64_VAL(id) valAt(P64_IDX(id))

//==============================================================================
Parallax64AudioProcessor::Parallax64AudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "STATE", createParameterLayout())
{
    rawByIdx.assign(xl::getSpecs().size(), nullptr);
    paramByIdx.assign(xl::getSpecs().size(), nullptr);
    arpHeld.reserve(256);   // bis zu 128 gehaltene Noten ohne Neuallokation im Audio-Thread

    for (size_t i = 0; i < xl::getSpecs().size(); ++i)
    {
        const auto& s = xl::getSpecs()[i];
        rawByIdx[i] = apvts.getRawParameterValue(s.id);
        paramByIdx[i] = apvts.getParameter(s.id);

        if (s.cc >= 0)
        {
            auto& r = ccRoutes[(size_t)s.cc];
            r.p = apvts.getParameter(s.id);
            r.idx = (int)i;
        }
    }

    reverbP.roomSize = 0.8f; reverbP.damping = 0.5f;
    reverbP.wetLevel = 1.0f; reverbP.dryLevel = 0.0f;
    reverbP.width = 1.0f;    reverbP.freezeMode = 0.0f;

    isStandalone = (wrapperType == wrapperType_Standalone);

    presetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Parallax64").getChildFile("Presets");
    presetDir.createDirectory();

    lastStateFile = presetDir.getParentDirectory().getChildFile("lastState.xml");

    if (isStandalone && lastStateFile.existsAsFile())
        if (auto xml = juce::parseXML(lastStateFile))
            if (xml->hasTagName(apvts.state.getType()))
                apvts.replaceState(juce::ValueTree::fromXml(*xml));

    ensureMidiMapNode();
    reloadMidiMapRuntime();
}

Parallax64AudioProcessor::~Parallax64AudioProcessor()
{
    if (isStandalone)
    {
        syncMidiMapToState();
        if (auto xml = apvts.copyState().createXml())
            xml->writeTo(lastStateFile);
    }
}

//==============================================================================
juce::AudioProcessorEditor* Parallax64AudioProcessor::createEditor()
{
    return new Parallax64AudioProcessorEditor(*this);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout Parallax64AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : xl::getSpecs())
        layout.add(xl::makeParam(s));
    return layout;
}

float Parallax64AudioProcessor::valAt(int idx) const
{
    return (idx >= 0 && idx < (int)rawByIdx.size())
        ? rawByIdx[(size_t)idx]->load(std::memory_order_relaxed)
        : 0.0f;
}

juce::String Parallax64AudioProcessor::getOledText() const
{
    int lt = learnTarget.load();
    if (lt >= 0 && lt < (int)xl::getSpecs().size())
        return "MIDI LEARN\n" + juce::String(xl::getSpecs()[(size_t)lt].name) + "\nCC senden...";

    int i = lastTouched.load();
    if (i == -2) return "PRESET\n" + lastPresetName;

    if (i >= 0 && i < (int)xl::getSpecs().size())
    {
        const auto& s = xl::getSpecs()[(size_t)i];
        juce::String id(s.id);

        bool lfoSync = P64_VAL("lfoSync") > 0.5f;
        bool dlySync = P64_VAL("dlySync") > 0.5f;

        if ((id == "lfoRate" && lfoSync) || (id == "dlyTime" && dlySync))
        {
            if (id == "lfoRate")
            {
                float norm = apvts.getParameter("lfoRate")->getValue();
                int st = juce::jlimit(0, 7, (int)(norm * 8.0f));
                return juce::String(s.name) + " SYNC\n" + xl::tempoStepNames[st];
            }
            else
            {
                float norm = apvts.getParameter("dlyTime")->getValue();
                int st = juce::jlimit(0, xl::kDelaySyncSteps - 1, (int)(norm * (float)xl::kDelaySyncSteps));
                return juce::String(s.name) + " SYNC\n" + xl::delaySyncNames[st];
            }
        }

        if (id == "dlyMode")
        {
            int m = (int)P64_VAL("dlyMode");
            if (m >= 0 && m < 5)
                return "DELAY MODE\n" + juce::String(xl::dlyModes[m]);
        }

        auto* p = apvts.getParameter(s.id);
        return juce::String(s.name) + "\n" + p->getText(p->getValue(), 32);
    }

    return "PARALLAX64-LCXL\nREADY";
}

//==============================================================================
bool Parallax64AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void Parallax64AudioProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    lastSR = sr;
    engine.prepare(sr);

    lastCCRaw.fill(-1);

    masterSm.reset(sr, 0.03);
    dlySm.reset(sr, 0.08);
    dlyMixSm.reset(sr, 0.05);
    fbSm.reset(sr, 0.05);
    driveSm.reset(sr, 0.03);
    revSm.reset(sr, 0.03);

    masterSm.setCurrentAndTargetValue(dbToGain(P64_VAL("master") + P64_VAL("presetLevel")));
    dlySm.setCurrentAndTargetValue(P64_VAL("dlyTime"));
    dlyMixSm.setCurrentAndTargetValue(P64_VAL("dlyMix"));
    fbSm.setCurrentAndTargetValue(P64_VAL("dlyFb"));
    driveSm.setCurrentAndTargetValue(P64_VAL("drive"));
    revSm.setCurrentAndTargetValue(P64_VAL("revOn") > 0.5f ? P64_VAL("revMix") : 0.0f);

    delay.prepare(sr, 4.5f);
    dampA = 1.0f - (float)std::exp(-2.0 * xl::kPi * 5200.0 / sr);

    reverb.prepare({ sr, (juce::uint32)samplesPerBlock, 2 });
    reverb.setParameters(reverbP);   // konstant -> einmal hier statt in jedem Block

    delayIdle = false;
    reverbIdle = false;

    dist.prepare(sr);

    tmpL.assign((size_t)std::max(samplesPerBlock, 256), 0.0f);
    tmpR.assign((size_t)std::max(samplesPerBlock, 256), 0.0f);
}

//==============================================================================
// ------------------------- Arpeggiator --------------------------------------
//==============================================================================
double Parallax64AudioProcessor::arpStepQuarters() const
{
    static const double q[8] = { 4.0, 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    int r = (int)P64_VAL("arpRate");
    return q[juce::jlimit(0, 7, r)];
}

void Parallax64AudioProcessor::buildArpLadder()
{
    arpLadderLen = 0;
    if (arpHeld.empty()) return;

    int oct = 1 + (int)P64_VAL("arpOct");
    int dir = (int)P64_VAL("arpDir");

    int base[32]; int nb = 0;
    for (auto& h : arpHeld)
        if (nb < 32) base[nb++] = h.first;

    int up[64]; int nu = 0;
    for (int o = 0; o < oct; ++o)
        for (int i = 0; i < nb; ++i)
            if (nu < 64) up[nu++] = base[i] + 12 * o;

    if (dir == 1)
    {
        for (int i = nu - 1; i >= 0; --i) arpLadder[arpLadderLen++] = up[i];
    }
    else if (dir == 2)
    {
        for (int i = 0; i < nu; ++i) arpLadder[arpLadderLen++] = up[i];
        for (int i = nu - 2; i >= 1; --i) arpLadder[arpLadderLen++] = up[i];
    }
    else
    {
        for (int i = 0; i < nu; ++i) arpLadder[arpLadderLen++] = up[i];
    }
}

void Parallax64AudioProcessor::arpAdvance(int slot)
{
    if (arpHeld.empty())
    {
        if (arpNote >= 0) { engine.noteOff(arpNote); arpNote = -1; }
        return;
    }
    buildArpLadder();
    if (arpLadderLen <= 0) return;

    static const int stepIdx[8] =
    { xl::indexOf("arpStep1"), xl::indexOf("arpStep2"), xl::indexOf("arpStep3"), xl::indexOf("arpStep4"),
      xl::indexOf("arpStep5"), xl::indexOf("arpStep6"), xl::indexOf("arpStep7"), xl::indexOf("arpStep8") };

    bool stepOn = valAt(stepIdx[slot & 7]) > 0.5f;

    if (!stepOn)
    {
        if (arpNote >= 0 && !gateOffDone)
        {
            engine.noteOff(arpNote);
            gateOffDone = true;
        }
        return;
    }

    int dir = (int)P64_VAL("arpDir");

    int idx;
    if (dir == 3)
    {
        arpRng = arpRng * 1664525u + 1013904223u;
        idx = (int)((arpRng >> 16) % (juce::uint32)arpLadderLen);
    }
    else
    {
        idx = arpLadPos % arpLadderLen;
        ++arpLadPos;
    }

    int note = arpLadder[(size_t)idx];

    if (note != arpNote || gateOffDone)
    {
        if (arpNote >= 0) engine.noteOff(arpNote);
        engine.noteOn(note, 1.0f);
        arpNote = note;
    }
    gateOffDone = false;
}

void Parallax64AudioProcessor::addHeldNote(int note, float vel)
{
    for (auto& h : arpHeld)
        if (h.first == note) { h.second = vel; return; }

    auto it = std::lower_bound(arpHeld.begin(), arpHeld.end(), note,
        [](const std::pair<int, float>& p, int x) { return p.first < x; });
    arpHeld.insert(it, { note, vel });
}

void Parallax64AudioProcessor::removeHeldNote(int note)
{
    arpHeld.erase(std::remove_if(arpHeld.begin(), arpHeld.end(),
        [note](const std::pair<int, float>& p) { return p.first == note; }),
        arpHeld.end());
}

//==============================================================================
void Parallax64AudioProcessor::releaseAllNotes(bool hard)
{
    sustainDown = false;
    sustained.reset();
    arpHeld.clear();
    if (arpNote >= 0) { if (!hard) engine.noteOff(arpNote); arpNote = -1; }
    gateOffDone = false;
    if (hard) engine.allSoundOff(); else engine.allNotesOff();
}

//==============================================================================
// ------------------------- MIDI Learn ---------------------------------------
//==============================================================================
bool Parallax64AudioProcessor::isLearnableCC(int cc)
{
    if (cc < 2)                       return false;
    if (cc == 6 || cc == 32 || cc == 38) return false;
    if (cc >= 96 && cc <= 101)        return false;
    if (cc >= 120)                    return false;
    return true;
}

void Parallax64AudioProcessor::ensureMidiMapNode()
{
    auto first = apvts.state.getOrCreateChildWithName("MIDIMAP", nullptr);
    for (int i = apvts.state.getNumChildren(); --i >= 0;)   // Ueberzaehlige Knoten (aus alten States) entfernen
        if (apvts.state.getChild(i).hasType("MIDIMAP") && apvts.state.getChild(i) != first) apvts.state.removeChild(i, nullptr);
}

void Parallax64AudioProcessor::reloadMidiMapRuntime()
{
    auto node = apvts.state.getChildWithName("MIDIMAP");
    int n = 0;

    if (node.isValid())
    {
        for (int i = 0; i < node.getNumChildren() && n < kMaxMaps; ++i)
        {
            auto e = node.getChild(i);
            int cc = (int)e.getProperty("cc", -1);
            juce::String id = e.getProperty("id", "").toString();
            if (cc < 0 || id.isEmpty()) continue;

            int pi = -1;
            for (int s = 0; s < (int)xl::getSpecs().size(); ++s)
                if (id == xl::getSpecs()[(size_t)s].id) { pi = s; break; }
            if (pi < 0) continue;

            midiMap[n].cc.store(cc);
            midiMap[n].paramIdx.store(pi);
            ++n;
        }
    }
    midiMapCount.store(n);
}

void Parallax64AudioProcessor::syncMidiMapToState()
{
    auto node = apvts.state.getOrCreateChildWithName("MIDIMAP", nullptr);
    node.removeAllChildren(nullptr);

    for (int i = 0, cnt = midiMapCount.load(); i < cnt; ++i)
    {
        int cc = midiMap[i].cc.load();
        int pi = midiMap[i].paramIdx.load();
        if (cc < 0 || pi < 0 || pi >= (int)xl::getSpecs().size()) continue;

        juce::ValueTree e("map");
        e.setProperty("cc", cc, nullptr);
        e.setProperty("id", juce::String(xl::getSpecs()[(size_t)pi].id), nullptr);
        node.appendChild(e, nullptr);
    }
    mapDirty.store(false);
}

void Parallax64AudioProcessor::syncMidiMapIfDirty()
{
    if (mapDirty.load())
        syncMidiMapToState();
}

void Parallax64AudioProcessor::startMidiLearn(int specIdx)
{
    if (specIdx < 0 || specIdx >= (int)xl::getSpecs().size()) return;
    learnTarget.store(specIdx);
}

void Parallax64AudioProcessor::cancelMidiLearn()
{
    learnTarget.store(-1);
}

bool Parallax64AudioProcessor::isLearning(int specIdx) const
{
    return learnTarget.load() == specIdx;
}

int Parallax64AudioProcessor::getMappedCCFor(int specIdx) const
{
    for (int i = 0, cnt = midiMapCount.load(); i < cnt; ++i)
        if (midiMap[i].paramIdx.load() == specIdx && midiMap[i].cc.load() >= 0)
            return midiMap[i].cc.load();
    return -1;
}

void Parallax64AudioProcessor::removeMappingForParam(int specIdx)
{
    for (int i = 0, cnt = midiMapCount.load(); i < cnt; ++i)
        if (midiMap[i].paramIdx.load() == specIdx)
            midiMap[i].cc.store(-1);
    mapDirty.store(true);
    syncMidiMapToState();
}

void Parallax64AudioProcessor::clearAllMidiMappings()
{
    midiMapCount.store(0);
    mapDirty.store(true);
    syncMidiMapToState();
}

int Parallax64AudioProcessor::getMappingCount() const
{
    int n = 0;
    for (int i = 0, cnt = midiMapCount.load(); i < cnt; ++i)
        if (midiMap[i].cc.load() >= 0) ++n;
    return n;
}

//==============================================================================
void Parallax64AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    // ---------- 1) MIDI: CC / Pitchbend / Modwheel ----------
    for (const auto md : midi)
    {
        auto m = md.getMessage();

        if (m.isPitchWheel())
        {
            bendSemis = (float)(m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f;
        }
        else if (m.isController())
        {
            int num = m.getControllerNumber();
            int v = m.getControllerValue();

            if (num == 1) { modVal = (float)v / 127.0f; continue; }

            int lt = learnTarget.load();
            if (lt >= 0)
            {
                if (isLearnableCC(num))
                {
                    int cnt = midiMapCount.load();
                    for (int i = 0; i < cnt; ++i)
                        if (midiMap[i].cc.load() == num || midiMap[i].paramIdx.load() == lt)
                            midiMap[i].cc.store(-1);

                    if (cnt < kMaxMaps)
                    {
                        midiMap[cnt].cc.store(num);
                        midiMap[cnt].paramIdx.store(lt);
                        midiMapCount.store(cnt + 1);
                    }
                    learnTarget.store(-1);
                    lastTouched.store(lt);
                    mapDirty.store(true);
                }
                continue;
            }

            int idx = -1;
            for (int i = 0, cnt = midiMapCount.load(); i < cnt; ++i)
                if (midiMap[i].cc.load() == num)
                {
                    idx = midiMap[i].paramIdx.load();
                    break;
                }

            const xl::Spec* sp = nullptr;
            if (idx >= 0)
            {
                sp = &xl::getSpecs()[(size_t)idx];
            }
            else
            {
                auto& r = ccRoutes[(size_t)num];
                if (r.p == nullptr) continue;
                idx = r.idx;
                sp = &xl::getSpecs()[(size_t)idx];
            }

            auto* p = paramByIdx[(size_t)idx];
            if (p == nullptr) continue;

            if (sp->kind == xl::kBool)
            {
                if (v == lastCCRaw[(size_t)num]) continue;
                lastCCRaw[(size_t)num] = v;

                float cur = p->getValue();
                p->setValueNotifyingHost(cur > 0.5f ? 0.0f : 1.0f);
                lastTouched.store(idx);
            }
            else
            {
                lastCCRaw[(size_t)num] = v;
                p->setValueNotifyingHost((float)v / 127.0f);
                lastTouched.store(idx);
            }
        }
    }

    // ---------- 2) Parameter-Snapshot ----------
    xl::EngineParams ep;
    // Choice-Index 0..4 -> Oktavversatz -2..+2 ("OCT 0" = Index 2 = kein Versatz)
    ep.osc1Wave = (int)P64_VAL("osc1Wave");  ep.osc1Oct = (int)P64_VAL("osc1Oct") - 2;
    ep.osc1Tune = P64_VAL("osc1Tune");        ep.osc1Level = P64_VAL("osc1Level");
    ep.osc2Wave = (int)P64_VAL("osc2Wave");  ep.osc2Oct = (int)P64_VAL("osc2Oct") - 2;
    ep.osc2Tune = P64_VAL("osc2Tune");        ep.osc2Level = P64_VAL("osc2Level");
    ep.subLevel = P64_VAL("subLevel");        ep.noiseLevel = P64_VAL("noiseLevel");
    ep.glide = P64_VAL("glide");              ep.cutoff = P64_VAL("cutoff");
    ep.reso = P64_VAL("reso");                ep.envAmt = P64_VAL("envAmt");
    ep.drive = P64_VAL("drive");              ep.fltMode = (int)P64_VAL("fltMode");
    ep.lfoRate = P64_VAL("lfoRate");          ep.lfoWave = (int)P64_VAL("lfoWave");
    ep.lfoDepth = xl::xclamp(P64_VAL("lfoDepth") + modVal, 0.0f, 1.0f);
    ep.dlyTime = P64_VAL("dlyTime");          ep.dlyFb = P64_VAL("dlyFb");
    ep.dlyMix = P64_VAL("dlyMix");            ep.revMix = P64_VAL("revMix");
    ep.dlyMode = (int)P64_VAL("dlyMode");
    ep.master = P64_VAL("master") + P64_VAL("presetLevel");   // Preset-Pegel-Abgleich hinter den Effekten
    ep.a1 = P64_VAL("ampA"); ep.d1 = P64_VAL("ampD"); ep.s1 = P64_VAL("ampS"); ep.r1 = P64_VAL("ampR");
    ep.a2 = P64_VAL("fltA"); ep.d2 = P64_VAL("fltD"); ep.s2 = P64_VAL("fltS"); ep.r2 = P64_VAL("fltR");

    ep.osc1On = P64_VAL("osc1On") > 0.5f;   ep.osc2On = P64_VAL("osc2On") > 0.5f;
    ep.sync = P64_VAL("sync") > 0.5f;       ep.subOn = P64_VAL("subOn") > 0.5f;
    ep.noiseOn = P64_VAL("noiseOn") > 0.5f; ep.lfoPitch = P64_VAL("lfoPitch") > 0.5f;
    ep.lfoFilter = P64_VAL("lfoFilter") > 0.5f; ep.lfoPW = P64_VAL("lfoPW") > 0.5f;
    ep.lfoRetrig = P64_VAL("lfoRetrig") > 0.5f;
    ep.mono = P64_VAL("mono") > 0.5f;       ep.legato = P64_VAL("legato") > 0.5f;
    ep.glideOn = P64_VAL("glideOn") > 0.5f; ep.unison = P64_VAL("unison") > 0.5f;
    ep.dlyOn = P64_VAL("dlyOn") > 0.5f;     ep.revOn = P64_VAL("revOn") > 0.5f;
    ep.arpOn = P64_VAL("arpOn") > 0.5f;

    float distAmount = P64_VAL("distAmt");
    // Frueher "||": distAmt-Default 0.5 schaltete die Distortion in 121 von 128 Presets dauerhaft ein.
    // Jetzt bestimmt ausschliesslich der DIST-ON-Schalter (distOn), der Regler nur die Menge.
    bool  distActive = (P64_VAL("distOn") > 0.5f) && (distAmount > 0.001f);

    ep.distOn = distActive;
    ep.distAmt = distAmount;

    ep.bendFactor = std::pow(2.0f, bendSemis / 12.0f);

    // ---------- 2a) BPM + LFO-/Delay-Sync ----------
    double bpmNow = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpmNow = *b;

    auto stepIndexFor = [this](int idx)
        {
            float norm = paramByIdx[(size_t)idx]->getValue();
            return juce::jlimit(0, 7, (int)(norm * 8.0f));
        };

    bool lfoSyncOn = P64_VAL("lfoSync") > 0.5f;
    bool dlySyncOn = P64_VAL("dlySync") > 0.5f;

    if (lfoSyncOn)
    {
        int s = stepIndexFor(P64_IDX("lfoRate"));
        ep.lfoRate = (float)xl::xclamp(bpmNow / 60.0 / xl::tempoStepQuarters[s], 0.02, 60.0);
    }
    if (dlySyncOn)
    {
        float norm = paramByIdx[(size_t)P64_IDX("dlyTime")]->getValue();
        int s = juce::jlimit(0, xl::kDelaySyncSteps - 1, (int)(norm * (float)xl::kDelaySyncSteps));
        ep.dlyTime = (float)xl::xclamp(xl::delaySyncQuarters[s] * 60.0 / bpmNow, 0.01, 4.5);
    }

    engine.setParams(ep);

    // ---------- 2a2) Poly/Mono/Unison-Wechsel: gehaltene Noten sauber beenden ----------
    // Der Mono-Notenstapel und die Poly-/Unison-Stimmen kennen die Noten des jeweils anderen Modus nicht;
    // ohne Reset bleiben Noten haengen (Stimmen ohne Note-Off, veralteter Mono-Stapel).
    if (ep.mono != lastMono || ep.unison != lastUnison)
    {
        lastMono = ep.mono; lastUnison = ep.unison;
        engine.allNotesOff();
        sustained.reset();
    }

    // ---------- 2b) ARP-Mode-Wechsel: Engine sauber leeren ----------
    if (ep.arpOn != lastArpOn)
    {
        lastArpOn = ep.arpOn;
        arpHeld.clear();
        if (arpNote >= 0) { engine.noteOff(arpNote); arpNote = -1; }
        engine.allNotesOff();
        sustained.reset();
        gateOffDone = false;
        arpPos = 0; arpLadPos = 0;
        arpFrac = 0.0; lastArpStep = -1;
    }

    // ---------- 3) Noten ----------
    bool arp = ep.arpOn;

    for (const auto md : midi)
    {
        auto m = md.getMessage();

        auto doNoteOff = [&](int note) { if (arp) removeHeldNote(note); else engine.noteOff(note); };

        if (m.isNoteOn())
        {
            sustained.reset((size_t)m.getNoteNumber());
            if (arp) addHeldNote(m.getNoteNumber(), m.getFloatVelocity());
            else     engine.noteOn(m.getNoteNumber(), m.getFloatVelocity());
        }
        else if (m.isNoteOff())
        {
            const int note = m.getNoteNumber();
            if (sustainDown) sustained.set((size_t)note);   // Pedal gedrueckt: Note-Off erst beim Loslassen
            else             doNoteOff(note);
        }
        else if (m.isController())
        {
            const int cc = m.getControllerNumber();
            if (cc == 64)   // Sustain-Pedal
            {
                const bool down = m.getControllerValue() >= 64;
                if (!down && sustainDown)
                {
                    sustainDown = false;
                    for (int note = 0; note < 128; ++note)
                        if (sustained.test((size_t)note)) { sustained.reset((size_t)note); doNoteOff(note); }
                }
                else if (down) sustainDown = true;
            }
            else if (cc == 123) releaseAllNotes(false);   // All Notes Off (z. B. DAW-Stop)
            else if (cc == 120) releaseAllNotes(true);    // All Sound Off
        }
    }

    // ---------- 4) Arpeggiator-Clock ----------
    if (arp)
    {
        double gate = P64_VAL("arpGate");
        bool  syncOn = P64_VAL("arpSync") > 0.5f;
        double stepQ = arpStepQuarters();

        bool playing = false, hasPpq = false; double ppq = 0.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                playing = pos->getIsPlaying();
                if (auto o = pos->getPpqPosition()) { ppq = *o; hasPpq = true; }
            }

        bool useSync = syncOn && hasPpq && playing;

        if (useSync)
        {
            double sIdx = std::floor(ppq / stepQ);
            double posIn = ppq / stepQ - sIdx;

            if ((long long)sIdx != lastArpStep)
            {
                lastArpStep = (long long)sIdx;
                int slot = (int)(((long long)sIdx % 8 + 8) % 8);
                arpAdvance(slot);
            }
            if (!gateOffDone && arpNote >= 0 && posIn >= gate)
            {
                engine.noteOff(arpNote);
                gateOffDone = true;
            }
        }
        else
        {
            bool wasSync = (lastArpStep != -1);
            lastArpStep = -1;

            if (wasSync)
            {
                arpFrac = 0.0;
                if (arpNote >= 0 && !gateOffDone)
                {
                    engine.noteOff(arpNote);
                    gateOffDone = true;
                }
            }

            double stepsPerSec = bpmNow / 60.0 / stepQ;
            arpFrac += (double)n * stepsPerSec / lastSR;

            if (arpFrac >= 1.0)
            {
                arpFrac -= std::floor(arpFrac);
                int slot = arpPos % 8;
                ++arpPos;
                arpAdvance(slot);
            }
            if (!gateOffDone && arpNote >= 0 && arpFrac >= gate)
            {
                engine.noteOff(arpNote);
                gateOffDone = true;
            }
        }
    }
    else
    {
        arpHeld.clear();
        if (arpNote >= 0) { engine.noteOff(arpNote); arpNote = -1; }
        lastArpStep = -1; arpFrac = 0.0; gateOffDone = false;
        arpPos = 0; arpLadPos = 0;
    }

    // ---------- 5) Audio ----------
    auto* L = buffer.getWritePointer(0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : L;

    engine.process(L, R, n);

    bool  distNow = distActive;
    float dAmt = distAmount;

    driveSm.setTargetValue(ep.drive);
    dlySm.setTargetValue(ep.dlyTime);
    fbSm.setTargetValue(ep.dlyOn ? ep.dlyFb : 0.0f);
    dlyMixSm.setTargetValue(ep.dlyOn ? ep.dlyMix : 0.0f);

    // Delay komplett ueberspringen, sobald es aus ist und die Glaettung ausgelaufen ist
    // (vorher lief es mit Mix 0 / Feedback 0 trotzdem jedes Sample durch). Beim Abschalten
    // wird der Puffer einmal geleert, damit beim Wiedereinschalten keine alten Echos auftauchen.
    const bool delayActive = ep.dlyOn || dlyMixSm.isSmoothing() || fbSm.isSmoothing();
    if (delayActive) delayIdle = false;
    else if (!delayIdle) { delay.reset(); delayIdle = true; }

    for (int i = 0; i < n; ++i)
    {
        float d = driveSm.getNextValue();

        float dl = L[i], dr = R[i];
        if (d > 0.0005f)   // bei Drive ~0 die beiden tanh sparen
        {
            float pre = 1.0f + 8.0f * d;
            float post = 1.0f / (1.0f + 1.5f * d);
            dl += d * (std::tanh(dl * pre) * post - dl);
            dr += d * (std::tanh(dr * pre) * post - dr);
        }

        if (distNow && dAmt > 0.001f)
        {
            dl = dist.shape(dl, dAmt);
            dr = dist.shape(dr, dAmt);

            dl = dist.toneL(dl);
            dr = dist.toneR(dr);

            float trim = 1.0f / (1.0f + 0.6f * dAmt);
            dl *= trim;
            dr *= trim;

            dl = dist.limit(dl);
            dr = dist.limit(dr);
        }

        float dS = dlySm.getNextValue() * (float)lastSR;
        float fbv = fbSm.getNextValue();
        float mix = dlyMixSm.getNextValue();

        if (delayActive)
            delay.processSample(dl, dr, dS, fbv, ep.dlyMode, mix);
        L[i] = dl; R[i] = dr;
    }

    // Reverb-Mix wird geglaettet (kein Knacksen beim Ein-/Ausschalten). Ist er ~0 und die
    // Glaettung fertig, wird der Reverb ganz uebersprungen; sein Zustand wird dabei einmal
    // geleert, damit beim Wiedereinschalten kein eingefrorener alter Nachhall auftaucht.
    const float rmTarget = ep.revOn ? ep.revMix : 0.0f;
    revSm.setTargetValue(rmTarget);
    const bool revActive = rmTarget > 0.0005f || revSm.isSmoothing();

    if (n > 0 && revActive)
    {
        reverbIdle = false;

        // Grosse Host-Bloecke (> prepareToPlay) werden in Stuecken verarbeitet: keine Allokation im Audio-Thread
        const int chunkMax = (int)tmpL.size();
        for (int start = 0; start < n; start += chunkMax)
        {
            const int len = std::min(chunkMax, n - start);
            for (int i = 0; i < len; ++i)
            {
                tmpL[(size_t)i] = xl::xclamp(L[start + i], -3.0f, 3.0f);
                tmpR[(size_t)i] = xl::xclamp(R[start + i], -3.0f, 3.0f);
            }

            float* chs[2] = { tmpL.data(), tmpR.data() };
            juce::dsp::AudioBlock<float> blk(chs, 2, (size_t)len);
            reverb.process(juce::dsp::ProcessContextReplacing<float>(blk));

            bool revOk = true;
            for (int i = 0; i < len; ++i)
                if (!std::isfinite(tmpL[(size_t)i]) || !std::isfinite(tmpR[(size_t)i])) { revOk = false; break; }

            if (!revOk)
            {
                reverb.reset();
                revSm.skip(len);
            }
            else
            {
                for (int i = 0; i < len; ++i)
                {
                    const float rm = revSm.getNextValue();
                    L[start + i] = L[start + i] * (1.0f - rm) + tmpL[(size_t)i] * rm;
                    R[start + i] = R[start + i] * (1.0f - rm) + tmpR[(size_t)i] * rm;
                }
            }
        }
    }
    else if (n > 0 && !reverbIdle)
    {
        reverb.reset();
        reverbIdle = true;
    }

    masterSm.setTargetValue(dbToGain(ep.master));
    for (int i = 0; i < n; ++i)
    {
        float g = masterSm.getNextValue();
        float outL = L[i] * g;
        float outR = R[i] * g;
        if (!std::isfinite(outL)) outL = 0.0f;
        if (!std::isfinite(outR)) outR = 0.0f;
        L[i] = std::tanh(outL);
        R[i] = std::tanh(outR);
    }
}

//==============================================================================
void Parallax64AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    syncMidiMapToState();

    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void Parallax64AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto mmCopy = apvts.state.getChildWithName("MIDIMAP").createCopy();

    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));

    // Der geladene State bringt seine eigene MIDI-Zuordnung mit. Nur wenn er keine hat (alter State),
    // bleibt die bisherige erhalten. Vorher wurde sie immer zusaetzlich angehaengt (State wuchs bei jedem Laden).
    if (!apvts.state.getChildWithName("MIDIMAP").isValid() && mmCopy.isValid())
        apvts.state.addChild(mmCopy, -1, nullptr);
    ensureMidiMapNode();
    reloadMidiMapRuntime();
}

//==============================================================================
int Parallax64AudioProcessor::getNumFactory() const
{
    return (int)xl::getFactoryPresets().size();
}

juce::String Parallax64AudioProcessor::getFactoryName(int idx) const
{
    const auto& list = xl::getFactoryPresets();
    return juce::String(list[(size_t)idx].name);
}

void Parallax64AudioProcessor::applyFactoryPreset(int idx)
{
    const auto& list = xl::getFactoryPresets();
    if (idx < 0 || idx >= (int)list.size()) return;
    const auto& pd = list[(size_t)idx];

    std::map<juce::String, float> ov;
    for (const auto& o : pd.over) ov[o.first] = o.second;

    for (const auto& s : xl::getSpecs())
    {
        auto* p = apvts.getParameter(s.id);
        if (p == nullptr) continue;

        float norm = 0.0f;
        auto it = ov.find(s.id);

        if (s.kind == xl::kBool)
        {
            float v = (it != ov.end()) ? it->second : s.def;
            norm = v > 0.5f ? 1.0f : 0.0f;
        }
        else if (s.kind == xl::kChoice)
        {
            float v = (it != ov.end()) ? it->second : (float)s.defChoice;
            int nc = juce::jmax(1, s.nChoices);
            int i = juce::jlimit(0, nc - 1, (int)std::round(v));
            norm = nc > 1 ? (float)i / (float)(nc - 1) : 0.0f;
        }
        else
        {
            float v = (it != ov.end()) ? it->second : s.def;
            norm = p->getNormalisableRange().convertTo0to1(juce::jlimit(s.min, s.max, v));
        }

        p->setValueNotifyingHost(norm);
    }

    engine.allNotesOff();
    delay.reset();
    reverb.reset();
    dist.reset();

    lastTouched.store(-2);
    lastPresetName = juce::String(pd.name);
}

int Parallax64AudioProcessor::refreshAndGetUserPresets()
{
    userFiles.clear();
    if (presetDir.isDirectory())
        presetDir.findChildFiles(userFiles, juce::File::findFiles, false, "*.xml");

    std::sort(userFiles.begin(), userFiles.end(),
        [](const juce::File& a, const juce::File& b)
        {
            auto ta = a.getCreationTime().toMilliseconds();
            auto tb = b.getCreationTime().toMilliseconds();
            if (ta != tb) return ta < tb;
            return a.getFileName().toLowerCase() < b.getFileName().toLowerCase();
        });

    return userFiles.size();
}

juce::String Parallax64AudioProcessor::getUserPresetName(int idx) const
{
    if (idx < 0 || idx >= userFiles.size()) return {};
    return userFiles[(size_t)idx].getFileNameWithoutExtension();
}

void Parallax64AudioProcessor::applyUserPreset(int idx)
{
    if (idx < 0 || idx >= userFiles.size()) return;

    auto mmCopy = apvts.state.getChildWithName("MIDIMAP").createCopy();

    if (auto xml = juce::parseXML(userFiles[(size_t)idx]))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));

    // Ein User-Preset ersetzt nur den Klang; die aktuelle MIDI-Zuordnung bleibt (genau ein MIDIMAP-Knoten)
    for (int i = apvts.state.getNumChildren(); --i >= 0;)
        if (apvts.state.getChild(i).hasType("MIDIMAP")) apvts.state.removeChild(i, nullptr);
    if (mmCopy.isValid())
        apvts.state.addChild(mmCopy, -1, nullptr);
    ensureMidiMapNode();
    reloadMidiMapRuntime();

    engine.allNotesOff();
    delay.reset();
    reverb.reset();
    dist.reset();

    lastTouched.store(-2);
    lastPresetName = userFiles[(size_t)idx].getFileNameWithoutExtension();
}

juce::String Parallax64AudioProcessor::sanitizeFileName(const juce::String& in)
{
    juce::String fn;
    for (int i = 0; i < in.length(); ++i)
    {
        auto c = in[i];
        if (juce::CharacterFunctions::isLetterOrDigit(c) || c == '-' || c == '_' || c == ' ')
            fn << c;
        else
            fn << '_';
    }
    fn = fn.trim();
    if (fn.isEmpty()) fn = "USER_PATCH";
    return fn;
}

juce::String Parallax64AudioProcessor::nextAutoPresetName() const
{
    int n = 1;
    for (;; ++n)
    {
        juce::String cand = "UserPreset" + juce::String(n);
        bool used = false;
        for (const auto& f : userFiles)
            if (f.getFileNameWithoutExtension().equalsIgnoreCase(cand)) { used = true; break; }
        if (!used) return cand;
    }
}

int Parallax64AudioProcessor::saveUserPresetAsNew(const juce::String& name)
{
    juce::String display = name.trim();
    if (display.isEmpty()) display = nextAutoPresetName();

    juce::String base = sanitizeFileName(display);
    juce::String fn = base;
    int k = 2;
    while (presetDir.getChildFile(fn + ".xml").existsAsFile())
        fn = base + "_" + juce::String(k++);

    syncMidiMapToState();

    auto st = apvts.copyState();
    st.setProperty("presetName", display, nullptr);

    if (auto xml = st.createXml())
        xml->writeTo(presetDir.getChildFile(fn + ".xml"));

    refreshAndGetUserPresets();

    lastTouched.store(-2);
    lastPresetName = fn;

    for (int i = 0; i < userFiles.size(); ++i)
        if (userFiles[(size_t)i].getFileName() == fn + ".xml")
            return kUserBase + i;

    return kUserBase;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Parallax64AudioProcessor();
}
