#!/bin/bash

echo "🚀 Starting Flow Environment Setup..."

# 1. Check/Install Homebrew
if ! command -v brew &> /dev/null; then
    echo "📦 Homebrew not found. Installing..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    
    # Add to PATH for Apple Silicon
    if [[ -f /opt/homebrew/bin/brew ]]; then
        echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
        eval "$(/opt/homebrew/bin/brew shellenv)"
    fi
     # Add to PATH for Intel
    if [[ -f /usr/local/bin/brew ]]; then
        echo 'eval "$(/usr/local/bin/brew shellenv)"' >> ~/.zprofile
        eval "$(/usr/local/bin/brew shellenv)"
    fi
else
    echo "✅ Homebrew is already installed."
fi

# 2. Update Homebrew
echo "🔄 Updating Homebrew..."
brew update

# 3. Install CMake
if ! command -v cmake &> /dev/null; then
    echo "📦 Installing CMake..."
    brew install cmake
else
    echo "✅ CMake is already installed."
fi

# 4. Install Qt6
echo "📦 Installing Qt6..."
brew install qt@6

# 5. Link Qt6
echo "🔗 Linking Qt6..."
brew link qt@6 --force

echo "✅ Environment Setup Complete!"
echo "👉 Please restart your terminal or run: source ~/.zprofile"
echo ""
echo "Then you can run the build command:"
echo "   cd flow-client/scripts && ./build_macos.sh"
