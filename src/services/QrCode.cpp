#include "QrCode.h"

#include <QBuffer>
#include <QPainter>

#include <qrencode.h>

#include <memory>

namespace QrCode {

QImage render(const QString &text, int moduleSize, int margin)
{
    if (text.isEmpty() || moduleSize <= 0 || margin < 0) {
        return {};
    }
    const QByteArray data = text.toUtf8();
    std::unique_ptr<QRcode, decltype(&QRcode_free)> code(
        QRcode_encodeData(static_cast<int>(data.size()), reinterpret_cast<const unsigned char *>(data.constData()), 0, QR_ECLEVEL_M),
        &QRcode_free);
    if (!code) {
        return {};
    }
    const int size = code->width;
    const int pixels = (size + 2 * margin) * moduleSize;
    QImage image(pixels, pixels, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (code->data[y * size + x] & 0x01) { // bit 0: dark module
                painter.drawRect((x + margin) * moduleSize, (y + margin) * moduleSize, moduleSize, moduleSize);
            }
        }
    }
    return image;
}

QString dataUrl(const QString &text)
{
    const QImage image = render(text);
    if (image.isNull()) {
        return {};
    }
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
}

} // namespace QrCode
