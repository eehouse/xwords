# XWords4 Platform Porting Guide

## Overview

XWords4 is a cross-platform Scrabble-rules word game with a C-based common engine and platform-specific interfaces. This guide explains how to port XWords4 to a new platform.

## Architecture Overview

XWords4 follows a **platform-agnostic core with platform-specific interfaces** pattern:

```
┌─────────────────────────────────────┐
│           Platform Layer            │
│         (Your New Platform)         │
├─────────────────────────────────────┤
│        Platform Interface           │
│       (Function calls only)         │
├─────────────────────────────────────┤
│           Common Code               │
│          (C Language)               │
│    ┌─────────────────────────────┐  │
│    │ Game Engine & Logic        │  │
│    └─────────────────────────────┘  │
└─────────────────────────────────────┘
```

**Integration Method:**
- **Simple linking**: Your platform links directly to common code C functions
- **No JNI complexity**: Android's JNI is platform-specific, not required
- **Standard C calling**: All interfaces use standard C function calls

## Required Platform Interface Implementation

You must implement three vtable-based "classes" that the common code calls:

### 1. DrawCtxt (Drawing Interface)

**Purpose**: Handle all rendering operations initiated by common code.

**Key Concept**: 
- Common code decides **what** to draw and **when**
- Your platform implements **how** to draw
- Common code calls your DrawCtxt methods with drawing commands

**Implementation Strategy by Platform Type:**
- **GUI platforms**: Draw to offscreen bitmap, blit when complete
- **Terminal platforms**: Direct character output (like ncurses)  
- **Web platforms**: Canvas operations, DOM manipulation
- **Embedded platforms**: Direct framebuffer access

**DrawCtxt Variants to Support:**
- **Main DrawCtxt**: Interactive game board rendering
- **Thumbnail DrawCtxt**: Small preview images for game lists

### 2. DUtil (Device Utilities)

**Purpose**: System-wide device operations.

**Scope**: Platform-level functionality that affects the entire application.

**Typical Operations to Implement:**
- **Database storage (CRITICAL)**: get/set key/value pairs - primary performance bottleneck
- **Networking**: Send packets (e.g. SMS, Bluetooth, internet)
- **Time**: Get current time, set timers or request onIdle callback
- **Platform services**: Device-specific capabilities

**Performance Note**: The database get/set operations are called for every loading level transition and state save. Your database implementation will be the primary performance factor for game loading and saving.

### 3. Util (Game Utilities)

**Purpose**: Individual game instance operations.

**Scope**: Game-specific functionality for individual games.

**Typical Operations to Implement:**
- **UI callbacks**: Show dialogs, handle user confirmations
- **Game notifications**: Score updates, turn changes
- **Input handling**: Platform-specific input methods
- **Game-specific storage**: Individual game state persistence

## Threading Requirements

**Critical Constraint**: Common code is **not reentrant** and must always be called from the same thread.

**Implementation Approaches:**

### Simple Platforms (Linux, embedded)
- **Single-threaded**: Natural fit, call common code directly
- **No special handling**: Your main thread calls common functions

### Multi-threaded Platforms (GUI applications)
- **Dedicated thread**: Create one thread for all common code calls
- **Thread serialization**: Ensure only one thread calls common code
- **Optional queuing**: Queue requests if needed, but not required

**Note**: Android uses a JNI threading system, but this is **not required** for other platforms. Direct function calls are sufficient.

## Data Flow Patterns

### Input Flow (Your Platform → Common Code)
1. User interacts with your platform (mouse, touch, keyboard)
2. Your platform captures input
3. Call appropriate common code function with input data
4. Common code processes input, updates game state

### Output Flow (Common Code → Your Platform)
1. Common code decides what needs to be drawn/done
2. Common code calls your vtable methods
3. Your platform performs the actual operations
4. Results appear to user

### Example Flow
```c
// User clicks at position (x,y)
// Your platform calls:
common_handle_click(gameRef, x, y);

// Common code processes click, then calls your DrawCtxt:
your_drawctx_draw_tile(drawCtxt, tileData, x, y);

// Your platform draws the tile and updates display
```

## Error Handling

**Philosophy**: C-style error handling, no exceptions across the interface boundary.

**Convention**: 
- `false` or `NULL` return values may indicate errors
- Common code uses simple success/failure patterns

## Platform-Specific Considerations

### Storage
- Implement game save/load through DUtil vtable
- Common code handles game state serialization
- Your platform provides file I/O operations

### Networking
- Platform code is responsible for interfacing with native phone/BT stack/MQTT library/NFC/etc. Common code sends packets through DUtil vtable. Platform code receives packets and passes them to common code for processing.
- Common code handles game protocol

### User Interface
- Common code makes all board drawing decisions through DrawCtxt calls
- Your platform handles actual rendering/display
- UI layout and styling are platform-specific

## Getting Started

### 1. Study Existing Ports
- **Linux**: `linux/` directory - simplest reference implementation
- **Android**: `android/` directory - complex but complete
- **WASM**: `wasm/` directory - web platform example

### 2. Initialize DUtil First (CRITICAL)
- **DUtil must be first**: Create and fully populate DUtilCtxt* vtable
- **Initialization sequence**: Call `dvc_init()` with complete DUtilCtxt*
- **Validation**: Common code will reject incomplete vtables (all slots must be filled)
- **Database operations**: DUtil get/set key/value methods are essential for startup

### 3. Add Game Creation Support
- **Platform UI**: Build your platform's game creation interface (e.g., "+" button above games list)
- **Common code calls**: Use common code functions to actually create games
- **Invitation handling**: Support receiving game invitations from other devices  
- **Basic functionality**: Get game creation/loading working without display

### 4. Implement DrawCtxt and Util Together
- **Required pair**: Both needed simultaneously to open a game
- **Can be stubbed**: Start with logging implementations that just record method calls
- **DrawCtxt stubs**: Log drawing commands without actual rendering
- **Util stubs**: Log callbacks without actual UI responses
- **Testing phase**: Verify game opening logic works before implementing real functionality

### 5. Add Real Drawing Implementation
- **Replace DrawCtxt stubs**: Implement actual rendering after stubbed testing works
- **Game display**: Show actual game boards to users
- **Visual polish**: Make games look good

### 6. Add Real Util Implementation  
- **Replace Util stubs**: Implement actual dialogs, notifications after core works
- **User interaction**: Handle real game callbacks and alerts

### 5. Test Incrementally
- Create single-player games first (simpler)
- Add networking after basic gameplay works
- Test with existing platforms for compatibility

## Common Code Interface Reference

The common code provides functions for:
- **Game iteration**: Discover and enumerate available games
- **Game creation**: Create new games with specified parameters
- **Game loading**: Load existing games from storage
- **Input processing**: Handle user input (clicks, key presses, etc.)
- **Network processing**: Handle incoming packets, connection management

See `common/` directory for detailed function signatures and documentation.

## Platform Examples

### Minimal Console Port
```c
// Just implement the three vtables for text output
// Perfect for testing common code functionality
```

### GUI Desktop Port  
```c
// Use native graphics API (GTK, Qt, Win32, Cocoa)
// Implement DrawCtxt with 2D graphics
// Handle window management and user input
```

### Web Browser Port
```c
// Compile to WebAssembly
// Implement DrawCtxt with HTML5 Canvas
// Handle web-specific storage and networking
```

### Mobile/Touch Port
```c
// Implement DrawCtxt for mobile graphics APIs
// Handle touch input through common input functions
// Integrate with mobile storage and notification systems
```

---

This document focuses on platform porting requirements. For internal common code architecture details, see `INTERNAL.md`. For Android-specific implementation details, see `ANDROID.md`.
