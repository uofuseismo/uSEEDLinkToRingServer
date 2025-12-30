#.rst:
# FindDataLink
# ------------
# 
# Finds the DataLink library and include.
#
# Imported targets
# ^^^^^^^^^^^^^^^^
#
# ``DataLink_FOUND``
#   True indicates DataLink was found.
# ``DataLink_VERSION_STRING``
#   The version found.
# ``DataLink::DataLink``
#   The DataLink library, if found. 

# Already in cache, be silent
if (DataLink_INCLUDE_DIR AND DataLink_LIBRARY)
    set(DataLink_FIND_QUIETLY TRUE)
endif()

# Find the include directory
find_path(DataLink_INCLUDE_DIR
          NAMES libdali.h
          PATHS /usr/local/include
                /usr/include
                "$ENV{DataLink_ROOT}/include"
                "$ENV{DataLink_ROOT}")
# Find the library components
if (BUILD_SHARED_LIBS)
   message("Looking for libdali shared library")
   find_library(DataLink_LIBRARY
                NAMES libdali.so
                PATHS /usr/local/lib
                      /usr/local/lib64
                      "$ENV{DataLink_ROOT}/lib/"
                      "$ENV{DataLink_ROOT}/"
                )
else()
   message("Looking for libdali static library")
   find_library(DataLink_LIBRARY
                NAME libdali.a
                PATHS /usr/local/lib
                      /usr/local/lib64
                      "$ENV{DataLink_ROOT}/lib/"
                      "$ENV{DataLink_ROOT}/"
               )
endif()

file(STRINGS ${DataLink_INCLUDE_DIR}/libdali.h LIBDALI_HEADER_DATA)
set(DataLink_VERSION "")
while (LIBDALI_HEADER_DATA)
  list(POP_FRONT LIBDALI_HEADER_DATA LINE)
  if (LINE MATCHES "#define LIBDALI_VERSION")
     #message("Found libdali version line: " ${LINE})
     string(REPLACE "#define LIBDALI_VERSION " "" LINE ${LINE})
     string(REPLACE "\"" "" LINE ${LINE})
     string(REPLACE "/**< libdali version */" "" LINE ${LINE})
     string(STRIP ${LINE} LINE)
     set(LINE_LIST ${LINE})
     list(GET LINE_LIST 0 DataLink_VERSION)
     message("Extracted libdali version ${DataLink_VERSION}")
     break()
  endif()
endwhile()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DataLink
                                  FOUND_VAR DataLink_FOUND
                                  REQUIRED_VARS DataLink_LIBRARY DataLink_INCLUDE_DIR
                                  VERSION_VAR ${DataLink_VERSION})
if (DataLink_FOUND AND NOT TARGET DataLink::DataLink)
   add_library(DataLink::DataLink UNKNOWN IMPORTED)
   set_target_properties(DataLink::DataLink PROPERTIES
                         IMPORTED_LOCATION "${DataLink_LIBRARY}"
                         INTERFACE_INCLUDE_DIRECTORIES "${DataLink_INCLUDE_DIR}")
endif()
mark_as_advanced(DataLink_INCLUDE_DIR DataLink_LIBRARY)
