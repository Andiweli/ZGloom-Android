# ZGloom-Android <br/>Amiga Gloom engine for Android & OUYA

ZGloom-Android is an Android port of the modern **ZGloom** engine, bringing the classic Amiga FPS **Gloom** (plus Gloom Deluxe, Gloom 3 and Zombie Massacre) to Android devices and the OUYA micro-console.

This project is based on the [Windows ZGloom fork](https://github.com/Andiweli/ZGloom) and adapts the renderer and game logic for Android (SDL2 + native C++).

---

## ✨ Key Features

- **Android port of the Amiga Gloom engine**  
  Runs the original Gloom data files on Android / OUYA using a native C++ core.

- **4:3 and 16:9 display modes with FOV control**  
  Switch between the classic 4:3 look and a widescreen 16:9 mode and adjust the field of view to match your TV or display.

- **Improved software renderer**  
  Uses the updated ZGloom renderer with cleaner perspective, fewer glitches and subtle visual polish.

- **Dynamic muzzle flash & projectile reflections**  
  Each shot briefly brightens the scene, and colored reflection ellipses are drawn under projectiles and weapon upgrade orbs, matching weapon type and upgrade level.

> Note: ZGloom-Android is under active development. Not all features from the Windows port are available in every test build yet.

<img width="1280" height="1440" alt="image" src="https://github.com/user-attachments/assets/a5414fd4-56f8-4db9-97b3-ca781fd41e31" />


---

## 📦 Status

ZGloom-Android is currently in **early development / testing**:

- Focus: OUYA and Android TV-style devices with a gamepad  
- Touch controls are not in the scope of this Port  

Check the **Releases** section for test builds once they become available.

---

## 🚀 Getting Started

1. Install the ZGloom-Android `.apk` on your Android device or OUYA.
2. Launch ZGloom-Android.

---

## 🛠 Building from Source (brief)

This project is intended to be built with **Android Studio** using **Gradle** and **CMake**:

- Native core: C++ (ZGloom engine + SDL2)
- Java/Kotlin wrapper: Android launcher activity and input handling

High-level steps:

1. Clone this repository.
2. Open the project in **Android Studio**.
3. Let Gradle sync and download dependencies.
4. Build and run on an Android device (or OUYA) with **developer mode** enabled.

Detailed build instructions will be added as the project matures.

---

## ℹ️ About

ZGloom-Android aims to bring the enhanced **ZGloom** experience to Android and OUYA:

- modernized renderer and visual tweaks  
- support for multiple Gloom-based games and selected mods  
- console-style controls and TV-friendly presentation
- background ambience by Prophet
