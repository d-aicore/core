// Autosave (M6): where the copies go, and whether the session that wrote one is still running.
#pragma once
#include <cstdint>
#include <juce_core/juce_core.h>
#include "autosave.hpp"

namespace core::app {

// %LOCALAPPDATA%\Core\Autosave: the user's own, outside the program and the songs' folders.
juce::File autosaveFolder();
int64_t currentProcessId();
// What Windows says of the process that has this id now: whether there is one, its executable, when it started.
doc::ProcessProbe probeProcess(int64_t processId);
// True while the Core session that wrote an autosave (its process id, at savedAtMs) is still running: another
// window of Core, so its autosave is not from a crash. A live process with that id is not enough - the id of a
// Core that crashed goes to another program (doc::autosaveWriterRunning has the rule).
bool sessionRunning(int64_t processId, int64_t savedAtMs);

} // namespace core::app
