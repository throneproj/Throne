#!/bin/bash
set -e

echo "🔧 Nekoray Dependencies Manager"

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="$PROJECT_ROOT/libs/deps"
BUILT_DIR="$DEPS_DIR/built"

# Function to check if dependencies are built
check_deps_built() {
    if [ -d "$BUILT_DIR" ] && [ -f "$BUILT_DIR/lib/libprotobuf.a" ]; then
        echo "✅ Dependencies are already built"
        return 0
    else
        echo "❌ Dependencies are not built"
        return 1
    fi
}

# Function to clean dependencies
clean_deps() {
    echo "🧹 Cleaning dependencies..."
    rm -rf "$DEPS_DIR"
    echo "✅ Dependencies cleaned"
}

# Function to build dependencies
build_deps() {
    echo "🔧 Building dependencies..."
    cd "$PROJECT_ROOT"
    
    if [ ! -f "script/build_deps_all.sh" ]; then
        echo "❌ Dependencies build script not found"
        exit 1
    fi
    
    # Ensure we have the required tools
    for tool in cmake ninja git; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            echo "❌ Required tool not found: $tool"
            echo "💡 Install with: sudo pacman -S $tool"
            exit 1
        fi
    done
    
    chmod +x script/build_deps_all.sh
    ./script/build_deps_all.sh
    
    if check_deps_built; then
        echo "✅ Dependencies built successfully"
    else
        echo "❌ Failed to build dependencies"
        exit 1
    fi
}

# Function to show status
show_status() {
    echo "📊 Dependencies Status:"
    echo "   Project root: $PROJECT_ROOT"
    echo "   Dependencies dir: $DEPS_DIR"
    echo "   Built dir: $BUILT_DIR"
    
    if check_deps_built; then
        echo "   Status: ✅ Built"
        if [ -d "$BUILT_DIR" ]; then
            echo "   Size: $(du -sh "$BUILT_DIR" 2>/dev/null | cut -f1 || echo 'Unknown')"
            echo "   Files:"
            find "$BUILT_DIR" -name "*.a" -o -name "*.so" 2>/dev/null | head -5 | while read file; do
                echo "     - $(basename "$file")"
            done
        fi
    else
        echo "   Status: ❌ Not built"
    fi
    
    echo ""
    echo "🔧 Build Environment:"
    echo "   CMake: $(cmake --version 2>/dev/null | head -1 || echo 'Not found')"
    echo "   Ninja: $(ninja --version 2>/dev/null || echo 'Not found')"
    echo "   Git: $(git --version 2>/dev/null || echo 'Not found')"
    echo "   Go: $(go version 2>/dev/null || echo 'Not found')"
}

# Function to check build environment
check_env() {
    echo "🔍 Checking build environment..."
    
    local missing=()
    
    # Check required tools
    for tool in cmake ninja git go; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            missing+=("$tool")
        fi
    done
    
    # Check required packages
    for pkg in qt6-base qt6-tools qt6-declarative qt6-networkauth protobuf base-devel; do
        if ! pacman -Q "$pkg" &>/dev/null; then
            missing+=("$pkg")
        fi
    done
    
    if [ ${#missing[@]} -gt 0 ]; then
        echo "❌ Missing: ${missing[*]}"
        echo "💡 Install with: sudo pacman -S ${missing[*]}"
        return 1
    else
        echo "✅ All required tools and packages found"
        return 0
    fi
}

# Main execution
case "${1:-status}" in
    "build")
        check_env || exit 1
        build_deps
        ;;
    "clean")
        clean_deps
        ;;
    "rebuild")
        check_env || exit 1
        clean_deps
        build_deps
        ;;
    "status")
        show_status
        ;;
    "check")
        check_env
        ;;
    *)
        echo "Usage: $0 {build|clean|rebuild|status|check}"
        echo ""
        echo "Commands:"
        echo "  build    - Build dependencies (if not already built)"
        echo "  clean    - Clean all built dependencies"
        echo "  rebuild  - Clean and rebuild dependencies"
        echo "  status   - Show current status (default)"
        echo "  check    - Check build environment"
        exit 1
        ;;
esac 