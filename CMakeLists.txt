cmake_minimum_required(VERSION 3.15)

project(WingConnector VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Enable Objective-C for macOS settings dialog
if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fPIC")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fPIC -std=c++17")
endif()

# Platform detection
if(APPLE)
    set(EXTENSION_SUFFIX ".dylib")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
elseif(WIN32)
    set(EXTENSION_SUFFIX ".dll")
else()
    set(EXTENSION_SUFFIX ".so")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/wing_osc.cpp
    src/wing_config.cpp
    src/track_manager.cpp
    src/reaper_extension.cpp
    src/logger.cpp
    src/osc_routing.cpp
)

# Platform-specific source files
if(APPLE)
    list(APPEND SOURCES 
        src/settings_dialog_macos.mm
        src/wing_connector_dialog_macos.mm
    )
    set_source_files_properties(
        src/settings_dialog_macos.mm
        src/wing_connector_dialog_macos.mm
        PROPERTIES
        LANGUAGE OBJCXX
    )
endif()

# Header files
set(HEADERS
    include/wing_osc.h
    include/wing_config.h
    include/track_manager.h
    include/reaper_extension.h
)

# Create the extension library
add_library(reaper_wingconnector SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(reaper_wingconnector
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/reaper-sdk
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/oscpack
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/nlohmann
)

# Compiler definitions
target_compile_definitions(reaper_wingconnector
    PRIVATE
        REAPER_EXTENSION
)

# Compiler flags
if(MSVC)
    target_compile_options(reaper_wingconnector PRIVATE /W4 /WX-)
else()
    target_compile_options(reaper_wingconnector PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Link oscpack library
find_library(OSCPACK_LIB
    NAMES oscpack
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/lib/oscpack
)

if(OSCPACK_LIB)
    target_link_libraries(reaper_wingconnector PRIVATE ${OSCPACK_LIB})
else()
    message(STATUS "oscpack not found, will use included source")
    # Add oscpack source files if not found as library
    file(GLOB OSCPACK_SOURCES 
        "lib/oscpack/osc/*.cpp"
        "lib/oscpack/ip/*.cpp"
    )
    if(WIN32)
        file(GLOB OSCPACK_PLATFORM_SOURCES "lib/oscpack/ip/win32/*.cpp")
    else()
        file(GLOB OSCPACK_PLATFORM_SOURCES "lib/oscpack/ip/posix/*.cpp")
    endif()
    
    target_sources(reaper_wingconnector PRIVATE 
        ${OSCPACK_SOURCES} 
        ${OSCPACK_PLATFORM_SOURCES}
    )
endif()

# Platform-specific settings
if(APPLE)
    set_target_properties(reaper_wingconnector PROPERTIES
        SUFFIX ${EXTENSION_SUFFIX}
        PREFIX ""
        MACOSX_RPATH ON
        BUILD_WITH_INSTALL_RPATH ON
        INSTALL_NAME_DIR "@rpath"
    )
    # Link with Cocoa for native dialogs and CoreFoundation
    target_link_libraries(reaper_wingconnector PRIVATE 
        "-framework Cocoa"
        "-framework CoreFoundation"
    )
    
elseif(WIN32)
    set_target_properties(reaper_wingconnector PROPERTIES
        SUFFIX ${EXTENSION_SUFFIX}
        PREFIX ""
    )
    target_link_libraries(reaper_wingconnector PRIVATE ws2_32 winmm)
    
else() # Linux
    set_target_properties(reaper_wingconnector PROPERTIES
        SUFFIX ${EXTENSION_SUFFIX}
        PREFIX ""
    )
    target_link_libraries(reaper_wingconnector PRIVATE pthread)
endif()

# Set output name
set_target_properties(reaper_wingconnector PROPERTIES
    OUTPUT_NAME "reaper_wingconnector"
)

# Installation
install(TARGETS reaper_wingconnector
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)

# Copy config file
install(FILES config.json
    DESTINATION .
)

# Print build info
message(STATUS "WingConnector ${PROJECT_VERSION}")
message(STATUS "Building for: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Extension suffix: ${EXTENSION_SUFFIX}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")

# Optional: Build tests
option(BUILD_TESTS "Build test programs" OFF)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
