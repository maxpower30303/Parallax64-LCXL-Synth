// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Flix
#pragma once
#include <memory>
#include <vector>
#include <functional>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

//==============================================================================
// Farbschemata – sec[9] = Sektionsfarben:
//   0=OSC1  1=OSC2  2=MIX  3=FILTER  4=LFO  5=FX  6=AMP ENV  7=ARP  8=GLOBAL
struct Theme
{
    const char* name;
    uint32_t bg, panel, border, ac, acDim, txt, lbl, header;
    uint32_t displayBg, displayTxt;
    uint32_t knobFill, knobLine;
    uint32_t faderRail, faderThumb, faderStripe;
    uint32_t btnOnTxt, btnOff, btnOffTxt;
    uint32_t popupHi;
    bool serif;
    uint32_t sec[9];
};

inline const std::vector<Theme>& getThemes()
{
    static const std::vector<Theme> t =
    {
        // 0: Night Blue
        { "Night Blue",
          0xff0a0e14, 0xff0d141c, 0xff23303d, 0xff4C9BE8, 0xff2a5378, 0xff8fc3f0, 0xff7ba2c4, 0xff5d7590,
          0xff020507, 0xff67e8f9,
          0xff0d141c, 0xff4C9BE8,
          0xff1b2530, 0xff4C9BE8, 0xffcfe6ff,
          0xff0a0e14, 0xff0d141c, 0xff8fc3f0,
          0xff172435, false,
          { 0xffc084fc, 0xffe879f9, 0xff38bdf8, 0xff22d3ee, 0xff4ade80, 0xfffb923c, 0xff60a5fa, 0xfff472b6, 0xff94a3b8 } },

          // 1: Vintage Cream ("Analogue Eight")
          { "Vintage Cream",
            0xfff2e8d5, 0xffe0d3b0, 0xff8a6d47, 0xffc9622a, 0xff5c4326, 0xff4a3c28, 0xff6b5a3e, 0xff8a6d47,
            0xffd8cba8, 0xff4a3c28,
            0xffc9622a, 0xfff2e8d5,
            0xffe0d3b0, 0xffc9622a, 0xfff2e8d5,
            0xfff2e8d5, 0xffe0d3b0, 0xff4a3c28,
            0xffd6c39a, true,
            { 0xffc9622a, 0xff8f3e12, 0xff6b7a2e, 0xff5c4326, 0xff4a7c59, 0xff2e6e6a, 0xffb07a3a, 0xff9b3b3b, 0xff4a3c28 } },

            // 2: Dark Red
            { "Dark Red",
              0xff140a0c, 0xff1c0e11, 0xff3d1f24, 0xffe05050, 0xff6e2a33, 0xfff0b8b8, 0xffc08a8a, 0xffa06060,
              0xff050202, 0xffff9e9e,
              0xff1c0e11, 0xffe05050,
              0xff2a1418, 0xffe05050, 0xfff5d5d5,
              0xff140a0c, 0xff1c0e11, 0xfff0b8b8,
              0xff241014, false,
              { 0xffff6b6b, 0xffe05050, 0xfff0a868, 0xffff9e9e, 0xff9ecf7a, 0xffe0b050, 0xffd98a8a, 0xffe86ba8, 0xfff0b8b8 } },

              // 3: Green
              { "Green",
                0xff071008, 0xff0c1a0e, 0xff1e3a22, 0xff4ade80, 0xff2f6e3a, 0xffa7f3c0, 0xff7bbd8f, 0xff6fae7f,
                0xff020602, 0xff86efac,
                0xff0c1a0e, 0xff4ade80,
                0xff12281a, 0xff4ade80, 0xffd9ffe4,
                0xff071008, 0xff0c1a0e, 0xffa7f3c0,
                0xff12241a, false,
                { 0xff4ade80, 0xff22c55e, 0xff86efac, 0xff2dd4bf, 0xffa3e635, 0xfffbbf24, 0xff34d399, 0xfffb7185, 0xffa7f3c0 } },

                // 4: White / Red
                { "White / Red",
                  0xfff5f5f2, 0xffffffff, 0xffc9c9c2, 0xffd92b2b, 0xff8f8f89, 0xff2b2b28, 0xff6e6e66, 0xff8a8a82,
                  0xff1f1d1a, 0xffff6b6b,
                  0xffffffff, 0xffd92b2b,
                  0xffdcdcd6, 0xffd92b2b, 0xffffffff,
                  0xffffffff, 0xffffffff, 0xff2b2b28,
                  0xffececea, false,
                  { 0xffd92b2b, 0xffa02020, 0xff55554e, 0xff2b2b28, 0xff5a7a3a, 0xffb0763a, 0xff3d6b6b, 0xff8a1f4a, 0xff6e6e66 } },
    };
    return t;
}

inline juce::String copyrightSign() { return juce::String(juce::CharPointer_UTF8("\xc2\xa9")); }

inline juce::Font themeF(const Theme& th, float size, bool bold = false, bool italic = false)
{
    if (th.serif)
        return juce::Font(juce::FontOptions("Georgia", size,
            (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0)));

    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
        size, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
class ParLookAndFeel : public juce::LookAndFeel_V4
{
public:
    const Theme* th = &getThemes()[0];

    void setTheme(const Theme* t) { th = t; }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
        float pos, float startAngle, float endAngle,
        juce::Slider& slider) override
    {
        auto b = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h);
        float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f - 1.5f;
        float cx = b.getCentreX(), cy = b.getCentreY();

        g.setColour(juce::Colour(th->knobFill));
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colour(th->acDim));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.4f);

        auto ind = slider.findColour(juce::Slider::rotarySliderFillColourId);
        if (ind.isTransparent()) ind = juce::Colour(th->ac);

        float a = startAngle + pos * (endAngle - startAngle);
        juce::Path p;
        p.startNewSubPath(cx + std::sin(a) * r * 0.15f, cy - std::cos(a) * r * 0.15f);
        p.lineTo(cx + std::sin(a) * r * 0.78f, cy - std::cos(a) * r * 0.78f);
        g.setColour(ind);
        g.strokePath(p, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float /*min*/, float /*max*/,
        juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        auto ind = slider.findColour(juce::Slider::trackColourId);
        if (ind.isTransparent()) ind = juce::Colour(th->ac);

        if (slider.getSliderStyle() == juce::Slider::LinearVertical)
        {
            float cx = (float)x + (float)w * 0.5f;

            g.setColour(juce::Colour(th->faderRail));
            g.fillRoundedRectangle(cx - 2.0f, (float)y, 4.0f, (float)h, 2.0f);

            auto t = juce::Rectangle<float>(cx - 14.0f, sliderPos - 6.0f, 28.0f, 12.0f);
            g.setColour(ind);
            g.fillRoundedRectangle(t, 2.0f);
            g.setColour(juce::Colour(th->faderStripe));
            g.fillRect(t.getX() + 3.0f, sliderPos - 1.0f, t.getWidth() - 6.0f, 2.0f);
        }
        else
        {
            float cy = (float)y + (float)h * 0.5f;

            g.setColour(juce::Colour(th->faderRail));
            g.fillRoundedRectangle((float)x, cy - 2.0f, (float)w, 4.0f, 2.0f);

            auto t = juce::Rectangle<float>(sliderPos - 7.0f, cy - 11.0f, 14.0f, 22.0f);
            g.setColour(ind);
            g.fillRoundedRectangle(t, 2.0f);
            g.setColour(juce::Colour(th->faderStripe));
            g.fillRect(sliderPos - 1.0f, cy - 7.0f, 2.0f, 14.0f);
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        bool on = b.getToggleState();

        auto col = b.findColour(juce::ToggleButton::tickColourId);
        if (col.isTransparent()) col = juce::Colour(th->ac);

        if (on)
        {
            g.setColour(col);
            g.fillRoundedRectangle(r, 3.0f);
            g.setColour(col.brighter(0.25f));
            g.drawRoundedRectangle(r, 3.0f, 1.0f);
            g.setColour(juce::Colour(th->bg));
        }
        else
        {
            g.setColour(juce::Colour(th->btnOff));
            g.fillRoundedRectangle(r, 3.0f);
            g.setColour(col.withAlpha(0.55f));
            g.drawRoundedRectangle(r, 3.0f, 1.0f);
            g.setColour(col);
        }

        g.setFont(themeF(*th, 11.0f, true));
        g.drawText(b.getButtonText(), r, juce::Justification::centred);
    }

    void drawComboBox(juce::Graphics& g, int /*w*/, int /*h*/, bool /*down*/,
        int, int, int, int, juce::ComboBox& c) override
    {
        auto b = c.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(c.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(c.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(b, 3.0f, 1.0f);

        float cx = b.getRight() - 14.0f, cy = b.getCentreY();
        juce::Path a;
        a.addTriangle(cx - 4.0f, cy - 2.5f, cx + 4.0f, cy - 2.5f, cx, cy + 3.0f);
        g.setColour(c.findColour(juce::ComboBox::arrowColourId));
        g.fillPath(a);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override { return themeF(*th, 13.0f); }
    juce::Font getPopupMenuFont() override { return themeF(*th, 13.0f); }

    juce::Font getTextButtonFont(juce::TextButton&, int) override
    {
        return themeF(*th, 13.0f, true);
    }

    void drawPopupMenuBackgroundWithOptions(juce::Graphics& g, int w, int h,
        const juce::PopupMenu::Options&) override
    {
        g.fillAll(juce::Colour(th->panel));
        g.setColour(juce::Colour(th->border));
        g.drawRect(0, 0, w, h);
    }

    void drawPopupMenuItemWithOptions(juce::Graphics& g, const juce::Rectangle<int>& area,
        bool highlighted, const juce::PopupMenu::Item& item,
        const juce::PopupMenu::Options&) override
    {
        auto b = area.reduced(8, 0);
        if (item.isSeparator)
        {
            g.setColour(juce::Colour(th->border));
            g.fillRect(b.withHeight(1).withY(area.getCentreY()));
            return;
        }
        if (highlighted)
        {
            g.setColour(juce::Colour(th->popupHi));
            g.fillRoundedRectangle(area.reduced(2, 0).toFloat(), 3.0f);
        }
        g.setColour(item.isEnabled
            ? (highlighted ? juce::Colour(th->ac) : juce::Colour(th->txt))
            : juce::Colour(th->lbl));
        g.setFont(themeF(*th, 13.0f));

        auto tb = item.isTicked ? b.withTrimmedLeft(12) : b;
        g.drawText(item.text, tb, juce::Justification::centredLeft);

        if (item.isTicked)
        {
            g.setColour(juce::Colour(th->ac));
            g.fillEllipse((float)b.getX() + 2.0f, (float)area.getCentreY() - 2.0f, 4.0f, 4.0f);
        }
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&,
        bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(th->panel));
        g.fillRoundedRectangle(r, 3.0f);
        if (down) { g.setColour(juce::Colour(th->popupHi)); g.fillRoundedRectangle(r, 3.0f); }
        else if (over) { g.setColour(juce::Colour(th->popupHi).withAlpha(0.5f)); g.fillRoundedRectangle(r, 3.0f); }
        g.setColour(juce::Colour(th->acDim));
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
    }
};

//==============================================================================
struct NameField : juce::TextEditor
{
    std::function<void()> onReturn, onEscape;
    void returnPressed() override { if (onReturn) onReturn(); }
    void escapePressed() override { if (onEscape) onEscape(); }
};

//==============================================================================
struct OledPanel : juce::Component
{
    juce::Label text;
    const Theme* th = &getThemes()[0];

    OledPanel()
    {
        addAndMakeVisible(text);
        text.setJustificationType(juce::Justification::centred);
        text.setColour(juce::Label::textColourId, juce::Colour(th->displayTxt));
        text.setFont(themeF(*th, 14.0f));
    }

    void setTheme(const Theme* t)
    {
        th = t;
        text.setColour(juce::Label::textColourId, juce::Colour(th->displayTxt));
        text.setFont(themeF(*th, 14.0f));
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(th->displayBg));
        g.fillRoundedRectangle(b, 4.0f);
        g.setColour(juce::Colour(th->border));
        g.drawRoundedRectangle(b, 4.0f, 1.0f);
    }

    void resized() override { text.setBounds(getLocalBounds().reduced(7, 4)); }
    void set(const juce::String& s) { text.setText(s, juce::dontSendNotification); }
};

//==============================================================================
struct SaveDialog : juce::Component
{
    juce::Label head, hint;
    NameField nameField;
    juce::TextButton okBtn{ "SAVE" }, cancelBtn{ "CANCEL" };
    std::function<void(const juce::String&)> onSave;
    std::function<void()> onCancel;
    const Theme* th = &getThemes()[0];

    SaveDialog()
    {
        head.setText("PRESET SPEICHERN", juce::dontSendNotification);
        hint.setText("Leer lassen = automatisch (UserPresetN)", juce::dontSendNotification);

        addAndMakeVisible(head);
        addAndMakeVisible(nameField);
        addAndMakeVisible(hint);
        addAndMakeVisible(cancelBtn);
        addAndMakeVisible(okBtn);

        nameField.onReturn = [this] { confirm(); };
        nameField.onEscape = [this] { if (onCancel) onCancel(); };
        okBtn.onClick = [this] { confirm(); };
        cancelBtn.onClick = [this] { if (onCancel) onCancel(); };

        setInterceptsMouseClicks(true, true);
        applyColours();
    }

    void applyColours()
    {
        head.setColour(juce::Label::textColourId, juce::Colour(th->txt));
        head.setFont(themeF(*th, 13.0f, true));
        hint.setColour(juce::Label::textColourId, juce::Colour(th->lbl));
        hint.setFont(themeF(*th, 9.0f));

        nameField.setColour(juce::TextEditor::backgroundColourId, juce::Colour(th->bg));
        nameField.setColour(juce::TextEditor::outlineColourId, juce::Colour(th->acDim));
        nameField.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(th->ac));
        nameField.setColour(juce::TextEditor::textColourId, juce::Colour(th->txt));
        nameField.setColour(juce::TextEditor::highlightColourId, juce::Colour(th->popupHi));
        nameField.setFont(themeF(*th, 12.0f));

        okBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(th->ac));
        cancelBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(th->txt));
    }

    void setTheme(const Theme* t) { th = t; applyColours(); repaint(); }

    juce::Rectangle<float> panelBounds() const
    {
        float w = (float)getWidth(), h = (float)getHeight();
        return { w * 0.5f - 190.0f, h * 0.5f - 75.0f, 380.0f, 150.0f };
    }

    void confirm() { if (onSave) onSave(nameField.getText()); }

    void open(const juce::String& suggestion, juce::Component* parent)
    {
        setBounds(parent->getLocalBounds());
        nameField.setText(suggestion, juce::dontSendNotification);
        nameField.selectAll();
        setVisible(true);
        toFront(false);
        nameField.grabKeyboardFocus();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff000000).withAlpha(0.55f));
        auto b = panelBounds();
        g.setColour(juce::Colour(th->panel));
        g.fillRoundedRectangle(b, 6.0f);
        g.setColour(juce::Colour(th->acDim));
        g.drawRoundedRectangle(b, 6.0f, 1.2f);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!panelBounds().contains(e.position))
            if (onCancel) onCancel();
    }

    void resized() override
    {
        auto b = panelBounds();
        head.setBounds((int)b.getX() + 16, (int)b.getY() + 12, (int)b.getWidth() - 32, 16);
        nameField.setBounds((int)b.getX() + 16, (int)b.getY() + 34, (int)b.getWidth() - 32, 28);
        hint.setBounds((int)b.getX() + 16, (int)b.getY() + 66, (int)b.getWidth() - 32, 12);
        cancelBtn.setBounds((int)b.getX() + 16, (int)b.getY() + 96, 110, 26);
        okBtn.setBounds((int)b.getRight() - 126, (int)b.getY() + 96, 110, 26);
    }
};

//==============================================================================
struct AboutDialog : juce::Component
{
    juce::Label head, ver, body;
    juce::TextButton okBtn{ "OK" };
    std::function<void()> onClose;
    const Theme* th = &getThemes()[0];

    AboutDialog()
    {
        head.setText("PARALLAX64-LCXL-SYNTH", juce::dontSendNotification);

        ver.setText(juce::String("Version ") + xl::kPluginVersion,
            juce::dontSendNotification);

        // Lizenz: JUCE 9 steht unter AGPLv3 (oder kommerziell). Das Projekt nutzt die AGPLv3-Option und
        // steht deshalb selbst unter der AGPLv3 (siehe LICENSE.txt / THIRD_PARTY_NOTICES.txt).
        body.setText(
            copyrightSign() + juce::String(" 2026 @ Flix  |  v") + xl::kPluginVersion + "\n\n"
            + "Free software, licensed under the GNU Affero General Public\n"
            "License v3. Provided without any warranty.\n\n"
            + "8-voice synthesizer with two oscillators, sub and noise,\n"
            "filter, LFO, arpeggiator and effects, made to be played\n"
            "from a Novation Launch Control XL MK3.\n\n"
            + "Built with JUCE 9 (juce.com), used under the AGPLv3.\n"
            "Includes libFLAC and Ogg Vorbis (Xiph.Org, BSD-style licences).\n\n"
            + "VST is a trademark of Steinberg Media Technologies GmbH,\n"
            "registered in Europe and other countries.\n"
            "ASIO is a trademark and software of Steinberg Media\n"
            "Technologies GmbH.\n"
            "Launch Control XL is a trademark of Novation; this project\n"
            "is not affiliated with Novation.\n"
            "All other names are the property of their respective owners.\n\n"
            + "Supported by AI (artificial intelligence).",
            juce::dontSendNotification);
        head.setJustificationType(juce::Justification::centred);
        ver.setJustificationType(juce::Justification::centred);
        body.setJustificationType(juce::Justification::centredTop);

        addAndMakeVisible(head);
        addAndMakeVisible(ver);
        addAndMakeVisible(body);
        addAndMakeVisible(okBtn);

        okBtn.onClick = [this] { if (onClose) onClose(); };

        setInterceptsMouseClicks(true, true);
        applyColours();
    }

    void applyColours()
    {
        head.setColour(juce::Label::textColourId, juce::Colour(th->ac));
        head.setFont(themeF(*th, 20.0f, true, th->serif));
        ver.setColour(juce::Label::textColourId, juce::Colour(th->lbl));
        ver.setFont(themeF(*th, 12.0f));
        body.setColour(juce::Label::textColourId, juce::Colour(th->txt));
        body.setFont(themeF(*th, 12.0f));
        okBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(th->ac));
    }

    void setTheme(const Theme* t) { th = t; applyColours(); repaint(); }

    juce::Rectangle<float> panelBounds() const
    {
        float w = (float)getWidth(), h = (float)getHeight();
        return { w * 0.5f - 280.0f, h * 0.5f - 220.0f, 560.0f, 440.0f };
    }

    void open(juce::Component* parent)
    {
        setBounds(parent->getLocalBounds());
        setVisible(true);
        toFront(false);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff000000).withAlpha(0.55f));
        auto b = panelBounds();
        g.setColour(juce::Colour(th->panel));
        g.fillRoundedRectangle(b, 6.0f);
        g.setColour(juce::Colour(th->acDim));
        g.drawRoundedRectangle(b, 6.0f, 1.2f);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!panelBounds().contains(e.position))
            if (onClose) onClose();
    }

    void resized() override
    {
        auto b = panelBounds();
        head.setBounds((int)b.getX() + 16, (int)b.getY() + 16, (int)b.getWidth() - 32, 26);
        ver.setBounds((int)b.getX() + 16, (int)b.getY() + 44, (int)b.getWidth() - 32, 16);
        body.setBounds((int)b.getX() + 16, (int)b.getY() + 66, (int)b.getWidth() - 32, 320);
        okBtn.setBounds((int)b.getRight() - 116, (int)b.getBottom() - 42, 100, 28);
    }
};

//==============================================================================
class Parallax64AudioProcessorEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    explicit Parallax64AudioProcessorEditor(Parallax64AudioProcessor&);
    ~Parallax64AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

private:
    int specIndexFor(const juce::MouseEvent& e);   // Parameter unter dem Mauszeiger (-1 = keiner)
    void timerCallback() override;
    void buildWidgets();
    void buildArpPanel();
    void randomizeArpSteps();
    void rebuildPresetList();
    void applyTheme(int idx, bool store);
    void stepPreset(int dir);
    void updateWaveDisplays();

    struct PanelGeom { int x, y, w, h, sec; const char* title; };

    static PanelGeom panelAt(int idx)
    {
        static const PanelGeom P[] =
        {
            {  16,  88, 430, 112, 0, "OSC 1" },
            { 454,  88, 430, 112, 1, "OSC 2" },
            {  16, 210, 319, 112, 2, "MIX" },
            { 343, 210, 541, 112, 3, "FILTER" },
            {  16, 332, 319, 112, 4, "LFO" },
            { 343, 332, 426, 112, 5, "FX" },
            { 777, 332, 107, 112, 8, "MASTER" },
            {  16, 454, 430, 150, 6, "AMP ENV" },
            { 454, 454, 430, 150, 3, "FILTER ENV" },
            {  16, 610, 868,  26, -1, "" },
            {  16, 646, 868,  26, -1, "" },
        };
        return P[idx];
    }

    static PanelGeom arpPanel() { return { 16, 684, 868, 76, 7, "ARP PATTERN" }; }
    static PanelGeom stepsPanel() { return { 16, 772, 868, 52, 7, "STEPS" }; }

    static void widgetCell(int i, int& p, int& c, int& n)
    {
        if (i < 4) { p = 0;  c = i;      n = 4; }
        else if (i < 8) { p = 1;  c = i - 4;  n = 4; }
        else if (i < 11) { p = 2;  c = i - 8;  n = 3; }
        else if (i < 16) { p = 3;  c = i - 11; n = 5; }
        else if (i < 19) { p = 4;  c = i - 16; n = 3; }
        else if (i < 23) { p = 5;  c = i - 19; n = 4; }
        else if (i < 24) { p = 6;  c = 0;      n = 1; }
        else if (i < 28) { p = 7;  c = i - 24; n = 4; }
        else if (i < 32) { p = 8;  c = i - 28; n = 4; }
        else if (i < 40) { p = 9;  c = i - 32; n = 8; }
        else { p = 10; c = i - 40; n = 8; }
    }

    static juce::Rectangle<int> cellCentreRect(int p, int c, int n, int w, int h, int yOff)
    {
        auto P = panelAt(p);
        int cw = P.w / n;
        int cx = P.x + cw * c + cw / 2;
        return { cx - w / 2, P.y + yOff, w, h };
    }

    static juce::Rectangle<int> knobRectFor(int p, int c, int n) { return cellCentreRect(p, c, n, 44, 44, 30); }
    static juce::Rectangle<int> faderRectFor(int p, int c, int n) { return cellCentreRect(p, c, n, 28, 104, 26); }
    static juce::Rectangle<int> ledRectFor(int p, int c, int n) { return cellCentreRect(p, c, n, 10, 3, 78); }

    static juce::Rectangle<int> labelRectFor(int p, int c, int n, bool fader)
    {
        auto P = panelAt(p);
        int cw = P.w / n;
        return { P.x + cw * c + 3, P.y + (fader ? 132 : 82), cw - 6, 16 };
    }

    static juce::Rectangle<int> btnRectFor(int row, int c)
    {
        auto P = panelAt(row == 0 ? 9 : 10);
        int cw = P.w / 8;
        int cx = P.x + cw * c + cw / 2;
        return { cx - 48, P.y, 96, 26 };
    }

    static juce::Rectangle<int> stepRectFor(int i)
    {
        auto P = stepsPanel();
        int cw = P.w / 8;
        int cx = P.x + cw * i + cw / 2;
        return { cx - 44, P.y + 22, 88, 24 };
    }

    static juce::Rectangle<int> waveBoxRect(int panelIdx)
    {
        auto P = panelAt(panelIdx);
        int cw = P.w / 4;
        int cx = P.x + cw / 2;
        return { cx + 27, P.y + 40, 54, 22 };
    }

    static juce::Rectangle<int> knobSideToggleRect(int panelIdx, int cellIdx,
        int cellsPerRow, int w, int h)
    {
        auto P = panelAt(panelIdx);
        int cw = P.w / cellsPerRow;
        int cx = P.x + cw * cellIdx + cw / 2;
        return { cx + 27, P.y + 42, w, h };
    }

    static juce::Rectangle<int> distKnobRect()
    {
        auto P = panelAt(3);
        int cw = P.w / 5;
        int cx = P.x + cw * 3 + cw / 2;
        return { cx + 28, P.y + 37, 30, 30 };
    }

    // DIST-ON-Schalter oberhalb des DIST-Knopfs (freier Platz im FILTER-Panel)
    static juce::Rectangle<int> distOnRect()
    {
        auto P = panelAt(3);
        int cw = P.w / 5;
        int cx = P.x + cw * 3 + cw / 2;
        return { cx + 22, P.y + 14, 44, 18 };
    }

    static juce::Rectangle<int> distLblRect()
    {
        auto P = panelAt(3);
        int cw = P.w / 5;
        int cx = P.x + cw * 3 + cw / 2;
        return { cx + 10, P.y + 68, 66, 13 };
    }

    static int sectionFor(int i)
    {
        static const int map[48] = {
            0,0,0,0, 1,1,1,1,
            2,2,2, 3,3,3,3,3,
            4,4,4, 5,5,5,5, 8,
            6,6,6,6, 3,3,3,3,
            0, 1, 1, 2, 2, 4, 4, 4,
            8, 8, 2, 8, 5, 5, 4, 7
        };
        return map[i];
    }

    static constexpr int kNWidgets = 48;

    Parallax64AudioProcessor& proc;
    ParLookAndFeel laf;

    juce::Label title, themeLabel;
    OledPanel oled;
    juce::ComboBox presetBox, themeBox;
    juce::TextButton saveBtn{ "SAVE" };
    juce::TextButton prevBtn{ "PREV" };
    juce::TextButton nextBtn{ "NEXT" };
    std::unique_ptr<SaveDialog> saveDlg;
    std::unique_ptr<AboutDialog> aboutDlg;

    juce::Label waveLbl1, waveLbl2;

    juce::ToggleButton lfoSyncBtn{ "SYNC" };
    juce::ToggleButton dlySyncBtn{ "SYNC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lfoSyncAtt, dlySyncAtt;
    juce::Label dlyModeLbl;
    juce::ComboBox dlyModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dlyModeAtt;

    juce::Slider distKnob;
    juce::Label  distLbl;
    juce::ToggleButton distOnBtn{ "ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> distOnAtt;

    juce::Label arpRateLbl, arpDirLbl, arpOctLbl, arpGateLbl;
    juce::ComboBox arpRateBox, arpDirBox, arpOctBox;
    juce::Slider arpGateSlider;
    juce::ToggleButton arpSyncBtn{ "SYNC" };
    juce::TextButton  arpRandBtn{ "RANDOM" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> arpRateAtt, arpDirAtt, arpOctAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   arpGateAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   arpSyncAtt;

    juce::ToggleButton arpStepBtn[8]{ juce::ToggleButton("1"), juce::ToggleButton("2"),
                                       juce::ToggleButton("3"), juce::ToggleButton("4"),
                                       juce::ToggleButton("5"), juce::ToggleButton("6"),
                                       juce::ToggleButton("7"), juce::ToggleButton("8") };
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 8> arpStepAtt;
    std::array<int, 8> arpStepSpecIdx{ -1, -1, -1, -1, -1, -1, -1, -1 };

    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::ToggleButton>> buttons;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Parallax64AudioProcessorEditor)
};
