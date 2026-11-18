#include "Autosave.h"
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace core::app {

juce::File autosaveFolder() {
  return juce::File::getSpecialLocation(juce::File::windowsLocalAppData).getChildFile("Core").getChildFile("Autosave");
}

int64_t currentProcessId() {
#ifdef _WIN32
  return (int64_t)GetCurrentProcessId();
#else
  return 0;
#endif
}

doc::ProcessProbe probeProcess(int64_t processId) {
  doc::ProcessProbe out;
#ifdef _WIN32
  if (processId <= 0 || processId > 0xFFFFFFFFLL) return out;
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)processId);
  if (!h) return out;
  DWORD code = 0;
  out.exists = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
  if (out.exists) {
    std::wstring path(32768, L'\0');  // the longest path Windows gives
    DWORD length = (DWORD)path.size();
    if (QueryFullProcessImageNameW(h, 0, path.data(), &length) && length > 0) {
      const int bytes = WideCharToMultiByte(CP_UTF8, 0, path.data(), (int)length, nullptr, 0, nullptr, nullptr);
      if (bytes > 0) {
        out.image.resize((size_t)bytes);
        WideCharToMultiByte(CP_UTF8, 0, path.data(), (int)length, out.image.data(), bytes, nullptr, nullptr);
      }
    }
    FILETIME created{}, exited{}, kernel{}, user{};
    if (GetProcessTimes(h, &created, &exited, &kernel, &user)) {
      // FILETIME counts 100 ns from 1601; the autosave's date is in ms from 1970.
      const uint64_t ticks = ((uint64_t)created.dwHighDateTime << 32) | created.dwLowDateTime;
      const uint64_t epoch = 116444736000000000ULL;
      if (ticks > epoch) out.createdAtMs = (int64_t)((ticks - epoch) / 10000);
    }
  }
  CloseHandle(h);
#else
  (void)processId;
#endif
  return out;
}

bool sessionRunning(int64_t processId, int64_t savedAtMs) {
  // This Core's own executable, read the same way as the other process's.
  static const std::string own = probeProcess(currentProcessId()).image;
  return doc::autosaveWriterRunning(processId, savedAtMs, own, probeProcess);
}

} // namespace core::app
