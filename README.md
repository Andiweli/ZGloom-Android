# ZGloom-Android <br/>Amiga Gloom engine for Android & OUYA

ZGloom-Android is an Android port of the modern **ZGloom** engine, bringing the classic Amiga FPS **Gloom** (plus Gloom Deluxe, Gloom 3 and Zombie Massacre) to Android devices and the OUYA micro-console.

This project is based on the Windows ZGloom fork and adapts the renderer and game logic for Android (SDL2 + native C++).

---

## ✨ Key Features

- **Android port of the Amiga Gloom engine**  
  Runs the original Gloom data files on Android / OUYA using a native C++ core.

- **Supports original games and compatible mods**  
  Works with the Gloom family of games and compatible mods (e.g. “Death Mask”, “8bit Killer”) as long as they follow the original engine layout.

- **Built-in multi-game launcher**  
  When multiple games are installed, a simple launcher lets you choose between **Gloom**, **Gloom Deluxe**, **Gloom 3**, **Zombie Massacre** or supported mods at startup.

- **4:3 and 16:9 display modes with FOV control**  
  Switch between the classic 4:3 look and a widescreen 16:9 mode and adjust the field of view to match your TV or display.

- **Improved software renderer**  
  Uses the updated ZGloom renderer with cleaner perspective, fewer glitches and subtle visual polish.

- **Dynamic muzzle flash & projectile reflections**  
  Each shot briefly brightens the scene, and colored reflection ellipses are drawn under projectiles and weapon upgrade orbs, matching weapon type and upgrade level.

> Note: ZGloom-Android is under active development. Not all features from the Windows port are available in every test build yet.

---

## 📦 Status

ZGloom-Android is currently in **early development / testing**:

- Focus: OUYA and Android TV-style devices with a gamepad  
- Touch controls are not the main target right now  

Check the **Releases** section for test builds once they become available.

---

## 🚀 Getting Started

1. Install the ZGloom-Android `.apk` on your Android device or OUYA.
2. Copy your original Gloom game data to the app’s data directory (exact paths will be documented in the first release).
3. Launch ZGloom-Android and select the game you want to play from the launcher.

You will need data from at least one of:

- **Gloom**
- **Gloom Deluxe**
- **Gloom 3**
- **Zombie Massacre**
- compatible mods

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

If you are interested in testing or contributing, feel free to open an issue or pull request.
