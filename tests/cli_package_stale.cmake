# BUG-SWEEP-14: the development zip must not keep files that were removed from the repo.
# cmake copy_directory does not delete destination-only files, so a lang/patterns/pcore file deleted
# from the source tree used to survive in package/Core and in Core-windows-x64.zip. This test seeds
# destination-only stale files into the staging area, runs the real core-package target, and requires
# the seeds gone from both the folder and the zip while every current repo resource (and the
# exe/demo/README) is present. The templates folder left the package with the builtin-effect templates
# (owner «remove templates com efeitos», 2026-11-18), so it is no longer a packaged resource here.
#   cmake -DBUILD_DIR=<build> -DSRC=<repo> -P cli_package_stale.cmake

set(stage "${BUILD_DIR}/package/Core")
set(zip "${BUILD_DIR}/Core-windows-x64.zip")

# Destination-only seeds: paths that exist in no source tree, one per packaged resource folder.
set(stale_rel
  lang/stale-removed-lang.json
  patterns/stale-removed-pattern.json
  pcore/stale-removed-style.json)
foreach(rel IN LISTS stale_rel)
  file(WRITE "${stage}/${rel}" "{\"stale\": \"BUG-SWEEP-14 seed; core-package must remove this\"}\n")
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target core-package
                OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "core-package exited with ${rc}: ${err}${out}")
endif()

set(survivors "")
foreach(rel IN LISTS stale_rel)
  if(EXISTS "${stage}/${rel}")
    list(APPEND survivors "package/Core/${rel}")
  endif()
endforeach()
if(survivors)
  message(FATAL_ERROR "BUG-SWEEP-14: stale files survived core-package in staging: ${survivors}")
endif()

# Everything the repo ships must be in the staging folder, and the exe/demo/README beside it.
foreach(dir lang patterns pcore)
  file(GLOB_RECURSE files RELATIVE "${SRC}/${dir}" "${SRC}/${dir}/*")
  list(LENGTH files n)
  if(n EQUAL 0)
    message(FATAL_ERROR "no resources found under ${SRC}/${dir}")
  endif()
  foreach(f IN LISTS files)
    if(IS_DIRECTORY "${SRC}/${dir}/${f}")
      continue()
    endif()
    if(NOT EXISTS "${stage}/${dir}/${f}")
      message(FATAL_ERROR "current resource missing from package/Core: ${dir}/${f}")
    endif()
  endforeach()
endforeach()
foreach(f Core.exe core-engine.exe demo.core-document.json README.md)
  if(NOT EXISTS "${stage}/${f}")
    message(FATAL_ERROR "package/Core is missing ${f}")
  endif()
endforeach()

# The zip itself: the seeds must not be entries, and every current resource must be one.
if(NOT EXISTS "${zip}")
  message(FATAL_ERROR "core-package did not write ${zip}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${zip}"
                OUTPUT_VARIABLE entries ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "cannot list ${zip}: ${err}")
endif()
string(REPLACE "\\" "/" entries "${entries}")
string(REPLACE "\r" "" entries "${entries}")
string(REPLACE "\n" ";" entries "${entries}")
set(zip_survivors "")
foreach(rel IN LISTS stale_rel)
  if("Core/${rel}" IN_LIST entries)
    list(APPEND zip_survivors "Core/${rel}")
  endif()
endforeach()
if(zip_survivors)
  message(FATAL_ERROR "BUG-SWEEP-14: stale files survived in Core-windows-x64.zip: ${zip_survivors}")
endif()
foreach(dir lang patterns pcore)
  file(GLOB_RECURSE files RELATIVE "${SRC}/${dir}" "${SRC}/${dir}/*")
  foreach(f IN LISTS files)
    if(IS_DIRECTORY "${SRC}/${dir}/${f}")
      continue()
    endif()
    if(NOT "Core/${dir}/${f}" IN_LIST entries)
      message(FATAL_ERROR "Core-windows-x64.zip is missing the entry Core/${dir}/${f}")
    endif()
  endforeach()
endforeach()
message(STATUS "core-package: stale seeds removed; all current resources present in package/Core and the zip")