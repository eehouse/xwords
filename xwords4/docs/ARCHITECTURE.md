# XWords4 Architecture Documentation

## Overview

XWords4 is a Scrabble-rules word game with 20+ years of evolution across multiple platforms. Originally developed for PalmOS, it has been successfully ported to Windows Mobile, Symbian, Franklin devices, and currently runs on Android and Linux, with WebAssembly support.

## High-Level Architecture

### Cross-Platform Design Philosophy

The architecture follows a **platform-agnostic core with platform-specific interfaces** pattern:

```
┌─────────────────────────────────────┐
│           Platform Layer            │
│        (Android/Linux/WASM)         │
├─────────────────────────────────────┤
│        Platform Interface           │
│     (JNI on Android, linker on      │
│        Linux/WASM, etc.)            │
├─────────────────────────────────────┤
│           Common Code               │
│          (C Language)               │
│    ┌─────────────────────────────┐  │
│    │ Game Engine & Logic        │  │
│    └─────────────────────────────┘  │
└─────────────────────────────────────┘
```

### Core Components

- **Common Code**: C language game engine in `common/` directory
- **Platform Interface**: Three vtable-based abstraction layers
- **Android Bridge**: `xwjin.c` and JNI threading (Android-specific)
- **Direct Integration**: Direct calls (Linux/WASM)
- **Threading Model**: Single-threaded common code with platform-appropriate interface

## Platform Abstraction Layer

The common code interfaces with platforms through three vtable-based "classes":

### 1. DrawCtxt (Drawing Interface)
- **Purpose**: All rendering and drawing operations
- **Flow**: Common code initiates all drawing, platform provides drawing implementation
- **Platform-specific**: Each platform implements DrawCtxt differently (Android: offscreen bitmaps; Linux ncurses: direct terminal output; etc.)
- **Types**: 
  - Main DrawCtxt: Interactive game board rendering
  - Thumbnail DrawCtxt: List item preview generation

### 2. DUtil (Device Utilities)
- **Purpose**: Device-level operations
- **Scope**: System-wide functionality (notifications, networking, time, storage)
- **Examples**: Send network packets, post system notifications, request on-idle callbacks, get current time, store and load data

### 3. Util (Game Utilities) 
- **Purpose**: Game-specific operations
- **Scope**: Individual game instances
- **Examples**: Game state callbacks, player interactions, game-specific UI

## GameRef Architecture (Current Development)

### The Problem (main branch)
- Platform code owned game instances
- Concurrency issues when multiple operations needed same game
- Complex state synchronization between platform and common code

### The Solution (gameref branch)
- **Single Ownership**: Game data only ever opened in common code
- **Reference Pattern**: Platform gets opaque GameRef handles
- **Centralized Management**: Common code handles all state, concurrency, lifecycle

### GameRef API Pattern

```kotlin
// Discovery and summary
val gameRefs = commonCode.getGameIterator()
for (gameRef in gameRefs) {
    val summary = gameRef.getSummary()  // List display info
}

// Game interaction - platform provides DrawCtxt, common code decides what to draw
when (user.opensGame(gameRef)) {
    // Platform provides drawing context, common code handles all drawing decisions
    // Drawing happens through DrawCtxt vtable callbacks initiated by common code
}
```

## Game Loading Levels

The gameref architecture supports multiple distinct loading levels defined by the `NeedsLevel` enum in `gameref.c`:

### Loading Level Hierarchy

1. **GI Level** (Game Info)
   - **Purpose**: Basic game metadata for iteration
   - **Required for**: Game discovery, all games must reach this level for iterator
   - **Memory**: Minimal footprint

2. **SUM Level** (Summary)
   - **Purpose**: List display information  
   - **Required for**: Games visible in scrolling list
   - **Capabilities**: Game name, player info, status, thumbnails

3. **COMMS Level** (Communications)
   - **Purpose**: Network traffic handling
   - **Required for**: Any network operations (incoming/outgoing packets)
   - **Includes**: Packet management, retransmission, ACK handling

4. **DICTS Level** (Dictionaries)
   - **Purpose**: Dictionary/wordlist management
   - **Required for**: Processing invitations with uninstalled wordlists
   - **Triggers**: App downloads missing dictionaries as needed

5. **MODEL Level** (Game Model)
   - **Purpose**: Game state processing
   - **Required for**: Processing incoming network packets, AI moves
   - **Capabilities**: Game logic, move validation, state updates

6. **BOARD Level** (Game Board)
   - **Purpose**: Full rendering and user interaction
   - **Required for**: Drawing game on screen, user input handling
   - **Includes**: Complete game state, UI interaction, drawing commands

### Loading Level Usage

```
┌─────────────────────┐  BOARD Level
│    User Interaction │  (Full rendering, input)
├─────────────────────┤  
│      MODEL Level    │  MODEL Level  
│   (State Processing)│  (Packet processing, AI)
├─────────────────────┤
│     DICTS Level     │  DICTS Level
│  (Dictionary Mgmt)  │  (Download missing wordlists)
├─────────────────────┤
│     COMMS Level     │  COMMS Level
│    (Networking)     │  (Network traffic)
├─────────────────────┤
│      SUM Level      │  SUM Level
│   (List Display)    │  (Visible list items)
├─────────────────────┤
│       GI Level      │  GI Level
│   (Basic Metadata)  │  (All games for iteration)
└─────────────────────┘
```

### Typical Loading Scenarios

**Game List Display:**
- All games: GI level (for iteration)  
- Visible games: SUM level (for list display)
- Hidden games: Remain at GI level

**Sorting Optimization:**
- Sort order requires GI + SUM level data
- Games in expanded groups: Load to SUM level for sorting
- Games in collapsed groups: Can remain unloaded (significant memory savings)
- Sorting scope: Within groups only, not global

**Network Game Background Processing:**
- Incoming packet: COMMS → MODEL level (process move)
- Missing dictionary: COMMS → DICTS level (download wordlist)
- Turn notification: Triggers system notification via DUtil

**Active Gameplay:**
- User opens game: Escalate to BOARD level
- Full interaction: Drawing, input handling, complete game state  
- User leaves: Can drop back to lower levels as needed

### Level Management
Each level maintains its own persistent state and games can transition fluidly between levels as requirements change.

**Note**: The specific loading level requirements and transitions described above may change if memory optimization becomes necessary. Current testing with 400+ games may drive refinements to the loading strategy and level definitions.

## Threading Model

### Single-Thread Constraint
- **Common Code**: Deliberately not reentrant, always called on same thread
- **Enforcement**: Asserts to catch threading violations
- **Platform Differences**:
  - **Linux/WASM**: Direct function calls (linker only), naturally single-threaded
  - **Android**: JNI boundary requires dedicated thread and queue system

### Android Threading Bridge (Device.kt)

**Note**: This threading complexity is Android-specific. Linux and WASM platforms, single-threaded and coded in C, call common code directly without needing JNI or queue systems.

The Device.kt file creates a dedicated thread for all JNI operations with three call patterns:

#### 1. Block
```kotlin
val result = gameRef.someOperation().block()  // Synchronous
```
- Blocks calling thread until completion
- Originally for UI interactions (penDown)
- May be unnecessary due to fast queue service

#### 2. Await  
```kotlin
val result = gameRef.someOperation().await()  // Suspend function
```
- Yields calling coroutine, resumes when complete
- Most common pattern for responsive UI
- Integrates cleanly with Kotlin coroutines

#### 3. Post
```kotlin
gameRef.someOperation().post()  // Fire and forget
```
- Queues operation, returns immediately  
- No result returned
- Used for one-way operations

### Queue System

**Android-specific**: Multiple priority queues service the single JNI thread:
- **UI Queue**: Highest priority (user interactions)
- **General Queue**:  Medium priority (general tasks)
- **Networking Queue**: Lowest priority (network operations)

**Monitoring**: Queue service times are logged for performance analysis.

*Linux and WASM platforms don't need queues since they call common code directly from a single thread.*

## Data Flow Patterns

### Input Flow (Platform → Common)
1. User interaction on Android (touch, key press)
2. Android captures event
3. JNI call to common code with input data
4. Common code processes input, updates game state, may draw or notify of changes

### Output Flow (Common → Platform)
1. Common code determines what needs to be drawn
2. Common code calls DrawCtxt vtable methods with drawing commands
3. Platform's DrawCtxt implementation handles drawing (varies by platform):
   - Android: Calls into kotlin which draws to offscreen bitmap, then blits to screen
   - Linux ncurses: Direct terminal output
   - WASM and linux GTK: Canvas operations, etc.
4. Platform never initiates rendering beyond providing a DrawCtxt - all drawing decisions made by common code

### Network Game Flow
1. Network packet arrives → Android receives
2. `Device.processPacket(data).post()` → Queued for processing
3. Common code processes at partial level → Updates game state
4. If turn changes → Callback via DUtil → Android posts notification
5. User taps notification → Game elevated to full level → User interaction

## Memory Management

### Current Strategy
- **Load as needed**: Games loaded when accessed
- **Never unload**: Simple approach, works for typical usage
- **Group optimization**: Games in collapsed groups can remain unloaded, providing significant memory savings
- **Sorting impact**: Expanded groups require SUM-level loading for sort order, collapsed groups avoid this cost
- **Future optimization**: LRU unloading planned if memory pressure occurs with high game counts (400+ games)
- **Level refinement**: Loading level requirements may be optimized based on memory testing results

### Resource Considerations
- **Primary cost**: Memory (not CPU or I/O)
- **Multiple games**: No limit on fully open games - design supports many simultaneous BOARD-level games
- **Platform examples**: GTK Linux variant allows opening all games, each in its own window
- **Efficient transitions**: Games move fluidly between GI↔SUM↔COMMS↔DICTS↔MODEL↔BOARD levels

## Error Handling

### Philosophy
- **No exceptions**: C-style error handling across JNI boundary
- **Return values**: null or false indicates error conditions
- **Consistency**: Matches C common code expectations

### Approach
```kotlin
val result = gameRef.someOperation().await()
if (result == null) {
    // Handle error condition
}
```

## Game Types & Usage Patterns

### Local Games (majority of users)
- **Pattern**: Summary level → Full level (direct)
- **AI Processing**: Happens during user interaction (full level)
- **No networking**: Single device gameplay

### Network Games  
- **Pattern**: Summary → Partial (background) → Full (interaction)
- **Serverless**: Peer-to-peer over Bluetooth/SMS/NFC, or via Mosquitto MQTT broker
- **Background processing**: Moves processed without the need to fully load game
- **Notifications**: DUtil callbacks trigger system notifications

## Development History & Evolution

### Platform Timeline
- **Original**: PalmOS (20+ years ago)
- **Second published platform**: Windows Mobile 
- **Additional platforms**: Symbian, Franklin devices
- **Current**: Android, Linux
- **Experimental**: WebAssembly

### Architectural Evolution
- **main branch**: Traditional platform-owned games, proven but with concurrency issues
- **gameref branch**: Centralized ownership, cleaner concurrency, improved scalability

## Key Design Principles

1. **Cross-platform core**: Maximum code reuse across platforms
2. **Single-threaded simplicity**: Avoid complex synchronization
3. **Layered loading**: Efficient memory usage with on-demand capabilities
4. **Clean boundaries**: Clear separation between platform and game logic
5. **Pragmatic optimization**: Monitor performance, optimize when needed

---

*This documentation reflects the current gameref branch architecture under development. The main branch represents the shipped architecture with different ownership and concurrency patterns.*
