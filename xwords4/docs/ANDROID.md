Yes# XWords4 Android Implementation

## Overview

This document covers Android-specific implementation details for XWords4, including the JNI threading system, Kotlin coroutine integration, and Android platform-specific considerations.

## Android Architecture Challenges

### JNI Boundary Complexity
Unlike Linux/WASM platforms that can call common code directly, Android requires crossing the JNI boundary between Kotlin/Java and C code. This creates several challenges:

- **Thread safety**: JNI calls must be made from the correct thread context
- **Memory management**: Careful handling of object references across JNI
- **Performance**: JNI crossing overhead requires optimization
- **Error handling**: C-style returns must integrate with Android exception patterns

### Threading Requirements
The common code's single-thread constraint requires special handling on Android's naturally multi-threaded platform:

- **UI thread**: Must remain responsive for Android UI guidelines
- **Background threads**: Network operations, file I/O need background processing
- **JNI thread**: Single dedicated thread for all common code calls
- **Thread coordination**: Smooth integration between Android and common code threading

## Device.kt Threading Bridge

### Architecture Overview
`Device.kt` implements a sophisticated threading bridge that creates a dedicated JNI worker thread and provides three patterns for calling common code:

```kotlin
// Three call patterns:
val result1 = gameRef.someOperation().block()  // Synchronous
val result2 = gameRef.someOperation().await()  // Coroutine suspend
gameRef.someOperation().post()                 // Fire-and-forget
```

### JNI Worker Thread
- **Single thread**: All common code calls serialized through one worker thread
- **Queue system**: Multiple priority queues for different operation types
- **Performance monitoring**: Queue service times logged for optimization

### Priority Queue System
```
┌─────────────────┐  Highest Priority
│   UI Queue      │  (User interactions: clicks, draws)
├─────────────────┤  
│ General Queue   │  (Background operations, maintenance)
├─────────────────┤  
│ Network Queue   │  (Network packets, connections)
└─────────────────┘  Lowest Priority
```

**Queue Benefits:**
- **UI responsiveness**: User interactions processed first
- **Background priority**: General operations before network
- **Network efficiency**: Network operations don't block UI or general operations

### Call Pattern Details

#### Block Pattern
```kotlin
val result = gameRef.getSummary().block()
```
- **Usage**: Originally for UI interactions (penDown, penMove)
- **Behavior**: Blocks calling thread until JNI call completes
- **Current status**: May be unnecessary due to fast queue service
- **Alternative**: `await()` pattern usually preferable for UI responsiveness

#### Await Pattern  
```kotlin
suspend fun loadGameSummary() {
    val summary = gameRef.getSummary().await()
    // Update UI with summary
}
```
- **Usage**: Most common pattern for responsive UI
- **Behavior**: Suspends coroutine, resumes when JNI call completes
- **Integration**: Clean integration with Kotlin coroutines and lifecycle
- **Performance**: Keeps UI thread free while background work happens

#### Post Pattern
```kotlin
gameRef.processNetworkPacket(data).post()
```
- **Usage**: Fire-and-forget operations
- **Behavior**: Queues JNI call, returns immediately
- **No result**: Used for operations that don't return data
- **Examples**: State updates, notifications, background processing

## Kotlin Coroutine Integration

### Lifecycle-Aware Operations
Android UI components have complex lifecycles. The gameref architecture integrates with Android's lifecycle system:

```kotlin
class GameListItem : LinearLayout {
    private var mLifecycleScope: LifecycleCoroutineScope? = null
    
    fun load(gameRef: GameRef, lifecycleScope: LifecycleCoroutineScope) {
        mLifecycleScope = lifecycleScope
        mLifecycleScope!!.launch {
            val summary = gameRef.getSummary().await()
            updateUI(summary)
        }
    }
}
```

### RecyclerView Integration
RecyclerView presents special challenges due to view recycling:

- **Problem**: Views get recycled, breaking lifecycle owner relationships
- **Solution**: Dependency injection of LifecycleCoroutineScope 
- **Pattern**: Pass lifecycle scope through adapter → ViewHolder → custom view
- **Benefit**: Reliable coroutine scoping regardless of view recycling

### Error Handling in Coroutines
```kotlin
suspend fun safeGameOperation(): GameSummary? {
    return try {
        gameRef.getSummary().await()
    } catch (e: Exception) {
        Log.e(TAG, "Game operation failed", e)
        null
    }
}
```

## Platform Interface Implementation

### DrawCtxt Implementation
Android implements DrawCtxt using offscreen Canvas drawing:

```kotlin
class AndroidDrawCtxt : DrawCtxt {
    private val canvas: Canvas
    private val bitmap: Bitmap
    
    override fun drawTile(x: Int, y: Int, tile: String) {
        // Draw to offscreen bitmap
        canvas.drawText(tile, x.toFloat(), y.toFloat(), paint)
    }
    
    fun blitToView(view: View) {
        // Copy bitmap to actual view when drawing complete
        view.canvas.drawBitmap(bitmap, 0f, 0f, null)
    }
}
```

**Offscreen Pattern Benefits:**
- **Atomic updates**: Complete drawing before showing to user
- **Performance**: Avoid partial redraws, flickering
- **Threading**: Safe to draw from JNI thread, blit on UI thread

### DUtil Implementation  
Device utilities integrate with Android system services:

```kotlin
object AndroidDUtil : DUtil {
    override fun postNotification(message: String) {
        val notification = NotificationCompat.Builder(context, CHANNEL_ID)
            .setContentText(message)
            .build()
        notificationManager.notify(notificationId, notification)
    }
    
    override fun sendPacket(data: ByteArray, address: String) {
        // Platform-specific networking (SMS, Bluetooth, TCP)
        when (transportType) {
            SMS -> smsManager.sendDataMessage(address, null, data, null, null)
            BLUETOOTH -> bluetoothService.send(address, data)
            TCP -> tcpClient.send(address, data)
        }
    }
}
```

### Util Implementation
Game-specific utilities handle individual game callbacks:

```kotlin
class AndroidUtil(private val activity: Activity) : Util {
    override fun showDialog(title: String, message: String) {
        activity.runOnUiThread {
            AlertDialog.Builder(activity)
                .setTitle(title)
                .setMessage(message)
                .show()
        }
    }
}
```

## Memory Management

### Android-Specific Considerations
- **Lifecycle awareness**: Games must respect Android activity lifecycle
- **Memory pressure**: Android can kill background activities
- **GC integration**: JNI object references need careful GC handling

### Loading Strategy
```kotlin
class GamesListDelegate {
    fun loadVisibleGames() {
        // Load games in expanded groups to SUM level
        expandedGroups.forEach { group ->
            group.games.forEach { gameRef ->
                launch {
                    gameRef.loadToSumLevel().await()
                }
            }
        }
    }
    
    fun handleMemoryPressure() {
        // Future: Unload games from collapsed groups
        collapsedGroups.forEach { group ->
            group.games.forEach { gameRef ->
                gameRef.unloadToGILevel().post()
            }
        }
    }
}
```

## Network Integration

### Transport Types
Android supports multiple network transports:

#### SMS Transport
```kotlin
class SMSTransport : Transport {
    fun sendGameData(phoneNumber: String, gameData: ByteArray) {
        smsManager.sendDataMessage(
            phoneNumber, 
            null, 
            gameData, 
            sentIntent, 
            deliveryIntent
        )
    }
}
```

#### Bluetooth Transport  
```kotlin
class BluetoothTransport : Transport {
    fun sendGameData(device: BluetoothDevice, gameData: ByteArray) {
        bluetoothSocket.outputStream.write(gameData)
    }
}
```

#### Relay Server Transport
```kotlin
class RelayTransport : Transport {
    fun sendGameData(relayAddress: String, gameData: ByteArray) {
        httpClient.post(relayAddress) {
            body = gameData
        }
    }
}
```

### Network Event Handling
```kotlin
class NetworkReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        when (intent.action) {
            SMS_RECEIVED -> {
                val gameData = extractGameData(intent)
                // Process on JNI thread
                gameRef.processIncomingPacket(gameData).post()
            }
        }
    }
}
```

## Performance Optimization

### JNI Call Optimization
- **Batching**: Group related operations to minimize JNI crossings
- **Caching**: Cache frequently accessed data on Android side
- **Background processing**: Use post() for operations that don't need results

### UI Responsiveness
```kotlin
class GameActivity : AppCompatActivity() {
    fun handleUserInput(x: Int, y: Int) {
        lifecycleScope.launch {
            // Non-blocking UI update
            val moveAccepted = gameRef.processPenDown(x, y).await()
            if (moveAccepted) {
                gameRef.requestRedraw().await()
            }
        }
    }
}
```

### Memory Optimization
- **Lazy loading**: Only load games to required levels
- **Group management**: Use collapsed groups for memory savings  
- **Lifecycle integration**: Unload inactive games when activity paused

## Testing and Debugging

### Thread Safety Validation
```kotlin
// Common code asserts single-thread access
// Test with thread checker enabled
System.setProperty("xwords.threadcheck", "true")
```

### Performance Monitoring
```kotlin
// Queue service time logging
Log.d(TAG, "JNI operation took ${duration}ms, queue depth: ${queueSize}")
```

### Memory Profiling
- **Game loading levels**: Monitor memory usage per level
- **JNI object tracking**: Ensure proper cleanup of JNI references
- **Android lifecycle**: Verify proper cleanup during activity destruction

## Migration from Main Branch

### Key Changes
- **GameRef adoption**: Replace direct game access with GameRef handles
- **Loading levels**: Games now load incrementally instead of all-or-nothing
- **Coroutine integration**: Background operations use suspend functions
- **Lifecycle awareness**: Proper integration with Android component lifecycles

### Migration Strategy
- **Incremental**: Convert one game operation at a time
- **Testing**: Extensive testing with existing game data  
- **Compatibility**: Maintain ability to read main branch save files
- **Performance**: Monitor and optimize during migration

---

This document covers Android-specific implementation. For cross-platform architecture, see `PORTING.md`. For internal common code details, see `INTERNAL.md`.
