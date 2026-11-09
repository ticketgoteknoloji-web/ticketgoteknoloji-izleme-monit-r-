import QtQuick

Image {
    property int markSize: 32
    width: markSize
    height: markSize
    fillMode: Image.PreserveAspectFit
    mipmap: true
    asynchronous: true
    sourceSize.width: markSize * 3
    sourceSize.height: markSize * 3
}
