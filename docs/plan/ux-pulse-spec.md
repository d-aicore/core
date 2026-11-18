> Translated from `ux-pulse.pt.md`, a study of commit e7968c8. Line numbers refer to e7968c8 and have drifted since; they are kept as written and not re-anchored to current HEAD.
>
> Origin-mark legend (1:1, none upgraded or downgraded): **[C]** = read in Pulse's code · **[D]** = documented only in Pulse's `FEATURE_STATUS.md`/README, with no corresponding code · **[V]** = seen in the screenshot · **[NV]** = not verified. The owner's quotes stay verbatim in Portuguese.

> Delivery of the subagent `mapa-ui` (DSH session 2ff54f03, 2026-11-18), extracted from the log without editing. Read-only study of commit e7968c8. In Portuguese; to be translated into English before entering the repo.

# Pulse Studio UI study and specification for Core

Study-only delivery: nothing was written in `D:\CORE\CORE`, no write git command, Pulse's `.exe` was not run and nothing in Core was compiled or tested.

**Origin marks**
- **[C]** read in Pulse's code.
- **[D]** documented only in Pulse's `FEATURE_STATUS.md`/README, with no corresponding code.
- **[V]** seen in the screenshot.
- **[NV]** not verified.

**Abbreviations (files read)**
- Core: PE = [PatternEditor.cpp](<CORE/app/desktop/src/PatternEditor.cpp>), PEh = [PatternEditor.h](<CORE/app/desktop/src/PatternEditor.h>), PW = [PatternsWindow.cpp](<CORE/app/desktop/src/PatternsWindow.cpp>), MC = [MainComponent.cpp](<CORE/app/desktop/src/MainComponent.cpp>), MCh = [MainComponent.h](<CORE/app/desktop/src/MainComponent.h>), Th = [Theme.h](<CORE/app/desktop/src/Theme.h>), KM = [Keymap.cpp](<CORE/app/desktop/src/Keymap.cpp>), GK = [GlobalKeys.cpp](<CORE/app/desktop/src/GlobalKeys.cpp>), rack = [pattern_rack.hpp](<CORE/app/domain/src/pattern_rack.hpp>), pcore = [pcore.hpp](<CORE/app/domain/src/pcore.hpp>).
- Pulse: Ed = [Editor.cpp](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp>), Ws = [Workspace.cpp](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Workspace.cpp>), SUI = [StudioUI.cpp](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/StudioUI.cpp>), St = [Studio.cpp](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Studio.cpp>).
- The lines are those of the reading (blank lines count): PE has 2128, MC 7068, PW 1207.

**Effort** (my estimate): small ≤ 1 day, medium 2–4 days, large ≥ 1 week.

## 0. Three warnings before anything

1. **I could not see the screenshot.** This session's model does not accept images. I extracted the text and positions with Windows OCR; colors, shapes and drawn notes remained unseen. It is worth someone with sight confirming point 1.
2. **Pulse's code in the repository is older than the program the owner used.** The screenshot says «Pulse Studio 0.112.0» and `FEATURE_STATUS.md` says 1.12.0, but `CMakeLists.txt` says 0.76.0 and `CHANGELOG.md` ends at 0.95.0. The code has features up to about 0.95 and lacks those of 0.96–0.112. Everything from those versions is marked [D].
3. **Pulse has no separate system windows.** It is a single window with sub-windows painted inside it, which do not go out to another monitor. This changes the architecture decision (point 8).

## 1. The owner's screenshot

A 1440×967 file. By OCR [V]:

- **Title and top:** «Pulse Studio 0.112.0», project «Midnight motion», tempo 118.000, buttons LOOP, CLICK, KEYS, UNDO, SAVE. Menu row (read «ADD», «PATTERNS», «HELP»). Pattern selector «Intro». View buttons «Playlist», «Channel rack», «Piano roll», «NOTES», «REC setup».
- **Left:** «Browser - All», «Local files», «Folder», tree with «Favorite», «Project», «No imported samples», «Drum rack / acoustic kit», Analog kick, Noise snare, Closed hat, Clap, Open hat, «Instruments», Mono bass, Warm pad, Pluck lead, Sampler, Pulse Analog, Pulse FM. At the bottom «Import», «Import MIDI», «Export WAV».
- **«Playlist - Arrangement» window:** bar with Delete, Select, «Snap: relative», Split, Auto, Markers, Fades, Fill pattern. Pattern list (Main, Intro, Variation). Tracks «Track 1…7». Ruler 2–9. One «groove» clip.
- **«Channel rack» window** floating over the Playlist (title at x≈470, y≈532; close at x≈1264): pattern «Intro», «Swing», channels 2 Snare, 3 Closed hat, 4 Clap, 5 Open hat, 6 Bass, 7 Chords, 8 Lead, and «32» (steps).
- No «Piano roll - …» or «Mixer - …» title appeared; they were probably not open [NV].

Reading: what it shows is the FL Studio-style flow, with several overlapping, movable windows inside the application and the channel rack over the arrangement.

## 2. License

- [LICENSE](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/LICENSE>) is the GNU AGPL v3 text.
- The README and `THIRD_PARTY_NOTICES.md` say «AGPL-3.0-only»; every `src/` file I read has the SPDX header `AGPL-3.0-only`.
- JUCE 8.0.16-security used under the AGPLv3 option, without a commercial license.
- The README claims «complete corresponding source», but the code is versions behind the binary (warning 2). For us it only matters that it describes accurately up to about 0.95.
- Pulse does not host plugins (items 73–76 «Planned») and has no hardware MIDI.
- Rule kept: nothing below transcribes Pulse code.

## 3. Inventory of Pulse's UI [C]

**Global model.** One native window with a single hand-painted «quadro» (frame), scalable 75–200 %. Fixed parts: header, Browser docked on the left (width 160–340 px, Alt+F8) and status bar. The rest is a work area with 15 sub-windows.

**Behavior of each sub-window**
- Dragging the title moves it; it snaps at 8 px from the edges and the other windows; Alt turns snapping off ([Ed:147](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp#L147>)).
- The bottom-right corner resizes, with per-window minimums ([Ws:9](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Workspace.cpp#L9>)).
- Double-clicking the title, or the «[]» button, maximizes to the work area and restores.
- «x» or Esc hides it. The «>» in the title opens a menu (maximize, close, «Arrange windows» and the window's own entries).
- Any click brings the window to the front. There is no «sempre por cima» (always on top), docking or tabs.
- They do not leave the main window. `FEATURE_STATUS` confirms: «docking and independent editor windows across monitors pending».

**Reopening.** Buttons in the header; F5 Playlist, F6 Channel rack, F7 Piano roll, F9 Mixer; VIEW menu with check marks; Tab cycles; F12 hides everything; Ctrl+Shift+H restores the layout and brings the main window back to the screen.

**Persistence.** The layout (rectangles, visible, maximized, order, Browser) is saved in a global JSON beside the executable on every move, resize, open or close. It is not per project. On open it is rescaled to the current size. There are 3 factory layouts (Composing = rack + piano; Arranging = playlist + mixer; Mixing = maximized mixer) and up to 32 personal ones.

| Pulse window | What it does | Relations |
|---|---|---|
| Header (fixed) | Menus, master volume, PAT/SONG, play/stop/rec, tempo, position, CPU, LOOP/CLICK/KEYS/UNDO/SAVE, pattern selector, view buttons | The active pattern commands the rack, the piano roll and what the Playlist paints |
| Browser (fixed) | Factory sounds, instruments, project samples, local folders; a click assigns to the channel, dragging drops on the rack or the mixer | — |
| Playlist - Arrangement | Tracks = channels; pattern, audio and automation clips; Draw/Paint/Delete/Select tools; markers | Double-clicking a pattern clip opens that channel's Piano roll; on an automation clip it opens the automation editor |
| Channel rack | See point 4C | Name → channel settings; thumbnail → Piano roll |
| Piano roll - ‹channel› | See points 4A and 4B | Follows the selected channel; double-clicking a note opens the channel settings with the note's properties |
| Mixer - ‹channel› | One strip per channel + Master, own-effect slots, output, sends | Follows the selected channel |
| Channel settings / Synthesizer | Parameters of the own instrument and of the note (velocity, probability, repeats, condition) | — |
| Piano roll - Note tools | Quantize, Humanize, Euclidean, Arpeggiator, Groove, Fit to scale, Chord, Progression; preview, Apply/Cancel | Opens together with the Piano roll |
| Automation, Mixer effects, Waveform, Project audio files, Templates, Tempo and recording, Export, Markers | Auxiliary editors and managers | — |

## 4. The six tables

In all of them, the design is made with what Core already has; where a component is missing, I say so.

### 4A. Mouse in the piano roll

Pulse has four tools (Draw, Paint, Delete, Select), shared between Piano roll and Playlist. Core has three (Draw, Select, Erase) at [PE:187](<CORE/app/desktop/src/PatternEditor.cpp#L187>).

| # | Pulse does [C] | Core today | Proposal | Effort / risk | Files |
|---|---|---|---|---|---|
| 1 | Clicking empty space creates a note with the **last used length**; without releasing, dragging sets the length ([Ed:100](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp#L100>)) | Creates a note with the snap length, no drag ([PE:927](<CORE/app/desktop/src/PatternEditor.cpp#L927>), [PE:1035](<CORE/app/desktop/src/PatternEditor.cpp#L1035>)) | Adopt: remember the last length and click-and-drag to size, in a single undo gesture | Small / low | PE, PEh |
| 2 | **Audition**: on creating or grabbing a note the pitch is heard until release; the keys on the left play ([Ed:81](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp#L81>)) | No audition on click; the keyboard is drawing only, clicks outside the grid are ignored ([PE:1008](<CORE/app/desktop/src/PatternEditor.cpp#L1008>)); only the «Play the selected notes» menu ([PE:1486](<CORE/app/desktop/src/PatternEditor.cpp#L1486>)) | Adopt both, through the track's plugin, reusing the live-notes path ([MCh:123](<CORE/app/desktop/src/MainComponent.h#L123>)). Option in Settings > Piano roll | Medium / medium (stuck notes if the note-off fails) | PE, MC, Settings.cpp |
| 3 | Dragging the body moves the whole selection; the last ~7 px on the right resize only that note ([Ed:158](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp#L158>)) | Same, with 6 px ([PE:717](<CORE/app/desktop/src/PatternEditor.cpp#L717>), [PE:1099](<CORE/app/desktop/src/PatternEditor.cpp#L1099>)) | Keep. Add resizing the whole selection (Pulse does not do it) | Small / low | PE |
| 4 | **Shift+drag** clones the selection and drags the copies ([Ed:96](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp#L96>)) | Shift = move off the grid ([PE:1096](<CORE/app/desktop/src/PatternEditor.cpp#L1096>)); duplicate only via Ctrl+D ([PE:962](<CORE/app/desktop/src/PatternEditor.cpp#L962>)) | Adopt. **Changes a Core habit**: Shift stops being «fora da grelha» (off the grid) | Small / medium | PE |
| 5 | **Alt** bypasses the snap when creating, moving, resizing and painting; Backspace toggles it; the menu has «None» ([Ws:48](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Workspace.cpp#L48>)) | Only Shift when moving; there is no «sem snap» (no snap); grids 1/128…1/4 ([PE:29](<CORE/app/desktop/src/PatternEditor.cpp#L29>)) | Alt = free in all gestures; add «Off» to the snap and an action in the Keymap | Small / low | PE, KM |
| 6 | Right button (or Delete) erases and keeps erasing while dragging ([Ed:93](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp#L93>)) | Same ([PE:1013](<CORE/app/desktop/src/PatternEditor.cpp#L1013>)) | Nothing to do | — | — |
| 7 | **Ctrl+drag in empty space** lassos in any tool; Shift adds; Ctrl+click toggles the note ([Ed:101](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp#L101>)) | Lasso only in the Select tool ([PE:1019](<CORE/app/desktop/src/PatternEditor.cpp#L1019>)); Ctrl+click already toggles ([PE:1040](<CORE/app/desktop/src/PatternEditor.cpp#L1040>)) | Adopt the Ctrl lasso in any tool | Small / low | PE |
| 8 | **Paint**: dragging seeds one note per cell ([Ed:141](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp#L141>)) | Does not exist for notes (it exists for drum steps) | Fourth tool. New icon to draw in the style of `drawIcon` ([Th:84](<CORE/app/desktop/src/Theme.h#L84>)) | Small / low | PE, Theme.cpp |
| 9 | Double-clicking a note opens the properties | None, on purpose ([PE:1012](<CORE/app/desktop/src/PatternEditor.cpp#L1012>)) | Do not adopt as is: the Inspector already shows the selection. Optional: open the Inspector if it is collapsed | Small / low | PE, InspectorPanel.cpp |
| 10 | Velocity lane: dragging changes the nearest note; Alt+wheel over the note ±5 % ([Ed:109](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp#L109>), [Ed:194](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp#L194>)) | Lane better than Pulse's (respects the selection, [PE:2041](<CORE/app/desktop/src/PatternEditor.cpp#L2041>)); the wheel **without modifier** over a note changes the velocity ([PE:2104](<CORE/app/desktop/src/PatternEditor.cpp#L2104>)) | Keep the lane. Move wheel-velocity to Alt+wheel; today scrolling with the mouse over a note changes it unintentionally | Small / low | PE |
| 11 | Wheel: plain = 3 semitones; Shift = 1 beat horizontally; Ctrl = horizontal zoom **not anchored** to the pointer ([Ed:193](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp#L193>)). [D] 0.101.1: Ctrl+Shift+wheel = anchored vertical zoom | Plain = 1 semitone; Ctrl = zoom anchored to the pointer; Alt = key height; Shift = horizontal ([PE:2081](<CORE/app/desktop/src/PatternEditor.cpp#L2081>)) | Core is already better. Scroll step of 3 semitones; vertical zoom on Ctrl+Shift+wheel (frees Alt for item 10) | Small / low | PE |
| 12 | Cursors: one per tool, hand over the body, ↔ at the edge; they change on pressing Ctrl or on the right button ([Ws:60](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Workspace.cpp#L60>)) | Fixed cursor per tool ([PE:194](<CORE/app/desktop/src/PatternEditor.cpp#L194>)) | Adopt the behavior. **New component**: tool cursors, to draw with Core's icons | Small / low | PE, Theme.cpp |
| 13 | Usage hint on hover; ruler: click positions | Hints and ruler already exist ([PE:993](<CORE/app/desktop/src/PatternEditor.cpp#L993>)); Core has ghost notes and alignment guides that Pulse does not have ([PE:242](<CORE/app/desktop/src/PatternEditor.cpp#L242>)) | Keep Core's | — | — |
| 14 | Tool keys P/B/D/E (with the piano keyboard off), Ctrl+A, Ctrl+B duplicate, Ctrl+↑/↓ octave | Ctrl+A, Ctrl+D, Delete ([PE:2120](<CORE/app/desktop/src/PatternEditor.cpp#L2120>)); letters without modifier play notes ([MC:5948](<CORE/app/desktop/src/MainComponent.cpp#L5948>)) | Keep Ctrl+D. New actions in the Keymap for the tools, without a default key that clashes with the piano keyboard; add Ctrl+↑/↓ | Small / low | KM, PE |

### 4B. «Piano que dá para ver tudo» (a piano where you can see everything)

| # | Pulse does | Core today | Proposal (in order of value) | Effort / risk | Files |
|---|---|---|---|---|---|
| a | **Ghost notes** of all the other non-percussion channels of the same pattern: behind, neutral color at ~25 %, visible only, not clickable ([SUI:173](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/StudioUI.cpp#L173>)) [C] | Only the notes of the pattern being edited ([PE:849](<CORE/app/desktop/src/PatternEditor.cpp#L849>)) | Ghosts of the other tracks, each in its track's color at low opacity (identity rule, [Th:92](<CORE/app/desktop/src/Theme.h#L92>)), with an on/off button. In Core patterns are per track, so «o mesmo padrão» (the same pattern) becomes the same rack slot ([rack:23](<CORE/app/domain/src/pattern_rack.hpp#L23>)) or, without a slot, what plays in that stretch of the song | Medium / medium (defining «o que toca junto» (what plays together); redraw with many notes) | PE, PEh, MC, pattern_rack |
| b | Time: opens with 8 beats, zooms to the whole pattern (max. 64 beats). Height: **24 fixed keys** [C]; [D] 0.101.1: 12–128 keys, «Keys −/+», «Fit keys». On open it centers on the notes. No mini-map | Time: already shows the whole pattern by default ([PE:626](<CORE/app/desktop/src/PatternEditor.cpp#L626>)). Height: 13–61 keys ([PE:389](<CORE/app/desktop/src/PatternEditor.cpp#L389>)); there is no «caber tudo» (fit all) nor centering on open (only `revealSelection`, [PE:945](<CORE/app/desktop/src/PatternEditor.cpp#L945>)) | «Caber tudo» (Fit all) in time and height (button + keyboard action + double-click on the ruler); center on the notes when changing track or pattern; limit up to 128 keys | Small / low | PE, KM |
| c | The sub-window maximizes to the work area; it does not go to another monitor [C] | The editor is stuck to the main window's bottom panel ([MCh:409](<CORE/app/desktop/src/MainComponent.h#L409>), [MC:5489](<CORE/app/desktop/src/MainComponent.cpp#L5489>)); the rack's «Piano» button only brings the main window to the front ([MC:2555](<CORE/app/desktop/src/MainComponent.cpp#L2555>)) | Detachable piano roll into its own window, resizable and maximizable (point 8) | Large / medium | MC, MCh, new file |
| d | 48 px «Note velocity» lane | Already exists ([PE:893](<CORE/app/desktop/src/PatternEditor.cpp#L893>)) | Only make the height draggable | Small / low | PE |
| e | Horizontal position bar at the bottom, not interactive | Not in the piano roll; the arrangement has `TimeBar` ([MC:1737](<CORE/app/desktop/src/MainComponent.cpp#L1737>)) | Reuse `TimeBar` as a draggable overview bar | Small / low | PE, MC |

### 4C. Channel rack «expansível» (expandable)

**What Pulse does** [C] ([SUI:115](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/StudioUI.cpp#L115>), [Ed:87](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Editor.cpp#L87>)):
- **Row** = one channel: mute light (right click = solo), pan, volume, number (selects; double-click renames), name (opens settings; right click = menu with Piano roll, clone, delete, rename).
- **Percussion or sample channels**: 1/16 steps in groups of 4. Click turns on, clicking an on step turns it off, dragging paints or erases, Shift+click cycles 1–4 repeats, Alt+right click opens the repeat condition, wheel over the step ±5 % velocity.
- **Melodic channels**: thumbnail of the notes; click opens the Piano roll.
- Footer with «+» (new channel) and «N steps» (pattern length, 16–256 in multiples of 4). «Swing» at the top.
- 16 channels in the code; [D] 64 since 0.96.

**What «expandir» (expand) is there:** only resizing or maximizing the sub-window. More height shows more rows (the wheel scrolls the rest). More width widens the steps; the pattern always fits whole, without horizontal scroll. The number of steps follows the pattern length (4–256).

**What Pulse does not do:** a channel does not open in detail; there are no folding groups in the rack (the 0.110 folders [D] are the Playlist's); I found no channel reordering; there is no per-channel mixer-track selector.

| # | Pulse | Core today («Core · Patterns» window) | Proposal | Effort / risk | Files |
|---|---|---|---|---|---|
| 1 | Movable, resizable sub-window | Already its own OS window, resizable (min. 1140×640), hides on close ([PW:1183](<CORE/app/desktop/src/PatternsWindow.cpp#L1183>)); position not saved; it seems not to apply the main window's «UI size» [NV at runtime] | Save position and size; apply the same scale; lower the minimum, allowing the template list to collapse | Small / low | PW, Main.cpp, MC |
| 2 | Whole pattern visible, 4–256 steps | **One bar at a time** (BAR 1…8 tabs, [PW:616](<CORE/app/desktop/src/PatternsWindow.cpp#L616>)), grid 1/8–1/32, 1/2/4/8 bars | **Width = more bars**: «todos os compassos» (all bars) option, the steps shrink to a minimum and then there is horizontal scroll; the bar tabs stay as a shortcut | Medium / medium (touches the geometry of the rows and of the velocity) | PW, PatternsWindow.h |
| 3 | More height = more rows | 46 px rows in a scrolling area ([PW:1105](<CORE/app/desktop/src/PatternsWindow.cpp#L1105>)) | **Height = more channels**: already works; add a compact mode | Small / low | PW |
| 4 | Does not exist | Melodic row = thumbnail of the bar; percussion = a single row per track, on the most used note ([rack:48](<CORE/app/domain/src/pattern_rack.hpp#L48>)) | **Per channel = open detail** (beyond Pulse): arrow at the head of the row (the «right»/«down» icons already exist, [Th:82](<CORE/app/desktop/src/Theme.h#L82>)). A percussion track opens into one sub-row per used note; a melodic track grows into an editable mini piano roll | Large / medium | PW, pattern_rack, PE |
| 5 | Name opens the own instrument | Name + plugin name; «Piano» and «FX» buttons; right menu with P-Core shapes, key and scale ([PW:841](<CORE/app/desktop/src/PatternsWindow.cpp#L841>)) | Clicking the plugin name opens that instance's editor ([MC:2204](<CORE/app/desktop/src/MainComponent.cpp#L2204>)); a track without a plugin offers the catalogue. Fulfills PROD-08 (open the exact instance) | Small / low | PW, MC |
| 6 | Mute, solo, pan, volume | M, S, volume, meter ([PW:53](<CORE/app/desktop/src/PatternsWindow.cpp#L53>)); no pan | Add pan with the rotary already used in the row | Small / low | PW |
| 7 | Steps: Shift+click repeats, wheel = velocity, condition | Click and drag paint ([PW:225](<CORE/app/desktop/src/PatternsWindow.cpp#L225>)); velocity in the lane below | Wheel over the step = velocity. Repeats: map to the notes' `roll` [NV whether it fits]. Conditions: do not exist in Core, stay out | Small–medium / low | PW |
| 8 | «+» creates a channel with the own instrument; clone and delete in the menu | «+ Track» calls the main window's menu ([MC:2564](<CORE/app/desktop/src/MainComponent.cpp#L2564>)) | «+» creates a track with a third-party plugin; clone and delete track in the row's menu | Small / low | PW, MC |
| 9 | Selecting a channel makes Piano roll and Mixer follow | Selecting a row does not change the piano roll; only «Piano» or double-click | Selecting a row → the piano roll (wherever it is) follows | Small / low | PW, MC |
| 10 | — | Slots, templates by style, Send to Arrange, playing the pattern alone | **Must survive intact**; it is the Pattern Maker (PROD-02, PROD-03) | — | — |

**How the design changes versus Pulse.** Each channel is a track with a third-party plugin: there is no synthesizer panel, «+» opens the plugin catalogue and the name opens the plugin's window (which belongs to the engine process). A drum track is a single plugin with several sounds per key; so per-channel expansion (item 4) makes more sense in Core than in Pulse. When expanding, use the notes present in the pattern and not the name tables of the old own instruments ([PE:44](<CORE/app/desktop/src/PatternEditor.cpp#L44>)), which are legacy.

### 4D. Separate, movable windows

| # | Pulse [C] | Core today | Proposal | Effort / risk | Files |
|---|---|---|---|---|---|
| 1 | 15 sub-windows inside a single window | Main window with fixed regions ([MC:5511](<CORE/app/desktop/src/MainComponent.cpp#L5511>)); in the bottom panel, piano roll, audio, automation, Stage and Mixer alternate ([MC:2598](<CORE/app/desktop/src/MainComponent.cpp#L2598>)) | See point 8: each editor detaches into an OS window and docks back | Large / medium | MC, MCh, new |
| 2 | Move, resize, maximize, close, reopen | Only Patterns, Settings and the plugin editors are their own windows | Same for piano roll and Mixer, then automation | Medium per panel | MC |
| 3 | No «sempre por cima» (always on top) | Plugin editors **are** always on top ([juce_host.cpp:220](<CORE/engine/src/plugins/juce_host.cpp#L220>)) | Normal Core windows; keep the plugins on top | — | — |
| 4 | Layout saved globally; restore with Ctrl+Shift+H | Saved: Library width, editor and mixer heights, visible panels ([MC:2042](<CORE/app/desktop/src/MainComponent.cpp#L2042>)). `Document::ui` exists ([document.hpp:249](<CORE/app/domain/src/document.hpp#L249>)) | Position, size and state of each window in the user settings (like Pulse), with validation against the monitors and a «Trazer janelas para o ecrã» (bring windows back to the screen) command | Medium / low | MC, new |
| 5 | F5/F6/F7/F9, Tab, F12, VIEW menu with check marks | View has Library, Inspector, Mixer; Patterns is in the Plugins menu ([MC:5198](<CORE/app/desktop/src/MainComponent.cpp#L5198>)); no F key in use ([KM:74](<CORE/app/desktop/src/Keymap.cpp#L74>)) | View lists all windows with check marks; F keys as a remappable default | Small / low | MC, KM |
| 6 | 3 factory layouts + 32 personal | None | After everything else | Medium / low | MC, new |
| 7 | A single component receives all keys | Only Space is global ([GK:42](<CORE/app/desktop/src/GlobalKeys.cpp#L42>)); the other shortcuts live in the main window ([MC:5895](<CORE/app/desktop/src/MainComponent.cpp#L5895>)); the Patterns window reimplements its own ([PW:1020](<CORE/app/desktop/src/PatternsWindow.cpp#L1020>)) | **Prerequisite**: a common command router, so undo, save, tools and transport work in any Core window | Medium / medium | GK, GlobalKeys.h, KM, MC, PW |
| 8 | Multiple monitors: only the main window | OS windows go to any monitor | Advantage of the proposed model over Pulse's | — | — |

### 4E. Scales

| # | Pulse [C] | Core today | Proposal | Effort / risk | Files |
|---|---|---|---|---|---|
| 1 | «Scale» button: click toggles; right click opens a dialog with root (12) and scale (13) ([HarmonyUI.cpp:12](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/HarmonyUI.cpp#L12>)) | No guide in the piano roll. There is key and scale **per slot** only for the P-Core shapes ([PW:856](<CORE/app/desktop/src/PatternsWindow.cpp#L856>)) and `songRoot` ([edit.hpp:182](<CORE/app/domain/src/edit.hpp#L182>)) | «Scale» button in the piano roll bar + root and scale menu | Small / low | PE |
| 2 | 13 scales: major, natural minor, dorian, phrygian, lydian, mixolydian, locrian, harmonic minor, melodic minor, major and minor pentatonic, minor blues, chromatic | P-Core's scale catalogue in `pcore/styles.json` ([pcore:22](<CORE/app/domain/src/pcore.hpp#L22>)) | Use the P-Core catalogue as the single source, completing whatever is missing of the 13 | Small / low | pcore/styles.json, pcore |
| 3 | On screen: scale lines with a faint highlight, the root's stronger; a mark on the root key and its name; nothing hides or folds ([SUI:161](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/StudioUI.cpp#L161>)) | Lines only distinguish black/white key ([PE:758](<CORE/app/desktop/src/PatternEditor.cpp#L758>)) | Highlight with Core's accent at low opacity, or the track's color; a 3 px mark on the root. **New drawing** within the palette | Small / low | PE |
| 4 | **Does not trap the mouse**: creating and moving ignore the scale | — | Guide as in Pulse; «prender à escala» (snap to scale) is question 4 | Small–medium / low | PE |
| 5 | The guide is a global preference, not the project's | — | In Core: per song, with the default key coming from the slot or from `songRoot`, so there are not two places for the same thing | Small / medium (migration: default value «desligado» (off)) | document, pattern_rack, PE |
| 6 | Tools with preview and Apply/Cancel: Fit to scale (nearest / down / up), Chord builder (13 qualities + scale triad and seventh, inversions, 1–3 octaves), Progression (1–16 degrees, voice leading) ([Harmony.cpp:22](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Harmony.cpp#L22>)) | Edit ▾ > Notes: transpose; add octave, fifth, minor or major chord ([PE:1511](<CORE/app/desktop/src/PatternEditor.cpp#L1511>)). I found no fit-to-scale | «Ajustar à escala» (Fit to scale) first; chords and progression as Tools (4F), in the `ToolDock` panel that already exists ([ToolDock.h](<CORE/app/desktop/src/ToolDock.h>)) | Medium / low | new domain header + tests, PE |

### 4F. «Generate»

| # | Pulse | Core today | Proposal | Effort / risk | Files |
|---|---|---|---|---|---|
| 1 | **Algorithmic and local**; no model or service ([St:265](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Studio.cpp#L265>)) [C]. `FEATURE_STATUS` describes it as «rule-generated notes; richer controls and models pending» | Everything by rules and seeds | **Fits in Tools; it is not AI.** Nothing leaves the binary | — | — |
| 2 | Percussion channel: opens the **Euclidean** rhythm (steps 1–64, pulses, rotation, repeats, note, interval, duration, velocity with seed variation, replace that note or only fill empties), with preview and Apply ([Rhythm.cpp:7](<C:/Users/vk_do/AppData/Local/Temp/pulse-ref/src/Rhythm.cpp#L7>)) | No Euclidean. There is «More/Fewer notes», «Notes per beat» ([PE:1541](<CORE/app/desktop/src/PatternEditor.cpp#L1541>)), P-Core shapes per row, STUTTER, FOLLOW, Build-up | Euclidean as a Tool in `ToolDock`, with preview and Apply, one Undo | Medium / low | new in the domain + tests, PE, BuildupPanel as a model |
| 3 | «pad» channel: **replaces all notes** with four fixed minor triads. Others: replaces with 16 random eighth notes in a fixed natural minor. Immediate, no preview | «Randomize»: shuffle pitches, random pitches within the notes' range ([PE:1621](<CORE/app/desktop/src/PatternEditor.cpp#L1621>)); P-Core shapes and «another take» ([PW:864](<CORE/app/desktop/src/PatternsWindow.cpp#L864>)) | Do not copy: it is poor and destructive. A «Generate» button that opens Core's generations (P-Core shape for this track, another take, Euclidean, chords, progression) | Small / low | PE, PW |
| 4 | **Ignores the scale guide** and the selection; erases what was there | The P-Core shapes respect the slot's key and scale | In Core it always respects the active scale and, with a selection, only the selected range | included | — |
| 5 | Undoable (one Undo point) | One Undo per tool | Keep | — | — |
| 6 | Arpeggiator (6 orders, rhythm, duration, octaves, transposition, seed), also with preview | P-Core has «arp» parts; there is no arpeggiator over selected notes | Later, as a Tool | Medium / low | domain, PE |

## 5. Pulse → Core map, window by window

| Pulse | Core today | Missing in Core | Must survive / does not come |
|---|---|---|---|
| Header | Top bar, which is also the title bar ([Main.cpp:46](<CORE/app/desktop/src/Main.cpp#L46>)) | Window toggles | Survives as is |
| Browser | `LibraryPanel` + Tools panel (`PluginCards.h`) | Dragging to the rack rows | **Do not come**: factory sounds, own instruments and sampler (PROD-04, PROD-05) |
| Playlist | Ruler + tracks + clips, in the main window | Paint/Delete tools, Shift+drag copies (outside the first slices) | Ableton-style clips, selection box, sections, audio |
| Channel rack | `PatternsPanel` / `PatternsWindow` | 4C | Slots, templates, catalogue, kick/bass/cymbal alignment, Send to Arrange |
| Piano roll | `PatternEditor`, docked | 4A, 4B, own window | Ghost notes, guides, Edit ▾, FOLLOW, STUTTER, Build-up, `ToolDock` |
| Mixer | `MixerPanel` in the bottom panel ([MC:2207](<CORE/app/desktop/src/MainComponent.cpp#L2207>)) | Detach | External plugin chain, sends, buses. The own EQ, compressor or reverb slots **do not come** |
| Channel settings / Synthesizer | Does not have it, and should not | — | **Does not come.** Equivalent: open the plugin's editor |
| Note tools | Edit ▾ + `ToolDock` | Preview with Apply/Cancel | Comes as behavior, inside Tools |
| Automation editor | `AutomationEditor` in the bottom panel | Detach (late) | Survives |
| Waveform editor | `AudioEditor` | — | Out of scope |
| Templates / Project audio / Export / Recording / Markers | Library (Templates), File > Export, sections | — | No change requested |
| Local MCP | Bridge to AICORE outside the binary (D91) | — | **Does not come** |

Core things that Pulse does not have and that stay: plugin hosting and its windows, engine state and recovery, undo by gestures, `Inspector`, Stage Core, Tools and role map, pattern catalogue, remappable Keymap, self-test.

## 6. What stays from Core (appearance)

From Pulse comes only behavior. The slate palette, the gradient buttons and the uppercase menus do not come in.

- **Palette:** five blue-grays ([Th:14](<CORE/app/desktop/src/Theme.h#L14>)), charcoal surfaces ([Th:36](<CORE/app/desktop/src/Theme.h#L36>)), text ([Th:52](<CORE/app/desktop/src/Theme.h#L52>)), accent ([Th:64](<CORE/app/desktop/src/Theme.h#L64>)), warm start line ([Th:68](<CORE/app/desktop/src/Theme.h#L68>)), states ([Th:75](<CORE/app/desktop/src/Theme.h#L75>)).
- **Shape and type:** 2 px corner on everything ([Th:22](<CORE/app/desktop/src/Theme.h#L22>)); five font sizes ([Th:27](<CORE/app/desktop/src/Theme.h#L27>)) in Segoe UI; spacings ([Th:33](<CORE/app/desktop/src/Theme.h#L33>)).
- **Buttons:** solid block without outline, dark at rest and light on hover, press or on ([Th:46](<CORE/app/desktop/src/Theme.h#L46>)); line icons per property ([Th:80](<CORE/app/desktop/src/Theme.h#L80>)).
- **Ready controls:** `BarSlider` ([Th:187](<CORE/app/desktop/src/Theme.h#L187>)), `InfoDot` ([Th:207](<CORE/app/desktop/src/Theme.h#L207>)), `OnCircle` ([Th:229](<CORE/app/desktop/src/Theme.h#L229>)), `paintCard` ([Th:112](<CORE/app/desktop/src/Theme.h#L112>)), `paintToneSection` and rule 11's neutral scale ([Th:121](<CORE/app/desktop/src/Theme.h#L121>)), track identity color ([Th:92](<CORE/app/desktop/src/Theme.h#L92>)).
- The LookAndFeel is global ([Main.cpp:85](<CORE/app/desktop/src/Main.cpp#L85>)): any new window inherits it without work.
- **Menus:** File, Edit, View, Audio, Plugins, Samples, AICORE, Help ([MC:5147](<CORE/app/desktop/src/MainComponent.cpp#L5147>)).
- **Shortcuts:** all of [KM:74](<CORE/app/desktop/src/Keymap.cpp#L74>) stay, including Ctrl+P = Patterns, Ctrl+D = duplicate, L = loop and Ctrl+M = Mixer (in Pulse Ctrl+P, Ctrl+D and Ctrl+M do other things; they are not adopted).

**Where Pulse's layout forces changes to the menus**
1. View starts listing all windows with check marks, plus «Trazer janelas para o ecrã» (bring windows back to the screen) and, later, layouts.
2. «Patterns…» moves from the Plugins menu to View. PROD-06 asks for a single primary place per operation, and PROD-02 asks that the visible name be «Pattern Maker» (today the window is called «Core · Patterns»).
3. The top bar's «Mixer» tab and the side rail ([MC:5599](<CORE/app/desktop/src/MainComponent.cpp#L5599>)) become window toggles.
4. New actions in the Keymap: tools, snap, fit all, toggle windows.

**Components Core does not have yet** (to draw within its style):
- tool cursors;
- «paint» icon;
- scale highlight and root mark;
- ghost-note style;
- panel header with collapse / dock / detach (starting from `ToolDock`'s header, [ToolDock.h:17](<CORE/app/desktop/src/ToolDock.h#L17>));
- row-expand arrow in the rack;
- Apply/Cancel preview panel.

## 7. Collisions to flag

- **Plan of 17/11** ([plano-interface-chain-abas-laterais.md](<CORE/docs/relatorios/2026-11-17-plano-interface-chain-abas-laterais.md>)): it says Piano Roll, Pattern Maker and Tools collapse into a side tab and that Stage 1 is a common tab container. Detachable windows are not there. It is not a contradiction if each panel has three states (collapsed in a tab, docked, detached), but the plan must be reviewed before Stage 1 and needs a numbered decision (AGENTS.md: architecture of new surfaces).
- **D83** (vertical master, visual refinement, DSP window and mockup out of the tree): nothing here reintroduces them. Care when detaching the Mixer: do not bring the vertical master along by drag.
- **Product direction:** «Generate» is algorithmic and fits in Tools. Pulse's instruments, effects, sounds and MCP do not come in.
- **Legacy:** `PatternEditor` still draws rows for old own instruments; the expandable rack must not lean on that.
- **Process:** AGENTS.md asks for a report in `docs/relatorios` and proposed lines for `ESTADO.md`. I did not do it by instruction of this task. The pending item with the owner's five quotes of 18/11, which I know only through the coordinator, remains to be opened.

## 8. Architecture of the change

**Today, confirmed in the code:** Core is a main window with regions computed by hand in a single `resized()`, and a bottom panel where five editors share the same rectangle. `PatternEditor` already talks to the window through an interface ([PEh:23](<CORE/app/desktop/src/PatternEditor.h#L23>)), which makes it easier to move it. The plugin editors are windows of another process.

| Approach | What it is | Cost | Risk | Reversible |
|---|---|---|---|---|
| A. Internal sub-windows (what Pulse does) | Work area inside the main window | Large, all at once: the arrangement stops being a fixed background and the whole `resized()` is redone | High: the plugins cannot come in (another process), it is stuck to one monitor and goes against the plan of 17/11 | Little |
| B. Everything in OS windows | Each panel becomes always a window like Patterns | Medium–large | Medium: on a single screen it becomes a pile of windows; the existing docking is lost | Medium |
| **C. Docked by default, detachable (I recommend)** | The main window stays as it is; each editor gains «destacar» (detach, moves to an OS window) and «encaixar» (dock, comes back); Patterns is already a window | Medium per panel, delivered gradually | Low–medium: focus and shortcuts, scale, persistence | Total, panel by panel |

**I recommend C.** It gives what the owner showed in the screenshot, goes beyond Pulse (another monitor), stays consistent with the plugin windows and does not throw away the tab plan.

**What must be split first in MainComponent.cpp** (7068 lines):
1. Key handling ([MC:5895](<CORE/app/desktop/src/MainComponent.cpp#L5895>)) → a command router callable from any window (generalize what `GlobalKeys` does for Space).
2. The «quem ocupa o painel inferior» (who occupies the bottom panel) logic ([MC:2598](<CORE/app/desktop/src/MainComponent.cpp#L2598>)–[MC:2640](<CORE/app/desktop/src/MainComponent.cpp#L2640>)) → a panel manager with the three states.
3. Opening the Mixer, which today hides Library and Inspector ([MC:2207](<CORE/app/desktop/src/MainComponent.cpp#L2207>)).
4. The UI scale, which only exists for the main window ([Main.cpp:23](<CORE/app/desktop/src/Main.cpp#L23>)).
5. Window persistence, with validation against disconnected monitors.

The whole file does not need to be split before starting; points 1 and 2 are enough for the first window.

## 9. Order in slices

**How it is tested.** There is a UI self-test: `Core.exe --self-test` ([Main.cpp:92](<CORE/app/desktop/src/Main.cpp#L92>)), registered as `desktop_self_test` ([tests/CMakeLists.txt:96](<CORE/tests/CMakeLists.txt#L96>)). Step 16 already injects synthetic mouse events into the piano roll ([MC:6709](<CORE/app/desktop/src/MainComponent.cpp#L6709>)); it is the extension point. New domain rules go to `domain_tests`.

| Slice | Content | How it is tested | Estimate |
|---|---|---|---|
| **1a. Mouse without changing gestures** | Audition on click and drag, clickable left keyboard, cursors, last length, «Off» snap, Alt+wheel = velocity | Self-test: click creates a note with the last length; plain wheel over a note does not change velocity. Owner: 5 min drawing a bass line | 2–3 days |
| **1b. Mouse with Pulse's gestures** | Drag to size on create, Shift+drag duplicates, Alt = free, Ctrl+drag = lasso, Paint | Self-test with synthetic mouse: note count, undo name, off-grid tick, selection | 2–3 days |
| **2. See everything** | «Caber tudo» (Fit all), center on open, up to 128 keys, overview bar, ghost notes | Domain test for «que padrões tocam junto» (which patterns play together); self-test: image + ghost count | 3–4 days |
| **3. Scale** | Button, menu, highlight, root mark, «Ajustar à escala» (Fit to scale) | Domain tests (belongs, nearest, down, up, limits 0/127); self-test: image | 3–4 days |
| **4. Detachable piano roll** | Command router, panel manager, scale, saved position | Self-test: detach, edit, Ctrl+Z in the detached window, dock. Owner: two monitors | 1–2 weeks |
| **5a. Rack: width and connections** | All bars, saved position, pan, name opens the plugin, selection commands the piano roll, wheel on the step | Self-test: open the window, paint a step in bar 3 without switching tab | ~1 week |
| **5b. Rack: expand channel** | Sub-rows per note; mini piano roll | Self-test + owner with a real drum plugin | ~1 week |
| **6. Detachable Mixer + View menu** | Window list, F keys, «trazer para o ecrã» (bring to the screen) | Self-test: toggle each window; off-screen coordinates are corrected | ~1 week |
| **7. Generate as Tools** | Euclidean, chords, progression, with preview and Apply | Deterministic domain tests per seed; self-test: Apply = one Undo | 1–2 weeks |
| **8. Layouts and the rest** | Saved layouts, detachable automation, arrangement parity | — | 2–3 weeks |

**Honest total:** 9 to 13 weeks of focused work up to «praticamente esse UI» (practically that UI), not counting the Windows qualification that AGENTS.md requires at each milestone. Slices 1a–3 (about two weeks) already change the feel of use without touching the architecture. These numbers are a code-reading estimate, not a measurement.

## 10. Questions for the owner

1. **Windows: how far?**
   - (a) Docked by default and detachable into a Windows window, able to go to another monitor. **I recommend.**
   - (b) Everything floating inside Core's window, exactly as in Pulse; the plugins stay out and there is no second monitor.
   - (c) Everything always in separate windows.
2. **«Channel rack expansível» (expandable channel rack) means:**
   - (a) the window grows and shows the whole pattern and more channels. It is what Pulse does; **I recommend starting here**.
   - (b) each channel opens in detail (one row per drum sound, mini piano roll). Pulse does not do it.
   - (c) both, in this order.
3. **«Piano que dá para ver tudo» (a piano where you can see everything) means:**
   - (a) see the other instruments' notes as ghosts;
   - (b) see the whole pattern and all octaves without scrolling;
   - (c) both. **I recommend (c)**: they are cheap and do not get in each other's way.
4. **Does the scale trap the mouse?**
   - (a) Visual guide only, as in Pulse.
   - (b) Guide + «prender à escala» (snap to scale) button, off by default. **I recommend.**
   - (c) Always traps.

**Decisions I assumed without asking** (say if they are wrong):
- Shift goes from «fora da grelha» (off the grid) to «duplicar» (duplicate) and Alt becomes «fora da grelha» (off the grid).
- The scale is per song, not global.
- «Generate» opens Core's generations instead of copying Pulse's.

**Trace left outside Core:** clone in `%TEMP%\pulse-ref`; OCR files in `%TEMP%` (`ocr-shot.ps1`, `ocr-owner.txt`, `ocr-repo.txt` and two enlarged copies of the screenshots); two entries in long-term memory, project «core».