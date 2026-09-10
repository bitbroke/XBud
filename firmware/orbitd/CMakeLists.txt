cmake_minimum_required(VERSION 3.20)
project(orbitd VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Build Options
# =============================================================================
option(BUILD_TESTS "Build unit and integration tests" OFF)
option(BUILD_BENCHMARKS "Build benchmark harnesses" OFF)
option(ENABLE_SANITIZERS "Enable ASan/UBSan (debug only)" OFF)
option(CROSS_COMPILE "Cross-compile for aarch64" OFF)
option(USE_RKNN "Enable RKNN NPU backend for llama.cpp" ON)
option(USE_NEON "Enable ARM NEON SIMD optimizations" ON)

# =============================================================================
# Cross-Compilation Toolchain
# =============================================================================
if(CROSS_COMPILE)
    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
endif()

# =============================================================================
# Compiler Flags
# =============================================================================
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -DNDEBUG -flto")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG -flto")
    if(USE_NEON)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+simd -mfpu=neon")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a+simd -mfpu=neon")
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND ENABLE_SANITIZERS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer")
endif()

# =============================================================================
# Static Linking (embedded target)
# =============================================================================
if(CROSS_COMPILE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
    set(BUILD_SHARED_LIBS OFF)
endif()

# =============================================================================
# External Dependencies (vendored or system-installed)
# =============================================================================

# whisper.cpp — ASR engine
set(WHISPER_DIR "${CMAKE_SOURCE_DIR}/third_party/whisper.cpp")
if(EXISTS "${WHISPER_DIR}/CMakeLists.txt")
    add_subdirectory(${WHISPER_DIR} whisper_build EXCLUDE_FROM_ALL)
else()
    message(STATUS "whisper.cpp not found at ${WHISPER_DIR} — using system install")
    find_library(WHISPER_LIB whisper REQUIRED)
    find_path(WHISPER_INCLUDE whisper.h REQUIRED)
endif()

# llama.cpp — SLM inference engine
set(LLAMA_DIR "${CMAKE_SOURCE_DIR}/third_party/llama.cpp")
if(EXISTS "${LLAMA_DIR}/CMakeLists.txt")
    set(LLAMA_CUBLAS OFF CACHE BOOL "" FORCE)
    set(LLAMA_METAL OFF CACHE BOOL "" FORCE)
    if(USE_RKNN)
        set(LLAMA_RKNN ON CACHE BOOL "" FORCE)
    endif()
    add_subdirectory(${LLAMA_DIR} llama_build EXCLUDE_FROM_ALL)
else()
    message(STATUS "llama.cpp not found at ${LLAMA_DIR} — using system install")
    find_library(LLAMA_LIB llama REQUIRED)
    find_path(LLAMA_INCLUDE llama.h REQUIRED)
endif()

# ONNX Runtime — for Silero VAD and DistilBERT NER
find_library(ONNXRUNTIME_LIB onnxruntime REQUIRED)
find_path(ONNXRUNTIME_INCLUDE onnxruntime_cxx_api.h
    PATH_SUFFIXES onnxruntime
    REQUIRED
)

# ALSA — for I2S audio capture
find_library(ALSA_LIB asound REQUIRED)
find_path(ALSA_INCLUDE alsa/asoundlib.h REQUIRED)

# SQLCipher — encrypted SQLite
find_library(SQLCIPHER_LIB sqlcipher REQUIRED)
find_path(SQLCIPHER_INCLUDE sqlcipher/sqlite3.h REQUIRED)

# BlueZ D-Bus — for BLE GATT
find_library(DBUS_LIB dbus-1 REQUIRED)
find_path(DBUS_INCLUDE dbus/dbus.h REQUIRED)

# nlohmann/json — header-only JSON library
find_path(NLOHMANN_JSON_INCLUDE nlohmann/json.hpp)
if(NOT NLOHMANN_JSON_INCLUDE)
    include(FetchContent)
    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
endif()

# =============================================================================
# orbitd Main Binary
# =============================================================================
set(ORBITD_SOURCES
    src/main.cpp

    # Core
    src/core/state_machine.cpp
    src/core/config.cpp
    src/core/watchdog.cpp
    src/core/thermal_governor.cpp

    # Audio
    src/audio/i2s_capture.cpp
    src/audio/ring_buffer.cpp
    src/audio/consent_chime.cpp

    # VAD
    src/vad/silero_vad.cpp
    src/vad/energy_detector.cpp

    # ASR
    src/asr/whisper_engine.cpp
    src/asr/transcript_buffer.cpp

    # NER
    src/ner/pii_scrubber.cpp
    src/ner/regex_prefilter.cpp

    # SLM
    src/slm/gemma_engine.cpp
    src/slm/prompt_templates.cpp
    src/slm/json_validator.cpp

    # Communications
    src/comms/ble_gatt_server.cpp
    src/comms/json_serializer.cpp

    # Storage
    src/storage/encrypted_db.cpp
    src/storage/call_repository.cpp
    src/storage/relationship_store.cpp
    src/storage/decision_ledger.cpp

    # Privacy
    src/privacy/volatile_manager.cpp
    src/privacy/consent_manager.cpp
    src/privacy/sandbox.cpp
)

add_executable(orbitd ${ORBITD_SOURCES})

target_include_directories(orbitd PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${WHISPER_INCLUDE}
    ${LLAMA_INCLUDE}
    ${ONNXRUNTIME_INCLUDE}
    ${ALSA_INCLUDE}
    ${SQLCIPHER_INCLUDE}
    ${DBUS_INCLUDE}
)

target_link_libraries(orbitd PRIVATE
    ${WHISPER_LIB}
    ${LLAMA_LIB}
    ${ONNXRUNTIME_LIB}
    ${ALSA_LIB}
    ${SQLCIPHER_LIB}
    ${DBUS_LIB}
    pthread
    rt          # POSIX real-time (mlock, clock_gettime)
    dl          # Dynamic loading
    m           # Math library
)

if(NLOHMANN_JSON_INCLUDE)
    target_include_directories(orbitd PRIVATE ${NLOHMANN_JSON_INCLUDE})
else()
    target_link_libraries(orbitd PRIVATE nlohmann_json::nlohmann_json)
endif()

# Install
install(TARGETS orbitd DESTINATION /opt/orbitd/bin)
install(DIRECTORY grammars/ DESTINATION /opt/orbitd/grammars)
install(FILES ${CMAKE_SOURCE_DIR}/orbitd.conf.example
    DESTINATION /opt/orbitd/etc
    RENAME orbitd.conf
)

# =============================================================================
# Tests
# =============================================================================
if(BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)

    # Unit tests
    file(GLOB_RECURSE TEST_SOURCES tests/unit/*.cpp)
    add_executable(orbitd_tests ${TEST_SOURCES})
    target_link_libraries(orbitd_tests PRIVATE gtest_main gmock)
    target_include_directories(orbitd_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME unit_tests COMMAND orbitd_tests)
endif()

if(BUILD_BENCHMARKS)
    file(GLOB_RECURSE BENCH_SOURCES tests/benchmark/*.cpp)
    add_executable(orbitd_bench ${BENCH_SOURCES})
    target_include_directories(orbitd_bench PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(orbitd_bench PRIVATE
        ${WHISPER_LIB} ${LLAMA_LIB} ${ONNXRUNTIME_LIB}
        pthread rt m
    )
endif()
