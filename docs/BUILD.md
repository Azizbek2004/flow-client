# Building FLOW Client

## Prerequisites
- Qt 6.x (with CMake support)
- CMake 3.20+
- C++17 compliant compiler (MSVC 2019+, GCC 9+, Clang 10+)
- Go (for compiling XRay core)

## Build Steps

### 1. Clone Repository
```bash
git clone https://github.com/your-username/flow-client.git
cd flow-client
git submodule update --init --recursive
```

### 2. Build XRay Core
```bash
cd third-party/xray-core
go build -o ../../bin/xray-core
cd ../..
```

### 3. Configure and Build Client
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## Platform Specifics

### Windows
- Use Visual Studio 2019 or newer.
- Ensure Qt is in your PATH.

### macOS
- Use Xcode or Clang.
- `brew install qt`

### Linux
- `sudo apt install qt6-base-dev qt6-declarative-dev build-essential cmake`
