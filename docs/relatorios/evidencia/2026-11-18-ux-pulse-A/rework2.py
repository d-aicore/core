import io, re

ROOT = 'D:/CORE/CORE/.agents/worktrees/ux-pulse-A/'

def rd(p): return io.open(ROOT + p, encoding='utf-8').read()
def wr(p, s): io.open(ROOT + p, 'w', encoding='utf-8', newline='').write(s)

def sub(s, a, b, tag):
    if s.count(a) == 0:
        if b and s.count(b) >= 1:
            print('  already applied:', tag); return s
        raise AssertionError('NOT FOUND: ' + tag)
    assert s.count(a) == 1, (tag, s.count(a))
    return s.replace(a, b)

def fn_replace(s, head, new, tag):
    """Replace a whole function definition (from `head` to the first line that is exactly `}`)."""
    i = s.find(head)
    if i < 0:
        if new.strip().splitlines()[0] in s:
            print('  already applied:', tag); return s
        raise AssertionError('NOT FOUND: ' + tag)
    j = s.index('\n}\n', i)
    return s[:i] + new.rstrip('\n') + '\n' + s[j + 3:]

BS = chr(92)
c = rd('app/desktop/src/PatternEditor.cpp')

c = sub(c, "constexpr int kRow2 = 26;  // the pitched tracks' second toolbar row: Keys" + BS + "u00b1, Fit keys, the scale dropdowns" + BS + "n", '', 'kRow2') if False else c
c = re.sub(r"constexpr int kRow2 = 26;[^\n]*\n", '', c, count=1)

c = sub(c, '  zoomIn_.setTooltip(u8("Zoom in: fewer bars across the grid (Ctrl+Shift+wheel up). Ctrl+wheel is Keys+/Keys' + chr(0x2212) + ' on the piano roll."));',
           '  zoomIn_.setTooltip(u8("Zoom in: fewer bars across the grid (Ctrl+wheel up)"));', 'zoomIn-tip')
c = sub(c, '  zoomOut_.setTooltip(u8("Zoom out: more bars across the grid (Ctrl+Shift+wheel down). Ctrl+wheel is Keys+/Keys' + chr(0x2212) + ' on the piano roll."));',
           '  zoomOut_.setTooltip(u8("Zoom out: more bars across the grid (Ctrl+wheel down)"));', 'zoomOut-tip')
c = sub(c, 'Previous bars (Shift+wheel scrolls; Ctrl+Shift+wheel, or ' + chr(0x2212) + ', shows more)',
           'Previous bars (a sideways swipe scrolls; Ctrl+wheel, or ' + chr(0x2212) + ', shows more)', 'pagePrev-tip')
c = sub(c, 'Next bars (Shift+wheel scrolls; Ctrl+Shift+wheel, or ' + chr(0x2212) + ', shows more)',
           'Next bars (a sideways swipe scrolls; Ctrl+wheel, or ' + chr(0x2212) + ', shows more)', 'pageNext-tip')
c = sub(c, 'Keys ' + chr(0x2212) + ': four fewer keys, taller rows (Ctrl+wheel down)',
           'Keys ' + chr(0x2212) + ': four fewer keys, taller rows (Shift+wheel down)', 'keysMinus-tip')
c = sub(c, 'Keys +: four more keys, shorter rows (Ctrl+wheel up)',
           'Keys +: four more keys, shorter rows (Shift+wheel up)', 'keysPlus-tip')
c = sub(c, '  noteTools_.onClick = [this] { openNoteTools(); };',
'''  noteTools_.onClick = [this] { openNoteTools(); };
  // View ''' + chr(0x25be) + ''': the key range, the scale guide and the zoom, so the toolbar stays one compact row.
  view_.setButtonText(u8("View ''' + chr(0x25be) + '''"));
  view_.setTooltip(u8("View: Keys+/Keys''' + chr(0x2212) + ''' (Shift+wheel), all 128 keys, the scale guide and its root, the zoom (Ctrl+wheel)"));
  view_.onClick = [this] { showViewMenu(); };''', 'view-setup')
c = sub(c, '''  for (auto* c : std::initializer_list<juce::Component*>{&zoomIn_, &zoomOut_, &patternBox_, &newPattern_, &clearPattern_, &removePattern_, &snap_, &length_, &octaveUp_, &octaveDown_, &title_, &hint_, &addSound_, &pagePrev_, &pageNext_,
                                                                &toolBox_, &edit_, &editInfo_, &keysMinus_, &keysPlus_, &fitKeys_, &scaleBox_, &rootBox_, &noteTools_, &detach_, &closePanel_}) {
    c->setWantsKeyboardFocus(false);
    addAndMakeVisible(c);
  }''',
'''  for (auto* c : std::initializer_list<juce::Component*>{&patternBox_, &newPattern_, &clearPattern_, &removePattern_, &snap_, &length_, &title_, &hint_, &addSound_, &pagePrev_, &pageNext_,
                                                                &toolBox_, &edit_, &editInfo_, &noteTools_, &view_, &detach_, &closePanel_}) {
    c->setWantsKeyboardFocus(false);
    addAndMakeVisible(c);
  }
  // The View menu's own controls: children, but not permanent toolbar residents (one compact row).
  for (auto* c : std::initializer_list<juce::Component*>{&zoomIn_, &zoomOut_, &octaveUp_, &octaveDown_, &keysMinus_, &keysPlus_, &fitKeys_, &scaleBox_, &rootBox_}) {
    c->setWantsKeyboardFocus(false);
    addChildComponent(c);
  }''', 'visible-list')
c = sub(c, '''  // Ctrl+Shift+wheel: zoom in or out around the mouse (down to a beat across the grid). Shift+wheel (or a sideways
  // swipe) while zoomed: move along the pattern.
  if (currentTrack() && ctrl && e.mods.isShiftDown()) {
    zoom(std::pow(2.0, -wheel.deltaY * 2.0), shownStart() + std::clamp((e.x - grid().getX()) / (double)std::max(1, grid().getWidth()), 0.0, 1.0) * shownLength());
    return;
  }
  // Ctrl+wheel on the piano roll: Keys+ / Keys- (the owner's expand); drums keep the horizontal zoom.
  if (currentTrack() && ctrl) {
    if (!drums()) { keysWheel(wheel.deltaY, e.y); return; }
    zoom(std::pow(2.0, -wheel.deltaY * 2.0), shownStart() + std::clamp((e.x - grid().getX()) / (double)std::max(1, grid().getWidth()), 0.0, 1.0) * shownLength());
    return;
  }''',
'''  // Ctrl+wheel: the horizontal/time zoom (down to a beat across the grid) - the owner's latest mapping; it was the
  // key range before. Ctrl+Shift+wheel while zoomed moves along the pattern: the horizontal navigation the old
  // Shift+wheel binding did, reassigned so one gesture never has two jobs.
  if (currentTrack() && ctrl) {
    if (e.mods.isShiftDown() && shownLength() < (double)pattern()->lengthTicks - 1) {
      viewStart_ = shownStart() - wheel.deltaY * shownLength() * 0.5;
      placePaging();
      repaint();
      return;
    }
    zoom(std::pow(2.0, -wheel.deltaY * 2.0), shownStart() + std::clamp((e.x - grid().getX()) / (double)std::max(1, grid().getWidth()), 0.0, 1.0) * shownLength());
    return;
  }''', 'wheel-ctrl')
c = sub(c, '''  if (currentTrack() && shownLength() < (double)pattern()->lengthTicks && (e.mods.isShiftDown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY))) {
    const double delta = std::abs(wheel.deltaX) > std::abs(wheel.deltaY) ? -wheel.deltaX : wheel.deltaY;''',
'''  // Shift+wheel on the piano roll: Keys+ / Keys-, the key under the mouse kept in place (the owner's latest:
  // Shift+wheel for more/fewer keys). Drums have no key range, so they keep the horizontal move there.
  if (currentTrack() && e.mods.isShiftDown() && !e.mods.isAltDown()) {
    if (!drums()) { keysWheel(wheel.deltaY, e.y); return; }
    if (shownLength() < (double)pattern()->lengthTicks - 1) {
      viewStart_ = shownStart() - wheel.deltaY * shownLength() * 0.5;
      placePaging();
      repaint();
    }
    return;
  }
  // A sideways swipe while zoomed: move along the pattern (the vertical wheel no longer does this).
  if (currentTrack() && shownLength() < (double)pattern()->lengthTicks && std::abs(wheel.deltaX) > std::abs(wheel.deltaY)) {
    const double delta = -wheel.deltaX;''', 'wheel-shift')
c = sub(c, '''void PatternEditor::keysWheel(float deltaY, int y) {
  juce::ignoreUnused(y);
  rowsWanted_ = std::clamp(keysShown() + (deltaY > 0 ? 4 : -4), kKeysMin, kKeysMax);
  lowPitch_ = std::clamp(lowPitch_, 0, 128 - keysShown());''',
'''void PatternEditor::keysWheel(float deltaY, int y) {
  const int under = pitchAtY(y), before = keysShown();
  rowsWanted_ = std::clamp(before + (deltaY > 0 ? 4 : -4), kKeysMin, kKeysMax);
  // The key under the mouse stays where it was: the range grows or shrinks around what is being looked at instead
  // of jumping to another octave.
  lowPitch_ = under >= 0
                ? std::clamp((int)std::lround(under - (under - lowPitch_) * (double)keysShown() / std::max(1, before)), 0, 128 - keysShown())
                : std::clamp(lowPitch_, 0, 128 - keysShown());''', 'keysWheel')
c = sub(c, '" keys ' + chr(0xb7) + ' Ctrl+wheel: Keys' + chr(0xb1) + ' ' + chr(0xb7) + ' Alt+wheel: velocity"',
           '" keys ' + chr(0xb7) + ' Shift+wheel: Keys' + chr(0xb1) + ' ' + chr(0xb7) + ' Ctrl+wheel: zoom ' + chr(0xb7) + ' Alt+wheel: velocity"', 'hint')
c = sub(c, 'Ctrl+drag: select ' + chr(0xb7) + ' Alt+wheel: velocity',
           'Ctrl+drag: select ' + chr(0xb7) + ' Shift+wheel: Keys' + chr(0xb1) + ' ' + chr(0xb7) + ' Ctrl+wheel: zoom ' + chr(0xb7) + ' Alt+wheel: velocity', 'editor-tip')
c = sub(c, 'viewLength_ ticks from viewStart_ (Ctrl+Shift+wheel, +/' + chr(0x2212) + ').',
           'viewLength_ ticks from viewStart_ (Ctrl+wheel, View ' + chr(0x25be) + ').', 'zoom-comment')

NEW_RESIZED = '''void PatternEditor::resized() {
  dock_.setBounds(dock_.hasTool() ? getLocalBounds().reduced(8).removeFromRight(dockWidth() - 8) : juce::Rectangle<int>());
  auto header = area().reduced(8, 4).removeFromTop(kHeader - 4);
  applyToolbarVisibility();  // the document state first, so the overflow below starts from a known visibility
  overflowed_.clear();
  // The dedicated header area: the window buttons at the far right (detach / dock, close). They never overflow, so
  // a narrow window can always dock or hide the panel again.
  const bool detached = isDetached && isDetached();
  detach_.getProperties().set(theme::kIconProperty, detached ? "dock" : "detach");
  detach_.setTooltip(detached ? u8("Dock the piano roll back into the main window") : u8("Detach the piano roll into its own window"));
  closePanel_.setBounds(header.removeFromRight(28).reduced(2));
  detach_.setBounds(header.removeFromRight(28).reduced(2));
  header.removeFromRight(6);

  // One compact row: the context (pattern, title), the Tool dropdown, Snap, Note tools, View and Edit. The key
  // range, the scale guide and the zoom are in the View menu and on the wheel gestures, so there is no second
  // permanent button row; a row that is too narrow overflows into "More" before anything is clipped.
  const bool pitched = currentTrack() && !drums();
  // The pattern's whole name when there is room (up to 260 px), never under 112; the title keeps at least 60.
  const int nameW = juce::GlyphArrangement::getStringWidthInt(juce::Font(juce::FontOptions(theme::kFontStrong)), patternBox_.getText()) + 44;
  const std::vector<ToolbarSlot> row = {{&editInfo_, theme::InfoDot::kSlot + 4, true}, {&edit_, 66}, {nullptr, 8},
                                        {&view_, 62}, {&noteTools_, 78}, {&snap_, 88}, {&toolBox_, 96}, {nullptr, 8},
                                        {&length_, 84}, {&removePattern_, 56}, {&clearPattern_, 52}, {&newPattern_, 48},
                                        {&patternBox_, std::clamp(nameW, 112, 260)}};
  auto work = header;
  work.removeFromLeft(60);  // the title's floor: the row is laid out in what is left of it
  if (wantedWidth(row) > work.getWidth()) {
    overflowMore_.setVisible(true);
    overflowMore_.setBounds(work.removeFromRight(64).reduced(2));
    work.removeFromRight(4);
  } else {
    overflowMore_.setVisible(false);
  }
  placeToolbarRow(work, row, true);
  work.removeFromRight(8);
  // What the row leaves is the context: the title, plus the key-range hint while there is room for both.
  auto left = header.withWidth(std::max(0, work.getRight() - header.getX()));
  const int hintW = juce::GlyphArrangement::getStringWidthInt(hint_.getFont(), hint_.getText()) + 16;
  if (pitched && left.getWidth() >= hintW + 60) {
    hint_.setBounds(left.removeFromRight(hintW).reduced(4, 0));
    hint_.setVisible(true);
  } else {
    hint_.setBounds({});
    hint_.setVisible(false);  // drums: the hint is the editor's tooltip; a narrow row: the View tooltip carries it
  }
  title_.setBounds(left.withSizeKeepingCentre(left.getWidth(), 18));  // one line: a long name ends in "..."
  // A narrow header drops the "PIANO ROLL" / "STEPS" prefix so the track's name is what stays readable.
  if (const doc::Track* t = currentTrack()) {
    const juce::String full = editorName() + u8(" ''' + chr(0x2014) + ''' ") + u8(t->name);
    const bool fits = juce::GlyphArrangement::getStringWidthInt(title_.getFont(), full) + 8 <= left.getWidth();
    title_.setText(fits ? full : u8(t->name), juce::dontSendNotification);
  }
  placeAddSound();
  placePaging();
}'''
c = fn_replace(c, 'void PatternEditor::resized() {', NEW_RESIZED, 'resized')

NEW_VIS = '''// What the document state shows on the compact row. The key-range, scale and zoom controls are not row residents:
// they live in the View menu, so they stay hidden here and the toolbar never grows a second line.
void PatternEditor::applyToolbarVisibility() {
  const bool editable = currentTrack() != nullptr;
  noteTools_.setVisible(editable && !drums());
  view_.setVisible(editable);
  for (auto* b : {&octaveUp_, &octaveDown_, &keysMinus_, &keysPlus_, &fitKeys_, &zoomIn_, &zoomOut_}) b->setVisible(false);
  scaleBox_.setVisible(false);
  rootBox_.setVisible(false);
}'''
c = fn_replace(c, '// What the document state shows on the toolbar', NEW_VIS, 'applyToolbarVisibility')

c = sub(c, 'int PatternEditor::headerHeight() const { return currentTrack() && !drums() ? kHeader + kRow2 : kHeader; }',
           'int PatternEditor::headerHeight() const { return kHeader; }', 'headerHeight')
c = sub(c, '// One toolbar row for drums, two for pitched tracks (the key range and the scale live on the second).',
           '// One compact toolbar row for every track: the key range and the scale live in the View menu, not on a second row.', 'headerHeight-comment')

NEW_MENUS = '''// The overflow menu: a hidden button keeps its own action, a hidden dropdown opens as a submenu of its choices.
void PatternEditor::buildOverflowMenu() {
  overflowMenu_.clear();
  overflowItems_.clear();
  int id = 1;
  for (auto* c : overflowed_) {
    if (auto* box = dynamic_cast<juce::ComboBox*>(c)) {
      juce::PopupMenu sub;
      for (int i = 0; i < box->getNumItems(); ++i) {
        const int itemId = box->getItemId(i);
        if (itemId <= 0) continue;
        sub.addItem(id, box->getItemText(i), box->isEnabled(), itemId == box->getSelectedId());
        overflowItems_[id] = {box, itemId};
        ++id;
      }
      overflowMenu_.addSubMenu(box == &toolBox_ ? u8("Tool") : box->getText(), sub);
    } else if (auto* b = dynamic_cast<juce::Button*>(c)) {
      overflowMenu_.addItem(id, b->getButtonText(), b->isEnabled());
      overflowItems_[id] = {b, 0};
      ++id;
    }
  }
}

void PatternEditor::runOverflowItem(int menuId) {
  const auto it = overflowItems_.find(menuId);
  if (it == overflowItems_.end()) return;
  if (auto* box = dynamic_cast<juce::ComboBox*>(it->second.first)) {
    if (it->second.second > 0 && box->isEnabled()) box->setSelectedId(it->second.second, juce::sendNotification);
  } else if (auto* b = dynamic_cast<juce::Button*>(it->second.first)) {
    // Synchronous, as the self-test's programmatic clicks: Button::triggerClick() only posts a message.
    if (b->isEnabled() && b->onClick) b->onClick();
  }
}

// View menu: the key range, the scale guide and the time navigation, so the toolbar stays one compact row. It
// drives the very functions the wheel gestures and the self-test drive, so docked, detached and menu all send the
// same command.
void PatternEditor::showViewMenu() {
  juce::PopupMenu m;
  if (currentTrack() && !drums()) {
    m.addItem(1, u8("Keys +  (Shift+wheel up): four more keys"));
    m.addItem(2, u8("Keys ''' + chr(0x2212) + '''  (Shift+wheel down): four fewer keys"));
    m.addItem(3, u8("All 128 keys"));
    m.addItem(4, u8("One octave up"));
    m.addItem(5, u8("One octave down"));
    m.addSeparator();
    juce::PopupMenu scales;
    scales.addItem(100, u8("Scale: off"), true, !scaleGuideOn());
    for (int i = 0; i < kScaleCount; ++i) scales.addItem(101 + i, u8(kScales[i].name), true, scaleName_ == kScales[i].name);
    m.addSubMenu(u8("Scale guide"), scales);
    juce::PopupMenu roots;
    for (int pc = 0; pc < 12; ++pc) roots.addItem(200 + pc, u8(pitchClassName(pc)), true, scaleRoot_ == pc);
    m.addSubMenu(u8("Root"), roots);
    m.addSeparator();
  }
  m.addItem(300, u8("Zoom in  (Ctrl+wheel up): fewer bars"));
  m.addItem(301, u8("Zoom out  (Ctrl+wheel down): more bars"));
  m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&view_),
                  [safe = juce::Component::SafePointer<PatternEditor>(this)](int r) {
                    if (!safe || r <= 0) return;
                    safe->runViewItem(r);
                  });
}

void PatternEditor::runViewItem(int id) {
  switch (id) {
    case 1: keysMore(); return;
    case 2: keysLess(); return;
    case 3: fitKeys(); return;
    case 4: lowPitch_ = std::min(128 - keysShown(), lowPitch_ + 12); repaint(); return;
    case 5: lowPitch_ = std::max(0, lowPitch_ - 12); repaint(); return;
    case 300: zoom(0.5, shownStart() + shownLength() / 2); return;
    case 301: zoom(2.0, shownStart() + shownLength() / 2); return;
    default: break;
  }
  if (id >= 100 && id <= 100 + kScaleCount) { setScaleGuide(scaleRoot_, id == 100 ? std::string() : kScales[id - 101].name); return; }
  if (id >= 200 && id < 212) setScaleGuide(id - 200, scaleName_);
}'''
i = c.find('// The overflow menu: a hidden button keeps its own action')
assert i >= 0, 'menus head'
j = c.index('\n}\n', c.index('\n}\n', i) + 3)  # end of buildOverflowMenu, then end of runOverflowItem
c = c[:i] + NEW_MENUS + '\n' + c[j + 3:]
wr('app/desktop/src/PatternEditor.cpp', c)

m = rd('app/desktop/src/MainComponent.cpp')
m = sub(m, '''        openPatterns();
        if (!patternsWindow_ || !patternsWindow_->isVisible() || patternsWindow_->panel().patternOf("kick") == nullptr)
          return finish(false, "patterns window");
        patternsWindow_->closeButtonPressed();''',
'''        openPatterns();
        // The rack is a docked panel since the detachable-windows slice (it becomes a native PatternsWindow only
        // while it is detached), so this probe reads the panel and its state, not a DocumentWindow.
        if (!panelShown("channelRack") || !patternsPanel_ || patternsPanel_->patternOf("kick") == nullptr)
          return finish(false, "patterns window");
        hidePanel("channelRack");''', 'case38-rack')
m = sub(m, '''        if (pluginCards_.onOpen) pluginCards_.onOpen(doc::corePlugins()[0], pluginCards_);
        if (!patternsWindow_ || !patternsWindow_->isVisible()) return finish(false, "pattern maker card did not open the window");
        patternsWindow_->closeButtonPressed();''',
'''        if (pluginCards_.onOpen) pluginCards_.onOpen(doc::corePlugins()[0], pluginCards_);
        if (!panelShown("channelRack") || !patternsPanel_ || !patternsPanel_->isVisible())
          return finish(false, "pattern maker card did not open the rack");
        hidePanel("channelRack");''', 'case38-card')
m = sub(m, '      // clones, Alt bypasses snap, Ctrl+drag lassos in any tool, Alt+wheel velocity (Ctrl+Alt fine), Ctrl+wheel = Keys+.',
'''      // clones, Alt bypasses snap, Ctrl+drag lassos in any tool, Alt+wheel velocity (Ctrl+Alt fine), Shift+wheel =
      // Keys+ and Ctrl+wheel = the time zoom (the owner's latest modifier mapping).''', 'case39-comment')
m = sub(m, '''        // 7. Ctrl+wheel expands the key range (Keys+): a key far above the view comes into it.
        if (ed.cellGrabPoint(0, 100).y != 0) return finish(false, "keys view precondition");
        for (int i = 0; i < 20; ++i) ed.mouseWheelMove(mouse(a, a, ctrl), up);''',
'''        // 7. Shift+wheel expands the key range (Keys+): a key far above the view comes into it. Ctrl+wheel is the
        // time zoom now, so it must leave the key count alone; that notch is zoomed straight back out so the steps
        // that follow keep the same view.
        if (ed.cellGrabPoint(0, 100).y != 0) return finish(false, "keys view precondition");
        const int keysBeforeCtrl = ed.keysShown();
        auto zoomBack = up;
        zoomBack.deltaY = -up.deltaY;
        ed.mouseWheelMove(mouse(a, a, ctrl), up);
        if (ed.keysShown() != keysBeforeCtrl) return finish(false, "ctrl wheel changed the key range");
        ed.mouseWheelMove(mouse(a, a, ctrl), zoomBack);
        for (int i = 0; i < 20; ++i) ed.mouseWheelMove(mouse(a, a, shift), up);''', 'case39-wheel')
m = sub(m, 'return finish(false, "ctrl wheel keys+ keys=" ', 'return finish(false, "shift wheel keys+ keys=" ', 'case39-msg')
wr('app/desktop/src/MainComponent.cpp', m)
print('ALL EDITS APPLIED')
