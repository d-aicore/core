"""Coverage assertion for the static-analysis scope (D92).

Fails when a file in scope is missing from compile_commands.json, so a tidy run can never look
complete while silently skipping code. Exclusions are derived from the build itself, not asserted
by the caller: juce_host.cpp needs the JUCE target to exist, engine_client.cpp is Windows-only.

  python3 ci/tidy_scope.py <compile_commands.json> <src-root> [out-list] [--require FILE]

--require adds a file to the demanded scope; it is the negative control (must fail on a build that
cannot contain it).
"""
import glob
import json
import os
import sys

GLOBS = (
    'engine/src/core/*.cpp', 'engine/src/dsp/*.cpp', 'engine/src/io/*.cpp', 'engine/src/app/*.cpp',
    'engine/src/plugins/*.cpp', 'app/domain/src/*.cpp', 'app/control/src/*.cpp',
    'app/engine-client/src/*.cpp', 'app/sampling/src/*.cpp', 'app/previews/src/*.cpp',
)


def main():
    required, args = [], []
    argv = sys.argv[1:]
    i = 0
    while i < len(argv):
        if argv[i] == '--require':
            required.append(argv[i + 1])
            i += 2
        else:
            args.append(argv[i])
            i += 1
    if len(args) < 2:
        sys.exit(__doc__)
    db_path, src = args[0], args[1]
    out_list = args[2] if len(args) > 2 else None

    db = json.load(open(db_path))
    files = [os.path.normpath(e['file']).replace('\\', '/') for e in db]
    commands = ' '.join(e.get('command', '') for e in db)
    windows = 'cl.exe' in commands.lower()
    juce = any('/juce' in f.lower() for f in files)

    os.chdir(src)
    wanted = sorted(set(sum([glob.glob(p) for p in GLOBS], []) + required))
    analyzed, excluded, missing = [], [], []
    for rel in wanted:
        rel = rel.replace('\\', '/')
        hit = [f for f in files if f.endswith('/' + rel)]
        if hit:
            analyzed.append(hit[0])
        elif rel in required:
            missing.append(rel)  # an explicit demand overrides any derived exclusion
        elif rel.endswith('plugins/juce_host.cpp') and not juce:
            excluded.append((rel, 'JUCE target absent from this build (CORE_BUILD_APP/WIN32 gate)'))
        elif rel.endswith('engine_client.cpp') and not windows:
            excluded.append((rel, 'Windows-only target; #error at engine_client.cpp:5'))
        else:
            missing.append(rel)

    print('TUs no banco: %d | windows=%s juce=%s' % (len(files), windows, juce))
    print('scope: %d pedidos -> %d analisados, %d excluidos, %d em falta'
          % (len(wanted), len(analyzed), len(excluded), len(missing)))
    for rel, why in excluded:
        print('  EXCLUIDO %s (%s)' % (rel, why))
    for rel in missing:
        print('  EM FALTA %s' % rel)
    if missing:
        sys.exit('SCOPE INCOMPLETO: %s' % missing)
    if out_list:
        with open(out_list, 'w') as fh:
            fh.write('\n'.join(analyzed) + '\n')
        print('lista escrita: %s' % out_list)


if __name__ == '__main__':
    main()
