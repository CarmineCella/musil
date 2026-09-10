# golden.cmake — run ${MUSIL} on ${INPUT}, compare with ${GOLDEN}.
# Used for reference.mu (core) and reference_std.mu; reference_system.mu has
# machine-dependent output and is only run.
# Lines containing "took" (timings), "rand" (platform-dependent RNG) or "bytes" (file sizes) are dropped
# before comparing, and absolute paths to reference.mu are reduced to the file name.
# To refresh the golden file after an intended change:
#   MUSIL_PATH=src cmake -DMUSIL=build/musil -DINPUT=examples/reference.mu -DGOLDEN=tests/golden/reference.out -DUPDATE=1 -P tests/golden.cmake

execute_process(COMMAND ${MUSIL} ${INPUT}
                OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "musil exited with ${rc}:\n${err}")
endif()

function(filter_volatile text var)
  string(REPLACE ";" "\\;" text "${text}")
  string(REGEX REPLACE "\n" ";" lines "${text}")
  set(kept "")
  foreach(line IN LISTS lines)
    if(NOT line MATCHES "took|rand|bytes")
      # error messages embed absolute paths of .mu files: keep only the file name,
      # and drop the line number when the file is a library (it moves with every edit)
      string(REGEX REPLACE "[^ ]*/([^ /]+\\.mu)" "\\1" line "${line}")
      string(REGEX REPLACE "(^| )(std|system|scientific)\\.mu:[0-9]+" "\\1\\2.mu" line "${line}")
      string(APPEND kept "${line}\n")
    endif()
  endforeach()
  string(STRIP "${kept}" kept)
  set(${var} "${kept}\n" PARENT_SCOPE)
endfunction()

filter_volatile("${out}" actual)

if(UPDATE)
  file(WRITE ${GOLDEN} "${actual}")
  message(STATUS "golden file updated: ${GOLDEN}")
  return()
endif()

if(NOT EXISTS ${GOLDEN})
  message(FATAL_ERROR "golden file missing: ${GOLDEN} (run with -DUPDATE=1 to create it)")
endif()
file(READ ${GOLDEN} expected)
filter_volatile("${expected}" expected)

if(NOT actual STREQUAL expected)
  file(WRITE ${CMAKE_BINARY_DIR}/reference.actual "${actual}")
  message(FATAL_ERROR "reference output differs from ${GOLDEN}\n(actual output written to reference.actual in the build dir)")
endif()
message(STATUS "reference output matches golden file")
