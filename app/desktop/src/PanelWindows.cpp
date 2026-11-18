// The OS window a docked panel pops out into — see PanelWindows.h.
#include "PanelWindows.h"
#include "GlobalKeys.h"
#include "Theme.h"

namespace core::app {

PanelWindow::PanelWindow(const juce::String& title, juce::Component* content, std::function<void()> closeRequested)
    : juce::DocumentWindow(title, theme::kBackground, juce::DocumentWindow::allButtons),
      closeRequested_(std::move(closeRequested)) {
  setUsingNativeTitleBar(true);
  setContentNonOwned(content, false);
  setResizable(true, true);
  setResizeLimits(420, 240, 16384, 16384);
  addKeyListener(this);  // what no child consumed goes to the main window
}

PanelWindow::~PanelWindow() {
  removeKeyListener(this);
  clearContentComponent();  // the content belongs to the main window: never deleted here
}

void PanelWindow::closeButtonPressed() {
  if (closeRequested_) closeRequested_();
}

void PanelWindow::moved() {
  juce::DocumentWindow::moved();
  if (onBoundsChanged && isVisible()) onBoundsChanged();
}

void PanelWindow::resized() {
  juce::DocumentWindow::resized();
  if (onBoundsChanged && isVisible()) onBoundsChanged();
}

bool PanelWindow::keyPressed(const juce::KeyPress& key, juce::Component*) {
  if (GlobalKeys::focusTakesKeys(getCurrentlyFocusedComponent())) return false;  // typing in a text box
  return forwardKey ? forwardKey(key) : false;
}

bool PanelWindow::keyStateChanged(bool isKeyDown, juce::Component*) {
  return forwardKeyState ? forwardKeyState(isKeyDown) : false;
}

}  // namespace core::app