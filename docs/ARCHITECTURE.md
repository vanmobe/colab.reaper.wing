# Wing Connector Architecture

## Overview

The Wing Connector is a modular C++ REAPER extension organized around functional domains. This document describes the codebase structure, design patterns, and key components.

---

## Directory Structure

### Headers (`include/`)

Headers are organized into two tiers:

#### Public API (`include/wingconnector/`)
These headers define the public interface of the extension:

- **wing_osc.h** - OSC communication protocol and messaging
- **wing_config.h** - Configuration management and settings
- **track_manager.h** - Track creation and management interface
- **reaper_extension.h** - REAPER plugin lifecycle and integration

#### Internal Implementation (`include/internal/`)
These headers contain implementation details and utilities:

- **logger.h** - Logging and debugging utilities
- **platform_util.h** - Platform-specific utilities (macOS Objective-C bridging)
- **string_format.h** - String formatting and parsing helpers
- **osc_routing.h** - OSC message routing and handling
- **osc_builder.h** - OSC packet construction utilities
- **osc_helpers.h** - OSC helper functions and constants
- **settings_dialog_macos.h** - macOS settings dialog interface
- **wing_connector_dialog_macos.h** - macOS Wing connection dialog interface

### Source Code (`src/`)

Source files are organized by functional domain:

#### Extension Layer (`src/extension/`)
REAPER plugin integration and bootstrap:

- **main.cpp** - Plugin entry point and registration
- **reaper_extension.cpp** - Extension lifecycle (load, unload, configure)

#### Core OSC Communication (`src/core/`)
OSC protocol implementation and message handling:

- **wing_osc.cpp** - UDP/OSC client implementation
- **osc_routing.cpp** - Message routing and dispatcher
- **osc_builder.cpp** - OSC packet construction utilities

#### Utilities (`src/utilities/`)
Cross-platform support libraries:

- **wing_config.cpp** - Configuration file I/O and management
- **logger.cpp** - Logging system implementation
- **platform_util.cpp** - Platform-specific utilities (macOS native dialogs)
- **string_format.cpp** - String operations and formatting

#### Track Management (`src/track/`)
REAPER track interaction:

- **track_manager.cpp** - Track creation, updates, and synchronization

#### UI Layer (`src/ui/`)
Platform-specific user interfaces (macOS Objective-C++):

- **settings_dialog_macos.mm** - Settings configuration dialog
- **wing_connector_dialog_macos.mm** - Wing connection dialog

---

## Build System (CMakeLists.txt)

The CMake configuration:

1. **Organizes sources** by category with clear comments
2. **Manages platform-specific files** (Objective-C++ for macOS dialogs)
3. **Sets include paths** for public API and internal headers
4. **Configures compiler flags** and optimization levels
5. **Links external dependencies** (Cocoa, CoreFoundation on macOS)

### Include Paths

```cmake
target_include_directories(reaper_wingconnector
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/wingconnector
        ${CMAKE_CURRENT_SOURCE_DIR}/include/internal
        ...
)
```

This triple-level include strategy enables:
- `#include "wing_osc.h"` for public headers
- `#include "internal/logger.h"` for internal utilities
- Implicit internal includes within the same module

---

## Functional Domains

### 1. Extension Lifecycle (`extension/`)

**Responsibility:** REAPER plugin bootstrap and lifecycle management

**Key Operations:**
- Register extension with REAPER on load
- Handle plugin configuration and enable/disable
- Clean up on unload

**Dependencies:** Minimal (only REAPER SDK)

### 2. OSC Communication (`core/`)

**Responsibility:** UDP/OSC protocol implementation and message handling

**Key Operations:**
- Connect to Wing console via UDP
- Send OSC queries to discover channels and settings
- Receive and route OSC responses
- Build OSC packets for commands

**Dependencies:** oscpack library, platform_util

**Design Pattern:** Message dispatcher with routing table

### 3. Track Management (`track/`)

**Responsibility:** REAPER track creation and synchronization

**Key Operations:**
- Create new tracks matching Wing channels
- Update track properties (name, color, routing)
- Link stereo channels into pairs
- Synchronize state with Wing

**Dependencies:** REAPER SDK, wing_config, logger

### 4. Configuration (`utilities/wing_config.cpp`)

**Responsibility:** Settings persistence and management

**Key Operations:**
- Load/save JSON configuration
- Manage connection parameters
- Store user preferences

**Format:** JSON (config.json in plugin directory)

### 5. Utilities (`utilities/`)

**Logging** - Debug output with level filtering
**String Formatting** - Parsing and formatting operations
**Platform Utilities** - Native dialog support on macOS

---

## Data Flow

### Startup Sequence

```
main() [reaper_extension.cpp]
  ↓
REAPER calls Extension::Init()
  ↓
Load configuration [wing_config.cpp]
  ↓
Show connection dialog [wing_connector_dialog_macos.mm]
  ↓
Connect to Wing [wing_osc.cpp]
  ↓
Query channel list [osc_routing.cpp]
  ↓
Create tracks [track_manager.cpp]
  ↓
Enter monitoring loop
```

### Message Processing

```
Wing Console (UDP/OSC)
  ↓
wing_osc.cpp (receive)
  ↓
osc_routing.cpp (dispatch)
  ↓
Handler [track_manager.cpp, etc.]
  ↓
Update Reaper state
```

---

## Key Design Patterns

### 1. Layered Architecture

- **Extension Layer** - REAPER plugin interface
- **Core Layer** - OSC communication and protocol
- **Application Layer** - Business logic (tracks, configuration)
- **Utility Layer** - Cross-cutting concerns

### 2. Separation of Concerns

- **Public API** (wingconnector/) separates interface from implementation
- **Internal headers** contain implementation details
- **Platform-specific code** isolated in ui/ and platform_util

### 3. Module Independence

Each domain can be:
- Built and tested independently
- Modified without affecting others
- Replaced with alternative implementations

---

## Compilation & Linking

### Source Organization Impact

The reorganized structure improves:

1. **Compile times** - Can potentially parallelize by domain
2. **Maintainability** - Clear ownership of functionality
3. **Testing** - Easier to mock and test individual modules
4. **Documentation** - Self-documenting module organization

### CMakeLists.txt Structure

```cmake
# Extension layer
SOURCES += src/extension/main.cpp
         + src/extension/reaper_extension.cpp

# Core OSC logic
SOURCES += src/core/wing_osc.cpp
         + src/core/osc_routing.cpp
         + src/core/osc_builder.cpp

# Utilities
SOURCES += src/utilities/*

# Track management
SOURCES += src/track/track_manager.cpp

# Platform-specific UI
if(APPLE)
    SOURCES += src/ui/*_macos.mm
endif()
```

---

## Development Workflow

### Adding a New Feature

1. **Identify the domain** - Which functional area does it belong to?
2. **Add public interface** - If it's a new capability, define header in `include/wingconnector/` or `include/internal/`
3. **Implement** - Add source file to appropriate `src/*/` directory
4. **Update CMakeLists.txt** - Reference new source if needed
5. **Test & Build** - Verify compilation and functionality
6. **Document** - Update this architecture guide if adding a new domain

### Adding Platform Support

1. Add platform-specific headers to `include/internal/`
2. Create platform directory in `src/ui/` (e.g., `src/ui/settings_dialog_windows.cpp`)
3. Update `src/utilities/platform_util.cpp` to dispatch to platform implementations
4. Conditionally compile in CMakeLists.txt

---

## Dependencies

### External Libraries

- **oscpack** - OSC protocol implementation
- **REAPER SDK** - Plugin interface
- **nlohmann/json** - JSON configuration parsing (if used)

### Platform Frameworks (macOS)

- **Cocoa** - Native dialogs and UI
- **CoreFoundation** - System utilities

---

## Future Improvements

With this structure, potential enhancements become easier:

1. **Unit testing** - Modular organization enables isolated testing of core/ and utilities/
2. **Cross-platform UI** - Add Windows/Linux UI layers alongside macOS
3. **Plugin architecture** - Core OSC layer could support other DAWs
4. **Performance optimization** - Clear module boundaries enable targeted optimization
5. **Documentation generation** - Clear separation enables better Doxygen documentation

---

## Build Verification Checklist

After changes to any module:

- [ ] CMakeLists.txt updated with correct source paths
- [ ] All includes resolve (no "file not found" errors)
- [ ] Clean build succeeds: `cmake . && make clean && make`
- [ ] Extension deploys to REAPER correctly
- [ ] Extension loads in REAPER (View → Wing Connector UI)
- [ ] Basic functionality verified (connect to Wing, create tracks)

---

## Questions & Troubleshooting

**Q: Why separate internal/ and wingconnector/ headers?**
- Cleanly separates public API (what other components depend on) from implementation (can change without breaking contracts)

**Q: Can I use includes like `#include <internal/logger.h>`?**
- Not recommended; use `#include "internal/logger.h"` (relative) instead

**Q: How do I add a new utility function?**
- Add declaration to `include/internal/string_format.h` (or appropriate header)
- Add implementation to `src/utilities/string_format.cpp`
- Update CMakeLists.txt if creating a new file

**Q: What if I need platform-specific code?**
- Add header to `include/internal/` with platform-agnostic interface
- Implement in `src/ui/` with platform suffix (e.g., `_macos.mm`, `_win.cpp`)
- Conditionally compile in CMakeLists.txt using platform checks

