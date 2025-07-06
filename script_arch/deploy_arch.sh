#!/bin/bash
set -e

echo "🚀 Nekoray Deployment for Arch Linux"

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
DEPLOY_DIR="$PROJECT_ROOT/deployment/linux64"

# Check if build exists
if [ ! -f "$BUILD_DIR/nekoray" ]; then
    echo "❌ Build not found. Please run quick_build_arch.sh first."
    exit 1
fi

echo "📁 Project root: $PROJECT_ROOT"
echo "📦 Build directory: $BUILD_DIR"
echo "📦 Deploy directory: $DEPLOY_DIR"

# Clean and create deployment directory
rm -rf "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR"

# Copy main binary
cp "$BUILD_DIR/nekoray" "$DEPLOY_DIR/"
cp "$BUILD_DIR/nekobox_core" "$DEPLOY_DIR/"
cp "$BUILD_DIR/updater" "$DEPLOY_DIR/"

# Copy assets
cp "$BUILD_DIR/geoip.db" "$DEPLOY_DIR/" 2>/dev/null || true
cp "$BUILD_DIR/geosite.db" "$DEPLOY_DIR/" 2>/dev/null || true

# Copy translations
mkdir -p "$DEPLOY_DIR/translations"
cp "$BUILD_DIR"/*.qm "$DEPLOY_DIR/translations/" 2>/dev/null || true

# Copy resources
mkdir -p "$DEPLOY_DIR/res"
cp -r "$PROJECT_ROOT/res"/* "$DEPLOY_DIR/res/" 2>/dev/null || true

# Get Qt plugin path
QT_PLUGIN_PATH=$(find /usr/lib/qt6 -name "plugins" -type d 2>/dev/null | head -1)
if [ -z "$QT_PLUGIN_PATH" ]; then
    QT_PLUGIN_PATH=$(find /usr/lib/qt -name "plugins" -type d 2>/dev/null | head -1)
fi

if [ -z "$QT_PLUGIN_PATH" ]; then
    echo "⚠️  Warning: Qt plugins not found. Trying to find Qt installation..."
    QT_PLUGIN_PATH=$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || echo "")
fi

if [ -n "$QT_PLUGIN_PATH" ] && [ -d "$QT_PLUGIN_PATH" ]; then
    echo "📦 Qt plugins found at: $QT_PLUGIN_PATH"
    
    # Create plugins directory structure
    mkdir -p "$DEPLOY_DIR/usr/plugins"
    mkdir -p "$DEPLOY_DIR/usr/plugins/platforms"
    mkdir -p "$DEPLOY_DIR/usr/plugins/platformthemes"
    mkdir -p "$DEPLOY_DIR/usr/plugins/imageformats"
    mkdir -p "$DEPLOY_DIR/usr/plugins/iconengines"
    mkdir -p "$DEPLOY_DIR/usr/plugins/tls"
    
    # Copy essential Qt plugins
    if [ -f "$QT_PLUGIN_PATH/platforms/libqxcb.so" ]; then
        cp "$QT_PLUGIN_PATH/platforms/libqxcb.so" "$DEPLOY_DIR/usr/plugins/platforms/"
    fi
    
    if [ -f "$QT_PLUGIN_PATH/platforms/libqwayland-generic.so" ]; then
        cp "$QT_PLUGIN_PATH/platforms/libqwayland-generic.so" "$DEPLOY_DIR/usr/plugins/platforms/"
    fi
    
    # Copy platform themes
    if [ -d "$QT_PLUGIN_PATH/platformthemes" ]; then
        cp -r "$QT_PLUGIN_PATH/platformthemes"/* "$DEPLOY_DIR/usr/plugins/platformthemes/"
    fi
    
    # Copy image formats
    if [ -d "$QT_PLUGIN_PATH/imageformats" ]; then
        cp -r "$QT_PLUGIN_PATH/imageformats"/* "$DEPLOY_DIR/usr/plugins/imageformats/"
    fi
    
    # Copy icon engines
    if [ -d "$QT_PLUGIN_PATH/iconengines" ]; then
        cp -r "$QT_PLUGIN_PATH/iconengines"/* "$DEPLOY_DIR/usr/plugins/iconengines/"
    fi
    
    # Copy TLS plugins
    if [ -d "$QT_PLUGIN_PATH/tls" ]; then
        cp -r "$QT_PLUGIN_PATH/tls"/* "$DEPLOY_DIR/usr/plugins/tls/"
    fi
    
    # Copy wayland plugins if they exist
    if [ -d "$QT_PLUGIN_PATH/wayland-shell-integration" ]; then
        cp -r "$QT_PLUGIN_PATH/wayland-shell-integration" "$DEPLOY_DIR/usr/plugins/"
    fi
    
    if [ -d "$QT_PLUGIN_PATH/wayland-decoration-client" ]; then
        cp -r "$QT_PLUGIN_PATH/wayland-decoration-client" "$DEPLOY_DIR/usr/plugins/"
    fi
    
    echo "✅ Qt plugins copied"
else
    echo "⚠️  Warning: Qt plugins not found. Application may not work properly."
fi

# Copy Qt libraries
echo "📚 Copying Qt libraries..."
mkdir -p "$DEPLOY_DIR/usr/lib"

# Find Qt libraries
QT_LIB_PATH=$(find /usr/lib/qt6 -name "libQt6Core.so*" 2>/dev/null | head -1)
if [ -n "$QT_LIB_PATH" ]; then
    QT_LIB_PATH=$(dirname "$QT_LIB_PATH")
elif [ -z "$QT_LIB_PATH" ]; then
    QT_LIB_PATH=$(find /usr/lib/qt -name "libQt6Core.so*" 2>/dev/null | head -1)
    if [ -n "$QT_LIB_PATH" ]; then
        QT_LIB_PATH=$(dirname "$QT_LIB_PATH")
    fi
fi

if [ -n "$QT_LIB_PATH" ] && [ -d "$QT_LIB_PATH" ]; then
    echo "📦 Qt libraries found at: $QT_LIB_PATH"
    
    # Copy essential Qt libraries
    for lib in libQt6Core libQt6Gui libQt6Widgets libQt6Network libQt6DBus; do
        if [ -f "$QT_LIB_PATH/$lib.so" ]; then
            cp "$QT_LIB_PATH/$lib.so"* "$DEPLOY_DIR/usr/lib/"
        fi
    done
    
    # Copy additional system libraries that might be needed
    for lib in libxcb-cursor libxcb-util libicuuc libicui18n libicudata; do
        if [ -f "/usr/lib/$lib.so" ]; then
            cp "/usr/lib/$lib.so"* "$DEPLOY_DIR/usr/lib/"
        fi
    done
    
    echo "✅ Qt libraries copied"
else
    echo "⚠️  Warning: Qt libraries not found. Trying alternative locations..."
    
    # Try to find Qt libraries in common locations
    for qt_path in "/usr/lib/qt6" "/usr/lib/qt" "/opt/qt6/lib" "/opt/qt/lib"; do
        if [ -d "$qt_path" ]; then
            echo "📦 Found Qt installation at: $qt_path"
            for lib in libQt6Core libQt6Gui libQt6Widgets libQt6Network libQt6DBus; do
                if [ -f "$qt_path/$lib.so" ]; then
                    cp "$qt_path/$lib.so"* "$DEPLOY_DIR/usr/lib/"
                fi
            done
            break
        fi
    done
    
    # Copy additional system libraries that might be needed
    for lib in libxcb-cursor libxcb-util libicuuc libicui18n libicudata; do
        if [ -f "/usr/lib/$lib.so" ]; then
            cp "/usr/lib/$lib.so"* "$DEPLOY_DIR/usr/lib/"
        fi
    done
    
    echo "✅ Qt libraries copied (alternative method)"
fi

# Fix library rpath if patchelf is available
if command -v patchelf >/dev/null 2>&1; then
    echo "🔧 Fixing library rpath..."
    
    # Fix main binary rpath
    if [ -f "$DEPLOY_DIR/nekoray" ]; then
        patchelf --set-rpath '$ORIGIN/usr/lib' "$DEPLOY_DIR/nekoray"
    fi
    
    # Fix Qt plugin rpath
    if [ -f "$DEPLOY_DIR/usr/plugins/platforms/libqxcb.so" ]; then
        patchelf --set-rpath '$ORIGIN/../../lib' "$DEPLOY_DIR/usr/plugins/platforms/libqxcb.so"
    fi
    
    if [ -f "$DEPLOY_DIR/usr/plugins/platforms/libqwayland-generic.so" ]; then
        patchelf --set-rpath '$ORIGIN/../../lib' "$DEPLOY_DIR/usr/plugins/platforms/libqwayland-generic.so"
    fi
    
    # Fix platform theme rpath
    if [ -f "$DEPLOY_DIR/usr/plugins/platformthemes/libqgtk3.so" ]; then
        patchelf --set-rpath '$ORIGIN/../../lib' "$DEPLOY_DIR/usr/plugins/platformthemes/libqgtk3.so"
    fi
    
    if [ -f "$DEPLOY_DIR/usr/plugins/platformthemes/libqxdgdesktopportal.so" ]; then
        patchelf --set-rpath '$ORIGIN/../../lib' "$DEPLOY_DIR/usr/plugins/platformthemes/libqxdgdesktopportal.so"
    fi
    
    echo "✅ Library rpath fixed"
else
    echo "⚠️  Warning: patchelf not found. Library rpath not fixed."
fi

# Make executable
chmod +x "$DEPLOY_DIR"/* 2>/dev/null || true

# Create package
cd "$DEPLOY_DIR"
tar -czf nekoray-linux64.tar.gz \
    nekoray \
    nekobox_core \
    updater \
    geoip.db \
    geosite.db \
    translations/ \
    res/ \
    usr/ 2>/dev/null || echo "⚠️  Warning: Some files may be missing in the package"

echo "🎉 Deployment completed!"
echo "📁 Files in: $DEPLOY_DIR"
echo "📦 Package: $DEPLOY_DIR/nekoray-linux64.tar.gz"
echo "🚀 Run with: cd $DEPLOY_DIR && ./nekoray" 