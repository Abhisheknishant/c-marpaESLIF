MACRO (FINDVSNPRINTF)
  GET_PROPERTY(source_dir_set GLOBAL PROPERTY MYPACKAGE_SOURCE_DIR SET)
  IF (NOT ${source_dir_set})
    MESSAGE (WARNING "Cannot check inline, property MYPACKAGE_SOURCE_DIR is not set")
  ELSE ()
    IF (NOT C_VSNPRINTF_SINGLETON)
      GET_PROPERTY(source_dir GLOBAL PROPERTY MYPACKAGE_SOURCE_DIR)
      SET (_C_VSNPRINTF_FOUND FALSE)
      #
      # We depend on stdio
      #
      INCLUDE (CheckIncludeFile)
      CHECK_INCLUDE_FILE ("stdio.h" HAVE_STDIO_H)
      IF (HAVE_STDIO_H)
        SET (_HAVE_STDIO_H 1)
      ELSE ()
        SET (_HAVE_STDIO_H 0)
      ENDIF ()
      #
      # Test
      #
      FOREACH (KEYWORD "vsnprintf" "_vsnprintf" "__vsnprintf")
        MESSAGE(STATUS "Looking for ${KEYWORD}")
        TRY_COMPILE (C_HAS_${KEYWORD} ${CMAKE_CURRENT_BINARY_DIR}
          ${source_dir}/vsnprintf.c
          COMPILE_DEFINITIONS "-DC_VSNPRINTF=${KEYWORD} -DHAVE_STDIO_H=${_HAVE_STDIO_H}")
        IF (C_HAS_${KEYWORD})
          MESSAGE(STATUS "Looking for ${KEYWORD} - found")
          SET (_C_VSNPRINTF ${KEYWORD})
          SET (_C_VSNPRINTF_FOUND TRUE)
          BREAK ()
        ENDIF ()
      ENDFOREACH ()
    ENDIF ()
    IF (_C_VSNPRINTF_FOUND)
      SET (C_VSNPRINTF "${_C_VSNPRINTF}" CACHE STRING "C vsnprintf implementation")
      MARK_AS_ADVANCED (C_VSNPRINTF)
    ENDIF ()
    SET (C_VSNPRINTF_SINGLETON TRUE CACHE BOOL "C vsnprintf check singleton")
    MARK_AS_ADVANCED (C_VSNPRINTF_SINGLETON)
  ENDIF ()
ENDMACRO()