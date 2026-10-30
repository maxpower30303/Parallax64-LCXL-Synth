// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Flix
#include "PluginEditor.h"
#include "SynthParams.h"
#include <cmath>

//==============================================================================
Parallax64AudioProcessorEditor::Parallax64AudioProcessorEditor(Parallax64AudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&laf);

    addAndMakeVisible(title);
    title.setText("PARALLAX64-LCXL-SYNTH", juce::dontSendNotification);
    title.setFont(themeF(*laf.th, 20.0f, true, laf.th->serif));
    title.setJustificationType(juce::Justification::centredLeft);
    title.addMouseListener(this, false);

    addAndMakeVisible(oled);

    addAndMakeVisible(prevBtn);
    addAndMakeVisible(nextBtn);
    prevBtn.onClick = [this] { stepPreset(-1); };
    nextBtn.onClick = [this] { stepPreset(+1); };

    addAndMakeVisible(presetBox);
    presetBox.onChange = [this]
        {
            int id = presetBox.getSelectedId();
            if (id <= 0) return;
            if (id < Parallax64AudioProcessor::kUserBase)
                proc.applyFactoryPreset(id - 1);
            else
                proc.applyUserPreset(id - Parallax64AudioProcessor::kUserBase);
        };

    addAndMakeVisible(saveBtn);
    saveBtn.onClick = [this]
        {
            if (!saveDlg)
            {
                saveDlg = std::make_unique<SaveDialog>();
                addAndMakeVisible(*saveDlg);
                saveDlg->setTheme(laf.th);
                saveDlg->onSave = [this](const juce::String& n)
                    {
                        saveDlg->setVisible(false);
                        int newId = proc.saveUserPresetAsNew(n);
                        rebuildPresetList();
                        presetBox.setSelectedId(newId, juce::dontSendNotification);
                    };
                saveDlg->onCancel = [this] { saveDlg->setVisible(false); };
            }
            saveDlg->open(proc.nextAutoPresetName(), this);
        };

    rebuildPresetList();
    presetBox.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(themeLabel);
    themeLabel.setText("Skin", juce::dontSendNotification);
    themeLabel.setFont(themeF(*laf.th, 11.0f));

    addAndMakeVisible(themeBox);
    const auto& ths = getThemes();
    for (size_t i = 0; i < ths.size(); ++i)
        themeBox.addItem(ths[i].name, (int)i + 1);

    int storedTheme = 0;
    if (proc.apvts.state.hasProperty("guiTheme"))
        storedTheme = (int)proc.apvts.state.getProperty("guiTheme");
    storedTheme = juce::jlimit(0, (int)ths.size() - 1, storedTheme);
    themeBox.setSelectedId(storedTheme + 1, juce::dontSendNotification);
    themeBox.onChange = [this] { applyTheme(themeBox.getSelectedId() - 1, true); };

    buildWidgets();
    buildArpPanel();

    applyTheme(storedTheme, false);

    setSize(900, 840);
    startTimerHz(25);
}

Parallax64AudioProcessorEditor::~Parallax64AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

//==============================================================================
void Parallax64AudioProcessorEditor::stepPreset(int dir)
{
    int curId = presetBox.getSelectedId();
    int nFact = proc.getNumFactory();
    int nUser = proc.refreshAndGetUserPresets();

    std::vector<int> ids;
    for (int i = 0; i < nFact; ++i) ids.push_back(i + 1);
    for (int i = 0; i < nUser; ++i) ids.push_back(Parallax64AudioProcessor::kUserBase + i);

    if (ids.empty()) return;

    int curIdx = 0;
    for (size_t i = 0; i < ids.size(); ++i)
        if (ids[i] == curId) { curIdx = (int)i; break; }

    int nextIdx = (curIdx + dir + (int)ids.size()) % (int)ids.size();
    int nextId = ids[(size_t)nextIdx];
    presetBox.setSelectedId(nextId, juce::sendNotificationSync);
}

void Parallax64AudioProcessorEditor::rebuildPresetList()
{
    presetBox.clear(juce::dontSendNotification);

    int nFact = proc.getNumFactory();
    for (int i = 0; i < nFact; ++i)
        presetBox.addItem(proc.getFactoryName(i), i + 1);

    int nUser = proc.refreshAndGetUserPresets();
    if (nUser > 0)
    {
        presetBox.addSeparator();
        for (int i = 0; i < nUser; ++i)
            presetBox.addItem(proc.getUserPresetName(i), Parallax64AudioProcessor::kUserBase + i);
    }
}

void Parallax64AudioProcessorEditor::updateWaveDisplays()
{
    static const char* const wnames[4] = { "SIN", "TRI", "SAW", "SQR" };
    int w1 = (int)std::round(proc.apvts.getRawParameterValue("osc1Wave")->load());
    int w2 = (int)std::round(proc.apvts.getRawParameterValue("osc2Wave")->load());
    waveLbl1.setText(wnames[juce::jlimit(0, 3, w1)], juce::dontSendNotification);
    waveLbl2.setText(wnames[juce::jlimit(0, 3, w2)], juce::dontSendNotification);
}

void Parallax64AudioProcessorEditor::applyTheme(int idx, bool store)
{
    const auto& ths = getThemes();
    if (idx < 0 || idx >= (int)ths.size()) idx = 0;

    laf.setTheme(&ths[(size_t)idx]);
    const auto& th = *laf.th;

    title.setColour(juce::Label::textColourId, juce::Colour(th.ac));
    title.setFont(themeF(th, 20.0f, true, th.serif));

    themeLabel.setColour(juce::Label::textColourId, juce::Colour(th.lbl));
    themeLabel.setFont(themeF(th, 11.0f));

    oled.setTheme(&th);
    if (saveDlg)  saveDlg->setTheme(&th);
    if (aboutDlg) aboutDlg->setTheme(&th);

    auto styleCombo = [&](juce::ComboBox& b)
        {
            b.setColour(juce::ComboBox::backgroundColourId, juce::Colour(th.panel));
            b.setColour(juce::ComboBox::outlineColourId, juce::Colour(th.acDim));
            b.setColour(juce::ComboBox::textColourId, juce::Colour(th.txt));
            b.setColour(juce::ComboBox::arrowColourId, juce::Colour(th.ac));
        };
    styleCombo(presetBox);
    styleCombo(themeBox);
    styleCombo(arpRateBox);
    styleCombo(arpDirBox);
    styleCombo(arpOctBox);
    styleCombo(dlyModeBox);

    auto styleBtn = [&](juce::TextButton& b)
        {
            b.setColour(juce::TextButton::textColourOffId, juce::Colour(th.txt));
        };
    styleBtn(saveBtn);
    styleBtn(prevBtn);
    styleBtn(nextBtn);
    styleBtn(arpRandBtn);

    auto styleToggle = [&](juce::ToggleButton& b, uint32_t c)
        {
            b.setColour(juce::ToggleButton::tickColourId, juce::Colour(c));
        };
    styleToggle(lfoSyncBtn, th.sec[4]);
    styleToggle(dlySyncBtn, th.sec[5]);
    styleToggle(arpSyncBtn, th.sec[7]);
    styleToggle(distOnBtn, th.sec[3]);

    distKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(th.sec[3]));
    distLbl.setColour(juce::Label::textColourId, juce::Colour(th.sec[3]));
    distLbl.setFont(themeF(th, 10.0f, true));

    dlyModeLbl.setColour(juce::Label::textColourId, juce::Colour(th.sec[5]));
    dlyModeLbl.setFont(themeF(th, 10.0f, true));

    arpRateLbl.setColour(juce::Label::textColourId, juce::Colour(th.sec[7]));
    arpDirLbl.setColour(juce::Label::textColourId, juce::Colour(th.sec[7]));
    arpOctLbl.setColour(juce::Label::textColourId, juce::Colour(th.sec[7]));
    arpGateLbl.setColour(juce::Label::textColourId, juce::Colour(th.sec[7]));
    arpGateSlider.setColour(juce::Slider::trackColourId, juce::Colour(th.sec[7]));

    for (int i = 0; i < 8; ++i)
        arpStepBtn[i].setColour(juce::ToggleButton::tickColourId, juce::Colour(th.sec[7]));

    auto styleWaveLbl = [&](juce::Label& l, uint32_t c)
        {
            l.setColour(juce::Label::backgroundColourId, juce::Colour(th.displayBg));
            l.setColour(juce::Label::textColourId, juce::Colour(c));
            l.setColour(juce::Label::outlineColourId, juce::Colour(th.border));
            l.setFont(themeF(th, 10.0f, true));
        };
    styleWaveLbl(waveLbl1, th.sec[0]);
    styleWaveLbl(waveLbl2, th.sec[1]);

    for (size_t i = 0; i < sliders.size(); ++i)
    {
        int sec = sectionFor((int)i);
        uint32_t col = th.sec[sec];
        auto& s = *sliders[i];
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(col));
        s.setColour(juce::Slider::trackColourId, juce::Colour(col));
    }

    for (size_t i = 0; i < buttons.size(); ++i)
    {
        int sec = sectionFor(32 + (int)i);
        uint32_t col = th.sec[sec];
        buttons[i]->setColour(juce::ToggleButton::tickColourId, juce::Colour(col));
    }

    for (size_t i = 0; i < labels.size(); ++i)
    {
        int sec = sectionFor((int)i);
        labels[i]->setColour(juce::Label::textColourId, juce::Colour(th.sec[sec]));
        labels[i]->setFont(themeF(th, 10.0f, true));
    }

    if (store)
        proc.apvts.state.setProperty("guiTheme", idx, nullptr);

    repaint();
}

//==============================================================================
void Parallax64AudioProcessorEditor::buildWidgets()
{
    sliders.clear();  buttons.clear(); labels.clear();
    sAtts.clear();    bAtts.clear();

    for (int i = 0; i < kNWidgets; ++i)
    {
        const auto& sp = xl::getSpecs()[(size_t)i];

        if (sp.kind == xl::kBool)
        {
            int r = i < 40 ? 0 : 1;
            int c = i < 40 ? (i - 32) : (i - 40);

            auto b = std::make_unique<juce::ToggleButton>(sp.name);
            b->setBounds(btnRectFor(r, c));
            b->addMouseListener(this, false);
            bAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                proc.apvts, sp.id, *b));
            addAndMakeVisible(*b);
            buttons.push_back(std::move(b));
        }
        else
        {
            int p, c, n;
            widgetCell(i, p, c, n);

            bool isFader = (p == 7 || p == 8);
            auto s = std::make_unique<juce::Slider>();

            if (isFader)
            {
                s->setSliderStyle(juce::Slider::LinearVertical);
                s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                s->setBounds(faderRectFor(p, c, n));
            }
            else
            {
                s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
                s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                s->setBounds(knobRectFor(p, c, n));
            }

            s->addMouseListener(this, false);
            sAtts.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                proc.apvts, sp.id, *s));
            addAndMakeVisible(*s);
            sliders.push_back(std::move(s));

            auto l = std::make_unique<juce::Label>();
            l->setText(sp.name, juce::dontSendNotification);
            l->setJustificationType(juce::Justification::centred);
            l->setBounds(labelRectFor(p, c, n, isFader));
            addAndMakeVisible(*l);
            labels.push_back(std::move(l));
        }
    }

    // OSC 1 / OSC 2 Waveform-Displays
    auto addWave = [&](juce::Label& l, int pIdx)
        {
            l.setBounds(waveBoxRect(pIdx));
            l.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(l);
        };
    addWave(waveLbl1, 0);
    addWave(waveLbl2, 1);

    // LFO SYNC & DLY SYNC Side-Toggles
    lfoSyncBtn.setBounds(knobSideToggleRect(4, 0, 3, 50, 20));
    lfoSyncBtn.addMouseListener(this, false);
    lfoSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, "lfoSync", lfoSyncBtn);
    addAndMakeVisible(lfoSyncBtn);

    dlySyncBtn.setBounds(knobSideToggleRect(5, 0, 4, 50, 20));
    dlySyncBtn.addMouseListener(this, false);
    dlySyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, "dlySync", dlySyncBtn);
    addAndMakeVisible(dlySyncBtn);

    auto P_fx = panelAt(5);
    dlyModeLbl.setText("MODE", juce::dontSendNotification);
    dlyModeLbl.setJustificationType(juce::Justification::centredRight);
    dlyModeLbl.setBounds(P_fx.x + 130, P_fx.y + 4, 42, 20);
    addAndMakeVisible(dlyModeLbl);

    dlyModeBox.setBounds(P_fx.x + 176, P_fx.y + 4, 115, 20);
    dlyModeBox.addItemList(juce::StringArray{ "STEREO", "PING-PONG", "TAPE DRIFT", "CROSS 3:4", "DIFFUSE" }, 1);
    dlyModeBox.addMouseListener(this, false);
    dlyModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        proc.apvts, "dlyMode", dlyModeBox);
    addAndMakeVisible(dlyModeBox);

    // DISTORTION Extra-Knob im FILTER-Panel
    distKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    distKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    distKnob.setBounds(distKnobRect());
    distKnob.addMouseListener(this, false);
    distAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "distAmt", distKnob);
    addAndMakeVisible(distKnob);

    distOnBtn.setBounds(distOnRect());
    distOnBtn.addMouseListener(this, false);
    distOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, "distOn", distOnBtn);
    addAndMakeVisible(distOnBtn);

    distLbl.setText("DIST", juce::dontSendNotification);
    distLbl.setJustificationType(juce::Justification::centred);
    distLbl.setBounds(distLblRect());
    addAndMakeVisible(distLbl);
}

//==============================================================================
void Parallax64AudioProcessorEditor::buildArpPanel()
{
    auto P = arpPanel();

    auto setupLbl = [this, P](juce::Label& l, const char* txt, int x, int y, int w, int h)
        {
            l.setText(txt, juce::dontSendNotification);
            l.setFont(themeF(*laf.th, 10.0f, true));
            l.setJustificationType(juce::Justification::centredLeft);
            l.setBounds(P.x + x, P.y + y, w, h);
            addAndMakeVisible(l);
        };

    setupLbl(arpRateLbl, "RATE", 14, 26, 40, 14);
    arpRateBox.setBounds(P.x + 14, P.y + 42, 80, 22);
    arpRateBox.addItemList(juce::StringArray{ "1/1","1/2","1/4","1/8","1/8T","1/16","1/16T","1/32" }, 1);
    arpRateBox.addMouseListener(this, false);
    arpRateAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        proc.apvts, "arpRate", arpRateBox);
    addAndMakeVisible(arpRateBox);

    setupLbl(arpDirLbl, "DIR", 104, 26, 40, 14);
    arpDirBox.setBounds(P.x + 104, P.y + 42, 88, 22);
    arpDirBox.addItemList(juce::StringArray{ "UP","DOWN","UP-DOWN","RANDOM" }, 1);
    arpDirBox.addMouseListener(this, false);
    arpDirAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        proc.apvts, "arpDir", arpDirBox);
    addAndMakeVisible(arpDirBox);

    setupLbl(arpOctLbl, "OCT", 202, 26, 40, 14);
    arpOctBox.setBounds(P.x + 202, P.y + 42, 72, 22);
    arpOctBox.addItemList(juce::StringArray{ "1 OCT","2 OCT","3 OCT","4 OCT" }, 1);
    arpOctBox.addMouseListener(this, false);
    arpOctAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        proc.apvts, "arpOct", arpOctBox);
    addAndMakeVisible(arpOctBox);

    setupLbl(arpGateLbl, "GATE", 284, 26, 50, 14);
    arpGateSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    arpGateSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    arpGateSlider.setBounds(P.x + 284, P.y + 44, 110, 18);
    arpGateSlider.addMouseListener(this, false);
    arpGateAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "arpGate", arpGateSlider);
    addAndMakeVisible(arpGateSlider);

    arpSyncBtn.setBounds(P.x + 404, P.y + 42, 60, 22);
    arpSyncBtn.addMouseListener(this, false);
    arpSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, "arpSync", arpSyncBtn);
    addAndMakeVisible(arpSyncBtn);

    arpRandBtn.setBounds(P.x + 474, P.y + 42, 70, 22);
    arpRandBtn.onClick = [this] { randomizeArpSteps(); };
    addAndMakeVisible(arpRandBtn);

    // 8 Step-Toggles
    static const char* const stepIds[8] =
    { "arpStep1","arpStep2","arpStep3","arpStep4","arpStep5","arpStep6","arpStep7","arpStep8" };

    for (int i = 0; i < 8; ++i)
    {
        arpStepBtn[i].setBounds(stepRectFor(i));
        arpStepBtn[i].addMouseListener(this, false);
        arpStepAtt[(size_t)i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.apvts, stepIds[i], arpStepBtn[i]);
        addAndMakeVisible(arpStepBtn[i]);

        for (int s = 0; s < (int)xl::getSpecs().size(); ++s)
            if (juce::String(stepIds[i]) == xl::getSpecs()[(size_t)s].id)
            {
                arpStepSpecIdx[(size_t)i] = s;
                break;
            }
    }
}

void Parallax64AudioProcessorEditor::randomizeArpSteps()
{
    static const char* const stepIds[8] =
    { "arpStep1","arpStep2","arpStep3","arpStep4","arpStep5","arpStep6","arpStep7","arpStep8" };

    juce::Random rng;
    for (int i = 0; i < 8; ++i)
    {
        bool on = (rng.nextFloat() > 0.35f);
        if (auto* p = proc.apvts.getParameter(stepIds[i]))
            p->setValueNotifyingHost(on ? 1.0f : 0.0f);
    }
}

//==============================================================================
void Parallax64AudioProcessorEditor::timerCallback()
{
    oled.set(proc.getOledText());
    updateWaveDisplays();
    proc.syncMidiMapIfDirty();
}

//==============================================================================
void Parallax64AudioProcessorEditor::resized()
{
    title.setBounds(16, 12, 260, 26);

    int oledW = 320, oledH = 64;
    int oledX = (getWidth() - oledW) / 2;
    oled.setBounds(oledX, 12, oledW, oledH);

    // Skin-Auswahl links unter dem PARALLAX64-LCXL-SYNTH Logo
    themeLabel.setBounds(16, 44, 38, 22);
    themeBox.setBounds(56, 44, 150, 22);

    // Preset-Auswahl oben rechts (wo vorher Skin war)
    int rX = getWidth() - 16;
    presetBox.setBounds(rX - 250, 14, 250, 24);

    // PREV, NEXT, SAVE direkt unter dem Preset-Feld rechts bündig
    prevBtn.setBounds(rX - 250, 44, 78, 22);
    nextBtn.setBounds(rX - 164, 44, 78, 22);
    saveBtn.setBounds(rX - 78, 44, 78, 22);
}

//==============================================================================
void Parallax64AudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto& th = *laf.th;
    g.fillAll(juce::Colour(th.bg));

    // Panels zeichnen
    for (int i = 0; i < 9; ++i)
    {
        auto P = panelAt(i);
        auto b = juce::Rectangle<float>((float)P.x, (float)P.y, (float)P.w, (float)P.h);

        g.setColour(juce::Colour(th.panel));
        g.fillRoundedRectangle(b, 4.0f);
        g.setColour(juce::Colour(th.border));
        g.drawRoundedRectangle(b, 4.0f, 1.0f);

        if (P.title && P.title[0])
        {
            uint32_t col = (P.sec >= 0 && P.sec < 9) ? th.sec[P.sec] : th.lbl;
            g.setColour(juce::Colour(col));
            g.setFont(themeF(th, 10.0f, true));
            g.drawText(P.title, b.reduced(8, 5), juce::Justification::topLeft);
        }
    }

    // ARP PATTERN & STEPS Panels
    auto drawSubPanel = [&](PanelGeom P)
        {
            auto b = juce::Rectangle<float>((float)P.x, (float)P.y, (float)P.w, (float)P.h);
            g.setColour(juce::Colour(th.panel));
            g.fillRoundedRectangle(b, 4.0f);
            g.setColour(juce::Colour(th.border));
            g.drawRoundedRectangle(b, 4.0f, 1.0f);

            if (P.title && P.title[0])
            {
                g.setColour(juce::Colour(th.sec[7]));
                g.setFont(themeF(th, 10.0f, true));
                g.drawText(P.title, b.reduced(8, 5), juce::Justification::topLeft);
            }
        };
    drawSubPanel(arpPanel());
    drawSubPanel(stepsPanel());

    // Hardware-LEDs (3x8 unter den Encodern)
    for (int i = 0; i < 24; ++i)
    {
        int p, c, n;
        widgetCell(i, p, c, n);
        int sec = sectionFor(i);
        auto r = ledRectFor(p, c, n).toFloat();
        g.setColour(juce::Colour(th.sec[sec]));
        g.fillRoundedRectangle(r, 1.5f);
    }
}

//==============================================================================
// Parameter (Spec-Index) unter dem Mauszeiger; wird fuer MIDI-Learn (Rechtsklick) und fuer die OLED-Anzeige benutzt.
int Parallax64AudioProcessorEditor::specIndexFor(const juce::MouseEvent& e)
{
    int foundSpecIdx = -1;

    auto checkComp = [&](juce::Component* c, int specIdx)
        {
            if (c != nullptr && (e.originalComponent == c || c->isParentOf(e.originalComponent)))
                foundSpecIdx = specIdx;
        };

    for (size_t i = 0; i < sliders.size() && foundSpecIdx < 0; ++i)
        checkComp(sliders[i].get(), (int)i);

    for (size_t i = 0; i < buttons.size() && foundSpecIdx < 0; ++i)
        checkComp(buttons[i].get(), 32 + (int)i);

    for (int i = 0; i < (int)xl::getSpecs().size() && foundSpecIdx < 0; ++i)
    {
        juce::String id(xl::getSpecs()[(size_t)i].id);
        if (id == "lfoSync") checkComp(&lfoSyncBtn, i);
        else if (id == "dlySync") checkComp(&dlySyncBtn, i);
        else if (id == "dlyMode") checkComp(&dlyModeBox, i);
        else if (id == "distAmt") checkComp(&distKnob, i);
        else if (id == "distOn")  checkComp(&distOnBtn, i);
        else if (id == "arpRate") checkComp(&arpRateBox, i);
        else if (id == "arpDir")  checkComp(&arpDirBox, i);
        else if (id == "arpOct")  checkComp(&arpOctBox, i);
        else if (id == "arpGate") checkComp(&arpGateSlider, i);
        else if (id == "arpSync") checkComp(&arpSyncBtn, i);
    }

    for (size_t i = 0; i < 8 && foundSpecIdx < 0; ++i)
        checkComp(&arpStepBtn[i], arpStepSpecIdx[i]);

    return (foundSpecIdx >= 0 && foundSpecIdx < (int)xl::getSpecs().size()) ? foundSpecIdx : -1;
}

void Parallax64AudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails&)
{
    int idx = specIndexFor(e);
    if (idx >= 0) proc.touchParameter(idx);   // Mausrad ueber einem Regler: OLED zeigt den Wert
}

//==============================================================================
void Parallax64AudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (e.originalComponent == &title && e.mods.isLeftButtonDown())
    {
        if (!aboutDlg)
        {
            aboutDlg = std::make_unique<AboutDialog>();
            addAndMakeVisible(*aboutDlg);
            aboutDlg->setTheme(laf.th);
            aboutDlg->onClose = [this] { aboutDlg->setVisible(false); };
        }
        aboutDlg->open(this);
        return;
    }

    const bool rightClick = e.mods.isPopupMenu() || e.mods.isRightButtonDown();
    const int foundSpecIdx = specIndexFor(e);

    // Linksklick/Ziehen auf einen Regler, Schalter oder eine Auswahl: OLED zeigt Name und Wert (wie bei den Hardware-Reglern)
    if (!rightClick)
    {
        if (foundSpecIdx >= 0) proc.touchParameter(foundSpecIdx);
        return;
    }

    // MIDI Learn per Rechtsklick
    {
        if (foundSpecIdx < 0) return;

        const auto& sp = xl::getSpecs()[(size_t)foundSpecIdx];
        int curCC = proc.getMappedCCFor(foundSpecIdx);
        int defCC = sp.cc;

        juce::PopupMenu m;
        m.addSectionHeader(juce::String(sp.name));

        if (curCC >= 0)
            m.addItem(1, "Mapping: CC " + juce::String(curCC) + " (User)", false);
        else if (defCC >= 0)
            m.addItem(1, "Mapping: CC " + juce::String(defCC) + " (Werk)", false);
        else
            m.addItem(1, "Kein Mapping", false);

        m.addSeparator();

        if (proc.isLearning(foundSpecIdx))
            m.addItem(2, "Learn abbrechen");
        else
            m.addItem(2, "MIDI Learn (CC senden)");

        if (curCC >= 0)
            m.addItem(3, "Mapping entfernen (auf Werk)");

        m.addSeparator();
        m.addItem(4, "Alle Mappings zuruecksetzen (" + juce::String(proc.getMappingCount()) + ")");

        // Menue direkt an der Mausposition oeffnen (vorher am Rand des Editors)
        m.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this).withMousePosition(),
            [this, foundSpecIdx](int result)
            {
                if (result == 2)
                {
                    if (proc.isLearning(foundSpecIdx)) proc.cancelMidiLearn();
                    else                              proc.startMidiLearn(foundSpecIdx);
                }
                else if (result == 3)
                {
                    proc.removeMappingForParam(foundSpecIdx);
                }
                else if (result == 4)
                {
                    proc.clearAllMidiMappings();
                }
            });
    }
}

