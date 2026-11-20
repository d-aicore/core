# S11 negative control - local unpatched JUCE copy

## Step 1 - robocopy D:\Core-deps\JUCE-9.0.5 -> D:\Core-build\s11-juce-unpatched (/E /MT:8)
- time: 2026-11-20 00:19:50 +00:00
- rc: 1 (robocopy 0-7 = success)

## Step 2 - reverse-apply D113 patch in the copy
- time: 2026-11-20 00:20:08 +00:00
- git apply -R rc: 0 (expect 0)
- git apply --check -R after reversal rc: 1 (expect non-zero = patch no longer applied)

- symbol verification: grep markCrashedByHost in copy modules/juce_audio_processors_headless/utilities/juce_AudioPluginExtensions.h = 0 matches; format_types/juce_VST3PluginFormatImpl.h = 0 matches (symbol GONE)
## Step 3 - cmake configure (Ninja, Debug, CORE_BUILD_TESTS=ON, FETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-build/s11-juce-unpatched)
- time: 2026-11-20 00:21:00 +00:00
- bat: C:\Users\vk_do\AppData\Local\Temp\s11-negctrl-configure-fd9ea1fe68b84dcc8b0bf4f0448314ac.bat
- rc: 0
- output: D:\Core-build\s11-local-unpatched-configure.txt

## Step 4 - cmake --build D:\Core-build\s11-local-unpatched --parallel 4 (no -k)
- time: 2026-11-20 00:21:32 +00:00
- bat: C:\Users\vk_do\AppData\Local\Temp\s11-negctrl-build-66407bf3ce4044b4b9dd6c0d795d9d22.bat
- rc: 2
- output: D:\Core-build\s11-local-unpatched-build.txt

- failing target: engine/CMakeFiles/core_plugin_host.dir/src/plugins/juce_host.cpp.obj (step [19/555])
- error excerpt (verbatim from build log; MSVC emitted Portuguese despite VSLANG=1033 because English resources are not installed on this machine - known limitation per AGENTS.md; chcp 65001 was active, localized lines appear mojibake-encoded in the UTF-8 log):
    FAILED: [code=2] engine/CMakeFiles/core_plugin_host.dir/src/plugins/juce_host.cpp.obj
    D:\CORE\CORE\engine\src\plugins\juce_host.cpp(136): error C2039: 'markCrashedByHost': nao e um membro de 'juce::AudioPluginExtensions::VST3Client'
    D:\Core-build\s11-juce-unpatched\modules\juce_audio_processors_headless/utilities/juce_AudioPluginExtensions.h(87): note: consulte a declaracao de 'juce::AudioPluginExtensions::VST3Client'
    ninja: build stopped: subcommand failed.
  English gloss: error C2039: 'markCrashedByHost' is not a member of 'juce::AudioPluginExtensions::VST3Client'; note: see declaration of 'juce::AudioPluginExtensions::VST3Client'.
- include paths in the failing cl.exe command point at D:\Core-build\s11-juce-unpatched\modules (confirms the local unpatched copy was used, not a fetch).

## Conclusion
Negative control CONFIRMED: local JUCE copy without the D113 patch fails the build the same way a clean fetch does - juce_host.cpp.obj fails with C2039 markCrashedByHost missing from juce::AudioPluginExtensions::VST3Client. No fixes applied; repo D:\CORE\CORE untouched (read-only), D:\Core-deps untouched.
