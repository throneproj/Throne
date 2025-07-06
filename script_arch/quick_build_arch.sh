#!/bin/bash
set -e

echo "🚀 Quick Nekoray Build for Arch Linux"

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
DEPLOY_DIR="$PROJECT_ROOT/deployment/linux64"

# Create directories
mkdir -p "$BUILD_DIR"
mkdir -p "$DEPLOY_DIR"

echo "📁 Project root: $PROJECT_ROOT"

# Check dependencies
echo "🔍 Checking dependencies..."
missing_deps=()
for pkg in cmake ninja qt6-base qt6-tools qt6-declarative qt6-networkauth go protobuf base-devel make; do
    if ! pacman -Q "$pkg" &>/dev/null; then
        missing_deps+=("$pkg")
    fi
done

if [ ${#missing_deps[@]} -gt 0 ]; then
    echo "❌ Missing: ${missing_deps[*]}"
    echo "💡 Install with: sudo pacman -S ${missing_deps[*]}"
    exit 1
fi

echo "✅ All dependencies found"

# Build dependencies (with cache check)
echo "🔧 Managing dependencies..."
cd "$PROJECT_ROOT"

# Use the dependencies manager
chmod +x script_arch/manage_deps.sh
if ./script_arch/manage_deps.sh status | grep -q "❌ Dependencies are not built"; then
    echo "🔧 Building dependencies..."
    ./script_arch/manage_deps.sh build
else
    echo "✅ Dependencies already built, skipping..."
fi

# Build Go core
echo "🐹 Building Go core..."
cd "$PROJECT_ROOT/core/server"
export GOOS=linux
export GOARCH=amd64
export CGO_ENABLED=1

# Initialize go module if needed
if [ ! -f "go.mod" ]; then
    echo "📝 Initializing Go module..."
    go mod init nekobox_core
    go mod tidy
else
    go mod tidy
fi

# Build all Go files together
go build -ldflags="-s -w" -o nekobox_core .
cp nekobox_core "$BUILD_DIR/"

# Build updater
cd "$PROJECT_ROOT/core/updater"
if [ ! -f "go.mod" ]; then
    echo "📝 Initializing Go module for updater..."
    go mod init nekobox_updater
    go mod tidy
else
    go mod tidy
fi

go build -ldflags="-s -w" -o updater .
cp updater "$BUILD_DIR/"

echo "✅ Go core built"

# Build C++ GUI
echo "⚙️ Building C++ GUI..."
cd "$BUILD_DIR"
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja -j$(nproc)

echo "✅ C++ GUI built"

# Download assets
echo "📥 Downloading assets..."
cd "$BUILD_DIR"
if [ ! -f "geoip.db" ]; then
    wget -O geoip.db "https://github.com/SagerNet/sing-geoip/releases/latest/download/geoip.db"
fi
if [ ! -f "geosite.db" ]; then
    wget -O geosite.db "https://github.com/SagerNet/sing-geosite/releases/latest/download/geosite.db"
fi

# Create deployment
echo "📦 Creating deployment..."
rm -rf "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR"

cp "$BUILD_DIR/nekoray" "$DEPLOY_DIR/"
cp "$BUILD_DIR/nekobox_core" "$DEPLOY_DIR/"
cp "$BUILD_DIR/updater" "$DEPLOY_DIR/"
cp "$BUILD_DIR/geoip.db" "$DEPLOY_DIR/"
cp "$BUILD_DIR/geosite.db" "$DEPLOY_DIR/"

# Copy translations
mkdir -p "$DEPLOY_DIR/translations"
cp "$BUILD_DIR"/*.qm "$DEPLOY_DIR/translations/" 2>/dev/null || true

# Copy resources
mkdir -p "$DEPLOY_DIR/res"
cp -r "$PROJECT_ROOT/res"/* "$DEPLOY_DIR/res/" 2>/dev/null || true

# Make executable
chmod +x "$DEPLOY_DIR"/*

# Create package
cd "$DEPLOY_DIR"
tar -czf nekoray-linux64.tar.gz \
    nekoray \
    nekobox_core \
    updater \
    geoip.db \
    geosite.db \
    translations/ \
    res/

echo "🎉 Build completed!"
echo "📁 Files in: $DEPLOY_DIR"
echo "📦 Package: $DEPLOY_DIR/nekoray-linux64.tar.gz"
echo "🚀 Run with: cd $DEPLOY_DIR && ./nekoray" 