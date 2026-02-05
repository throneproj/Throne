#!/bin/bash
# Локальная сборка Throne под Linux (amd64/arm64) с системным Qt6
set -e

SRC_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SRC_ROOT"

ARCH=$(uname -m)
[ "$ARCH" = "x86_64" ] && GOARCH=amd64 || GOARCH=arm64
export GOOS=linux
export GOARCH
export INPUT_VERSION="${INPUT_VERSION:-0.1}"

echo "=== Throne local build (linux/$GOARCH) ==="

# 1. Зависимости (запустить с sudo при необходимости)
# Go 1.25+ в Ubuntu нет — ставится отдельно: https://go.dev/dl/ или через go install/toochain
check_deps() {
    local missing=""
    command -v cmake >/dev/null || missing="cmake"
    command -v go    >/dev/null || missing="$missing go"
    command -v ninja >/dev/null || missing="$missing ninja"
    command -v protoc >/dev/null || missing="$missing protobuf-compiler"
    if ! pkg-config --exists Qt6Widgets 2>/dev/null; then
        missing="$missing Qt6 (qt6-base-dev qt6-tools-dev)"
    fi
    if [ -n "$missing" ]; then
        echo "Установите зависимости, например:"
        echo "  sudo apt update"
        echo "  sudo apt install -y build-essential cmake ninja-build protobuf-compiler \\"
        echo "    qt6-base-dev qt6-tools-dev"
        echo "  Go 1.25+ — не из apt: скачайте с https://go.dev/dl/ и добавьте bin/ в PATH (или export GO_BIN=/path/to/go/bin)."
        echo "Не хватает: $missing"
        return 1
    fi
    return 0
}

if ! check_deps; then
    exit 1
fi

# PATH: protoc-gen-go, protoc-gen-protorpc (go install → GOPATH/bin); кастомный Go (1.25+)
# Для Go 1.25: export GO_BIN=/path/to/go/bin перед запуском (в Ubuntu только 1.22 в пакетах)
[ -n "$GO_BIN" ] && export PATH="$GO_BIN:$PATH"
export PATH="$(go env GOPATH 2>/dev/null)/bin:$PATH"

# Проверка версии Go: Core требует 1.25+
need_go_minor=25
go_minor=$(go version 2>/dev/null | sed -n 's/.*go1\.\([0-9]*\).*/\1/p')
if [ -n "$go_minor" ] && [ "$go_minor" -lt "$need_go_minor" ]; then
    echo "Требуется Go 1.25+, в PATH сейчас: $(go version 2>/dev/null || echo 'go не найден')."
    echo "Установите Go 1.25+ с https://go.dev/dl/ и добавьте каталог bin в PATH,"
    echo "или укажите: export GO_BIN=/путь/к/go/bin перед запуском скрипта."
    exit 1
fi

if ! command -v protoc-gen-go >/dev/null 2>&1; then
    echo "protoc-gen-go не найден. Установите: go install google.golang.org/protobuf/cmd/protoc-gen-go@latest"
    echo "  (и protoc-gen-protorpc: cd core/protorpc && go install ./protoc-gen-protorpc)"
    exit 1
fi

# 2. Go Core
echo "--- Building Core (Go) ---"
./script/build_go.sh

# 3. srslist.h для маршрутов
if [ ! -f build/srslist.h ]; then
    mkdir -p build
    curl -fLso build/srslist.h "https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/srslist.h" || true
fi

# 4. CMake + Ninja
echo "--- Configuring and building Qt app ---"
mkdir -p build
cd build
# Системный Qt6: обычно pkg-config или стандартный путь
if [ -z "$CMAKE_PREFIX_PATH" ]; then
    QT_DIR=$(pkg-config --variable prefix Qt6Widgets 2>/dev/null) || true
    [ -n "$QT_DIR" ] && export CMAKE_PREFIX_PATH="$QT_DIR"
fi
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
ninja
cd ..

# 5. Собрать запускаемую папку: build/Throne + build/Core
DEPLOY="$SRC_ROOT/deployment/linux-$GOARCH"
CORE_DST="$SRC_ROOT/build/Core"

if [ -f "$DEPLOY/Core" ]; then
    if [ -f "$CORE_DST" ]; then
        # Проверка прав: можем ли перезаписать
        if [ ! -w "$CORE_DST" ]; then
            echo "Ошибка: build/Core существует, но нет прав на запись."
            echo "  Текущий владелец: $(stat -c '%U:%G' "$CORE_DST" 2>/dev/null || stat -f '%Su:%Sg' "$CORE_DST" 2>/dev/null)"
            echo "  Смените владельца: sudo chown \$(whoami): \"$CORE_DST\""
            exit 1
        fi
        # Бекап вместо удаления
        BAK="$CORE_DST.bak.$(date +%Y%m%d_%H%M%S)"
        if ! mv "$CORE_DST" "$BAK" 2>/dev/null; then
            echo "Ошибка: не удалось сделать бекап build/Core (закройте Throne?)."
            echo "  Смените владельца: sudo chown \$(whoami): \"$CORE_DST\""
            exit 1
        fi
        echo "  Бекап: $BAK"
    fi
    if ! cp -f "$DEPLOY/Core" "$CORE_DST"; then
        echo "Ошибка: не удалось скопировать Core в build/."
        echo "  Проверьте права на build/: sudo chown -R \$(whoami): \"$SRC_ROOT/build\""
        exit 1
    fi
    chmod +x "$CORE_DST"
fi
echo "--- Готово ---"
echo "Запуск: cd build && ./Throne"
echo ""
if [ ! -f "build/Core" ]; then
    echo "Core (Go) не собран — запуск профилей не будет работать."
    echo "Нужен Go 1.25+: установите и выполните:"
    echo "  export PATH=\"\$PATH:\$(go env GOPATH)/bin\""
    echo "  INPUT_VERSION=0.1 GOOS=linux GOARCH=$GOARCH ./script/build_go.sh"
    echo "  cp deployment/linux-$GOARCH/Core build/"
    echo "Или скопируйте бинарник Core из релиза Throne (ZIP) в build/"
fi
