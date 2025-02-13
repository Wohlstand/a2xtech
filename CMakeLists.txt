cmake_minimum_required(VERSION 3.5...3.10)

if(POLICY CMP0069) # Allow CMAKE_INTERPROCEDURAL_OPTIMIZATION (lto) to be set
    cmake_policy(SET CMP0069 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
endif()

project(TheXTech LANGUAGES C CXX)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(ExternalProject)
include(CheckLibraryExists)
include(CheckFunctionExists)
include(GNUInstallDirs)
include(FindBacktrace)
include(TestBigEndian)
include(CheckIncludeFile)


include(cmake/git_info.cmake)
message("== Current GIT hash [${GIT_COMMIT_HASH}], branch [${GIT_BRANCH}], package [${PACKAGE_SUFFIX}] ==")

if(NOT WIN32 AND NOT NINTENDO_SWITCH AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX))
    check_cxx_compiler_flag("-no-pie" HAS_NO_PIE)
endif()

function(pge_set_nopie _target)
    set_target_properties(${_target} PROPERTIES
        POSITION_INDEPENDENT_CODE False
    )
    if(HAS_NO_PIE AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX))
        set_property(TARGET ${_target} APPEND_STRING PROPERTY LINK_FLAGS " -no-pie")
    endif()
endfunction()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE FALSE)

set(CMAKE_INSTALL_RPATH ".")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

if(EMSCRIPTEN)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

if(APPLE)
    if(CMAKE_HOST_SYSTEM_VERSION VERSION_LESS 9)
        set(XTECH_PLIST_NAME "thextech.plist.tiger.in")
        set(XTECH_DEFAULT_ICNS "resources/tiger/thextech.icns")
    else()
        set(XTECH_PLIST_NAME "thextech.plist.in")
        set(XTECH_DEFAULT_ICNS "resources/thextech.icns")
    endif()

    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
        if(CMAKE_HOST_SYSTEM_VERSION VERSION_LESS 9)
            set(THEXTECH_MACOS_MIN_VERSION "10.4")
        else()
            set(THEXTECH_MACOS_MIN_VERSION "10.9")
        endif()
    else()
        set(THEXTECH_MACOS_MIN_VERSION "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()

    message("-> Mac OS X minimal version: ${THEXTECH_MACOS_MIN_VERSION}")
endif()

if(ANDROID)
    set(DEPENDENCIES_INSTALL_DIR ${CMAKE_BINARY_DIR}/output-deps)
    set(FDROID_BUILD OFF CACHE BOOL "Is this build a part of the F-Droid workflow?")
    mark_as_advanced(FDROID_BUILD)
else()
    set(DEPENDENCIES_INSTALL_DIR ${CMAKE_BINARY_DIR}/output)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bin)

foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    message("--> ${OUTPUTCONFIG}")
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

set(PGE_INSTALL_DIRECTORY "TheXTech")

include(cmake/build_props.cmake)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    # Update if necessary
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffloat-store")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffloat-store")
endif()

if(WIN32 AND NOT EMSCRIPTEN)
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()

if(UNIX)
    check_include_file(/opt/vc/include/bcm_host.h BCMHOST_H)
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

if(CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
    add_definitions(-DDEBUG_BUILD)
    if(CMAKE_COMPILER_IS_GNUCXX)
        add_definitions(-D_GLIBCXX_DEBUG=1 -D_GLIBCXX_ASSERTIONS=1)
    endif()
endif()

# Version
include(version.cmake)
# Default GIT version
include(cmake/git_version.cmake)


configure_file(lib/CrashHandler/backtrace.h.in
               generated-include/lib/CrashHandler/backtrace.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated-include)

if(NINTENDO_SWITCH)
    include_directories($ENV{DEVKITPRO}/portlibs/switch/include)
endif()

#if(WIN32 OR EMSCRIPTEN OR HAIKU)
#    set(USE_SYSTEM_LIBS_DEFAULT OFF)
#else()
#    set(USE_SYSTEM_LIBS_DEFAULT ON)
#endif()
set(USE_SYSTEM_LIBS_DEFAULT OFF)

if(HAIKU OR VITA OR NINTENDO_SWITCH OR NINTENDO_WII OR NINTENDO_WIIU OR XTECH_MACOSX_TIGER OR PORTMASTER)
    set(USE_SYSTEM_SDL2_DEFAULT ON)  # Should be used on Haiku, own SDL2 Haiku build on CMake is broken
else()
    set(USE_SYSTEM_SDL2_DEFAULT ${USE_SYSTEM_LIBS_DEFAULT})
endif()

if(NINTENDO_DS)
    set(THEXTECH_ENABLE_TTF_SUPPORT_DEFAULT OFF)
else()
    set(THEXTECH_ENABLE_TTF_SUPPORT_DEFAULT ON)
endif()

if((APPLE AND NOT XTECH_MACOSX_TIGER) OR WIN32 OR (CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    set(THEXTECH_USE_ANGLE_TRANSLATOR_DEFAULT ON)
else()
    set(THEXTECH_USE_ANGLE_TRANSLATOR_DEFAULT OFF)
endif()

option(USE_SYSTEM_LIBS "Use dependent libraries like SDL2, FreeImageLite and MixerX, installed in the system" ${USE_SYSTEM_LIBS_DEFAULT})
option(USE_SYSTEM_SDL2 "Use SDL2 from a system even prefering system libraries" ${USE_SYSTEM_SDL2_DEFAULT})

option(USE_STATIC_LIBC "Link libc and libstdc++ statically" OFF)

if(NINTENDO_3DS OR NINTENDO_WII OR NINTENDO_WIIU OR NINTENDO_DS)
    set(USE_STATIC_LIBC ON)
endif()

if(NINTENDO_3DS)
    option(THEXTECH_CUSTOM_AUDIO_LIBRARY "Use the custom audio library instead of MixerX" OFF)
endif()

if(ANDROID OR VITA OR NINTENDO_DS OR NINTENDO_3DS OR NINTENDO_WII OR NINTENDO_WIIU OR NINTENDO_SWITCH OR PGE_MIN_PORT OR THEXTECH_NO_SDL_BUILD OR PORTMASTER)
    set(THEXTECH_FORCE_FULLSCREEN ON)
else()
    option(THEXTECH_FORCE_FULLSCREEN "Force the game to only use fullscreen mode" OFF)
endif()

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux" AND "${TARGET_PROCESSOR}" MATCHES "arm.*")
    message("-- Desktop OpenGL build will be disabled on unsupported platform (${CMAKE_SYSTEM_NAME} on ${TARGET_PROCESSOR})")
    set(THEXTECH_BUILD_GL_DESKTOP_DEFAULT OFF)
else()
    set(THEXTECH_BUILD_GL_DESKTOP_DEFAULT ON)
endif()

option(THEXTECH_BUILD_GL_DESKTOP_MODERN "On SDL builds, include the OpenGL 2.0+ Core/Compatibility renderer" ${THEXTECH_BUILD_GL_DESKTOP_DEFAULT})
option(THEXTECH_BUILD_GL_ES_MODERN "On SDL builds, include the OpenGL ES 2.0+ renderer" ON)
option(THEXTECH_BUILD_GL_DESKTOP_LEGACY "On SDL builds, include the OpenGL 1.1+ renderer" ${THEXTECH_BUILD_GL_DESKTOP_DEFAULT})
option(THEXTECH_BUILD_GL_ES_LEGACY "On SDL builds, include the OpenGL ES 1.1 renderer" OFF)

option(THEXTECH_USE_ANGLE_TRANSLATOR "On modern OpenGL builds, include the ANGLE shader translator to improve shader support in OpenGL 3.0+ Core/Compatibility profiles" ${THEXTECH_USE_ANGLE_TRANSLATOR_DEFAULT})

option(RANGE_ARR_USE_HEAP "Store data of RangeArr<> template in a heap" OFF)
option(RANGE_ARR_UNSAFE_MODE "Disable all range checks at RangeArr<> template" OFF)

option(ENABLE_ANTICHEAT_TRAP "Enable anti-cheating trap for the \"redigitiscool\" cheat code" OFF)
option(ENABLE_OLD_CREDITS "Use original Redigit's credits without changes" OFF)
option(ENABLE_LOGGING "Enable debug logging written into a file (may not work on some platfors)" ON)

option(THEXTECH_ENABLE_LUA "Enable lua scripting support" OFF)
option(THEXTECH_ENABLE_LUAU "Enable luau build testing" OFF)
option(THEXTECH_ENABLE_LUNA_AUTOCODE "Enable LunaDLL Autocode scripting support" ON)

option(THEXTECH_ENABLE_EDITOR "Enable runtime support for editor features" ON)

option(THEXTECH_ENABLE_TTF_SUPPORT "Enable TTF fonts support (FreeType required)" ${THEXTECH_ENABLE_TTF_SUPPORT_DEFAULT})

option(THEXTECH_CLI_BUILD "Minimal CLI build for testing and benchmarking on desktop platforms" OFF)
option(THEXTECH_NO_SDL_BUILD "Do not use SDL in the CLI build; useful as a starting point for new ports" OFF)

option(THEXTECH_ENABLE_NEW_USER_PATHS "Use new default paths for user directory (experimental)" OFF)
option(THEXTECH_ENABLE_WIP_FEATURES "Enable experimental and incomplete features (disabled by default)" OFF)
mark_as_advanced(THEXTECH_ENABLE_WIP_FEATURES)

option(THEXTECH_NO_BUILD_DATE "Disables the using of the build timestamp in order to make the build reproducible" OFF)

option(THEXTECH_ENABLE_AUDIO_FX "Enable real-time audio effects support" ON)

option(ENABLE_ADDRESS_SANITIZER "Enable the Address Sanitizer GCC feature" OFF)

# ============ Customization ==============
set(LIB_SRC_EXTRA)

set(THEXTECH_PACKAGE_NAME "thextech" CACHE STRING "Name of package archive file")
set(THEXTECH_EXECUTABLE_NAME "thextech" CACHE STRING "Name of executable file")
set(THEXTECH_DIRECTORY_PREFIX "TheXTech" CACHE STRING "Name used for directories on installed targets (should vary across forks, not asset packs)")
set(THEXTECH_INSTALLER_PACKAGE_NAME "${THEXTECH_EXECUTABLE_NAME}" CACHE STRING "The package name for the package manager")

if(APPLE)
    set(THEXTECH_PRELOAD_ENVIRONMENT "" CACHE STRING "Path to resources root to pack")
    option(THEXTECH_PRELOAD_ENVIRONMENT_MANUALLY "Make application look for assets inside the bundle, but don't actually put any assets. The 'Template' app will be produced for manual addition of assets." OFF)
    set(THEXTECH_BUNDLE_NAME "TheXTech" CACHE STRING "Name of bundle folder")
    set(THEXTECH_ICON_NAME "thextech.icns" CACHE STRING "Name of bundle icon file")
    set(THEXTECH_CUSTOM_ICON_PATH "" CACHE STRING "Name of icon file to pack into the bundle")
    if(NOT THEXTECH_CUSTOM_ICON_PATH STREQUAL "")
        message("Use custom macOS icon: ${THEXTECH_CUSTOM_ICON_PATH}")
        list(APPEND LIB_SRC_EXTRA
            "${THEXTECH_CUSTOM_ICON_PATH}"
        )
    endif()
endif()

if(NOT APPLE AND NOT ANDROID AND NOT EMSCRIPTEN)
    set(THEXTECH_UNIX_INSTALL TRUE)
endif()

# ============ Customization ==end=========

# ============ Platform-conf ==============

if(EMSCRIPTEN)
    # config for web app deployment manifest
    set(THEXTECH_MANIFEST_NAME "TheXTech Engine ${THEXTECH_VERSION_STRING}" CACHE STRING "Web app manifest name, used as title after installation")
    set(THEXTECH_MANIFEST_ID "wohlsoft-thextech" CACHE STRING "Web app ID, uniquely identifies installed app")
    set(THEXTECH_MANIFEST_DESC "" CACHE STRING "Web app description")
    set(THEXTECH_DEPLOY_URL "http://localhost:8080/" CACHE STRING "Fully qualified HTTPS URL where application will be deployed (HTTP may only be used for local testing)")

    # versioned ID
    if(GIT_BRANCH STREQUAL "main")
        set(GIT_BRANCH_IF_NOT_MAIN "")
    else()
        set(GIT_BRANCH_IF_NOT_MAIN "-${GIT_BRANCH}")
    endif()

    set(THEXTECH_MANIFEST_ID_V "${THEXTECH_MANIFEST_ID}${GIT_BRANCH_IF_NOT_MAIN}${THEXTECH_VERSION_REL}")

    string(CONFIGURE "${THEXTECH_MANIFEST_NAME}" THEXTECH_MANIFEST_NAME_OUT)
    string(CONFIGURE "${THEXTECH_MANIFEST_ID_V}"   THEXTECH_MANIFEST_ID_OUT)
    string(CONFIGURE "${THEXTECH_MANIFEST_DESC}" THEXTECH_MANIFEST_DESC_OUT)
endif()

if(VITA)
    message("Enabling PS Vita Support and fast-math flags.")

    # Fast Math flags for Vita, ensuring -DVITA is passed to the compiler.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffast-math -mtune=cortex-a9 -mfpu=neon -DVITA")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -mtune=cortex-a9 -mfpu=neon -DVITA")

    # Disable annoying warning for Parameter passing.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")

    set(THEXTECH_BUILD_GL_DESKTOP_MODERN OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_DESKTOP_LEGACY OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_LEGACY OFF CACHE BOOL "" FORCE)

    # note: this needs to be removed, and USE_SYSTEM_SDL2 needs to be disabled, to test Vita GLESv2 support
    set(THEXTECH_BUILD_GL_ES_MODERN OFF CACHE BOOL "" FORCE)
endif()

if(NINTENDO_SWITCH)
    message("Enabling Nintendo Switch Support and fast-math flags.")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -DNINTENDO_SWITCH -D__SWITCH__")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -DNINTENDO_SWITCH -D__SWITCH__")
endif()

if(NINTENDO_WII OR NINTENDO_3DS OR NINTENDO_DS)
    set(PGE_MIN_PORT ON)
    set(THEXTECH_NO_ARGV_HANDLING ON)
    add_compile_definitions(PGE_MIN_PORT)

    set(THEXTECH_BUILD_GL_DESKTOP_MODERN OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_DESKTOP_LEGACY OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_MODERN OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_LEGACY OFF CACHE BOOL "" FORCE)
endif()

if(NINTENDO_WIIU)
    set(THEXTECH_NO_ARGV_HANDLING ON)
    set(THEXTECH_BUILD_GL_DESKTOP_MODERN OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_DESKTOP_LEGACY OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_MODERN OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_LEGACY OFF CACHE BOOL "" FORCE)
endif()

if(UNIX)
    option(PORTMASTER "Make a special build for PortMaster" OFF)
endif()

if(PORTMASTER)
    message("Special build for PortMaster.")

    set(THEXTECH_BUILD_GL_DESKTOP_MODERN OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_DESKTOP_LEGACY OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_MODERN ON CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_LEGACY OFF CACHE BOOL "" FORCE)
    set(USE_SYSTEM_SDL2 ON CACHE BOOL "" FORCE)
    set(PGE_SHARED_SDLMIXER OFF CACHE BOOL "" FORCE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

if(THEXTECH_CLI_BUILD)
    set(THEXTECH_BUILD_GL_DESKTOP_MODERN OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_DESKTOP_LEGACY OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_MODERN OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_LEGACY OFF CACHE BOOL "" FORCE)
endif()

if(THEXTECH_CLI_BUILD OR PGE_MIN_PORT OR EMSCRIPTEN OR PORTMASTER)
    set(RANGE_ARR_UNSAFE_MODE ON)
endif()

if(NINTENDO_DS)
    set(THEXTECH_NO_SDL_BUILD ON CACHE BOOL "" FORCE)
endif()

if(ANDROID)
    set(THEXTECH_BUILD_GL_DESKTOP_MODERN OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_DESKTOP_LEGACY OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_LEGACY ON CACHE BOOL "")
endif()

if(APPLE OR WIN32)
    set(THEXTECH_BUILD_GL_ES_MODERN OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_LEGACY OFF CACHE BOOL "" FORCE)
endif()

# 32-bit Android special case, temporarily disabled for some experiments
# if(ANDROID AND CMAKE_SIZEOF_VOID_P EQUAL 4)
#     set(THEXTECH_BUILD_GL_ES_MODERN OFF CACHE BOOL "" FORCE)
# endif()

if(EMSCRIPTEN)
    set(THEXTECH_BUILD_GL_DESKTOP_MODERN OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_DESKTOP_LEGACY OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_LEGACY      OFF CACHE BOOL "" FORCE)
endif()

if(NINTENDO_SWITCH)
    set(THEXTECH_BUILD_GL_ES_MODERN      OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_ES_LEGACY      OFF CACHE BOOL "" FORCE)
    set(THEXTECH_BUILD_GL_DESKTOP_LEGACY OFF CACHE BOOL "" FORCE)
endif()

if(THEXTECH_NO_SDL_BUILD)
    set(THEXTECH_ENABLE_AUDIO_FX OFF CACHE BOOL "" FORCE)
    set(MBEDISO_THREADS "NONE")
else()
    set(MBEDISO_THREADS "SDL2")
endif()

if(EMSCRIPTEN)
    message("Is EMSCRIPTEN!")
    set(PGE_PRELOAD_ENVIRONMENT "/home/vitaly/.PGE_Project/_emscripten/thextech" CACHE STRING "Path to resources root to pack")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
#    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse -msse2")
    # -DIS_CXX -s USE_PTHREADS=1
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s WASM=1 -s 'BINARYEN_METHOD=\"native-wasm\"'")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s EMTERPRETIFY=1 -s EMTERPRETIFY_ASYNC=1  -s 'EMTERPRETIFY_FILE=\"pge_engine.binary\"'")
    # -s \"EMTERPRETIFY_WHITELIST=['_main']\"
    if(CMAKE_BUILD_TYPE_LOWER STREQUAL "debug" OR CMAKE_BUILD_TYPE_LOWER STREQUAL "relwithdebinfo")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ASSERTIONS=1")
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_SDL=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=0")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s \"DEFAULT_LIBRARY_FUNCS_TO_INCLUDE=['\\$autoResumeAudioContext', '\\$dynCall']\"") #-s USE_SDL=2
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s LLD_REPORT_UNDEFINED -s ERROR_ON_UNDEFINED_SYMBOLS=1 -s ASYNCIFY=1 -lidbfs.js")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s \"EXPORTED_FUNCTIONS=['_main', '_unlockLoadingCustomState']\"")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s 'BINARYEN_TRAP_MODE=\"clamp\"'")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s TOTAL_MEMORY=83886080 --no-heap-copy -s ALLOW_MEMORY_GROWTH=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s EXPORTED_RUNTIME_METHODS=ccall")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s BINARYEN_EXTRA_PASSES=coalesce-locals")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s MAX_WEBGL_VERSION=2")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s GL_ENABLE_GET_PROC_ADDRESS")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s \"EXTRA_EXPORTED_RUNTIME_METHODS=['Pointer_stringify']\"")
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --embed-file \"${_LANGAUGES_TEMP_FOLDER}\"@\"languages/\"")
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file \"${PGE_PRELOAD_CONFIG_PACK}\"@\"configs/${PGE_PRELOAD_CONFIG_PACK_NAME}/\"")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file \"${PGE_PRELOAD_ENVIRONMENT}\"@\"/\"")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --shell-file '${CMAKE_CURRENT_SOURCE_DIR}/resources/emscripten/shell_minimal.html'")
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --use-preload-cache")
    # set(LINK_FLAGS "${LINK_FLAGS} -DIS_LINK -s USE_PTHREADS=1 -s FORCE_FILESYSTEM=1 --embed-file '${CMAKE_CURRENT_SOURCE_DIR}/@languages'")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions") # bump to -fwasm-exceptions once widely supported
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fexceptions") # bump to -fwasm-exceptions once widely supported
endif()

# ============ Platform-conf ==end=========

option(PGEFL_QT_SUPPORT "Build PGE-FL with Qt support [Unneeded]" OFF)
set(Moondust_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}) # Don't install PGE-FL libraries
set(PGEFL_ENABLE_RWOPS ON)

if(PGE_MIN_PORT)
    set(PGEFL_DISABLE_SMBX38A ON)
endif()

add_subdirectory(3rdparty/PGE_File_Formats)
mark_as_advanced(WITH_UNIT_TESTS PGEFL_QT_SUPPORT)

if(USE_STATIC_LIBC)
    set(BUILD_SHARED_LIBS OFF)
endif()

include(lib/Allocator/pool-allocator.cmake)
include(lib/pge_video_rec/pge-video-rec.cmake)
include(3rdparty/DirManager/dirman.cmake)

if(NOT VITA AND NOT NINTENDO_SWITCH AND NOT NINTENDO_3DS AND NOT NINTENDO_WII AND NOT NINTENDO_WIIU AND NOT NINTENDO_DS)
    include(3rdparty/FileMapper/FileMapper.cmake)
endif()

include(lib/fmt/fmt.cmake)
include(3rdparty/IniProcessor/IniProcessor.cmake)
include(lib/Utils/Utils.cmake)
include(lib/Logger/logger.cmake)
include(lib/AppPath/app_path.cmake)
include(lib/tclap/tclap.cmake)
include(lib/md5/md5.cmake)
include(lib/sdl_proxy/sdl_proxy.cmake)
if(WIN32)
    include(lib/CrashHandler/StackWalker/StackWalker.cmake)
endif()

include(cmake/library_zlib.cmake)

add_definitions("-DMOONDUST_LOGGER_FILENAME_PREFIX=\"TheXTech\"")

if(THEXTECH_ENABLE_TTF_SUPPORT)
#    option(THEXTECH_ENABLE_TTF_HARFBUZZ "Enable the HarfBuzz library use" OFF)
#    mark_as_advanced(THEXTECH_ENABLE_TTF_HARFBUZZ) # Temporarily disable
#    if(THEXTECH_ENABLE_TTF_HARFBUZZ AND NOT NINTENDO_3DS)
#        include(cmake/library_HarfBuzz.cmake)
#    else()
#        set(FT_DISABLE_HARFBUZZ ON)
#    endif()
    set(FT_DISABLE_HARFBUZZ ON)
    include(cmake/library_FreeType.cmake)
endif()

if(PGE_MIN_PORT)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ENABLE_UBSAN OFF CACHE BOOL "" FORCE)
endif()

if(THEXTECH_ENABLE_LUA)
    include(cmake/library_luajit.cmake)
    include(cmake/library_luabind.cmake)
endif()

if(NOT THEXTECH_CLI_BUILD AND NOT THEXTECH_NO_SDL_BUILD)
    include(cmake/library_discord_rpc.cmake)
endif()

set(A2XT_LIBS pgefl ${UTILS_LIBS})
set(A2XT_INCS src lib 3rdparty)
set(A2XT_DEPS)

add_library(A2XT_Int INTERFACE)

if(THEXTECH_NO_SDL_BUILD)
    add_subdirectory(lib/sdl_proxy_rwops)
    get_target_property(SDL_PROXY_RWOPS_INCS sdl_proxy_rwops INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(pgefl PRIVATE ${SDL_PROXY_RWOPS_INCS})
    add_dependencies(pgefl sdl_proxy_rwops)
    target_link_libraries(A2XT_Int INTERFACE sdl_proxy_rwops)
endif()

if(THEXTECH_ENABLE_TTF_SUPPORT)
    list(APPEND A2XT_DEPS FREETYPE_Local)
    list(APPEND A2XT_LIBS PGE_FreeType)
    if(TARGET PGE_HarfBuzz)
        list(APPEND A2XT_DEPS HARFBUZZ_Local)
        list(APPEND A2XT_LIBS PGE_HarfBuzz)
    endif()
endif()

if(THEXTECH_ENABLE_LUA)
    list(APPEND A2XT_DEPS LuaBind_Local)
    list(APPEND A2XT_INCS script/include)
endif()

include(cmake/library_glew.cmake)
include(cmake/library_syslibs.cmake)

if(USE_SYSTEM_LIBS)
    find_package(SDL2 REQUIRED)

    find_library(MIXERX_LIB NAMES SDL2_mixer_ext)
    find_path(MIXERX_HEAD_DIR SDL_mixer_ext.h PATH_SUFFIXES SDL2)

    find_library(FREEIMAGELITE_LIB NAMES FreeImageLite)
    find_path(FREEIMAGELITE_HEAD_DIR NAMES FreeImageLite.h)

    if(MINGW AND NOT SDL2_LIBRARIES)
        set(SDL2_LIBRARIES mingw32 SDL2main SDL2)
    endif()

    list(APPEND A2XT_LIBS ${FREEIMAGELITE_LIB} ${MIXERX_LIB} "${SDL2_LIBRARIES}" ${THEXTECH_SYSLIBS})
    list(APPEND A2XT_INCS ${SDL2_INCLUDE_DIRS} ${MIXERX_HEAD_DIR} ${FREEIMAGELITE_HEAD_DIR})
    target_include_directories(pgefl PRIVATE ${SDL2_INCLUDE_DIRS})
elseif(NOT THEXTECH_NO_SDL_BUILD)
    include(cmake/library_FreeImage.cmake)
    list(APPEND A2XT_DEPS FreeImage_Local)
    list(APPEND A2XT_LIBS PGE_FreeImage)

    include(cmake/library_SDLMixerX.cmake)
    list(APPEND A2XT_INCS "${DEPENDENCIES_INSTALL_DIR}/include" ${SDL2_INCLUDE_DIRS})
    target_include_directories(pgefl PRIVATE ${SDL2_INCLUDE_DIRS})

    # depend on RWops from AudioCodecs_Local SDL2 build
    add_dependencies(pgefl AudioCodecs_Local)

    if(NOT USE_SYSTEM_ZLIB) # Depend on ZLib at AudioCodecs
        if(TARGET FREETYPE_Local)
            add_dependencies(FREETYPE_Local AudioCodecs_Local)
        endif()
        add_dependencies(FreeImage_Local AudioCodecs_Local)
    endif()

    if(NOT THEXTECH_NO_MIXER_X)
        list(APPEND A2XT_DEPS SDLMixerX_Local)
    else()
        list(APPEND A2XT_DEPS AudioCodecs_Local)
    endif()

    if(PGE_SHARED_SDLMIXER)
        list(APPEND A2XT_LIBS PGE_SDLMixerX)
    else()
        list(APPEND A2XT_LIBS PGE_SDLMixerX_static)
    endif()

# UNIX seems to be too general -- how to target desktop Linux specifically?
#    if(UNIX AND (THEXTECH_BUILD_GL_DESKTOP_MODERN OR THEXTECH_BUILD_GL_DESKTOP_LEGACY OR THEXTECH_BUILD_GL_ES_MODERN OR THEXTECH_BUILD_GL_ES_LEGACY))
#        list(APPEND A2XT_LIBS GL)
#    endif()

    if(THEXTECH_ENABLE_VENDORED_GLEW)
        list(APPEND A2XT_DEPS GLEW_Local)
        list(APPEND A2XT_LIBS PGE_GLEW)
    endif()

    if(WIN32 AND (THEXTECH_BUILD_GL_DESKTOP_MODERN OR THEXTECH_BUILD_GL_DESKTOP_LEGACY))
        # glu32 not needed or provided on ARM
        if("${TARGET_PROCESSOR}" STREQUAL "i386" OR "${TARGET_PROCESSOR}" STREQUAL "x86_64")
            list(APPEND A2XT_LIBS glu32)
        endif()

        list(APPEND A2XT_LIBS opengl32 gdi32)
    endif()

    if(PGE_SHARED_SDLMIXER)
        if(NOT WIN32 AND NOT EMSCRIPTEN AND NOT APPLE AND NOT ANDROID AND NOT NINTENDO_SWITCH)
            if(THEXTECH_BUILD_GL_ES_MODERN AND NOT THEXTECH_BUILD_GL_DESKTOP_MODERN AND NOT THEXTECH_BUILD_GL_DESKTOP_LEGACY AND NOT THEXTECH_BUILD_GL_ES_LEGACY)
                list(APPEND A2XT_LIBS GLESv2)
            else()
                find_library(_LIB_GL GL)
                if(_LIB_GL)
                    list(APPEND A2XT_LIBS ${_LIB_GL})
                endif()
            endif()
        else()
            list(APPEND A2XT_LIBS shell32)
        endif()
    endif()

    if(THEXTECH_CUSTOM_AUDIO_LIBRARY)
        list(APPEND A2XT_LIBS gme vorbisidec ogg)
    endif()

    list(APPEND A2XT_LIBS PGE_ZLib)
endif()

if(THEXTECH_ENABLE_DISCORD_RPC)
    list(APPEND A2XT_DEPS DiscordPRC_Local)
    list(APPEND A2XT_LIBS PGE_DiscordRPC)
    set(THEXTECH_ENABLE_INTEGRATOR ON)  # This feature needs for an Integrator
endif()

if(PGE_ENABLE_VIDEO_REC)
    list(APPEND A2XT_LIBS ${VIDEO_REC_LIBS})
    list(APPEND A2XT_DEPS ${VIDEO_REC_DEPS})
    list(APPEND A2XT_INCS ${VIDEO_REC_INCS})
endif()

if(THEXTECH_BUILD_GL_DESKTOP_MODERN AND THEXTECH_USE_ANGLE_TRANSLATOR)
    add_subdirectory(3rdparty/angle-shader-translator)

    message("== Building ANGLE shader translator ==")
    # suppress some known warnings on GCC / Clang
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(angle_shader_translator PRIVATE -Wno-unused-parameter)
        target_compile_options(angle_shader_translator PRIVATE -Wno-c++17-attribute-extensions)
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
        target_compile_options(angle_shader_translator PRIVATE -Wno-unused-parameter)
        target_compile_options(angle_shader_translator PRIVATE -Wno-c++17-extensions)
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(angle_shader_translator PRIVATE /wd4100) # unreferenced formal parameter
    endif()

    target_link_libraries(A2XT_Int INTERFACE angle_shader_translator)
endif()

if(THEXTECH_ENABLE_LUAU)
    message("== Building Luau... but it doesn't do anything! ==")
    add_subdirectory(3rdparty/luau EXCLUDE_FROM_ALL)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(Luau.Compiler PRIVATE "-w")
        target_compile_options(Luau.Ast PRIVATE "-w")
        target_compile_options(Luau.VM PRIVATE "-w")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(Luau.Compiler PRIVATE /wd4100) # unreferenced formal parameter
        target_compile_options(Luau.Ast PRIVATE /wd4100) # unreferenced formal parameter
        target_compile_options(Luau.VM PRIVATE /wd4100) # unreferenced formal parameter
    endif()
    target_link_libraries(A2XT_Int INTERFACE Luau.Compiler Luau.VM)
endif()

if(NINTENDO_DS AND CALICO_ROOT)
    add_library(xtbootstrap src/core/16m/boot_16m.c)
    set_property(TARGET xtbootstrap PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    target_link_libraries(A2XT_Int INTERFACE xtbootstrap)
endif()

add_subdirectory(3rdparty/mbediso EXCLUDE_FROM_ALL)
if(NOT USE_SYSTEM_SDL2 AND NOT THEXTECH_NO_SDL_BUILD)
    target_include_directories(mbediso PRIVATE "${DEPENDENCIES_INSTALL_DIR}/include")
    add_dependencies(mbediso AudioCodecs_Local)
endif()
target_link_libraries(A2XT_Int INTERFACE mbediso)
target_compile_definitions(A2XT_Int INTERFACE
    -DPGE_USE_ARCHIVES  # DirMan should use the archives library
)

if(ANDROID)
    set_target_properties(lz4 PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(lz4_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(mbediso PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()


target_link_libraries(A2XT_Int INTERFACE ${A2XT_LIBS})
target_include_directories(A2XT_Int INTERFACE ${A2XT_INCS})

set(LIB_SRC
    lib/util.cpp
    lib/fader.cpp
    lib/Graphics/image_size.cpp
    lib/Graphics/size.cpp
    lib/Archives/archives_rwops.cpp
    lib/Archives/archives_mount.cpp
    lib/Archives/archives_dir.cpp
    ${APPPATH_SRCS}
    ${DIRMANAGER_SRCS}
    ${FMT_SRCS}
    ${MD5_SRCS}
    ${POOLALLOC_SRCS}
    ${INIPROCESSOR_SRCS}
    ${LOGGER_SRCS}
    ${UTILS_SRCS}
    ${LIB_SRC_EXTRA}
    ${SDLPROXY_SRCS}
)

if(NOT THEXTECH_NO_SDL_BUILD)
list(APPEND LIB_SRC
    lib/Graphics/graphics_funcs.cpp
    lib/Graphics/sizef.cpp
    lib/Graphics/rect.cpp
    lib/Graphics/rectf.cpp
    lib/Graphics/point.cpp
    lib/Graphics/pointf.cpp
)
endif()

if(NOT ANDROID
   AND NOT EMSCRIPTEN
   AND NOT VITA
   AND NOT NINTENDO_3DS
   AND NOT NINTENDO_WII
   AND NOT NINTENDO_WIIU
   AND NOT NINTENDO_SWITCH
   AND NOT THEXTECH_CLI_BUILD
   AND NOT PGE_MIN_PORT)
    list(APPEND LIB_SRC
        lib/InterProcess/editor_pipe.h
        lib/InterProcess/editor_pipe.cpp
        lib/InterProcess/intproc.h
        lib/InterProcess/intproc.cpp
        src/capabilities.cpp
    )
    set(THEXTECH_INTERPROC_SUPPORTED ON)
endif()

# Integrations with external applications
if(THEXTECH_ENABLE_INTEGRATOR)
    list(APPEND LIB_SRC
        lib/Integrator/integrator.h
        lib/Integrator/integrator.cpp)

    if(THEXTECH_ENABLE_DISCORD_RPC)
        list(APPEND LIB_SRC
            lib/Integrator/int_discorcrpc.h
            lib/Integrator/int_discorcrpc.cpp
        )
    endif()
endif()

if(NOT NINTENDO_3DS AND NOT NINTENDO_WII AND NOT NINTENDO_WIIU AND NOT NINTENDO_SWITCH AND NOT VITA AND NOT PGE_MIN_PORT)
    list(APPEND LIB_SRC ${FILEMAPPER_SRCS})
    set(THEXTECH_FILEMAPPER_SUPPORTED ON)
endif()


if(NINTENDO_3DS OR NINTENDO_WII OR NOT PGE_MIN_PORT)
    list(APPEND LIB_SRC
        ${STACK_WALKER_SRCS}
        lib/CrashHandler/crash_handler.cpp
    )
    set(THEXTECH_CRASHHANDLER_SUPPORTED ON)
endif()

if(PGE_ENABLE_VIDEO_REC)
    list(APPEND LIB_SRC ${VIDEO_REC_SRCS})
endif()

if(APPLE)
    file(GLOB PGE_FILE_ICONS "${CMAKE_CURRENT_SOURCE_DIR}/resources/file_icons/*.icns")
    list(APPEND LIB_SRC
        ${XTECH_DEFAULT_ICNS}
        ${PGE_FILE_ICONS}
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/PkgInfo
    )

    if(XTECH_MACOSX_TIGER)
        set(THEXTECH_MAC_EXEX_GENERATED "${CMAKE_BINARY_DIR}/generated/TheXTechRun")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/tiger/TheXTechRun.in" "${THEXTECH_MAC_EXEX_GENERATED}_tmp/TheXTechRun")
        file(COPY
            "${THEXTECH_MAC_EXEX_GENERATED}_tmp/TheXTechRun"
            DESTINATION "${CMAKE_BINARY_DIR}/generated/"
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
        list(APPEND LIB_SRC "${THEXTECH_MAC_EXEX_GENERATED}")
    endif()
endif()

set(THEXTECH_SRC
    src/game_main.cpp
    src/globals.cpp
    src/npc.cpp
    src/sound.cpp
    src/sound_spatial.cpp
    src/sound/sound_msgsnd.cpp
    src/frame_timer.cpp
    src/global_dirs.cpp
    src/global_strings.cpp
    src/config/config_base.cpp
    src/config/config_hooks.cpp
    src/config/config_legacy_compat.cpp
    src/config/config_main.cpp
    src/main/world_loop.cpp
    src/main/world_file.cpp
    src/main/game_info.cpp
    src/main/game_loop.cpp
    src/main/gameplay_timer.cpp
    src/main/player_frames.cpp
    src/main/setup_vars.cpp
    src/main/setup_physics.cpp
    src/main/speedrunner.cpp
    src/main/record.cpp
    src/main/game_save.cpp
    src/main/level_save_info.cpp
    src/main/level_medals.cpp
    src/main/main_config.cpp
    src/main/level_file.cpp
    src/main/menu_loop.cpp
    src/main/menu_main.cpp
    src/main/screen_pause.cpp
    src/main/screen_connect.cpp
    src/main/screen_options.cpp
    src/main/screen_quickreconnect.cpp
    src/main/screen_textentry.cpp
    src/main/screen_prompt.cpp
    src/main/screen_progress.cpp
    src/main/hints.cpp
    src/main/game_strings.cpp
    src/main/translate.cpp
    src/main/translate_episode.cpp
    src/main/menu_controls.cpp
    src/main/cheat_code.cpp
    src/main/outro_loop.cpp
    src/main/trees.cpp
    src/main/block_table.cpp
    src/main/asset_pack.cpp
    src/main/screen_asset_pack.cpp
    src/graphics/gfx_update2.cpp
    src/graphics/gfx_update.cpp
    src/graphics/gfx_background.cpp
    src/graphics/gfx_print.cpp
    src/graphics/gfx_message.cpp
    src/graphics/gfx_editor.cpp
    src/graphics/gfx_frame.cpp
    src/graphics/gfx_camera.cpp
    src/graphics/gfx_world.cpp
    src/graphics/gfx_credits.cpp
    src/graphics/gfx_hud.cpp
    src/graphics/gfx_enter_screen.cpp
    src/graphics/gfx_draw_player.cpp
    src/graphics/gfx_special_frames.cpp
    src/graphics/gfx_screen.cpp
    src/graphics/gfx_keyhole.cpp
    src/graphics/gfx_marquee.cpp
    src/control/duplicate.cpp
    src/control/controls.cpp
    src/script/msg_preprocessor.cpp
    src/logic/object_graph.cpp
    src/change_res.cpp
    src/effect.cpp
    src/collision.cpp
    src/load_gfx.cpp
    src/layers.cpp
    src/custom.cpp
    src/graphics.cpp
    src/main.cpp
    src/blocks.cpp
    src/gfx.cpp
    src/frm_main.cpp
    src/player.cpp
    src/rand.cpp
    src/sorting.cpp
    src/screen.cpp
    src/screen_fader.cpp
    src/message.cpp
    src/std_picture.cpp
    src/npc/npc_hit.cpp
    src/npc/npc_kill.cpp
    src/npc/npc_update.cpp
    src/npc/npc_update/npc_movement_logic.cpp
    src/npc/npc_update/npc_block_logic.cpp
    src/npc/npc_update/npc_collide.cpp
    src/npc/npc_update/npc_walking_logic.cpp
    src/npc/npc_update/npc_effects.cpp
    src/npc/npc_update/npc_special_maybe_held.cpp
    src/npc/npc_frames.cpp
    src/npc/npc_bonus.cpp
    src/npc/npc_queues.cpp
    src/npc/section_overlap.cpp
    src/npc/npc_activation.cpp
    src/player/player_update.cpp
    src/player/player_npc_logic.cpp
    src/player/player_block_logic.cpp
    src/player/player_vine_logic.cpp
    src/player/player_screen_logic.cpp
    src/player/player_fairy_logic.cpp
    src/player/player_action_logic.cpp
    src/player/player_movement_logic.cpp
    src/player/player_pinched_logic.cpp
    src/player/player_death_logic.cpp
    src/player/player_vehicle_logic.cpp
    src/player/player_char5_logic.cpp
    src/player/player_warp_logic.cpp
    src/fontman/font_manager.cpp
    src/fontman/font_manager_private.cpp
    src/fontman/raster_font.cpp
    src/fontman/legacy_font.cpp
    src/fontman/hardcoded_font.cpp
    src/fontman/utf8_helpers.cpp
)

if(THEXTECH_ENABLE_TTF_SUPPORT)
    add_definitions(-DTHEXTECH_ENABLE_TTF_SUPPORT)
    list(APPEND THEXTECH_SRC
        src/fontman/ttf_font.cpp
    )
endif()

# Luna Autocode script engine
list(APPEND THEXTECH_SRC
    src/script/luna/lunaglobals.cpp
    src/script/luna/lunacounter.cpp
    src/script/luna/lunacounter_record.cpp
)

if(THEXTECH_ENABLE_LUNA_AUTOCODE)
    add_definitions(-DTHEXTECH_ENABLE_LUNA_AUTOCODE)
    list(APPEND THEXTECH_SRC
        src/script/luna/luna.cpp
        src/script/luna/autocode.cpp
        src/script/luna/autocode_manager.cpp
        src/script/luna/csprite.cpp
        src/script/luna/lunaspriteman.cpp
        src/script/luna/hitbox.cpp
        src/script/luna/lunaimgbox.cpp
        src/script/luna/sprite_component.cpp
        src/script/luna/sprite_funcs.cpp
        src/script/luna/lunalevels.cpp
        src/script/luna/lunaplayer.cpp
        src/script/luna/lunanpc.cpp
        src/script/luna/lunablock.cpp
        src/script/luna/lunalayer.cpp
        src/script/luna/lunamisc.cpp
        src/script/luna/lunalevel.cpp
        src/script/luna/lunarender.cpp
        src/script/luna/lunacell.cpp
        src/script/luna/lunainput.cpp
        src/script/luna/lunavarbank.cpp
        src/script/luna/renderop_bitmap.cpp
        src/script/luna/renderop_rect.cpp
        src/script/luna/renderop_effect.cpp
        src/script/luna/renderop_string.cpp
        src/script/luna/mememu.cpp
        src/script/luna/levels/Docopoper-AbstractAssault.cpp
        src/script/luna/levels/Docopoper-Calleoca.cpp
        src/script/luna/levels/Docopoper-TheFloorisLava.cpp
        src/script/luna/levels/SAJewers-QraestoliaCaverns.cpp
        src/script/luna/levels/SAJewers-Snowboardin.cpp
        src/script/luna/levels/Talkhaus-Science_Final_Battle.cpp
        src/script/luna/levels/KilArmoryCode.cpp
    )
endif()

if(THEXTECH_ENABLE_AUDIO_FX)
    add_definitions(-DTHEXTECH_ENABLE_AUDIO_FX)
    list(APPEND THEXTECH_SRC
        src/sound/fx/spc_echo.cpp
        src/sound/fx/reverb.cpp
    )
endif()

if(THEXTECH_ENABLE_EDITOR)
    add_definitions(-DTHEXTECH_ENABLE_EDITOR)
    list(APPEND THEXTECH_SRC
        src/editor/editor.cpp
        src/editor/new_editor.cpp
        src/editor/write_common.cpp
        src/editor/write_level.cpp
        src/editor/write_world.cpp
        src/editor/editor_custom.cpp
        src/editor/editor_strings.cpp
        src/editor/magic_block.cpp
    )
endif()

if(NINTENDO_3DS)
    add_definitions(-DWINDOW_CUSTOM -DMSGBOX_CUSTOM -DEVENTS_CUSTOM -DRENDER_CUSTOM)
    list(APPEND THEXTECH_SRC
        src/control/input_3ds.cpp
        src/core/minport/render_minport_shared.cpp
        src/core/3ds/render_3ds.cpp
        src/core/3ds/window_3ds.cpp
        src/core/3ds/msgbox_3ds.cpp
        src/core/3ds/events_3ds.cpp
#        src/core/3ds/3ds-audio-lib.cpp
        src/core/sdl/sdl_core.cpp
        src/core/power/power_3ds.cpp
    )
elseif(NINTENDO_WII)
    add_definitions(-DWINDOW_CUSTOM -DMSGBOX_CUSTOM -DEVENTS_CUSTOM -DRENDER_CUSTOM)
    list(APPEND THEXTECH_SRC
        src/control/input_wii.cpp
        src/control/input_wii_gc.cpp
        src/core/minport/render_minport_shared.cpp
        src/core/wii/render_wii.cpp
        src/core/wii/window_wii.cpp
        src/core/wii/msgbox_wii.cpp
        src/core/wii/events_wii.cpp
        src/core/sdl/sdl_core.cpp
        src/core/power/power_wii.cpp
    )
elseif(NINTENDO_DS)
    add_definitions(-DWINDOW_CUSTOM -DMSGBOX_CUSTOM -DEVENTS_CUSTOM -DRENDER_CUSTOM -DTHEXTECH_NO_SDL_CORE)
    list(REMOVE_ITEM THEXTECH_SRC src/sound.cpp src/sound/sound_msgsnd.cpp)

    list(APPEND THEXTECH_SRC
        src/control/input_16m.cpp
        src/core/minport/render_minport_shared.cpp
        src/core/16m/render_16m.cpp
        src/core/16m/msgbox_16m.cpp
        src/core/16m/window_16m.cpp
        src/core/16m/events_16m.cpp
        src/core/16m/sound_16m.cpp
        src/core/16m/sound_stream_16m.cpp
        src/core/power/power_16m.cpp
    )
elseif(THEXTECH_NO_SDL_BUILD)
    add_definitions(-DWINDOW_CUSTOM -DMSGBOX_CUSTOM -DEVENTS_CUSTOM -DRENDER_CUSTOM -DTHEXTECH_NO_SDL_CORE)
    list(APPEND THEXTECH_SRC
        src/core/null/render_null.cpp
        src/core/null/window_null.cpp
        src/core/null/msgbox_null.cpp
        src/core/null/events_null.cpp
        src/core/power/power_null.cpp
    )
elseif(THEXTECH_CLI_BUILD)
    add_definitions(-DWINDOW_CUSTOM -DMSGBOX_CUSTOM -DEVENTS_CUSTOM -DRENDER_CUSTOM)
    list(APPEND THEXTECH_SRC
        src/core/null/render_null.cpp
        src/core/null/window_null.cpp
        src/core/null/msgbox_null.cpp
        src/core/null/events_null.cpp
        src/core/sdl/sdl_core.cpp
        src/core/power/power_null.cpp
    )
else()
    add_definitions(-DCORE_EVERYTHING_SDL)
    list(APPEND THEXTECH_SRC
        src/control/joystick.cpp
        src/control/keyboard.cpp
        src/control/touchscreen.cpp
        src/core/base/render_base.cpp
        src/core/base/window_base.cpp
        src/core/base/msgbox_base.cpp
        src/core/base/events_base.cpp
        src/core/sdl/render_sdl.cpp
        src/core/sdl/window_sdl.cpp
        src/core/sdl/msgbox_sdl.cpp
        src/core/sdl/events_sdl.cpp
        src/core/sdl/sdl_core.cpp
        src/core/power/power_sdl.cpp
    )

    if(NOT THEXTECH_FORCE_FULLSCREEN AND NOT EMSCRIPTEN)
        add_definitions(-DRENDER_FULLSCREEN_TYPES_SUPPORTED)
    endif()

    if(NINTENDO_WIIU)
        list(APPEND THEXTECH_SRC
            src/core/wiiu/msgbox_wiiu.cpp
        )
    else()
        list(APPEND THEXTECH_SRC
            src/core/sdl/msgbox_sdl.cpp
        )
    endif()

    if(THEXTECH_BUILD_GL_DESKTOP_MODERN OR THEXTECH_BUILD_GL_DESKTOP_LEGACY OR THEXTECH_BUILD_GL_ES_MODERN OR THEXTECH_BUILD_GL_ES_LEGACY)
        list(APPEND THEXTECH_SRC
            src/core/opengl/gl_program_object.cpp
            src/core/opengl/render_gl_frontend.cpp
            src/core/opengl/render_gl_backend.cpp
            src/core/opengl/render_gl_init.cpp
            src/core/opengl/render_gl_shaders.cpp
            src/core/opengl/render_gl_shader_lighting.cpp
        )
    endif()

    if(THEXTECH_BUILD_GL_DESKTOP_MODERN OR THEXTECH_BUILD_GL_ES_MODERN)
        list(APPEND THEXTECH_SRC
            src/core/opengl/gl_program_bank.cpp
            src/core/opengl/gl_particle_system.cpp
        )
    endif()

    if(THEXTECH_BUILD_GL_DESKTOP_MODERN AND THEXTECH_USE_ANGLE_TRANSLATOR)
        add_definitions(-DTHEXTECH_USE_ANGLE_TRANSLATOR)
        list(APPEND THEXTECH_SRC
            src/core/opengl/gl_shader_translator.cpp
        )
    endif()

endif()


list(APPEND THEXTECH_SRC src/core/language/language_common.cpp)

# Notes:
# - First, algorithm will try to detect language using SDL's method if available (Supported by SDL 2.0.14 and newer)
# - Then, falls to the system specific code to detect the language
# If there is a necessary to disable use of SDL-based language detection,
# define the macro `THEXTECH_DISABLE_SDL_LOCALE` which will completely disable the SDL-based language detector

if(THEXTECH_NO_SDL_BUILD)
    add_definitions(-DTHEXTECH_DISABLE_SDL_LOCALE)
endif()

if(WIN32)
    list(APPEND THEXTECH_SRC src/core/language/language_win32.cpp)
elseif(APPLE)
    list(APPEND THEXTECH_SRC src/core/language/language_apple.cpp)
elseif(ANDROID)
    list(APPEND THEXTECH_SRC src/core/language/language_android.cpp)
elseif(NINTENDO_DS)
    # FIXME: Implement the Nintendo DS language initializer
    add_definitions(-DTHEXTECH_DISABLE_SDL_LOCALE)
    list(APPEND THEXTECH_SRC src/core/language/language_dummy.cpp)
elseif(NINTENDO_3DS)
    add_definitions(-DTHEXTECH_DISABLE_SDL_LOCALE) # SDL for 3DS doesn't support language detection
    list(APPEND THEXTECH_SRC src/core/language/language_3ds.cpp)
elseif(NINTENDO_WII)
    add_definitions(-DTHEXTECH_DISABLE_SDL_LOCALE) # SDL for Wii doesn't support language detection
    list(APPEND THEXTECH_SRC src/core/language/language_wii.cpp)
elseif(NINTENDO_WIIU)
    add_definitions(-DTHEXTECH_DISABLE_SDL_LOCALE) # SDL for WiiU doesn't support language detection
    list(APPEND THEXTECH_SRC src/core/language/language_wiiu.cpp)
elseif(NINTENDO_SWITCH)
    add_definitions(-DTHEXTECH_DISABLE_SDL_LOCALE) # SDL for Switch doesn't support language detection
    list(APPEND THEXTECH_SRC src/core/language/language_switch.cpp)
elseif(VITA)
    add_definitions(-DTHEXTECH_DISABLE_SDL_LOCALE)
    list(APPEND THEXTECH_SRC src/core/language/language_vita.cpp)
elseif(UNIX)
    list(APPEND THEXTECH_SRC src/core/language/language_unix.cpp)
else()
    add_definitions(-DTHEXTECH_DISABLE_SDL_LOCALE)
    list(APPEND THEXTECH_SRC src/core/language/language_dummy.cpp)
endif()


# Add heads into the list
file(GLOB THEXTECH_HEADS_SRC
    *.h
    lib/*.h
    lib/AppPath/*.h
    lib/CrashHandler/*.h
    lib/Graphics/*.h
    lib/tclap/*.h
    lib/fmt/*.h
    lib/Archives/*.h
    lib/json/*.hpp
    src/*.h
    src/*.hpp
    src/script/*.h
    src/script/luna/*.h
    src/script/luna/level/*.h
    src/control/*.h
    src/core/*.h
    src/core/16m/*.h
    src/core/3ds/*.h
    src/core/base/*.h
    src/core/language/*.h
    src/core/minport/*.h
    src/core/null/*.h
    src/core/opengl/*.h
    src/core/power/*.h
    src/core/sdl/*.h
    src/core/vita/*.h
    src/core/wii/*.h
    src/core/wiiu/*.h
    src/editor/*.h
    src/main/*.h
    src/main/translate/*.h
    src/main/*.hpp
    src/npc/*.h
    src/npc/*.hpp
    src/sound/*.h
    src/sound/fx/*.h
    src/sound/fx/*.hpp
    src/fontman/*.h
)
list(APPEND THEXTECH_SRC ${THEXTECH_HEADS_SRC})

# ----------------------------------------------------

if(WIN32 AND NOT EMSCRIPTEN)
    list(APPEND THEXTECH_SRC resources/thextech.rc)
endif()

if(THEXTECH_ENABLE_LUA)
    add_subdirectory(script)
    target_link_libraries(A2XT_Int INTERFACE XTechLua ${libLuaBind_Lib} ${libLuaJit_Lib})
    if(UNIX)
        find_library(LIB_DL_PATH dl)
        if(LIB_DL_PATH)
            target_link_libraries(A2XT_Int INTERFACE ${LIB_DL_PATH})
        endif()
    endif()
endif()

if(ANDROID)
    set(CMAKE_DEBUG_POSTFIX "")
    add_library(thextech SHARED ${THEXTECH_SRC} ${LIB_SRC})
    target_compile_options(thextech PRIVATE -fPIC)

    # Include own-built libraries into the APK
    set(APK_PACK_LIBS ${CMAKE_SOURCE_DIR}/android-project/thextech/jniLibs/${CMAKE_BUILD_TYPE_LOWER}/${ANDROID_ABI})
    add_custom_target(SDL_ApkPackLibs_makeDir ALL COMMAND ${CMAKE_COMMAND} -E make_directory "${APK_PACK_LIBS}")

    if(PGE_SHARED_SDLMIXER)
#        add_library(SDL2_mixer_ext SHARED IMPORTED DEPENDS SDLMixerX_Local AudioCodecs_Local)
#        set_target_properties(SDL2_mixer_ext PROPERTIES IMPORTED_LOCATION "${DEPENDENCIES_INSTALL_DIR}/lib/libSDL2_mixer_ext.so")
        add_custom_target(SDL_ApkPackLibs_mixerx ALL
            COMMAND ${CMAKE_COMMAND} -E copy "${DEPENDENCIES_INSTALL_DIR}/lib/libSDL2_mixer_ext.so" "${APK_PACK_LIBS}"
            DEPENDS SDL_ApkPackLibs_makeDir ${DEPENDENCIES_INSTALL_DIR}/lib/libSDL2_mixer_ext.so AudioCodecs_Local
        )
        add_dependencies(thextech SDL_ApkPackLibs_mixerx)

#        add_library(SDL2 SHARED IMPORTED DEPENDS AudioCodecs_Local)
#        set_target_properties(SDL2 PROPERTIES IMPORTED_LOCATION "${DEPENDENCIES_INSTALL_DIR}/lib/libSDL2.so")
        add_custom_target(SDL_ApkPackLibs_sdl2 ALL
            COMMAND ${CMAKE_COMMAND} -E copy "${DEPENDENCIES_INSTALL_DIR}/lib/libSDL2.so" "${APK_PACK_LIBS}"
            DEPENDS SDL_ApkPackLibs_makeDir ${DEPENDENCIES_INSTALL_DIR}/lib/libSDL2.so AudioCodecs_Local
        )
        add_dependencies(thextech SDL_ApkPackLibs_sdl2)
    endif()

#    add_library(hidapi SHARED IMPORTED DEPENDS AudioCodecs_Local)
#    set_target_properties(hidapi PROPERTIES IMPORTED_LOCATION "${DEPENDENCIES_INSTALL_DIR}/lib/libhidapi.so")

# libhidapi.so is NO LONGER required since SDL 2.0.18
#    add_custom_target(SDL_ApkPackLibs_hidapi ALL
#        COMMAND ${CMAKE_COMMAND} -E copy "${DEPENDENCIES_INSTALL_DIR}/lib/libhidapi.so" "${APK_PACK_LIBS}"
#        DEPENDS SDL_ApkPackLibs_makeDir "${DEPENDENCIES_INSTALL_DIR}/lib/libhidapi.so" AudioCodecs_Local
#    )
#    add_dependencies(thextech SDL_ApkPackLibs_hidapi)

elseif(NINTENDO_WII OR NINTENDO_WIIU OR NINTENDO_3DS OR NINTENDO_DS)
    add_executable(thextech ${THEXTECH_SRC} ${LIB_SRC})
    set_target_properties(thextech PROPERTIES SUFFIX ".elf")

else()
    add_executable(thextech ${THEXTECH_SRC} ${LIB_SRC})
    pge_set_nopie(thextech)
endif()

add_dependencies(thextech git_version)
target_include_directories(thextech PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated-include")

set_target_properties(thextech PROPERTIES
    OUTPUT_NAME "${THEXTECH_EXECUTABLE_NAME}"
)

if(CMAKE_BUILD_TYPE_LOWER STREQUAL "debug" OR CMAKE_BUILD_TYPE_LOWER STREQUAL "relwithdebinfo")
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_DEBUG_INFO)
endif()

if(APPLE)
    if(XTECH_MACOSX_TIGER)
        set(THEXTECH_EXECUTABLE_NAME_OUT TheXTechRun)
    else()
        set(THEXTECH_EXECUTABLE_NAME_OUT ${THEXTECH_EXECUTABLE_NAME})
    endif()

    set_target_properties(thextech PROPERTIES
        OUTPUT_NAME "${THEXTECH_BUNDLE_NAME}"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/${XTECH_PLIST_NAME}"
        MACOSX_BUNDLE_BUNDLE_NAME "${THEXTECH_BUNDLE_NAME}"
        MACOSX_BUNDLE_EXECUTABLE_NAME "${THEXTECH_EXECUTABLE_NAME_OUT}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "ru.wohlsoft.thextech"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${THEXTECH_VERSION_STRING}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${THEXTECH_VERSION_STRING}"
        MACOSX_BUNDLE_ICON_FILE "${THEXTECH_ICON_NAME}"
        CPACK_BUNDLE_NAME "${THEXTECH_BUNDLE_NAME}"
        MACOSX_BUNDLE_INFO_STRING "TheXTech classic game engine"
    )
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/${XTECH_DEFAULT_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set_source_files_properties(${PGE_FILE_ICONS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/resources/PkgInfo PROPERTIES MACOSX_PACKAGE_LOCATION "")
    if(XTECH_MACOSX_TIGER)
        set_source_files_properties("${THEXTECH_MAC_EXEX_GENERATED}" PROPERTIES MACOSX_PACKAGE_LOCATION "MacOS")
    endif()

    if(THEXTECH_CUSTOM_ICON_PATH AND NOT THEXTECH_CUSTOM_ICON_PATH STREQUAL "")
        set_source_files_properties("${THEXTECH_CUSTOM_ICON_PATH}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    endif()

    if(THEXTECH_PRELOAD_ENVIRONMENT AND NOT THEXTECH_PRELOAD_ENVIRONMENT STREQUAL "")
        add_custom_command(TARGET thextech POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:thextech>/../Resources/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${THEXTECH_PRELOAD_ENVIRONMENT}" "$<TARGET_FILE_DIR:thextech>/../Resources/assets"
            COMMENT "Copying assets into bundle..."
        )
        target_compile_definitions(thextech PRIVATE -DUSE_BUNDLED_ASSETS)
    endif()

    if(THEXTECH_PRELOAD_ENVIRONMENT_MANUALLY)
        target_compile_definitions(thextech PRIVATE -DUSE_BUNDLED_ASSETS)
    endif()

    if(XTECH_MACOSX_TIGER) # Use X11 mode on Mac OS X Tiger
        target_compile_definitions(thextech PRIVATE -DUSE_APPLE_X11)
    endif()

    check_library_exists(iconv iconv_open "" HAVE_LIBICONV)
    if(HAVE_LIBICONV)
        target_link_libraries(A2XT_Int INTERFACE iconv)
    endif()

    if(PGE_USE_LUAJIT)
        # Required to link on 64-bit macOS
        # See: http://luajit.org/install.html
        set_property(TARGET thextech APPEND_STRING PROPERTY
            LINK_FLAGS " -pagezero_size 10000 -image_base 100000000"
        )
    endif()
endif()

if(ANDROID)
    target_link_libraries(A2XT_Int INTERFACE log)

    if(THEXTECH_BUILD_GL_ES_LEGACY OR THEXTECH_BUILD_GL_ES_MODERN)
        target_link_libraries(A2XT_Int INTERFACE EGL)
    endif()
endif()

if(NINTENDO_SWITCH)
    target_link_libraries(A2XT_Int INTERFACE nx)

    if(THEXTECH_BUILD_GL_DESKTOP_MODERN)
        target_link_libraries(A2XT_Int INTERFACE glad)
    endif()

    # Set explicit flags for this project
    target_compile_definitions(thextech PRIVATE -D__SWITCH__ -DNO_INTPTROC)
endif()

if(VITA)
    # Link Vita specific libraries to the generic interface.
    target_link_libraries(A2XT_Int INTERFACE ${VITA_ADDTL_LIBS})

    # Set explicit flags for this project
    target_compile_definitions(thextech PRIVATE
        -DVITA -DNO_SCREENSHOT -DTHEXTECH_PRELOAD_LEVELS -DNO_INTPTROC)
endif()

if(WIN32 AND WINDOWS_STORE)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_WINRT)
endif()

if(NINTENDO_3DS)
    target_compile_definitions(thextech PRIVATE -DPGE_SDL_MUTEX -DPGE_NO_THREADING -DLOW_MEM -DTHEXTECH_PRELOAD_LEVELS -D__3DS__)
elseif(NINTENDO_WII)
    target_compile_definitions(thextech PRIVATE -DPGE_SDL_MUTEX -DLOW_MEM -DTHEXTECH_PRELOAD_LEVELS -D__WII__)
elseif(NINTENDO_WIIU)
    target_compile_definitions(thextech PRIVATE -DPGE_SDL_MUTEX)
elseif(NINTENDO_DS)
    target_link_libraries(thextech PRIVATE mm9)
    target_compile_definitions(thextech PRIVATE -DPGE_NO_THREADING -DLOW_MEM -DTHEXTECH_PRELOAD_LEVELS -D__16M__)

    if(CALICO_ROOT)
        target_compile_definitions(thextech PRIVATE -D__CALICO__)
    elseif(BLOCKSDS)
        target_compile_definitions(thextech PRIVATE -D__BLOCKS__)
    endif()

    if(NOT BLOCKSDS)
        target_link_libraries(thextech PRIVATE fat)
        set(CMAKE_EXE_LINKER_FLAGS "-L${CALICO_ROOT}/lib -L${NDS_ROOT}/lib -specs=${CALICO_ROOT}/share/ds9.specs -T ${CMAKE_CURRENT_SOURCE_DIR}/cmake/overlays-16m.ld")
    endif()
elseif(PGE_MIN_PORT)
    target_compile_definitions(thextech PRIVATE -DLOW_MEM)
endif()

if(VITA OR NINTENDO_3DS OR NINTENDO_WII OR NINTENDO_WIIU OR NINTENDO_SWITCH)
    # Run in-game message box to show assert failures on platforms without SDL's message box
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_ASSERTS_INGAME_MESSAGE)
endif()

if(VITA OR NINTENDO_DS OR NINTENDO_3DS OR NINTENDO_WII OR NINTENDO_SWITCH OR ANDROID OR EMSCRIPTEN)
    # Disable language tools at non-desktop platforms
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_DISABLE_LANG_TOOLS)
endif()

if((UNIX OR NINTENDO_SWITCH OR ANDROID OR APPLE) AND NOT EMSCRIPTEN)
    # Allow hotswapping renderer on fully tested platforms
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_RESTART_RENDERER_SUPPORTED)
endif()

if(THEXTECH_NO_SDL_BUILD)
    target_compile_definitions(thextech PRIVATE -DPGE_NO_THREADING -DTHEXTECH_NO_SDL_BUILD)
endif()

if(THEXTECH_CLI_BUILD)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_CLI_BUILD)
endif()

if(THEXTECH_BUILD_GL_DESKTOP_MODERN)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_BUILD_GL_DESKTOP_MODERN)
endif()

if(THEXTECH_BUILD_GL_ES_MODERN)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_BUILD_GL_ES_MODERN)
endif()

if(THEXTECH_BUILD_GL_DESKTOP_MODERN OR THEXTECH_BUILD_GL_ES_MODERN)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_BUILD_GL_MODERN)
endif()

if(THEXTECH_BUILD_GL_DESKTOP_LEGACY)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_BUILD_GL_DESKTOP_LEGACY)
endif()

if(THEXTECH_BUILD_GL_ES_LEGACY)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_BUILD_GL_ES_LEGACY)
endif()

if(THEXTECH_ENABLE_NEW_USER_PATHS)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_NEW_USER_PATHS)
endif()

if(THEXTECH_ENABLE_WIP_FEATURES)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_WIP_FEATURES)
endif()

target_compile_definitions(thextech PRIVATE -DTHEXTECH_DIRECTORY_PREFIX="${THEXTECH_DIRECTORY_PREFIX}")

if(THEXTECH_NO_BUILD_DATE)
    target_compile_definitions(thextech PRIVATE -DDISABLE_XTECH_BUILD_DATE)
endif()

if(THEXTECH_ENABLE_LUA)
    target_compile_definitions(thextech PRIVATE -DENABLE_XTECH_LUA)
endif()

if(THEXTECH_ENABLE_INTEGRATOR)
    target_compile_definitions(thextech PRIVATE -DENABLE_XTECH_INTEGRATOR)
endif()

if(THEXTECH_ENABLE_DISCORD_RPC)
    target_compile_definitions(thextech PRIVATE -DENABLE_XTECH_DISCORD_RPC -DXTECH_DISCORD_APPID=${THEXTECH_DISCORD_APPID})
endif()

if(PGE_ENABLE_VIDEO_REC)
    target_compile_definitions(thextech PRIVATE -DPGE_ENABLE_VIDEO_REC)

    if(PGE_VIDEO_REC_WEBM_SUPPORTED)
        target_compile_definitions(thextech PRIVATE -DPGE_VIDEO_REC_WEBM_SUPPORTED)
    endif()
endif()

if(ENABLE_ADDRESS_SANITIZER)
    target_compile_options(thextech PRIVATE -fsanitize=address)
    target_link_options(thextech PRIVATE -fsanitize=address)
endif()

target_include_directories(thextech PRIVATE ${A2XT_INCS})
target_link_libraries(thextech PRIVATE A2XT_Int)

if(RANGE_ARR_USE_HEAP)
    target_compile_definitions(thextech PRIVATE -DRANGE_ARR_USE_HEAP)
endif()

if(RANGE_ARR_UNSAFE_MODE)
    target_compile_definitions(thextech PRIVATE -DRANGE_ARR_UNSAFE_MODE)
endif()

if(ENABLE_ANTICHEAT_TRAP)
    target_compile_definitions(thextech PRIVATE -DENABLE_ANTICHEAT_TRAP)
endif()

if(ENABLE_OLD_CREDITS)
    target_compile_definitions(thextech PRIVATE -DENABLE_OLD_CREDITS)
    set_target_properties(thextech PROPERTIES OUTPUT_NAME "smbx")
endif()

if(NOT ENABLE_LOGGING)
    target_compile_definitions(thextech PRIVATE -DDISABLE_LOGGING -DNO_FILE_LOGGING)
endif()

if(EMSCRIPTEN)
    target_compile_definitions(thextech PRIVATE -DPGE_NO_THREADING -DNO_FILE_LOGGING)
endif()

if(THEXTECH_NO_ARGV_HANDLING)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_NO_ARGV_HANDLING)
endif()

if(THEXTECH_INTERPROC_SUPPORTED)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_INTERPROC_SUPPORTED)
endif()

if(THEXTECH_FILEMAPPER_SUPPORTED)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_FILEMAPPER_SUPPORTED)
endif()

if(THEXTECH_CRASHHANDLER_SUPPORTED)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_CRASHHANDLER_SUPPORTED)
endif()

# ======================= Render specific global macros ========================
if(EMSCRIPTEN)
    target_compile_definitions(thextech PRIVATE
        -DNO_WINDOW_FOCUS_TRACKING  # Don't track for window focus
    )
endif()

if(NOT EMSCRIPTEN AND
   NOT NINTENDO_3DS AND
   NOT NINTENDO_WII AND
   NOT NINTENDO_WIIU AND
   NOT PGE_MIN_PORT AND
   NOT THEXTECH_CLI_BUILD)
    target_compile_definitions(thextech PRIVATE
        -DUSE_SCREENSHOTS_AND_RECS  # Built-in screenshots and GIF recording
    )
endif()

if(ANDROID)
    target_compile_definitions(thextech PRIVATE
        -DUSE_RENDER_BLOCKING
    )
endif()

if(THEXTECH_FORCE_FULLSCREEN)
    target_compile_definitions(thextech PRIVATE
        -DRENDER_FULLSCREEN_ALWAYS  # Game works always fullscreen
        -DNO_WINDOW_FOCUS_TRACKING  # Don't track for window focus
    )
endif()

# ==============================================================================

if(NOT USE_SYSTEM_LIBS AND A2XT_DEPS)
    add_dependencies(thextech ${A2XT_DEPS})
endif()

if(THEXTECH_IS_BIG_ENDIAN)
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_BIG_ENDIAN -DSOUND_FX_BIG_ENDIAN)
endif()

if(CMAKE_SIZEOF_VOID_P MATCHES "8")
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_WORDSIZE=64)
elseif(CMAKE_SIZEOF_VOID_P MATCHES "4")
    target_compile_definitions(thextech PRIVATE -DTHEXTECH_WORDSIZE=32)
else()
    message(WARNING "Unknown word size ${CMAKE_SIZEOF_VOID_P}!")
endif()

if(USE_STATIC_LIBC AND NOT USE_SYSTEM_LIBS)
    if(NOT APPLE AND NOT MSVC)
#        target_link_libraries(thextech PRIVATE -static)
        set_property(TARGET thextech APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -static-libstdc++")
    endif()

    if(MSVC)
        target_compile_options(thextech PRIVATE /MT)
        target_link_options(thextech PRIVATE /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT)
    endif()
endif()

if (Backtrace_FOUND)
    set_property(TARGET thextech APPEND_STRING PROPERTY LINK_FLAGS " ${Backtrace_LIBRARIES}")
endif()

if(WIN32)
    set_target_properties(thextech PROPERTIES WIN32_EXECUTABLE ON)
    target_compile_definitions(thextech PRIVATE -DNOMINMAX)
    target_link_libraries(thextech PRIVATE "version" dbghelp) # needed by StackWalker
endif()

include(cmake/deploy.cmake)

if(APPLE)
    install(TARGETS thextech DESTINATION .)
    install(FILES changelog.txt DESTINATION .)
    install(FILES README.md DESTINATION . RENAME ReadMe.txt)
    install(FILES LICENSE DESTINATION . RENAME License.txt)
elseif(WIN32)
    # For MinGW toolchain, copy missing DLLs
    if(MINGW)
        function(find_mingw_dll _FieldName _FileName _DestList _SearchPaths)
            find_file(MINGWDLL_${_FieldName} ${_FileName} PATH_SUFFIXES bin PATHS "${_SearchPaths}")
            if(MINGWDLL_${_FieldName})
                list(APPEND ${_DestList} "${MINGWDLL_${_FieldName}}")
                set(${_DestList} ${${_DestList}} PARENT_SCOPE)
            endif()
        endfunction()

        set(MINGW_BIN_PATH "")
        set(MINGW_DLLS)

        find_mingw_dll(LIBGCCDW         "libgcc_s_dw2-1.dll" MINGW_DLLS "${MINGW_BIN_PATH}")
        find_mingw_dll(LIBGCCSJLJ       "libgcc_s_sjlj-1.dll" MINGW_DLLS "${MINGW_BIN_PATH}")
        find_mingw_dll(LIBGCCSEC        "libgcc_s_seh-1.dll" MINGW_DLLS "${MINGW_BIN_PATH}")
        find_mingw_dll(MINGWEX          "libmingwex-0.dll" MINGW_DLLS "${MINGW_BIN_PATH}")
        find_mingw_dll(WINPTHREAD       "libwinpthread-1.dll" MINGW_DLLS "${MINGW_BIN_PATH}")
        find_mingw_dll(WINPTHREADGC3    "pthreadGC-3.dll" MINGW_DLLS "${MINGW_BIN_PATH}")
        find_mingw_dll(STDCPP           "libstdc++-6.dll" MINGW_DLLS "${MINGW_BIN_PATH}")

        message("MinGW DLLs: [${MINGW_DLLS}]")
        file(COPY ${MINGW_DLLS} DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
    endif()

    install(TARGETS thextech DESTINATION .)
    file(GLOB BUILT_DLLS "${DEPENDENCIES_INSTALL_DIR}/bin/*-*.dll")
    file(GLOB BUILT_DLLS2 "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/*-*.dll")
    list(APPEND BUILT_DLLS ${BUILT_DLLS2})
    if(NOT USE_SYSTEM_SDL2)
        list(APPEND BUILT_DLLS
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/SDL2.dll"
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/SDL2_mixer_ext.dll"
        )
    endif()
    install(FILES ${BUILT_DLLS} DESTINATION .)
    install(FILES changelog.txt DESTINATION .)
    install(FILES README.md DESTINATION . RENAME ReadMe.txt)
    install(FILES LICENSE DESTINATION . RENAME License.TheXTech.txt)
elseif(VITA)
    include(cmake/package_vita.cmake)
elseif(NINTENDO_3DS)
    if(NOT "${THEXTECH_VERSION_REL}" STREQUAL "")
        set(SMDH_VERSION "git branch ${GIT_BRANCH} #${GIT_COMMIT_HASH}")
    else()
        set(SMDH_VERSION "Based on SMBX 1.3 by Redigit")
    endif()

    ctr_generate_smdh(OUTPUT "thextech.smdh"
        NAME "TheXTech ${THEXTECH_VERSION_STRING}"
        DESCRIPTION "${SMDH_VERSION}"
        AUTHOR "Wohlstand and ds-sloth"
        ICON "${CMAKE_SOURCE_DIR}/resources/icon/thextech_48.png"
    )

    ctr_create_3dsx(thextech SMDH "thextech.smdh")
elseif(NINTENDO_WII)
    string(TIMESTAMP XTECH_WIIMETA_RELEASE_DATE "%Y%m%d")
    if(NOT "${THEXTECH_VERSION_REL}" STREQUAL "")
        set(XTECH_WIIMETA_SHORT_DESC "git ${GIT_BRANCH} #${GIT_COMMIT_HASH}")
    else()
        set(XTECH_WIIMETA_SHORT_DESC "Based on SMBX 1.3 by Redigit")
    endif()

    configure_file(${CMAKE_SOURCE_DIR}/resources/wii/icon.png ${CMAKE_BINARY_DIR}/package/icon.png COPYONLY)
    configure_file(${CMAKE_SOURCE_DIR}/resources/wii/meta.xml.in ${CMAKE_BINARY_DIR}/package/meta.xml)

    ogc_create_dol(thextech)
    add_custom_command(TARGET thextech POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy
                           ${CMAKE_BINARY_DIR}/thextech.dol
                           ${CMAKE_BINARY_DIR}/package/boot.dol
                       COMMENT "Copying thextech executable into package")
elseif(NINTENDO_WIIU)
    string(TIMESTAMP XTECH_WIIMETA_RELEASE_DATE "%Y%m%d")
    configure_file(${CMAKE_SOURCE_DIR}/resources/wiiu/icon.png ${CMAKE_BINARY_DIR}/icon.png COPYONLY)
    configure_file(${CMAKE_SOURCE_DIR}/resources/wiiu/meta.xml.in ${CMAKE_BINARY_DIR}/meta.xml)
    wut_create_rpx(thextech)
    wut_create_wuhb(thextech
        CONTENT
        NAME "TheXTech v${THEXTECH_VERSION_STRING}"
        SHORTNAME "TheXTech"
        AUTHOR "Wohlstand and ds-sloth"
        ICON "${CMAKE_SOURCE_DIR}/resources/switch/thextech-logo.png"
        TVSPLASH "${CMAKE_SOURCE_DIR}/resources/wiiu/wuhb-splash.png"
        DRCSPLASH "${CMAKE_SOURCE_DIR}/resources/wiiu/wuhb-splash.png"
    )
elseif(NINTENDO_DS)
    if(NOT "${THEXTECH_VERSION_REL}" STREQUAL "")
        set(ROM_VERSION "${GIT_BRANCH} #${GIT_COMMIT_HASH}")
    else()
        set(ROM_VERSION "Based on SMBX 1.3")
    endif()

    nds_create_rom(thextech
        NAME "TheXTech ${THEXTECH_VERSION_STRING}"
        SUBTITLE1 "Wohlstand and ds-sloth"
        SUBTITLE2 "${ROM_VERSION}"
        ICON "${CMAKE_SOURCE_DIR}/resources/nds/icon.bmp"
    )
elseif(NINTENDO_SWITCH)
    include(cmake/package_switch.cmake)
elseif(EMSCRIPTEN)
    add_custom_command(
        TARGET thextech POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rename
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${THEXTECH_EXECUTABLE_NAME}.html"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/index.html")

    add_custom_command(
        TARGET thextech POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                "${PGE_PRELOAD_ENVIRONMENT}/graphics/ui/icon/thextech.ico"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/favicon.ico")

    add_custom_command(
        TARGET thextech POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                "${PGE_PRELOAD_ENVIRONMENT}/graphics/ui/icon/thextech_32.png"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/thextech_32.png")

    add_custom_command(
        TARGET thextech POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                "${PGE_PRELOAD_ENVIRONMENT}/graphics/ui/icon/thextech_256.png"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/thextech_256.png")

    configure_file(${CMAKE_SOURCE_DIR}/resources/emscripten/manifest.json.in ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/manifest.json)
    configure_file(${CMAKE_SOURCE_DIR}/resources/emscripten/sw.js.in ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sw.js)

elseif(HAIKU)
    string(TIMESTAMP XTECH_HAIKU_COPYRIGHT_YEAR "%Y")
    message("-- Haiku package version: ${THEXTECH_HAIKU_VERSION_STRING} (Yer ${XTECH_HAIKU_COPYRIGHT_YEAR})")
    set(DESKTOP_NAME "TheXTech" CACHE STRING "Name for the application icon")
    set(XTECH_HAIKU_SHORT_DESC "TheXTech is an open-source continuation of the SMBX 1.3 platformer game engine. Its highest priority is full compatibility with the SMBX 1.3 content standard, but it also adds a number of enhancements and bugfixes." CACHE STRING "Description for the installable Haiku package.")
    configure_file("${CMAKE_SOURCE_DIR}/resources/haiku/PackageInfo.in" "${CMAKE_BINARY_DIR}/package/.PackageInfo")

    set_target_properties(thextech PROPERTIES LINK_FLAGS "-Wl,-rpath,'$ORIGIN/../lib'")

    add_custom_command(
        TARGET thextech POST_BUILD
        COMMAND rc
                -o "${CMAKE_BINARY_DIR}/icon.rsrc"
                "${CMAKE_SOURCE_DIR}/resources/haiku/icon.rdef"
                COMMENT "Preparing icon file...")

    add_custom_command(
        TARGET thextech POST_BUILD
        COMMAND resattr
                -o "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${THEXTECH_EXECUTABLE_NAME}"
                "${CMAKE_BINARY_DIR}/icon.rsrc"
        COMMENT "Assigning icon to ${THEXTECH_EXECUTABLE_NAME}...")

    if(NOT EXISTS "${CMAKE_BINARY_DIR}/package/apps/${THEXTECH_DIRECTORY_PREFIX}")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/package/apps/${THEXTECH_DIRECTORY_PREFIX}/bin")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/package/apps/${THEXTECH_DIRECTORY_PREFIX}/lib")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/package/apps/${THEXTECH_DIRECTORY_PREFIX}/doc")
    endif()

    if(NOT EXISTS "${CMAKE_BINARY_DIR}/package/data/deskbar/menu/Applications")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/package/data/deskbar/menu/Applications")
    endif()

    add_custom_target(make_hkpg
        DEPENDS "${CMAKE_BINARY_DIR}/package/apps/${THEXTECH_DIRECTORY_PREFIX}/bin/${THEXTECH_EXECUTABLE_NAME}"
        COMMENT "Creating HPKG package..."
    )

    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/package/apps/${THEXTECH_DIRECTORY_PREFIX}/bin/${THEXTECH_EXECUTABLE_NAME}"
        DEPENDS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${THEXTECH_EXECUTABLE_NAME}"
        COMMAND
            ${CMAKE_COMMAND} -E copy
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${THEXTECH_EXECUTABLE_NAME}"
            "${CMAKE_BINARY_DIR}/package/apps/${THEXTECH_DIRECTORY_PREFIX}/bin/${THEXTECH_EXECUTABLE_NAME}"
    )

    add_custom_command(
        TARGET make_hkpg PRE_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/changelog.txt"
            "${CMAKE_BINARY_DIR}/package/apps/${THEXTECH_DIRECTORY_PREFIX}/doc/changelog.txt"
    )

    add_custom_command(
        TARGET make_hkpg PRE_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/README.md"
            "${CMAKE_BINARY_DIR}/package/apps/${THEXTECH_DIRECTORY_PREFIX}/doc/ReadMe.txt"
    )

    add_custom_command(
        TARGET make_hkpg PRE_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/LICENSE"
            "${CMAKE_BINARY_DIR}/package/apps/${THEXTECH_DIRECTORY_PREFIX}/doc/License.TheXTech.txt"
    )

    add_custom_command(
        TARGET make_hkpg PRE_BUILD
        COMMAND resattr
                -o "${CMAKE_BINARY_DIR}/package/apps/${THEXTECH_DIRECTORY_PREFIX}/bin/${THEXTECH_EXECUTABLE_NAME}"
                "${CMAKE_BINARY_DIR}/icon.rsrc"
        COMMENT "Assigning icon to ${THEXTECH_EXECUTABLE_NAME}...")

    add_custom_command(
        TARGET make_hkpg PRE_BUILD
        COMMAND cp -a "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/*.so*" "${CMAKE_BINARY_DIR}/package/apps/TheXTech/lib/"
    )

    add_custom_command(
        TARGET make_hkpg PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                "../../../../apps/TheXTech/bin/${THEXTECH_EXECUTABLE_NAME}"
                "${CMAKE_BINARY_DIR}/package/data/deskbar/menu/Applications/${DESKTOP_NAME}"
    )

    add_custom_command(
        TARGET make_hkpg POST_BUILD
        COMMAND package create -C "${CMAKE_BINARY_DIR}/package/"
                "${CMAKE_BINARY_DIR}/${THEXTECH_INSTALLER_PACKAGE_NAME}_bin-${THEXTECH_HAIKU_VERSION_STRING}-${TARGET_PROCESSOR}.hpkg"
    )

else()
    install(TARGETS thextech
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    if(THEXTECH_UNIX_INSTALL)
        # set up application metadata
        if(NOT HAIKU)
            # application launcher
            if(NOT EXISTS "${DEPENDENCIES_INSTALL_DIR}/share/applications")
                file(MAKE_DIRECTORY "${DEPENDENCIES_INSTALL_DIR}/share/applications")
            endif()

            set(DESKTOP_EXEC ${THEXTECH_EXECUTABLE_NAME})
            set(DESKTOP_ICON ${THEXTECH_EXECUTABLE_NAME})
            set(DESKTOP_WMCLASS ${THEXTECH_EXECUTABLE_NAME})
            set(DESKTOP_NAME "TheXTech" CACHE STRING "Name for the application icon")
            set(DESKTOP_GENERIC_NAME "TheXTech Engine" CACHE STRING "")
            set(DESKTOP_COMMENT "TheXTech - the modern C++ port and successor of the SMBX engine" CACHE STRING "Short description of the application")

            configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/icon.desktop.in"
                           "${DEPENDENCIES_INSTALL_DIR}/share/applications/${THEXTECH_EXECUTABLE_NAME}.desktop")
            install(FILES "${DEPENDENCIES_INSTALL_DIR}/share/applications/${THEXTECH_EXECUTABLE_NAME}.desktop"
                    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

            # flatpak manifest
            set(FLATPAK_BUILD OFF CACHE BOOL "Is this build a part of a flatpak workflow?")
            mark_as_advanced(FLATPAK_BUILD)
            if(FLATPAK_BUILD)
                set(FLATPAK_DESCRIPTION "TheXTech is an open-source continuation of the SMBX 1.3 platformer game engine. Its highest priority is full compatibility with the SMBX 1.3 content standard, but it also adds a number of enhancements and bugfixes." CACHE STRING "Description for the installable flatpak package.")

                configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/flatpak/appdata.xml.in"
                               "${DEPENDENCIES_INSTALL_DIR}/share/metainfo/${THEXTECH_EXECUTABLE_NAME}.metainfo.xml")
                install(FILES "${DEPENDENCIES_INSTALL_DIR}/share/metainfo/${THEXTECH_EXECUTABLE_NAME}.metainfo.xml"
                        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
            endif()

            # application icons
            set(THEXTECH_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/")

            install(FILES "${THEXTECH_ICON_PATH}/thextech_16.png"
                    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/16x16/apps/" RENAME "${THEXTECH_EXECUTABLE_NAME}.png")
            install(FILES "${THEXTECH_ICON_PATH}/thextech_32.png"
                    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps/" RENAME "${THEXTECH_EXECUTABLE_NAME}.png")
            install(FILES "${THEXTECH_ICON_PATH}/thextech_48.png"
                    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps/" RENAME "${THEXTECH_EXECUTABLE_NAME}.png")
            install(FILES "${THEXTECH_ICON_PATH}/thextech_128.png"
                    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps/" RENAME "${THEXTECH_EXECUTABLE_NAME}.png")
            install(FILES "${THEXTECH_ICON_PATH}/thextech_256.png"
                    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps/" RENAME "${THEXTECH_EXECUTABLE_NAME}.png")
            install(FILES "${THEXTECH_ICON_PATH}/thextech_512.png" OPTIONAL
                    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/512x512/apps/" RENAME "${THEXTECH_EXECUTABLE_NAME}.png")
        endif()

        # install readmes
        install(FILES changelog.txt DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/${THEXTECH_EXECUTABLE_NAME})
        install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/${THEXTECH_EXECUTABLE_NAME} RENAME ReadMe.txt)
        install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/${THEXTECH_EXECUTABLE_NAME} RENAME License.TheXTech.txt)
    else()
        install(FILES changelog.txt DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/thextech)
        install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/thextech RENAME ReadMe.txt)
        install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/thextech RENAME License.TheXTech.txt)
    endif()
endif()
