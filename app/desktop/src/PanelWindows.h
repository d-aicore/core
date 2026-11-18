// The OS window a docked panel pops out into (UX detachable-windows slice, 2026-11-18; the owner: «meter
// janelas a sair da janela principal»). Pulse's window organization with Core's own look: the global
// LookAndFeel draws it, a native title bar like the Patterns window's, resizable, and it can go to any
// monitor. The content is NOT owned here: the main window keeps the panel and takes it back when it docks
// again, so nothing is rebuilt and no edit state is lost on the way out or back.
#pragma once
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace core::app {

class PanelWindow : public juce::DocumentWindow, private juce::KeyListener {
public:
  // `closeRequested`: the title bar's × (the owner hides the panel and takes its content back).
  PanelWindow(const juce::String& title, juce::Component* content, std::function<void()> closeRequested);
  ~PanelWindow() override;
  void closeButtonPressed() override;
  void moved() override;
  void resized() override;

  // The window moved or was resized: the owner keeps the place in the user settings.
  std::function<void()> onBoundsChanged;
  // Keys no child of the window consumed go to the main window (undo, save, transport, tools...), exactly as
  // when the panel is docked and the keys bubble up to it. Typing in a text box is left alone.
  std::function<bool(const juce::KeyPress&)> forwardKey;
  std::function<bool(bool)> forwardKeyState;

private:
  bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;
  bool keyStateChanged(bool isKeyDown, juce::Component* origin) override;
  std::function<void()> closeRequested_;
};

}  // namespace core::app