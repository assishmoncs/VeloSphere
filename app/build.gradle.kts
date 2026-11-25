@file:Suppress("UnstableApiUsage")

plugins {
    id("com.android.application")
}

android {
    namespace = "com.hsissa.velosphere"
    compileSdk = 35
    ndkVersion = "26.1.10909125"

    defaultConfig {
        applicationId = "com.hsissa.velosphere"
        minSdk = 24
        targetSdk = 35
        versionCode = 4
        versionName = "3.0.0"

        ndk {
            abiFilters.addAll(listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64"))
        }

        externalNativeBuild {
            cmake {
                arguments.addAll(
                    listOf(
                        "-DANDROID_STL=c++_shared",
                        "-DANDROID_TOOLCHAIN=clang"
                    )
                )
                cppFlags.addAll(
                    listOf(
                        "-std=c++17",
                        "-fexceptions",
                        "-frtti",
                        "-Wall",
                        "-Wextra"
                    )
                )
            }
        }
    }

    buildTypes {
        debug {
            externalNativeBuild {
                cmake {
                    cppFlags.addAll(listOf("-O0", "-g"))
                }
            }
        }
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            ndk {
                debugSymbolLevel = "FULL"
            }
            externalNativeBuild {
                cmake {
                    cppFlags.addAll(listOf("-O3", "-DNDEBUG"))
                }
            }
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        prefab = true
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    lint {
        disable += "GradleDependency"
    }
}

dependencies {
    implementation("androidx.core:core-splashscreen:1.2.0")
    implementation("androidx.games:games-activity:4.4.2")
    implementation("androidx.core:core-ktx:1.15.0")
    implementation("androidx.appcompat:appcompat:1.8.0")
}
