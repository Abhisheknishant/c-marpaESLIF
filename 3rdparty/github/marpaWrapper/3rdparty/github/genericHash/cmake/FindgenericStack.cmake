# Module for locating genericStack, based on ICU module.
#
# Cutomizable variables:
#   GENERICSTACK_ROOT_DIR
#     This variable points to the genericStack root directory. On Windows the
#     library location typically will have to be provided explicitly using the
#     -D command-line option. Alternatively, an environment variable can be set.
#
# Read-Only variables:
#   GENERICSTACK_FOUND
#     Indicates whether the library has been found.
#
#   GENERICSTACK_INCLUDE_DIRS
#     Points to the genericStack include directory.
#
INCLUDE (CMakeParseArguments)
INCLUDE (FindPackageHandleStandardArgs)

SET (_PF86 "ProgramFiles(x86)")
SET (_GENERICSTACK_POSSIBLE_DIRS
  ${GENERICSTACK_ROOT_DIR}
  "$ENV{GENERICSTACK_ROOT_DIR}"
  "C:/genericStack"
  "$ENV{PROGRAMFILES}/genericStack"
  "$ENV{${_PF86}}/genericStack")

SET (_GENERICSTACK_POSSIBLE_INCLUDE_SUFFIXES include)

IF (CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET (_GENERICSTACK_POSSIBLE_LIB_SUFFIXES lib64)
  SET (_GENERICSTACK_POSSIBLE_BIN_SUFFIXES bin64)

  IF (NOT WIN32)
    LIST (APPEND _GENERICSTACK_POSSIBLE_LIB_SUFFIXES lib)
    LIST (APPEND _GENERICSTACK_POSSIBLE_BIN_SUFFIXES bin)
  ENDIF (NOT WIN32)
ELSE (CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET (_GENERICSTACK_POSSIBLE_LIB_SUFFIXES lib)
  SET (_GENERICSTACK_POSSIBLE_BIN_SUFFIXES bin)
ENDIF (CMAKE_SIZEOF_VOID_P EQUAL 8)

FIND_PATH (GENERICSTACK_ROOT_DIR
  NAMES include/genericStack.h
  PATHS ${_GENERICSTACK_POSSIBLE_DIRS}
  DOC "genericStack root directory")

IF (GENERICSTACK_ROOT_DIR)
  # Re-use the previous path:
  FIND_PATH (GENERICSTACK_INCLUDE_DIR
    NAMES genericStack.h
    PATHS ${GENERICSTACK_ROOT_DIR}
    PATH_SUFFIXES ${_GENERICSTACK_POSSIBLE_INCLUDE_SUFFIXES}
    DOC "genericStack include directory"
    NO_DEFAULT_PATH)
ELSE (GENERICSTACK_ROOT_DIR)
  # Use default path search
  FIND_PATH (GENERICSTACK_INCLUDE_DIR
    NAMES genericStack.h
    DOC "genericStack include directory"
    )
ENDIF (GENERICSTACK_ROOT_DIR)

IF (GENERICSTACK_INCLUDE_DIR)
    SET (GENERICSTACK_INCLUDE_DIRS "${GENERICSTACK_INCLUDE_DIR}")
ENDIF (GENERICSTACK_INCLUDE_DIR)

MARK_AS_ADVANCED (GENERICSTACK_ROOT_DIR GENERICSTACK_INCLUDE_DIR)

FIND_PACKAGE_HANDLE_STANDARD_ARGS (GENERICSTACK
  REQUIRED_VARS
  GENERICSTACK_INCLUDE_DIR
  )

IF(GENERICSTACK_FOUND)
  MESSAGE(STATUS "-----------------------------------------")
  MESSAGE(STATUS "Setup GENERICSTACK:")
  MESSAGE(STATUS "")
  MESSAGE(STATUS "           ROOT_DIR: ${GENERICSTACK_ROOT_DIR}")
  MESSAGE(STATUS "        INCLUDE_DIR: ${GENERICSTACK_INCLUDE_DIR}")
  MESSAGE(STATUS "-----------------------------------------")
ENDIF()

MARK_AS_ADVANCED (GENERICSTACK_FOUND)
