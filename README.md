# Attercop
## 🌐 WebGPU Graphics Engine 🚀

[![CMake CI](https://github.com/ThomasJowett/AttercopEngine/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/ThomasJowett/AttercopEngine/actions/workflows/cmake-multi-platform.yml)
![GitHub License](https://img.shields.io/github/license/ThomasJowett/AttercopEngine)
![Stars](https://img.shields.io/github/stars/ThomasJowett/AttercopEngine?style=social)


A cutting-edge **C++ graphics engine** built on **WebGPU** that brings high-performance, modern rendering capabilities to your applications. A work in progress, this engine aims to leverage the **power and flexibility of WebGPU** with seamless CMake integration for easy project setup and extension.

![Screenshot of WebGPU Graphics Engine](screenshots/Screenshot%202024-10-25%20at%2020.01.58.png)

## ✨ Features (Planned and In Progress)

- **Multi-platform support** (desktop and web)
- **Advanced shading capabilities** with real-time lighting
- **Cross-platform compatibility** powered by WebGPU
- Highly modular **architecture for ease of customization**

## 🔧 Installation Instructions

### Prerequisites
Ensure you have these installed:
- **C++17** or later
- **CMake** 3.8 or newer

### Quick Start

> ⚠️ **Warning**: This repository uses Git submodules. When cloning, make sure to use the `--recursive` flag:


1. Clone the repository:
   ```bash
   git clone https://github.com/ThomasJowett/AttercopEngine.git --recursive
   cd Attercop
   ```

   If you have already cloned without --recursive, you can initialize the submodules afterward by running:

   ```
   git submodule update --init --recursive
   ```

2. Create a build directory:
   ```bash
   mkdir build && cd build
   ```

3. Run CMake to configure the project:
   ```bash
   cmake ..
   ```

4. Build the project:
   ```bash
   cmake --build .
    ```

### Running the App
After a successful build, run the executable from the build directory:
```
./App/App
```

## 🤝 Contributing

Interested in contributing? Just open a pull request or an issue!
