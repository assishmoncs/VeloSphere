# VeloSphere 2.0 — Production Edition

[![Build Status](https://github.com/assishmoncs/VeloSphere/actions/workflows/build.yml/badge.svg)](https://github.com/assishmoncs/VeloSphere/actions/workflows/build.yml)
[![Android](https://img.shields.io/badge/Platform-Android_24%2B-3DDC84?style=for-the-badge&logo=android&logoColor=white)](https://developer.android.com)
[![OpenGL ES](https://img.shields.io/badge/Graphics-OpenGL_ES_3.0-5586A4?style=for-the-badge&logo=opengl&logoColor=white)](https://www.khronos.org/opengles/)
[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/17)
[![GameActivity](https://img.shields.io/badge/Games_SDK-GameActivity_4.4.2-4285F4?style=for-the-badge&logo=google&logoColor=white)](https://developer.android.com/games/agdk/game-activity)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](LICENSE)

> A hardware-accelerated 3D synthwave endless runner built for Android using C++17, OpenGL ES 3.0, and Google's GameActivity library.

---

## Overview

**VeloSphere 2.0** delivers a high-speed retro-futuristic synthwave arcade experience engineered directly on top of the Android NDK. Built without heavy third-party engines, VeloSphere uses a custom C++17 game loop, procedural 3D mesh pipelines, custom GLSL ES 3.0 shaders, and CRC32-verified persistence to achieve locked 60/120 FPS gameplay with zero garbage-collection pauses.

---

## What's New in VeloSphere 2.0 (Production Release)

- **3D Jump & Mid-Air Slam Physics**:
  - Swipe **Up** to launch over low laser barriers and grab airborne Data Orbs.
  - Swipe **Down** while airborne to execute a fast-drop slam for tight hazard recovery.
- **4 Distinct Hazard Archetypes**:
  - **Static Ruby Monoliths**: Full-lane barriers requiring rapid lateral weaving.
  - **Amber Sweeper Hazards**: Dynamic lane-to-lane sweeping hazards with pre-sweep warning pulses.
  - **Violet Low Laser Hurdles**: Low-profile barriers you can leap over for bonus airborne Data Orbs.
  - **Crimson Overhead Gates**: Elevated gates supported by side pillars—stay grounded to roll safely underneath.
- **4 Collectible Power-Ups**:
  - **Aegis Shield**: Wraps the sphere in a Fresnel holographic energy shell that absorbs an impact and shatters the obstacle (`+300 SHIELD SMASH!`).
  - **Overdrive Boost**: 5-second hyper-warp (+35% speed, wider FOV, auto-smashes obstacles in your path for `+200` points each).
  - **Chrono Slow-Mo**: 6-second time dilation field that slows obstacle movement for surgical weaving.
  - **Orb Magnet**: 8-second magnetic field that pulls Data Orbs from all three lanes directly into the sphere.
- **Combo Multiplier, Near-Miss Alerts & Data Orbs**:
  - Chain clean dodges to build your multiplier from **1x up to 5x**.
  - Pass closely by obstacles or jump directly over low hurdles to trigger **`NEAR MISS!`** bonus popups.
  - Collect spinning **Data Orbs** to unlock custom sphere skins.
- **3 Selectable Game Modes**:
  - **Endless Arcade**: Progressive acceleration with all power-ups and hazards.
  - **Time Attack (90s)**: Race against the clock—collecting orbs, dodging obstacles, and scoring near-misses extend your timer.
  - **Hardcore Mode**: Starts at high velocity (`28 u/s`), disables shields, and awards **2x Score and 2x Orbs**.
- **5 Unlockable Sphere Skins & 4 Dynamic Synthwave Biomes**:
  - Unlock and equip **Cyber Cyan**, **Solar Flare**, **Void Violet**, **Emerald Matrix**, and **Supernova Chrome**.
  - Automatic biome transitions every 450m (**Neon Grid** → **Solar Outrun** → **Cyber Matrix** → **Void Horizon**) with real-time shader palette shifts.
- **CRC32-Verified Binary Persistence (`velosphere_save.dat`)**:
  - Versioned binary save format with `VELO` magic header, version tag, and CRC32 checksum verification (plus automatic migration from legacy `highscore.dat`).
  - Persists per-mode high scores, total Data Orbs, unlocked skins, active skin/mode selections, and audio/haptic preferences.

---

## Controls

| Action | Touch Gesture | Description |
| :--- | :--- | :--- |
| **Launch / Retry** | **Tap Screen** | Starts the run or retries from Game Over |
| **Steer Sphere** | **Swipe / Drag Left & Right** | Smoothly steers and banks the sphere across the 3 lanes |
| **Jump** | **Swipe Up** | Jumps over Low Laser Hurdles and collects high Data Orbs |
| **Fast-Drop Slam** | **Swipe Down (in air)** | Rapidly drops back to the runway surface |
| **Pause / Menu** | **Tap Top-Center `II`** | Opens the interactive Pause overlay (Resume, Restart, Main Menu) |

---

## Architecture & Project Structure

```
VeloSphere/
├── app/
│   ├── src/main/
│   │   ├── cpp/
│   │   │   ├── CMakeLists.txt        # Native build config linking GLESv3, EGL & GameActivity
│   │   │   ├── GameLogic.h           # Core state, 3 modes, 4 hazard types, power-ups, combos & SaveData
│   │   │   ├── Math3D.h              # 3D Vector, Color3 & Mat4 matrix math library
│   │   │   ├── Mesh.h                # Procedural Sphere, Box, Track, Quad, Particles & 512-char HUD
│   │   │   ├── Shader.h              # GLES 3.0 shaders with non-uniform normals, biomes & Fresnel shield
│   │   │   └── main.cpp              # EGL context, android_main loop, gestures, menus & persistence
│   │   ├── java/com/hsissa/velosphere/
│   │   │   └── MainActivity.kt       # GameActivity wrapper with multi-SFX pitch SoundPool & haptics
│   │   ├── res/                      # App resources, raw audio, icons, and themes
│   │   └── AndroidManifest.xml       # Manifest specifying GLES 3.0 & touchscreen configuration
│   ├── build.gradle.kts              # App module build config with R8 minification & NDK -O3 release flags
│   └── proguard-rules.pro            # R8/ProGuard keep rules for GameActivity & JNI callbacks
├── tests/
│   └── GameLogicTest.cpp             # Standalone C++17 unit test suite for math, physics, collisions & saves
├── .github/workflows/
│   └── build.yml                     # CI workflow running C++17 unit tests + Debug/Release APK builds
├── LICENSE                           # MIT License
└── README.md                         # Project documentation
```

---

## Running the Native C++17 Unit Tests

You can compile and run the core engine unit tests on any machine with a C++17 compiler (`g++` or `clang++`) without needing an Android device:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
  -Iapp/src/main/cpp \
  tests/GameLogicTest.cpp -o build_test_runner
./build_test_runner
```

---

## Building the Android APK

```bash
# Build Debug and Release APKs
./gradlew assembleDebug assembleRelease

# Install Debug APK on a connected Android device
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

---

## License

Distributed under the MIT License. See [`LICENSE`](LICENSE) for more information.
