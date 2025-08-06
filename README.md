# Arbit -

Arbit is a modular trading and arbitrage engine integrating multiple exchange (Bitcoin, Bybit, Okx ) WebSocket clients, order book management, and performance monitoring, built with CMake.

Demo video:


##  Folder Structure

- `src/` - Source files
- `include/` - Header files
- `build/` - Build output directory
- `extern/` - External dependencies (if any)
- `.vscode/` - VSCode settings
- `CMakeLists.txt` - CMake build file

## Build & Run

### Prerequisites

- CMake ≥ 3.10
- g++ or clang++
- Make

### Steps

```bash
# Go to project root
cd Arbit

# Create build directory
mkdir -p build && cd build

# Run CMake
cmake ..

# Build the project
make

# Run the executable (adjust name as needed)
./Arbit
