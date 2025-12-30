cmake_minimum_required(VERSION 3.28)
project(uSEEDLinkToRingServer VERSION 0.0.1 LANGUAGES C CXX)
enable_testing()

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

include(GenerateExportHeader)
include(FetchContent)

configure_file(${CMAKE_SOURCE_DIR}/src/version.hpp.in
               ${CMAKE_SOURCE_DIR}/include/uSEEDLinkToRingServer/version.hpp)

FetchContent_Declare(
  readerwriterqueue
  GIT_REPOSITORY    https://github.com/cameron314/readerwriterqueue
  GIT_TAG           3f4f80f4efb840a4483f1b80869d61811f9e8398 
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(readerwriterqueue)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(Boost_USE_STATIC_LIBS ON)
if (NOT ${BUILD_SHARED_LIBS})
   set(ZLIB_USE_STATIC_LIBS ON)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
find_package(spdlog REQUIRED)
find_package(libmseed REQUIRED)
find_package(SEEDLink REQUIRED)
find_package(DataLink REQUIRED)
find_package(opentelemetry-cpp REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)
find_package(Catch2)
find_package(Threads REQUIRED)

set(LIBRARY_SRC
    src/dataLinkClient.cpp
    src/dataLinkClientOptions.cpp
    src/packet.cpp
    src/seedLinkClient.cpp
    src/seedLinkClientOptions.cpp
    src/streamIdentifier.cpp
    src/streamSelector.cpp
    src/version.cpp)
if (BUILD_SHARED_LIBS)
   add_library(libuSEEDLinkToRingServer SHARED ${LIBRARY_SRC})
else()
   add_library(libuSEEDLinkToRingServer STATIC ${LIBRARY_SRC})
endif()
set_target_properties(libuSEEDLinkToRingServer PROPERTIES
                      CXX_STANDARD 23
                      CXX_STANDARD_REQUIRED YES 
                      CXX_EXTENSIONS NO 
                      OUTPUT_NAME uSEEDLinkToRingServer)
target_sources(libuSEEDLinkToRingServer
               PUBLIC
               FILE_SET HEADERS
               BASE_DIRS
                  ${CMAKE_CURRENT_SOURCE_DIR}
               FILES 
                  include/uSEEDLinkToRingServer/dataLinkClient.hpp
                  include/uSEEDLinkToRingServer/dataLinkClientOptions.hpp
                  include/uSEEDLinkToRingServer/packet.hpp
                  include/uSEEDLinkToRingServer/seedLinkClient.hpp
                  include/uSEEDLinkToRingServer/seedLinkClientOptions.hpp
                  include/uSEEDLinkToRingServer/streamIdentifier.hpp
                  include/uSEEDLinkToRingServer/streamSelector.hpp
                  include/uSEEDLinkToRingServer/version.hpp)
target_link_libraries(libuSEEDLinkToRingServer
                      PRIVATE readerwriterqueue
                              opentelemetry-cpp::prometheus_exporter
                              mseed::mseed_static
                              SEEDLink::SEEDLink DataLink::DataLink
                              Boost::boost Threads::Threads)
target_include_directories(libuSEEDLinkToRingServer
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
                           PUBLIC  $<INSTALL_INTERFACE:include>)
add_library(uSEEDLinkToRingServer::libuSEEDLinkToRingServer ALIAS libuSEEDLinkToRingServer)



add_executable(seedLinkToRingServer
               src/seedLinkToRingServer.cpp
               )
set_target_properties(seedLinkToRingServer PROPERTIES
                      CXX_STANDARD 23
                      CXX_STANDARD_REQUIRED YES
                      CXX_EXTENSIONS NO)
target_include_directories(seedLinkToRingServer
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
                                   $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>)
target_link_libraries(seedLinkToRingServer
                      PRIVATE uSEEDLinkToRingServer::libuSEEDLinkToRingServer
                              readerwriterqueue
                              opentelemetry-cpp::prometheus_exporter
                              mseed::mseed_static
                              Threads::Threads
                              Boost::boost Boost::program_options)

##########################################################################################
#                                         Tests                                          #
##########################################################################################

if (${Catch2_FOUND})
   add_executable(unitTests
                  testing/packet.cpp
                  testing/seedLink.cpp
                  )
   set_target_properties(unitTests PROPERTIES
                         CXX_STANDARD 23
                         CXX_STANDARD_REQUIRED YES
                         CXX_EXTENSIONS NO)
   target_link_libraries(unitTests 
                         PRIVATE uSEEDLinkToRingServer::libuSEEDLinkToRingServer
                                 Threads::Threads
                                 mseed::mseed_static
                                 Catch2::Catch2 Catch2::Catch2WithMain)
   target_include_directories(unitTests
                              PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                              PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/data>
                              PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
                              PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>)
   add_test(NAME unitTests
            COMMAND unitTests)
endif()


##########################################################################################
#                                      Installation                                      #
##########################################################################################
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/${PROJECT_NAME}Config.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION "${version}"
    COMPATIBILITY AnyNewerVersion
)

install(TARGETS libuSEEDLinkToRingServer
        EXPORT ${PROJECT_NAME}-targets
        FILE_SET HEADERS
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT libraries)
install(TARGETS seedLinkToRingServer
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT applications)
#export(EXPORT ${PROJECT_NAME}-targets
#       FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")
install(EXPORT ${PROJECT_NAME}-targets
        NAMESPACE ${PROJECT_NAME}::
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

##########################################################################################
#                                     CPack Packaging                                    #
##########################################################################################
include(CPackComponent)
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR "UUSS")
set(CPACK_PACKAGE_CONTACT "ben.baker@utah.edu")
set(CPACK_PACKAGE_LICENSE "MIT")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A tool for import (and monitoring) of seismic data via SEEDLink.  Imported data is deposited in a RingServer ring.")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
cpack_add_component(libraries
                    DISPLAY_NAME "uSEEDLinkToRingServer common library")
cpack_add_component(headers
                    DISPLAY_NAME "uSEEDLinkToRingSErver library header files"
                    DEPENDS libraries)
set(CPACK_GENERATOR ZIP TGZ)
find_program(RPMBUILD_PROGRAM rpmbuild)
if (RPMBUILD_PROGRAM)
   list(APPEND CPACK_GENERATOR RPM)
   #set(CPACK_RPM_PACKAGE_REQUIRES "${CPACK_RPM_PACKAGE_REQUIRES abc >= 123")
endif()
if (${BUILD_FOR_DEBIAN})
   find_program(DPKG_PROGRAM dpkg REQUIRED)
else()
   find_program(DPKG_PROGRAM dpkg)
endif()
if (DPKG_PROGRAM)
   list(APPEND CPACK_GENERATOR DEB)
   set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}")
   set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON) 
endif()
set(CPACK_SOURCE_IGNORE_FILES
  /\\.git/
  \\.swp
  \\.orig
  /CMakeLists\\.txt\\.user
  /private/
)
include(CPack) # Put this last!

