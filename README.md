# Electrostatics & Electrodynamics Sandbox

A real-time, interactive physics sandbox built in C++ and SFML. Users can place, drag, and modify charged particles to visualize electric fields and particle dynamics.

## Features
* **Interactive Sandbox**: Left-click to spawn and drag positive charges; Right-click for negative charges.
* **Physics Engine**: Calculates real-time electrostatic forces using $F = ma$ and supports particle collisions with energy damping.
* **Dynamic Control**:
    * **Scroll Wheel**: Adjust the magnitude ($q$) of the selected charge.
    * **M / N Keys**: Adjust the mass of the selected charge.
    * **Delete Key**: Remove a selected charge from the simulation.
    * **Spacebar**: Toggle the physics simulation pause/play state.
    * **R Key**: Reset the entire sandbox.
* **Visual Feedback**: Real-time field arrows updated with color-coded strength, and a selection "bulls-eye" indicator.

## Requirements
* [SFML (Simple and Fast Multimedia Library)](https://www.sfml-dev.org/)
* A C++17 compatible compiler (or later).

## How to Compile & Run
1. Ensure SFML is installed on your system.
2. Compile the source file using `g++`:
   ```bash
   g++ Electrostat.cpp -o Electrostat -lsfml-graphics -lsfml-window -lsfml-system
3. Run the executable from project root:
    ```bash
    ./bin/elec.exe

## Controls Reference
| Key / Action | Function |
| :--- | :--- |
| **Left Click** | Select / Drag / Spawn positive charge |
| **Right Click** | Spawn negative charge |
| **Scroll Wheel** | Modify charge magnitude ($q$) |
| **Spacebar** | Toggle simulation (Pause / Play) |
| **M / N** | Increase / Decrease mass |
| **Delete** | Remove selected charge |
| **R** | Reset sandbox (Clear all charges) |
| **Esc** | Deselect charge |