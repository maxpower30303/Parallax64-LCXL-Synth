# Parallax64-LCXL-Synth – Changelog

## 1.0.11
Stresstest vor dem Release (Zufalls-MIDI/-Parameter/-Presets, 16 Laeufe, 387k Bloecke, AddressSanitizer, Threads, State, Randfaelle).
- State wuchs bei jedem Speichern/Laden (MIDIMAP-Knoten verdoppelt): behoben, genau ein Knoten.
- MIDI All Notes Off (CC123) und All Sound Off (CC120) werden beachtet (DAW-Stop haengt keine Noten mehr).
- Sustain-Pedal (CC64) unterstuetzt.
- Keine Heap-Allokation mehr im Audio-Thread bei >64 Arp-Noten und bei Host-Bloecken groesser als vorbereitet (Reverb in Stuecken).
- Statische VC++-Runtime (keine Redistributable noetig); EXE-Versionsinfo korrekt (Flix / Version).
- Messung: 1.7 % CPU im Mittel (48 kHz/256, 8 Stimmen), Worst-Case 19.6 % bei 192 kHz/128; 0 Fehler unter ASan.

## 1.0.10
OLED-Display zeigt jetzt auch Werte, die in der Oberflaeche eingestellt werden.
- Klick/Ziehen auf einen Regler, Schalter oder eine Auswahl sowie das Mausrad ueber einem Regler stellen das Display auf
  den Parameter (Name + Wert, z. B. "Cutoff | 240 Hz"). Vorher reagierte das Display nur auf Hardware-Regler (CC);
  die vorhandene Funktion touchParameter() wurde nirgends aufgerufen.
- Rechtsklick (MIDI-Learn-Menue) und Preset-Anzeige unveraendert.

## 1.0.09
Release-Vorbereitung und Fix fuer haengende Noten.
- **Haengende Noten behoben** (Test mit dem echten Processor, 15 Szenarien, vorher 3 haengend, jetzt 0):
  - Mehrere Tasten in Poly gehalten, dann Mono eingeschaltet: beim Loslassen wurde nur Stimme 0 freigegeben, die
    uebrigen Stimmen klangen ewig weiter. Mono gibt jetzt alle Stimmen frei, sobald der Notenstapel leer ist.
  - Veralteter Mono-Notenstapel nach dem Umschalten Mono -> Poly -> Mono (naechste Note blieb haengen).
  - Dieselbe Note zweimal angeschlagen, einmal losgelassen (Mono): doppelter Stapel-Eintrag entfernt.
  - Wechsel von Mono/Unison beendet gehaltene Noten sauber (`allNotesOff`), wie schon beim Arp.
- **CMake ohne feste Pfade:** `JUCE_DIR` (Variable oder Umgebungsvariable) oder automatischer Download von JUCE 9.0.2
  per FetchContent. ASIO ueber die in JUCE enthaltenen Header (`JUCE_ASIO=1`), kein externes ASIO SDK mehr noetig.
  `COMPANY_NAME` = "Flix" (Plugin-Hersteller-Name; PLUGIN_CODE/MANUFACTURER_CODE unveraendert).
- `README.md`, `.gitignore`, SPDX-Zeilen (AGPL-3.0-only, Copyright Flix) in allen Quelldateien.
## 1.0.08
Knacken zwischen den Noten behoben.
- **Ursache:** Beim Start einer neuen Note wurden Huellkurve (Pegel -> 0), Oszillator-Phasen, Filter-Zustand und
  Cutoff-Glaettung hart zurueckgesetzt, auch wenn die Stimme noch ausklang (Mono-Retrigger, Unison, Stimmenklau).
  Im Mono-Modus wurden ausserdem alle uebrigen Stimmen mit `hardStop()` abgeschnitten. Das ergibt einen Sprung im
  Signal genau auf dem ersten Sample der Note. Durch die Preset-Pegel aus 1.0.07 (bis +28 dB) wurde es deutlich hoerbarer.
- **Messung** (echter Processor, Noten alle 0.3 s, 8 Noten, Sprung auf dem ersten Sample der Note gegen den groessten
  Sprung der vorherigen 20 ms): kurz gespielt 9 Presets / 23 Spruenge -> 1 / 1; ueberlappend gespielt
  **21 Presets / 71 Spruenge -> 1 / 1** (u. a. B03, B06-B11, B14).
- **Fixes (SynthEngine.h):** laufende Stimme behaelt beim Neustart Phasen, Filter und Huellkurven-Pegel (Attack startet
  vom aktuellen Pegel); Mono laesst die anderen Stimmen ausklingen statt hardStop(); Attack mindestens 2 ms;
  Release-Ende bei 0.0002 statt 0.0005.
- Preset-Pegel nach der Engine-Aenderung neu eingemessen (127 von 128 Presets zwischen -20.5 und -19.5 dBFS).
- Nicht per Gehoer geprueft. In je einem Preset bleibt ein verdaechtiger Sprung (kurz: L14, ueberlappend: D05), das
  koennte auch ein normaler Wellenform-Einsatz sein.
## 1.0.07
Lautstaerke-Abgleich aller 128 Factory-Presets.
- Vorher (gemessen): lautestes 0.4-s-Fenster je Preset von -48.0 bis -8.4 dBFS RMS = **40 dB Spanne**
  (z. B. B09 -46 dBFS, B07 -21 dBFS).
- Neuer, versteckter Parameter `presetLevel` (-40..+36 dB, kein Regler/CC, nicht im GUI). Wirkt zusammen mit dem
  Master-Gain hinter allen Effekten (Distortion/Delay/Reverb behalten ihren Charakter); der MASTER-Regler der
  Hardware bleibt unberuehrt. Presets ohne Eintrag haben 0 dB; User-Presets und alte Zustaende bleiben kompatibel.
- Pegel je Preset eingetragen (Bereich -12.5 .. +28 dB). Ziel: **-20 dBFS RMS**, Spitzen <= -1.5 dBFS.
- Nachher (gemessen, echter Processor, Phrase je Kategorie: Bass 1 Note, Lead, Pluck, Akkord fuer Keys/Pads/Arp,
  Drums, FX): 127 von 128 Presets zwischen -20.1 und -19.8 dBFS. Ausnahme **D05 Closed Hat** (-26.1 dBFS, wegen
  Spitzen-Grenze). Spitzen max. -1.6 dBFS, keine NaN/Inf.
- Hinweis: Sehr hohe Zuschlaege (K01 +28, B09 +26.5 dB) kommen von Presets mit Ladder-Filter (LAD) und hoher
  Resonanz, der im Durchlassbereich viel Pegel verliert. Nicht per Gehoer geprueft.
## 1.0.06
Markenbereinigung (fuer die Veroeffentlichung). Klang unveraendert, nur Namen.
- 21 Presets umbenannt (fremde Synth-/Geraete-/Kuenstler-/Filmnamen): B09 Ladder 24dB, B10 Sub Boom, L06 Warm Tines,
  L13 Flute Voyage, K01 Chorus Staccato, K02 Progressive Pulse, Y01 Analog Brass, Y02 Wide Poly Sweep, Y05 Neo Soul Tines,
  Y06 Drawbar House Organ, Y07 Vintage Reed Keys, Y08 Classic Poly, Y11 Funky Snap Keys, P07 Neon Skyline,
  A02 Sequencer Dream, A06 Acid Rolling, A07 Night Signals, A10 Deep Space, D01 Sub Kick, D02 Punch Kick, D10 Analog Cowbell.
- Klasse MoogLadder -> LadderFilter (Variable moog -> ladder), Kommentare angepasst. Filtermodus heisst weiter LAD.

## 1.0.05
THIRD_PARTY_NOTICES.txt vollstaendig: zusaetzlich HarfBuzz, SheenBidi, LunaSVG, PlutoVG, libpng, IJG JPEG, zlib;
FLAC/Ogg/Vorbis, VST3 SDK (MIT) und ASIO-Lizenz jetzt wortgetreu aus dem JUCE-9.0.2-Quellbaum (Versionen laut JUCE.spdx.json).

## 1.0.04
About-Dialog: Lizenz bestaetigt (AGPLv3), Satz zum Projekt-Repository entfernt, KI-Hinweis lautet jetzt
"Supported by AI (artificial intelligence)."

## 1.0.03
Lizenz und About-Dialog.

- **About-Dialog** an den Drumz-Dialog angeglichen (englisch, zentriert, groesseres Feld): (c) 2026 @ Flix,
  Lizenzhinweis, Kurzbeschreibung, JUCE / libFLAC / Ogg Vorbis, Marken-Hinweise (VST, ASIO, Novation), KI-Hinweis.
- **Lizenz korrigiert:** Der alte Text ("Freeware, nur privat/nicht-kommerziell, unveraendert weitergeben") war
  von der Drum Machine uebernommen bzw. nicht mit JUCE vereinbar. JUCE 9 ist AGPLv3 oder kommerziell; das Projekt
  nutzt die AGPLv3-Option und steht deshalb selbst unter der **GNU AGPL v3**.
- Neu im Projektordner: LICENSE.txt (AGPLv3-Volltext) und THIRD_PARTY_NOTICES.txt (JUCE 9.0.2, VST3 SDK MIT,
  ASIO SDK 2.3 GPLv3-Option, libFLAC, Ogg Vorbis, Novation-Markenhinweis). Beide gehoeren zu jeder Binaer-Auslieferung
  und liegen deshalb auch in out\bin\.
## 1.0.02  (Basis: 1.0.01)
Echtzeit-Sicherheit und Performance. **Klanglich identisch zu 1.0.01** (Engine-Ausgabe per Test bitgenau gleich).

### Echtzeit (Audio-Thread)
- **Keine String-Allokationen mehr in `processBlock`.** `val("id")` und `apvts.getParameter("id")` bauten pro
  Aufruf einen `juce::String` (~70 Heap-Allokationen pro Block). Jetzt: `P64_VAL("id")` loest den Spec-Index
  pro Aufrufstelle einmal auf (`xl::indexOf`, function-local static) und liest ueber `rawByIdx[]`; Parameter-
  Zeiger liegen in `paramByIdx[]`.
- `arpHeld.reserve(64)` – kein Wachsen des Vektors mehr im Audio-Thread.

### Performance
- Oszillator-Frequenzfaktoren (Oktave + Tune + Unison-Detune) einmal pro Block in `Engine::setParams` statt
  2x `std::pow` pro Sample und Stimme.
- `std::fmod` in Triangle/Square durch einen bedingten Abzug ersetzt.
- Drive-tanh wird bei Drive ~0 uebersprungen.
- Delay wird komplett uebersprungen, wenn es aus ist und die Glaettung ausgelaufen ist; der Puffer wird dabei
  einmal geleert (keine alten Echos beim Wiedereinschalten).
- Reverb: `setParameters` nur noch einmal in `prepareToPlay`; der Reverb-Mix wird geglaettet (kein Knacksen beim
  Ein-/Ausschalten); bei Mix ~0 wird der Reverb uebersprungen und einmal geleert (kein "eingefrorener" Nachhall).
- CMake: `juce_recommended_lto_flags` und `juce_recommended_warning_flags` aktiviert.

### Messung (Release, Testprogramm mit dem echten Processor, 48 kHz / 256 Samples, 8 Stimmen, Moog-Ladder)
| Version | Heap-Allokationen pro Block | Rechenzeit, Delay+Reverb an | Delay+Reverb aus |
|---|---|---|---|
| 1.0.0 (Basis) | 52 | 0.054 ms | 0.044 ms |
| 1.0.02 | **0** | 0.035 ms | 0.020 ms |

Die Basis-Zahlen enthalten noch die dauerhaft aktive Distortion (Fix in 1.0.01), der Vergleich ist also nicht
rein "Optimierung". Engine-Ausgabe 1.0.01 == 1.0.02 bitgenau (80 zufaellige Faelle).

## 1.0.01  (Basis: 1.0.0 = Gemini v1.2.4)
Fehlerbehebungen aus dem Code-Review.

- **Distortion war praktisch immer an.** `distActive` war `distAmt > 0.001 || distOn`, der Default von
  `distAmt` ist 0.5 -> 121 von 128 Factory-Presets liefen dauerhaft mit 50 % Distortion. Jetzt gilt
  `distOn && distAmt > 0.001`. Neuer **ON-Schalter** ueber dem DIST-Knopf im FILTER-Panel (MIDI-Learn per
  Rechtsklick moeglich). Die 7 Presets mit `distOn = 1` klingen wie zuvor.
- **Distortion-Hochpass** war amount-abhaengig 80..500 Hz (bei 0.5 ca. 290 Hz -> Bass wurde duenn) und rechnete
  pro Sample ein `std::exp`. Jetzt fester 40-Hz-DC-Blocker, Koeffizient einmal in `prepareToPlay`.
- **Oktav-Regler (OCT) sind jetzt 0-basiert.** Vorher ging der Choice-Index 0..4 direkt als Oktavversatz in die
  Engine ("OCT 0" = Index 2 = +2 Oktaven). Der Processor uebergibt jetzt `index - 2`; die Tastatur-Transponierung
  steckt in `Engine::kKeyTransposeSemis = 12` (SynthEngine.h). **Klang aller Presets unveraendert**
  (per Test bitgenau bestaetigt). `kKeyTransposeSemis = 0` = Konzert-Tonhoehe (alles 1 Oktave tiefer).
- **Ping-Pong-Delay** klang wie ein normales Mono-Delay, weil das Eingangssignal in beide Puffer gleichzeitig
  geschrieben wurde. Jetzt geht im Modus PING-PONG die Mono-Summe nur in den linken Puffer, das Kreuz-Feedback
  laesst die Echos wirklich L -> R -> L springen.
- **Unison:** zufaellige Startphasen pro Stimme statt Phase 0 (vorher addierten sich die 8 Stimmen phasengleich:
  Lautstaerkesprung / Kammfilter, bei 0 ct Detune sogar exakt 8x lauter).
- **Arp UP/DOWN Pufferueberlauf:** `arpLadder[64]` reichte ab > 33 Eintraegen nicht (z. B. 9 Noten x 4 Oktaven).
  Jetzt `arpLadder[128]`.
- **Glide, erste Note:** Die allererste Note jeder Stimme glitt bei aktivem Glide von 440 Hz aus los (Startton
  abhaengig von Oktavlage). Jetzt startet sie direkt auf der Zieltonhoehe.

### Verifikation
- Engine (80 zufaellige Faelle, ohne Unison): Ausgabe **bitgenau identisch** zur Basis 1.0.0.
- Alle 128 Factory-Presets durch den echten Processor gespielt: keine NaN/Inf, Pegel <= 1.0.
- Ping-Pong-Test (Impuls, 160 ms): Echos wechseln jetzt L -> R -> L; in 1.0.0 war L == R in jedem Fenster.
- Gebaut (Release, MSVC, VST3 + Standalone): 0 Warnungen in den Projektdateien.
- Nur per Code-Analyse belegt, nicht per Absturz reproduziert: Arp-Pufferueberlauf (undefiniertes Verhalten).
- Nicht per Gehoer geprueft.

Nicht geaendert (bewusst): Sub-Oszillator bleibt ein Saegezahn; Unison-Detune haengt weiter an `osc2Tune`;
Unison-Lautstaerke unveraendert (Presets sind darauf abgestimmt).
