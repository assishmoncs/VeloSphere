# VeloSphere 4.0 — Apex Edition

[![Build Status](https://github.com/assishmoncs/VeloSphere/actions/workflows/build.yml/badge.svg)](https://github.com/assishmoncs/VeloSphere/actions/workflows/build.yml)
[![Android](https://img.shields.io/badge/Platform-Android_24%2B-3DDC84?style=for-the-badge&logo=android&logoColor=white)](https://developer.android.com)
[![OpenGL ES](https://img.shields.io/badge/Graphics-OpenGL_ES_3.0-5586A4?style=for-the-badge&logo=opengl&logoColor=white)](https://www.khronos.org/opengles/)
[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/17)
[![GameActivity](https://img.shields.io/badge/Games_SDK-GameActivity_4.4.2-4285F4?style=for-the-badge&logo=google&logoColor=white)](https://developer.android.com/games/agdk/game-activity)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](LICENSE)

> A portrait-locked, hardware-accelerated 3D curved-world synthwave endless runner built for Android using C++17, OpenGL ES 3.0, and Google's GameActivity library.

## What's New in VeloSphere 4.0 — Apex Edition

### Total Visual & 3D Animation Redesign
- **Dynamic Curved World Horizon Shader**: Camera-relative vertex curvature bends the 8-slice cyber-highway, arches, hazards, trail ribbon, and particles smoothly left, right, and over the horizon (toggleable in Settings) with zero impact on deterministic collision physics.
- **Articulated 3D Apex Gyro-Core Vessel**: Multi-part hierarchical 3D player rig with a metallic plasma core, two counter-rotating gyroscopic `TorusMesh` rings that bank into turns, smooth 360° jump flips, spring-damped fighter-jet camera roll, and expanding **3D Shockwave Rings** on jump, landing, near-miss, shield shatter, and Phase-Dash.
- **Roadside 3D Cyber-Architecture**: Glowing neon overhead cyber-arches and floating rotating roadside energy pylons rush past to deliver intense 3D depth and speed.
- **6 Articulated 3D Hazard Machines**: Cyber-Pillar Monoliths (collar ring + floating capstone crystal), Articulated Sweeper Drones (dual counter-spinning rotor rings), Twin-Emitter Low Laser Hurdles, Oscillating Hydraulic Overhead Gates, **Rotating Laser Spinners**, and animated **Split Gates** that slide open on approach.
- **High-Res Smooth Anti-Aliased Vector Font**: Replaced blocky 5×7 pixel text with a procedurally rasterized `512×256` smooth SDF-like geometric font atlas (`GL_LINEAR` + crisp shader edge shaping).

### Challenging Gameplay & Meta-Progression
- **Double-Tap Warp Phase-Dash**: Active clutch ability with cooldown bar—double-tap to phase straight through hazards for `+150 PHASE DODGE!`, shockwave bursts, and cinematic micro-hitstop.
- **Intensity Surge Waves**: Every 250m escalates obstacle density, speed, and hazard variety (Laser Spinners, Split Gates, Dual-Lane Walls).
- **Hyperflow (Fever) 2x, Rotating Missions, 4 Tech Upgrades & Revive**: Stack up to **10x** multipliers, complete 3 rotating missions to rank up (`ROOKIE` → `LEGEND`), unlock 7 Skins & 4 Tech Upgrades (Lv 0–5), and use 25-Orb Second Chance Revives.

### Dedicated Portrait-Locked Screens & Audio
- **9 Complete Screens**: **Home**, **Garage** (3D turntable Skins + Tech Upgrades), **Missions & Stats**, **Settings** (8 independent toggles including World Curve & Graphics Quality), **Help** (Controls, 6 Hazards & Pro Tips), **About** (Engine Specs, Credits & MIT License), **In-Game HUD**, **Pause**, and **Run Summary / Revive**.
- **Strict Portrait Lock**: Enforced in both `AndroidManifest.xml` and `MainActivity.kt` (`SCREEN_ORIENTATION_PORTRAIT`).
- **13-Channel Procedural Synthwave SFX Bank** + CRC32 `SaveData` v4 with automatic v2 migration.

## Running Native C++17 Unit Tests

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
