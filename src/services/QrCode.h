#pragma once

#include <QImage>
#include <QString>

// QR codes rendered locally with libqrencode: the Pix key never leaves the computer.
namespace QrCode {

// Black-on-white QR code with `margin` quiet modules; null image on failure.
QImage render(const QString &text, int moduleSize = 8, int margin = 2);
// "data:image/png;base64,…" for QML, empty on failure.
QString dataUrl(const QString &text);

} // namespace QrCode
