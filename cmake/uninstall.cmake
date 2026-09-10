# uninstall.cmake — remove what `cmake --install` put in place, without conditions:
#   <prefix>/bin/musil, <prefix>/bin/musil-listener, <prefix>/include/musil/, ~/.musil/
# and, when a manifest of the last install exists, every file it lists (covers an install
# made with --prefix somewhere else).
set(n 0)
function(gone f)
  if(EXISTS "${f}" OR IS_SYMLINK "${f}")
    message(STATUS "removing ${f}")
    file(REMOVE_RECURSE "${f}")
    math(EXPR n "${n} + 1")
    set(n ${n} PARENT_SCOPE)
  endif()
endfunction()

gone("${PREFIX}/bin/musil")
gone("${PREFIX}/bin/musil-listener")
gone("${PREFIX}/include/musil")
gone("${MUSIL_HOME}")
if(EXISTS "${MANIFEST}")
  file(STRINGS "${MANIFEST}" files)
  foreach(f ${files})
    gone("${f}")
    if(f MATCHES "^(.*)/include/musil/")      # an install made elsewhere: remove that include/musil too
      gone("${CMAKE_MATCH_1}/include/musil")
    endif()
  endforeach()
  file(REMOVE "${MANIFEST}")
endif()
message(STATUS "uninstall: ${n} item(s) removed from ${PREFIX} and ${MUSIL_HOME}")
