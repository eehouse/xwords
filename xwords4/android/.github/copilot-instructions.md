<!-- .github/copilot-instructions.md for xwords4/android -->
# Quick onboarding notes for AI coding agents

Purpose: give immediate, actionable context so an AI can be productive editing, building, and testing this Android project (Kotlin app + native C engine).

- Big picture
  - This is an Android app written mostly in Kotlin under `app/src/main/java/org/eehouse/android/xw4` with a native C core compiled via the NDK. The native code lives in `jni/` and pulls common C sources from `../../common` via `jni/Android.mk`.
  - Product flavors (variants) are important: `xw4GPlay`, `xw4fdroid`, `xw4d`, `xw4dGPlay`, `xw4Foss`. Flavor-specific resources and code live under `app/src/<flavor>/` (e.g. `app/src/xw4d/`). Build logic and feature flags are in `app/build.gradle.kts`.
  - The app uses a fragment/delegate UI pattern (see `XWFragment`, `DelegateBase`, `MainActivity.kt`) and many long-running components are Android Services and BroadcastReceivers (e.g. `XWService.kt`, `WiDirService.kt`, `SMSReceiver.kt`, `KAService.kt`). Network/comms subsystems include Bluetooth, SMS, Wi-Direct, MQTT and an NBS protocol (see `MQTTUtils.kt`, `WiDir*`, `NBSProto.kt`).

- Build & developer workflows (concrete)
  - Primary working directory: `xwords4/android` (all scripts and gradle commands run here).
  - Debug build (developer/debug variant):
    - Common: `./gradlew asXw4dDeb` (used by CI and scripts). Alternates used in project scripts: `insXw4dDeb` (see `INSTALL.txt`).
    - Install to device/emulator: build the apk then use `./scripts/adb-install.sh -d -p /path/to/app-<variant>-<gitrev>.apk` (script will detect package name via `AndroidManifest.xml` and needs `xmlstarlet` installed).
  - Release builds and signing: use `./scripts/arelease.sh --variant <VARIANT>` or the wrapper `./scripts/arelease-clone.sh --variant <VARIANT>` which clones a clean tree and runs signing (`sign-align.sh`). See `scripts/arelease.sh` for the exact assembly/signing flow.
  - Native / NDK: `app/build.gradle.kts` pins `ndkVersion = "21.4.7075529"`. You can run `./scripts/ndkbuild.sh` from repo root (it looks for `ANDROID_NDK` or `ndk-build` on PATH).
  - Pre-build steps: Gradle `preBuild` depends on tasks that call `make` and scripts (images, localization, prefs wrapper). See `app/build.gradle.kts` tasks: `mkImages`, `copyLocStrings`, `mkPrefsWrapper`, `makeBuildAssets`.

- Project-specific conventions and gotchas
  - Feature and variant flags are set in two places: Java/Kotlin via `buildConfigField` in `app/build.gradle.kts` and native C via `LOCAL_DEFINES` / `externalNativeBuild.ndkBuild.arguments` in `jni/Android.mk` and `app/build.gradle.kts`. To change a feature for native code, update the cflags/defines in `Android.mk` or the `externalNativeBuild` arguments per-flavor in `build.gradle.kts`.
  - The native shared library module is `xwjni` (see `LOCAL_MODULE := xwjni`). JNI entry points are in `xwjni.c` and use C sources from `../../common`.
  - Build artifacts are renamed to include `GITREV` (see `makeBuildAssets` and `buildOutputs` renaming in `app/build.gradle.kts`). Don’t assume stable artifact names — use the Gradle outputs/finder scripts when scripting.
  - Localization strings are generated/copied via `scripts/copy-strings.mk` / `scripts/copy-strings.py`. Translations live in `res_src/` and are staged into `app/src/.../res/values/` by Gradle pre-build tasks.
  - Images are generated from SVGs by `scripts/mkimages.sh` (invoked by the `mkImages` Gradle task).

- Integration points & external dependencies
  - MQTT client: `com.hivemq:hivemq-mqtt-client` (look at `MQTTUtils.kt`).
  - JNI/C libraries: many C files in `jni/` and `../../common` produce the game engine and communication stack.
  - Signing and release: `sign-align.sh` is used by `scripts/arelease.sh`.
  - CI/Upload: `scripts/ci-build.sh` and `scripts/build-debug.sh` demonstrate how CI and release uploading happen (they rely on env vars like `XW4D_UPLOAD` / `XW_RELEASE_SCP_DEST`).

- How an AI should make safe changes (concrete checklist)
  1. Build locally with the same variant as you plan to modify (use `./gradlew as<Variant>Deb` / `as<Variant>Rel`).
  2. If you touch native code, run `./scripts/ndkbuild.sh` or a full Gradle build to ensure the NDK flags and ABI filters are correct. The app sets `abiFilters` per buildType in `app/build.gradle.kts`.
  3. If changing UI/layout resources, update flavor-specific `res/` directories under `app/src/<flavor>/res` and run the `mkImages` and localization pre-build tasks if needed.
  4. When changing features controlled by `buildConfigField` or `LOCAL_DEFINES`, update both JVM/Kotlin side and C side to keep behavior consistent.

- Key files to inspect for any change
  - `app/build.gradle.kts` — build variants, ndkVersion, pre-build tasks, BuildConfig fields
  - `jni/Android.mk` and `jni/xwjni.c` — native flags and JNI bindings
  - `app/src/main/java/org/eehouse/android/xw4/*` — primary Kotlin app code (MainActivity.kt, XWService.kt, various Receivers/Services)
  - `scripts/` — release, ci, ndk, images, localization helpers (important: `arelease.sh`, `arelease-clone.sh`, `ndkbuild.sh`, `adb-install.sh`, `mkimages.sh`, `copy-strings.*`)
  - `res_src/` — source SVGs and localized string templates

If anything here is unclear or you want guidance tuned to a particular task (e.g. "add a new product flavor", "modify JNI call X", or "run CI locally"), tell me which area and I will expand the instructions or run a targeted build to verify changes.
