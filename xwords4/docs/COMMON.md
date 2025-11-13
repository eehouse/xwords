# XWords4 Internal Architecture

## Overview

This document describes the internal architecture of XWords4's common code engine, focusing on implementation details relevant to common code maintainers and contributors working on the core game engine.

## GameRef Architecture Evolution

### The Problem (main branch)
- **Platform-owned games**: Platform code (Android) managed game instances
- **Concurrency issues**: Need for multiple copies of same game (user interaction + network packets)
- **State synchronization**: Complex coordination between platform and common code
- **Ownership ambiguity**: Unclear who owned game state at any given time

### The Solution (gameref branch)
- **Single ownership**: Game data only ever opened in common code
- **Reference pattern**: Platform gets opaque GameRef handles only
- **Centralized management**: Common code handles all concurrency, state management, lifecycle
- **Clean boundaries**: Clear separation of responsibilities

### GameRef Benefits
- **No race conditions**: Single-threaded common code eliminates synchronization issues
- **Simplified platform code**: Platform only needs to manage GameRef handles
- **Better resource management**: Common code can optimize memory usage across all games
- **Concurrent network handling**: Multiple games can process packets and user interaction simultaneously without conflicts

## Game Loading Levels

The gameref architecture implements sophisticated loading levels defined by the `NeedsLevel` enum in `gameref.c`:

### Loading Level Hierarchy

```
BOARD   ┌─────────────────────┐  
        │  Full User Interaction │  (Drawing, input, complete state)
MODEL   ├─────────────────────┤  
        │   Game State Processing │  (AI, move validation, state updates)
DICTS   ├─────────────────────┤  
        │  Dictionary Management │  (Wordlist downloads, validation)
COMMS   ├─────────────────────┤
        │     Networking      │  (Packet handling, retransmission)
SUM     ├─────────────────────┤
        │   Summary Display   │  (List view information)
GI      └─────────────────────┘
        │   Basic Metadata    │  (Minimal game info)
```

### Level-Specific Capabilities

#### GI Level (Game Info)
- **Purpose**: Minimal metadata for game discovery
- **Required for**: Iterator must load all games to this level
- **Memory cost**: Lowest possible footprint
- **Persistent state**: Basic game identification, file paths

#### SUM Level (Summary)  
- **Purpose**: Information needed for game list display
- **Required for**: Games visible in UI lists (expanded groups)
- **Includes**: Game names, player info, status, last move time
- **Sorting dependency**: List sorting requires SUM-level data

#### COMMS Level (Communications)
- **Purpose**: Network packet processing
- **Required for**: Any network operations (send/receive)
- **Persistent state**: Unacknowledged packets, retransmission timers
- **Background operation**: Can run without user interaction

#### DICTS Level (Dictionaries)
- **Purpose**: Dictionary and wordlist management  
- **Required for**: Processing invitations requiring missing wordlists
- **Triggers**: Automatic wordlist downloads when needed
- **Dependency management**: Ensures required dictionaries are available

#### MODEL Level (Game Model)
- **Purpose**: Game state processing and logic
- **Required for**: AI moves, move validation, packet processing
- **Includes**: Complete game state, move history, player turns
- **Background AI**: Robot moves processed at this level

#### BOARD Level (Game Board)
- **Purpose**: Full user interaction and rendering
- **Required for**: Drawing game board, handling user input
- **Includes**: UI state, drawing context, interaction handlers
- **Resource intensive**: Highest memory usage per game

### Loading Optimizations

#### Group-Based Lazy Loading
- **Expanded groups**: Games load to SUM level for sorting and display
- **Collapsed groups**: Games remain at GI level (major memory savings)
- **User-controlled**: UI interactions naturally control memory usage
- **Sorting scope**: Within groups only, not global sorting

#### Loading Strategy Evolution
- **Current**: Load as needed, never unload (simple, effective)
- **Group optimization**: Collapsed groups provide automatic memory management  
- **Future**: LRU unloading possible if needed, e.g. for high game counts
- **Performance driven**: Optimizations based on real usage data

#### Memory Management Scenarios
```
User with 400 games in 10 groups:
├─ 8 collapsed groups: ~320 games at GI level (minimal memory)
├─ 2 expanded groups: ~80 games at SUM level (moderate memory)  
├─ 3 network games: COMMS/MODEL level (background processing)
└─ 1 active game: BOARD level (full interaction)
```

## Multi-Game Concurrency

### Concurrent Game States
The architecture supports unlimited simultaneous games at any loading level:

- **Multiple BOARD games**: No artificial limits (GTK/Linux version can open all games in separate windows)
- **Background processing**: Many games at MODEL/COMMS levels simultaneously  
- **Efficient scaling**: Memory usage scales with actual requirements

### Concurrency Examples
```
Typical Android scenario:
├─ 50 games at GI level (game list iteration)
├─ 12 games at SUM level (expanded groups) 
├─ 3 games at MODEL level (processing network moves)
├─ 1 game at BOARD level (active user interaction)
└─ Total: 66 games available, but only 1 using significant resources
```

### Resource Management Strategy
- **Lazy loading**: Only load to required level
- **Background efficiency**: Network processing without UI overhead
- **Platform flexibility**: Each platform can optimize for its constraints
- **Future-proof**: Architecture supports aggressive caching when needed

## State Persistence Model

### Layered State Management
Each loading level maintains independent persistent state:

- **GI state**: Game metadata, file locations, basic info
- **SUM state**: Display information, player names, status
- **COMMS state**: Network packets, retransmission queues, connection info  
- **MODEL state**: Complete game state, move history, AI status
- **BOARD state**: UI preferences, drawing context, interaction state

### State Independence Benefits
- **Atomic transitions**: Load/unload individual levels without affecting others
- **Crash recovery**: Can recover at any loading level from persistent state
- **Efficient caching**: Only persist state for loaded levels
- **Clean upgrades**: Level-specific state evolution without breaking other levels

## Threading Model

### Single-Thread Constraint
- **Common code**: Deliberately non-reentrant for simplicity
- **Assertion enforcement**: Runtime checks catch threading violations
- **Platform adaptation**: Each platform ensures single-thread access
- **Scalability**: Single-thread eliminates synchronization complexity

### Platform-Specific Threading
- **Linux/WASM**: Naturally single-threaded, direct function calls
- **Android**: JNI threading bridge with queuing system (see ANDROID.md)
- **Future platforms**: Must ensure single-thread access to common code

## Performance Characteristics

### Memory Usage Patterns
The common code was designed for extremely constrained environments (original 64K PalmOS) and remains very memory-efficient:

- **C code heritage**: Designed when entire app had 64K to run in
- **Minimal footprint**: Each game uses very little memory by modern standards
- **Not a bottleneck**: Memory usage is dominated by platform-specific UI, not common code
- **Level differences**: More about functionality than memory consumption

*Note: The loading levels are primarily about functionality and organization rather than memory optimization*

### Loading Performance
- **Database-driven**: Each loading level requires database fetch through dutil get/set key/value interface
- **Platform bottleneck**: Performance depends heavily on how platform implements database storage
- **State persistence**: All game state saving also uses dutil database interface
- **Critical dutil methods**: get/set key/value pairs are the primary performance factor
- **Optimization target**: Database implementation is where platforms should focus performance efforts

### Scalability Testing
- **Target**: 400+ games with responsive UI
- **Current status**: No performance issues with current strategy
- **Monitoring**: Loading times logged for optimization guidance
- **Optimization trigger**: Real performance problems, not theoretical limits

## Future Architecture Evolution

### Planned Optimizations
- **Unloading strategy**: LRU-based when memory pressure detected
- **Level refinement**: May adjust level requirements based on usage data
- **Caching improvements**: More aggressive state caching for frequently accessed games
- **Platform-specific tuning**: Memory limits and loading strategies per platform

### Architecture Stability
- **vtable interfaces**: Stable, platform compatibility guaranteed
- **Loading levels**: May evolve, internal implementation detail
- **GameRef API**: Stable for platform code
- **Performance characteristics**: Subject to optimization, not behavioral changes

---

This document covers internal implementation details. For platform porting information, see `PORTING.md`. For Android-specific details, see `ANDROID.md`.
