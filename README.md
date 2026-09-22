# VeloSphere

[![Android](https://img.shields.io/badge/Platform-Android_24%2B-3DDC84?style=for-the-badge&logo=android&logoColor=white)](https://developer.android.com)
[![OpenGL ES](https://img.shields.io/badge/Graphics-OpenGL_ES_3.0-5586A4?style=for-the-badge&logo=opengl&logoColor=white)](https://www.khronos.org/opengles/)
[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/17)
[![GameActivity](https://img.shields.io/badge/Games_SDK-GameActivity_3.0.5-4285F4?style=for-the-badge&logo=google&logoColor=white)](https://developer.android.com/games/agdk/game-activity)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](LICENSE)

> A hardware-accelerated 3D endless runner built for Android using C++17, OpenGL ES 3.0, and Google's GameActivity library.

---

## Overview

**VeloSphere** delivers a retro-futuristic synthwave arcade experience engineered directly on top of the Android NDK. Rather than relying on third-party game engines, VeloSphere utilizes a custom C++ game loop, procedural 3D mesh pipelines, and custom GLSL shaders to achieve consistent 60/120 FPS performance with low-latency touch controls.

---

## Features

- **Native C++ Game Engine**:
  - Direct integration with Android surface lifecycle and hardware input buffers via Google's `GameActivity` (AGDK).
  - Custom column-major matrix math library (`Math3D.h`) implementing perspective projection, camera look-at tracking, and orthographic HUD projection.
  - Zero runtime garbage collection pauses during gameplay.

- **Procedural Synthwave Visuals & Shaders**:
  - Custom GLSL ES 3.0 shaders with multiple procedural rendering modes.
  - Synthwave infinite grid runway with glowing edge rails and dashed center markers.
  - Atmospheric depth fog fading dynamically into the dark horizon.
  - Blinn-Phong lighting model with high-specular chrome reflections.

- **3D Rolling Physics & Dynamics**:
  - **Visible Surface Markings**: Procedural glowing latitude rings and meridian bands on the sphere that roll in real time with the ball's 3D rotation.
  - **Dual-Axis Angular Roll**: Calculates forward pitch rotation ($\Delta \theta_x = \frac{v_{\text{fwd}}}{R} \Delta t$) and sideways yaw/roll ($\Delta \theta_z = -\frac{\Delta x}{R}$) based on lateral steering velocity.
  - **Aerodynamic Banking Tilt**: Smooth marble leaning into sharp steering turns with spring inertia.
  - **Track Contact Suspension**: Subtle vertical micro-vibration based on surface contact and ground speed.

- **Impact Rebound & Particle Simulation**:
  - Collisions trigger full physical velocity rebound, gravity drops, elastic track bounces, and 3D tumbling rotation.
  - Particle engine simulating 64 radial neon fire sparks upon collision, plus friction sparks on high-speed lateral drift.

- **Scoring System & Persistent Storage**:
  - Dynamic score formula combining distance traveled with speed-based multipliers (up to $2.5\times$ at maximum velocity).
  - $+100$ bonus points awarded for successfully dodging obstacles.
  - **Data Persistence**: Best score is saved to and loaded from private app storage (`internalDataPath/highscore.dat`) across application launches, process kills, and device reboots.

- **On-Screen OpenGL ES Arcade HUD**:
  - Built-in 5x7 dot-matrix bitmap font engine generating texture atlases and dynamic quad batches entirely in native code.
  - Renders live **SCORE**, persistent **BEST**, real-time **SPEED (MPH)**, animated **+100 DODGE!** alerts, and stylized **READY** / **GAME OVER** interactive cards.

- **Dynamic Camera**:
  - Speed-scaled FOV widening smoothly from 60° up to 70° as speed accelerates from 18 to 55 units/sec.
  - Smooth camera tracking lag with dynamic collision shake decay.

---

## How to Play

| Action | Control | Description |
| :--- | :--- | :--- |
| **Start / Retry** | **Tap Screen** | Starts the run or restarts from Game Over screen |
| **Steer Ball** | **Swipe / Drag Left & Right** | Controls the lateral movement and banking of the sphere |
| **Dodge Hazards** | **Weave between obstacles** | Avoid ruby barriers and moving amber hazards |

---

## Architecture & Project Structure

```
VeloSphere/
├── app/
│   ├── src/main/
│   │   ├── cpp/
│   │   │   ├── CMakeLists.txt        # Native build config linking GLESv3, EGL & GameActivity
│   │   │   ├── GameLogic.h           # Core game state, treadmill, ball physics & collision
│   │   │   ├── Math3D.h              # 3D Vector & Mat4 matrix mathematics (LookAt, Ortho, etc.)
│   │   │   ├── Mesh.h                # Procedural Sphere, Box, Track, Quad & HUD font engine
│   │   │   ├── Shader.h              # GLES 3.0 shader compilation & procedural GLSL programs
│   │   │   └── main.cpp              # EGL context, android_main loop, input & persistence
│   │   ├── java/.../MainActivity.kt  # Thin GameActivity wrapper with immersive fullscreen mode
│   │   ├── res/                      # App resources, icons, and themes
│   │   └── AndroidManifest.xml       # Manifest specifying GLES 3.0 & touchscreen requirements
│   └── build.gradle.kts              # App module build configuration
├── .github/workflows/
│   └── build.yml                     # Continuous Integration workflow building debug APK
├── .gitignore                        # Comprehensive Android & NDK ignore configuration
├── LICENSE                           # MIT License
└── README.md                         # Project documentation
```

---

## Tech Stack & Requirements

| Component | Specification |
| :--- | :--- |
| **Operating System** | Android 7.0 (API Level 24) or higher |
| **Target SDK** | Android 14 (API Level 34) |
| **Graphics API** | OpenGL ES 3.0 (`GLESv3`, `EGL`) |
| **NDK Version** | 26.1.10909125+ |
| **Build Tools** | CMake 3.22.1+, Ninja, Gradle 8.5+ |
| **Java / Kotlin** | JDK 17, Kotlin 1.9.24 |
| **Libraries** | `androidx.games:games-activity:3.0.5` |

---

## Building & Running

### Option 1: Android Studio
1. Open **Android Studio** (Hedgehog, Iguana, Koala, Ladybug, or newer).
2. Select **Open** and choose the `VeloSphere` root directory.
3. Allow Gradle to sync dependencies and CMake to configure native targets.
4. Connect an Android device (or launch an emulator with OpenGL ES 3.0 support).
5. Click **Run** (`Shift + F10`).

### Option 2: Command Line (Gradle)
```bash
# Clone the repository
git clone https://github.com/<your-username>/VeloSphere.git
cd VeloSphere

# Build the Debug APK
./gradlew assembleDebug

# Install on a connected device via ADB
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

---

## Contributing

Contributions, issues, and feature requests are welcome. Feel free to check the [issues page](https://github.com/<your-username>/VeloSphere/issues).

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/YourFeature`)
3. Commit your Changes (`git commit -m 'Add some feature'`)
4. Push to the Branch (`git push origin feature/YourFeature`)
5. Open a Pull Request

---

## License

Distributed under the MIT License. See [`LICENSE`](LICENSE) for more information.
