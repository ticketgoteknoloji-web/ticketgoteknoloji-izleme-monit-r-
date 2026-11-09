#!/bin/zsh
set -euo pipefail
ROOT="/Users/ticket-go/Desktop/ticketgoteknoloji sistem analiz /ticket-go-izleme-qt"
export QML_IMPORT_PATH="/opt/homebrew/opt/qtdeclarative/share/qt/qml:/opt/homebrew/opt/qtquick3d/share/qt/qml"
export QT_PLUGIN_PATH="/opt/homebrew/opt/qtbase/share/qt/plugins"
exec "$ROOT/build/TicketGoIzleme.app/Contents/MacOS/TicketGoIzleme" "$@"
