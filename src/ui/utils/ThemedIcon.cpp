#include "include/ui/utils/ThemedIcon.hpp"

#include "include/ui/setting/ThemeManager.hpp"

#include <QGuiApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QPixmapCache>

namespace {
    class ThemedIconEngine final : public QIconEngine {
    public:
        ThemedIconEngine(const QString &name, int hue) : name(name), hue(hue) {
            const QString base = QStringLiteral(":/icon/lucide/") + name;
            mask.addFile(base + QStringLiteral(".png"));
            mask.addFile(base + QStringLiteral("@2x.png"));
            mask.addFile(base + QStringLiteral("@3x.png"));
        }

        void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override {
            const qreal scale = painter->device() != nullptr ? painter->device()->devicePixelRatio() : qApp->devicePixelRatio();
            painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, scale));
        }

        QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
            return scaledPixmap(size, mode, state, 1.0);
        }

        QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State, qreal scale) override {
            const QColor color = tint(mode, hue);
            const QString key = QStringLiteral("themed-icon/%1/%2x%3/%4/%5")
                                    .arg(name)
                                    .arg(size.width())
                                    .arg(size.height())
                                    .arg(scale)
                                    .arg(color.rgba());
            QPixmap tinted;
            if (QPixmapCache::find(key, &tinted)) return tinted;

            const QPixmap source = mask.pixmap(size, scale);
            if (source.isNull()) return source;
            QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
            const qreal ratio = image.devicePixelRatio();
            image.setDevicePixelRatio(1);
            QPainter painter(&image);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(image.rect(), color);
            painter.end();
            image.setDevicePixelRatio(ratio);
            tinted = QPixmap::fromImage(image);
            QPixmapCache::insert(key, tinted);
            return tinted;
        }

        QString key() const override {
            return QStringLiteral("ThemedIconEngine");
        }

        QIconEngine *clone() const override {
            return new ThemedIconEngine(*this);
        }

        bool isNull() override {
            return mask.isNull();
        }

        QList<QSize> availableSizes(QIcon::Mode mode, QIcon::State state) override {
            return mask.availableSizes(mode, state);
        }

    private:
        static QColor tint(QIcon::Mode mode, int hue) {
            const QPalette palette = QGuiApplication::palette();
            switch (mode) {
                case QIcon::Disabled: return palette.color(QPalette::Disabled, QPalette::ButtonText);
                case QIcon::Selected: return palette.color(QPalette::Active, QPalette::HighlightedText);
                default: break;
            }
            if (hue < 0) return palette.color(QPalette::Active, QPalette::ButtonText);
            return MutedTint(hue, palette.color(QPalette::Active, QPalette::Window));
        }

        QString name;
        int hue;
        QIcon mask;
    };
} // namespace

QIcon ThemedIcon(const QString &name, int hue) {
    return QIcon(new ThemedIconEngine(name, hue));
}
