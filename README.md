# ZGloom-Android - Amiga Gloom port for Android & OUYA

Android / OUYA / Logitech G-Cloud port of the modern **ZGloom** engine, bringing the classic Amiga FPS **Gloom** and its successors to **gamepad-driven** Android and Chrome OS devices.  

> Play Gloom, Gloom Deluxe, Gloom 3 and Zombie Massacre on Android and OUYA with a fixed renderer, widescreen support, post-processing overlays and save/load position – while staying faithful to the original Amiga gameplay.

[![Android 16](https://img.shields.io/badge/up%20to-Android%2016-green)](https://github.com/andiweli/ZGloom-Android/releases/latest)
[![Architecture](https://img.shields.io/badge/architecture-32%2F64bit-orange.svg)](https://github.com/andiweli/ZGloom-Android/releases/latest)
[![Engine](https://img.shields.io/badge/engine-SDL2%20%2B%20LibXMP-brightgreen.svg)](https://github.com/andiweli/ZGloom-Android/releases/latest)
![AI](https://img.shields.io/badge/AI-assisted%20coding-6e7781)
[![Controller](https://img.shields.io/badge/controls-Gamepad/Touch/Keyb-blueviolet)](https://github.com/andiweli/ZGloom-Android/releases/latest)
[![Support via PayPal](https://img.shields.io/badge/Support%20via-PayPal-0070BA?logo=paypal\&logoColor=white)](https://paypal.me/andiweli)


ZGloom-Android is part of a family of cross-platform Gloom source ports that share the same renderer, options and feature set across desktop and console-style systems. This edition targets Android-based hardware such as the OUYA micro-console and Android TV boxes, with a focus on couch play using a controller.

For other platforms, see the companion projects [ZGloom-x86 (Windows)](https://github.com/Andiweli/ZGloom-x86), [ZGloom-Vita-Vita2D (PS Vita / PSTV)](https://github.com/Andiweli/ZGloom-Vita-Vita2D) and [ZGloom-macOS](https://github.com/Andiweli/ZGloom-macOS).

---

## 🕹 What is Gloom?

[Gloom](https://en.wikipedia.org/wiki/Gloom_(video_game)) was a 1995 Doom-like first-person shooter from **Black Magic Software** for the Commodore Amiga. It featured very messy and meaty graphics and required a powerful Amiga at the time (an A1200 with 030 CPU was still on the low end). The engine later powered several related games and successors, including:

- **Gloom Deluxe / Ultimate Gloom** – enhanced graphics and effects  
- **Gloom 3**  
- **Zombie Massacre**  
- Various full-game conversions of other 90’s Amiga titles

ZGloom is a modern reimplementation of this engine.

---

## ✨ Key Features

- Modern source port of the Amiga Gloom engine  
  Runs the original Gloom data files on Android devices (with a focus on OUYA and Android TV-style boxes) using the modern ZGloom C++ engine.

- Supports multiple official games  
  Play **Gloom**, **Gloom Deluxe / Ultimate Gloom**, **Gloom 3** and **Zombie Massacre** (plus selected mods where available).

- Built-in multi-game launcher  
  If more than one game or mod is present, a simple launcher lets you pick what to play at startup.

- 4:3 and 16:9 display modes with FOV control  
  Switch between the classic 4:3 Amiga look and a widescreen 16:9 mode and adjust the field of view to match your TV or monitor.

- Improved renderer, lighting and effects  
  Uses the fixed ZGloom renderer with cleaner perspective, fewer glitches and subtle lighting tweaks, including dust particles, dynamic muzzle flashes and colored floor reflections under projectiles and weapon upgrade orbs.

- Atmospheric post-processing overlays (optional)  
  Enable vignette, film grain and scanlines for a more gritty, CRT-style presentation without changing gameplay.

- Save/Load position and extended options  
  Save your in-level position (including health, weapon and ammo state) and tweak many more options than in the original Amiga release.

---

## 🖼️ Screenshots

<img width="1920" height="1080" alt="gloom0" src="https://github.com/user-attachments/assets/94e04fe1-75d7-4623-b9c9-dceda3078d46" />
<img width="1920" height="1080" alt="gloom1" src="https://github.com/user-attachments/assets/3b33f14e-f3a9-4b1e-b679-6c0e97f32759" />
<img width="1920" height="1080" alt="gloom2" src="https://github.com/user-attachments/assets/4bfb349c-c74c-4115-a31c-4794bfa52be2" />
<img width="1920" height="1080" alt="gloom3" src="https://github.com/user-attachments/assets/3af93d27-7bda-498d-a23c-bf80313a579c" />
<img width="1920" height="1080" alt="gloom4" src="https://github.com/user-attachments/assets/41ae174c-c69d-48f7-a768-30a430faa8e5" />


---

## 🚀 Getting Started

1. Install the ZGloom-Android `.apk` on your Android device or OUYA (gamedata is included and will be installed upon first start).
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

## ❤️ Support

If you enjoy this project and would like to support my work, you can make a small contribution via PayPal.

Your support helps me spend more time maintaining existing projects, fixing bugs, improving compatibility, and working on new features.

[![Support via PayPal](https://img.shields.io/badge/Support%20via-PayPal-0070BA?logo=paypal\&logoColor=white)](https://paypal.me/andiweli)

Thank you for your support!

---

## ℹ️ About

ZGloom-Android aims to bring the enhanced **ZGloom** experience to Android and OUYA:

- modernized renderer and visual tweaks  
- support for multiple Gloom-based games and selected mods  
- console-style controls and TV-friendly presentation
- background ambience credit goes to Prophet
  
**Keywords / Topics:**  
_amiga • gloom • vita • psvita • windows • x86 • android • macos • homebrew • zgloom • gloomdeluxe • zombiemassacre • sdl • libxmp • vita2d • ps tv shooter_

If you enjoy it, feel free to ⭐ star the repo so other PS Vita & Amiga fans can find it more easily.
