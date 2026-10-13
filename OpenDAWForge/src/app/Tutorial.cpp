#include "Tutorial.h"
#include <array>
namespace forge
{
namespace
{
struct Page
{
    const char *title, *body;
};
constexpr std::array<Page, 16> pages{
    {{"1. Dein erstes Projekt",
      "LS Factory startet mit einem leeren Projekt: keine Tracks, keine Instrumente und keine vorgegebenen "
      "Noten. Pattern 1 ist ein leerer Baustein.\n\nKlicke + TRACK und waehle Kick, Snare, Hi-hat, Synth "
      "oder Sampler. Jeder Aufruf legt einen Instrumentenkanal und eine leere Playlist-Spur an.\n\nNEW "
      "erstellt wieder ein leeres Projekt. Bei ungespeicherten Aenderungen fragt die App nach. OPEN laedt "
      "ein .odaw-Projekt. Das Demo findest du unter PROJECT > Open demonstration project."},
     {"2. Fenster und Arbeitsbereich",
      "Ziehe die Titelleiste der App, um das Hauptfenster zu verschieben. Der Quadrat-Knopf maximiert es. "
      "F11 schaltet Vollbild ein und aus; Escape verlaesst Vollbild.\n\nZiehe CHANNEL RACK oder die "
      "Titelleiste von Playlist, Piano roll und Mixer innerhalb des Arbeitsbereichs. Ziehe die untere rechte "
      "Ecke, um die Groesse anzupassen. Jede Ansicht behaelt ihre Position. LAYOUT setzt die "
      "Fensterpositionen zurueck.\n\nF5 Playlist, F6 Rack ein/aus, F7 Piano roll, F9 Mixer. Die "
      "Arbeitsfenster bleiben innerhalb der App."},
     {"3. Instrumente und Samples",
      "+ TRACK bietet die eingebauten Instrumente an: Kick, Snare und Hi-hat synthetisieren Drums, Synth "
      "erzeugt melodische Klaenge, Sampler spielt Audiodateien.\n\nWaehle den Kanal oben oder klicke seinen "
      "Namen im Rack. Ziehe eine WAV-, AIFF-, FLAC-, OGG- oder MP3-Datei auf den Sampler-Kanal im Rack. Die "
      "Datei wird im Hintergrund geladen. Speichere die Originaldatei dauerhaft: Projekte merken sich den "
      "Dateipfad.\n\nIm Mixer regelst du Lautstaerke, Panorama und die eingebauten Effekte. Fange leise an "
      "und beobachte die Pegel."},
     {"4. Einen Beat im Rack bauen",
      "Waehle Pattern 1 oben. Das Rack zeigt 16 Schritte pro Takt. Ein Klick aktiviert einen Schritt, ein "
      "weiterer Klick entfernt ihn.\n\nBeispiel: Kick auf 1, 5, 9, 13; Snare auf 5 und 13; Hi-hat auf jeden "
      "zweiten Schritt. Jeder Drum-Track muss zuerst mit + TRACK angelegt werden.\n\nDer runde Schalter "
      "links schaltet einen Kanal stumm. Die leuchtende Schrittmarkierung folgt der Wiedergabe. Rack und "
      "Piano roll bearbeiten dieselben Noten."},
     {"5. Transport, Tempo und Patterns",
      "Space oder PLAY startet/pausiert. STOP stoppt und setzt die Position zurueck. Das BPM-Feld bestimmt "
      "das Tempo. LOOP wiederholt den Loop-Bereich.\n\nSONG spielt die Playlist. Klicke SONG, um zu PATTERN "
      "zu wechseln und nur das ausgewaehlte Pattern zu hoeren. Ein leeres Arrangement ist im SONG-Modus "
      "stumm; teste deinen Beat zuerst im PATTERN-Modus.\n\nCtrl/Cmd+Shift+P erstellt ein neues leeres "
      "Pattern. Der Pattern-Waehler wechselt zwischen deinen Bausteinen. Speichern sichert alle Patterns "
      "gemeinsam."},
     {"6. Melodien im Piano roll",
      "Waehle einen Synth-Kanal und das gewuenschte Pattern. Oeffne Piano roll (F7). Klicke ins Notenraster, "
      "um eine Note zu setzen. Rechtsklick entfernt sie. Ein Klick auf eine Klaviertaste, eine neue oder "
      "ausgewaehlte Note spielt den Klang kurz vor, ohne den Song zu starten oder aufzunehmen.\n\nZiehe eine "
      "Note, um Tonhoehe und Startzeit zu "
      "aendern. Ziehe ihre rechte Kante fuer die Laenge. Die Balken unten steuern die Anschlagstaerke. Das "
      "Mausrad scrollt die Tonhoehen; Ctrl/Cmd+Mausrad zoomt die Zeitachse.\n\nCtrl/Cmd+Q quantisiert den "
      "ausgewaehlten Kanal auf Viertelschritte. Ctrl/Cmd+Pfeil hoch/runter transponiert um einen Halbton, "
      "mit Shift um eine Oktave."},
     {"7. Ein Arrangement aufbauen",
      "Oeffne Playlist (F5), waehle dein Pattern und klicke in eine vorhandene Spur, um einen Pattern-Clip "
      "einzusetzen. Ein Pattern kann mehrfach vorkommen.\n\nZiehe Clips entlang der Zeitachse. Ihre rechte "
      "Kante aendert die Dauer. Rechtsklick entfernt einen Clip. Ctrl/Cmd+Mausrad zoomt. + TRACK legt "
      "weitere leere Spuren samt Instrument an; Ctrl/Cmd+Shift+T legt nur eine Playlist-Spur an.\n\nSchalte "
      "zurueck auf SONG und druecke PLAY, um das gesamte Arrangement zu hoeren. Die Zahlen oben sind Takte; "
      "vier Beats ergeben einen Takt."},
     {"8. Mischen und Effekte",
      "Oeffne Mixer (F9). MASTER regelt die Gesamtausgabe, die weiteren Streifen gehoeren zu den "
      "Instrumentenkanaelen. Ziehe die Fader, um die Lautstaerke einzustellen.\n\nMute schaltet einen Kanal "
      "stumm, Solo isoliert ihn. Panorama verteilt ihn im Stereobild. Die Kanalregler umfassen Filter, "
      "Delay, Reverb, Kompression und Drive.\n\nHalte den Masterpegel unter der oberen Grenze. Exportiere "
      "einen kurzen Ausschnitt und hoere ihn vor einem langen Render an. Effekte und Noten bleiben beim "
      "Wechsel der Fenster erhalten."},
     {"9. VST3-Master-Effekte",
      "INSERTS oeffnet die Effektkette. Fuege eine lokale .vst3-Datei bzw. ein .vst3-Bundle hinzu. Der Scan "
      "laeuft in einem getrennten Prozess. Unterstuetzt sind bis zu acht Stereo-Effekte auf dem "
      "Master.\n\nOeffne den Plugin-Editor und aendere Parameter. Bypass umgeht einen Effekt. Speichere dein "
      "Projekt, um Plugin-Zustand und Parameter zu sichern. Dieselbe Masterkette wird beim WAV-Export "
      "verwendet.\n\nVST3-Instrumente, CLAP, Effekte pro Kanal und automatische Latenzkompensation sind noch "
      "nicht implementiert. Verwende dafuer keine angeblichen Bedienelemente; diese Funktionen sind nicht "
      "verfuegbar."},
     {"10. Audio und MIDI aufnehmen",
      "AUDIO oeffnet die Geraeteeinstellungen. Waehle Ausgabegeraet, Eingangsgeraet und passende Kanaele. "
      "Fuer Echtzeit-Audio benoetigst du ein funktionierendes Systemgeraet.\n\nMIDI REC nimmt MIDI-Noten in "
      "das ausgewaehlte Pattern und den ausgewaehlten Kanal auf. Waehle vor Beginn beides bewusst und "
      "aktiviere die Wiedergabe. Beende die Aufnahme mit MIDI REC.\n\nREC IN nimmt den konfigurierten "
      "Audioeingang als WAV auf; waehle den Speicherort. Klicke erneut, um abzuschliessen. Importiere diese "
      "WAV danach in einen Sampler. Ohne Eingangsgeraet ist eine Audioaufnahme nicht moeglich."},
     {"11. Speichern, Undo und Export",
      "SAVE bzw. Ctrl/Cmd+S speichert ein .odaw-Projekt. OPEN bzw. Ctrl/Cmd+O oeffnet es wieder. Die App "
      "speichert Noten, Clips, Mixerwerte und Plugin-Zustaende. Audiosamples und externe Plugins muessen auf "
      "deinem Rechner vorhanden bleiben.\n\nUNDO/REDO machen Bearbeitungen rueckgaengig bzw. stellen sie "
      "wieder her. Die automatische Wiederherstellung ersetzt regelmaessiges Speichern nicht.\n\nEXPORT WAV "
      "rendert das Arrangement als 24-Bit-Stereo-WAV bei 48 kHz. Der Offline-Export funktioniert auch ohne "
      "angeschlossenes Audiogeraet. Die Master-Plugins werden mitgerendert."},
     {"12. Abschluss und Kurzreferenz",
      "Uebung: Erstelle ein leeres Projekt, lege Kick/Snare/Hi-hat an, baue einen Beat, fuege einen Synth "
      "mit einer Piano-roll-Melodie hinzu und platziere dein Pattern in der Playlist. Mische leise, "
      "speichere .odaw und exportiere WAV.\n\nSpace Wiedergabe | F1 Tutorial | F5 Playlist | F6 Rack | F7 "
      "Piano roll | F9 Mixer | F11 Vollbild\nCtrl/Cmd+S Speichern | +O Oeffnen | +Z Undo | +Shift+Z Redo | "
      "+Shift+P Pattern | +Shift+T Playlist-Spur | +Shift+N Synth-Kanal\n\nDieses Tutorial ist jederzeit "
      "ueber HELP verfuegbar. Das Kapitelmenue springt direkt zum gewuenschten Thema."},
     {"13. Factory Library",
      "FACTORY oeffnet Presets, Samples, Loops und One-Shots. Suche und filtere nach Typ, Kategorie, Genre, "
      "Instrument, Stimmung, BPM und Tags. BPM kann auch eingetippt werden.\n\nPreview oder Doppelklick "
      "hoert einen Sound vor, ohne das Projekt zu aendern. Favourite markiert ihn; Recent zeigt verwendete "
      "Sounds. + Collection erstellt eigene Sammlungen.\n\nLoad to project bietet neue Spuren, "
      "Kanalersetzung oder das aktuelle Pattern. Ziehe Sounds direkt ins Rack oder in die Playlist. "
      "Audio-Loops sollten zur Projekt-BPM passen."},
     {"14. Presets und eigene Packs",
      "Volume, Pan, Filter, Delay, Reverb, Compression und Drive bearbeiten den ausgewaehlten Sound. Preview "
      "hoert Aenderungen; Save preset speichert eine eigene Kopie. Die Factory-Definition bleibt "
      "erhalten.\n\nPacks / downloads trennt kostenlose Factory-Inhalte, eigene Packs und optionales "
      "Premium. Eigene freie Packs werden per JSON-Manifest und Audiodateien importiert, geprueft und "
      "dauerhaft lokal gespeichert. Export/Import favourites uebertraegt Favoriten, Verlauf und "
      "Sammlungen.\n\nNoch kein Cloud-/Shop-Dienst ist verbunden. Premium-Kaeufe, Lizenzen und "
      "Synchronisierung sind daher deaktiviert; lokale Sounds funktionieren ohne Konto."},
     {"15. Loop Starter",
      "LOOP STARTER erzeugt acht Beats aus Genre, BPM, Grundton, Dur/Moll und Stimmung. Generate erzeugt "
      "eine neue Variation. Waehle eine Zeile und Replace selected loop, um nur diesen Teil zu "
      "ersetzen.\n\nPreview idea hoert die gesamte Idee, Preview selected nur den Sound. Das bestehende "
      "Projekt und seine Songposition bleiben beim Vorhoeren unveraendert.\n\nAdd to pattern fuegt ein "
      "kombiniertes Pattern hinzu. Add to arrangement legt separate Spuren und Patterns an. Vorhandene "
      "Inhalte bleiben erhalten. Adopt this BPM ist anfangs aus: aktiviere es bewusst, wenn du das "
      "Projekttempo aendern willst. Stoppe Aufnahmen vor strukturellem Laden."},
     {"16. Programm-Updates",
      "SETTINGS bietet Jetzt pruefen, die Update-Suche beim Start und optional automatische Installation. "
      "Automatische Installation ist anfangs aus. Bei einem neuen Release wird der Toolbar-Knopf zu "
      "UPDATE.\n\nDownloads werden geprueft. Speichere dein Projekt, stoppe Wiedergabe/Aufnahme und "
      "schliesse Library-/Plugin-Fenster vor der Installation. Windows/Linux warten auf die alte App, "
      "installieren und starten neu. Projekte, Library-Cache und Einstellungen bleiben "
      "erhalten.\n\nVersionen ohne Updater muessen einmal per neuem Installer aktualisiert werden. Auf macOS "
      "erfolgt die Installation derzeit manuell ueber die Release-Seite."}}};
} // namespace
TutorialPanel::TutorialPanel()
{
    setSize(700, 530);
    for (auto *c :
         std::initializer_list<juce::Component *>{&heading_, &progress_, &body_, &chapters_, &back_, &next_})
        addAndMakeVisible(c);
    heading_.setFont(juce::Font(juce::FontOptions(22)));
    body_.setMultiLine(true);
    body_.setReadOnly(true);
    body_.setScrollbarsShown(true);
    body_.setFont(juce::Font(juce::FontOptions(16)));
    body_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff263238));
    for (int i = 0; i < static_cast<int>(pages.size()); ++i)
        chapters_.addItem(pages[static_cast<size_t>(i)].title, i + 1);
    chapters_.onChange = [this] { showPage(chapters_.getSelectedId() - 1); };
    back_.onClick = [this] { showPage(page_ - 1); };
    next_.onClick = [this] { showPage(page_ + 1); };
    showPage(0);
}
void TutorialPanel::showPage(int page)
{
    page_ = juce::jlimit(0, static_cast<int>(pages.size()) - 1, page);
    const auto &p = pages[static_cast<size_t>(page_)];
    heading_.setText(p.title, juce::dontSendNotification);
    body_.setText(juce::String::fromUTF8(p.body), false);
    chapters_.setSelectedId(page_ + 1, juce::dontSendNotification);
    progress_.setText(juce::String(page_ + 1) + " / " + juce::String(pages.size()),
                      juce::dontSendNotification);
    back_.setEnabled(page_ > 0);
    next_.setEnabled(page_ + 1 < static_cast<int>(pages.size()));
}
void TutorialPanel::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(0xff303a40));
}
void TutorialPanel::resized()
{
    auto r = getLocalBounds().reduced(24);
    heading_.setBounds(r.removeFromTop(36));
    r.removeFromTop(10);
    chapters_.setBounds(r.removeFromTop(30));
    r.removeFromTop(16);
    auto bottom = r.removeFromBottom(34);
    back_.setBounds(bottom.removeFromLeft(110));
    next_.setBounds(bottom.removeFromRight(110));
    progress_.setBounds(bottom);
    r.removeFromBottom(16);
    body_.setBounds(r);
}
} // namespace forge
