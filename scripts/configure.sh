#!/bin/zsh
set -euo pipefail
ROOT="/Users/ticket-go/Desktop/ticketgoteknoloji sistem analiz /ticket-go-izleme-qt"
rm -rf "$ROOT/build"
export QT_ADDITIONAL_PACKAGES_PREFIX_PATH="/opt/homebrew/opt/qtdeclarative;/opt/homebrew/opt/qtquick3d;/opt/homebrew/opt/qtwebsockets;/opt/homebrew/opt/qtshadertools;/opt/homebrew/opt/qtsvg;/opt/homebrew/opt/qtquicktimeline"
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qtbase"
cmake --build "$ROOT/build"
ctest --test-dir "$ROOT/build" --output-on-failure
