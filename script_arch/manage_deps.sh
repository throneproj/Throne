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
        echo "   Size: $(du -sh "$BUILT_DIR" 2>/dev/null | cut -f1 || echo 'Unknown')"
    else
        echo "   Status: ❌ Not built"
    fi
}

# Main execution
case "${1:-status}" in
    "build")
        build_deps
        ;;
    "clean")
        clean_deps
        ;;
    "rebuild")
        clean_deps
        build_deps
        ;;
    "status")
        show_status
        ;;
    *)
        echo "Usage: $0 {build|clean|rebuild|status}"
        echo ""
        echo "Commands:"
        echo "  build    - Build dependencies (if not already built)"
        echo "  clean    - Clean all built dependencies"
        echo "  rebuild  - Clean and rebuild dependencies"
        echo "  status   - Show current status (default)"
        exit 1
        ;;
esac 