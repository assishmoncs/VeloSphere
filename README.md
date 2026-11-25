# VeloSphere 3.0 — Hyperflow Edition

[![Build Status](https://github.com/assishmoncs/VeloSphere/actions/workflows/build.yml/badge.svg)](https://github.com/assishmoncs/VeloSphere/actions/workflows/build.yml)
[![Android](https://img.shields.io/badge/Platform-Android_24%2B-3DDC84?style=for-the-badge&logo=android&logoColor=white)](https://developer.android.com)
[![OpenGL ES](https://img.shields.io/badge/Graphics-OpenGL_ES_3.0-5586A4?style=for-the-badge&logo=opengl&logoColor=white)](https://www.khronos.org/opengles/)
[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/17)
[![GameActivity](https://img.shields.io/badge/Games_SDK-GameActivity_4.4.2-4285F4?style=for-the-badge&logo=google&logoColor=white)](https://developer.android.com/games/agdk/game-activity)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](LICENSE)

> A hardware-accelerated 3D synthwave endless runner built for Android using C++17, OpenGL ES 3.0, and Google's GameActivity library.

## What's New in VeloSphere 3.0 (Hyperflow Edition)

### Addictive Gameplay & Meta-Progression
- **Hyperflow (Fever) Risk-Reward Gauge**: Chain dodges, Data Orbs, and **Near Misses** to fill the Hyperflow meter to 100% and unleash a 6-second **2x Score Multiplier** (stacking with 1x–5x combos up to **10x**!) with rainbow neon trails and near-miss duration extensions.
- **Rotating Missions & Pilot Rank**: 3 active rotating challenges (Collect Orbs, Dodge Obstacles, Near Misses, Distance, Combos, Hyperflow triggers) that award Data Orbs mid-run and level up your Pilot Rank (`ROOKIE` → `PILOT` → `RACER` → `ACE` → `PHANTOM` → `LEGEND`).
- **4 Permanent Tech Upgrades (Levels 0–5)**: Spend Data Orbs in the Garage to permanently upgrade **Aegis Capacitor** (Lv3+ starts runs with a Shield), **Magnet Coil**, **Chrono Core**, and **Hyper Thruster**.
- **Second Chance Revive**: Spend 25 Data Orbs once per run on Game Over to clear nearby hazards with a shockwave and keep your high-score run alive.
- **Dual-Lane Wall Formations & Crystal Pickups**: High-speed 2-lane hazard walls (always guaranteeing a fair safe lane) and faceted octahedron Data Orb & Power-Up crystals.
- **Pro-Feel Controls**: Jump Input Buffering (`0.15s`), elastic landing squash-and-stretch, and selectable **Smooth Drag** or **Swipe-to-Lane** control schemes with Low/Med/High sensitivity.

### Zero-Lag OpenGL ES 3.0 Graphics Pipeline
- **4 Specialized Shaders**: Split the legacy uber-shader into dedicated `SceneShader` (zero per-vertex `inverse()` matrix calls), 1-draw-call `SkyShader`, batched `ParticleShader`, and SDF Glassmorphic `UiShader`.
- **1-Draw-Call Procedural Synthwave Horizon**: Animated striped Outrun sun, twilight gradient, scrolling neon mountain ridges, and twinkling starfield rendered in a single fullscreen quad with zero textures.
- **Batched Rendering & Zero Per-Frame Allocations**: All particles (up to 160) render in **1 single draw call**; `HudRenderer` uses fixed pre-allocated vertex arrays with zero `std::vector` allocations per frame; `GL_CULL_FACE` enabled on closed 3D meshes.
- **3D Neon Energy Trail Ribbon & Ground Contact Shadow**: Smooth 18-point glowing trail ribbon and height-reactive contact shadow for crisp jump depth perception.
- **Smooth Biome Interpolation**: Seamless color blending across 450m sector transitions (`NEON GRID` → `SOLAR OUTRUN` → `CYBER MATRIX` → `VOID HORIZON`).

### Full Multi-Screen UI & Procedural Synthwave Audio
- **7 Complete Screens**: **Home** (3D showcase), **Garage** (Live 3D turntable for 7 Skins + Tech Upgrades tab), **Missions & Career Stats**, **Settings** (Music, SFX, Haptics, Controls, Sensitivity, Graphics Quality Low/Med/High, FPS Counter), **In-Game HUD**, **Pause Overlay**, and **Run Summary / Revive**.
- **12-Channel Procedural Synthwave SFX Bank**: Synthesizes distinct 16-bit PCM WAV sound effects at startup in `MainActivity.kt` for every arcade event (orbs, near-misses, Hyperflow chord, jump, slam, shield smash, UI taps, mission fanfare).
- **CRC32 SaveData v3**: Automatic binary migration from v2 (`velosphere_save.dat`) and v1 (`highscore.dat`).

## Running the Native C++17 Unit Tests

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
  -Iapp/src/main/cpp \
  tests/GameLogicTest.cpp -o build_test_runner
./build_test_runner
```

## Building the Android APK

```bash
./gradlew assembleDebug assembleRelease
```

## License

Distributed under the MIT License. See [`LICENSE`](LICENSE) for more information.
