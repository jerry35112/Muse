#pragma once
#include <QQuickImageProvider>
#include <QCache>
#include <QMutex>
namespace muse {
class AppIcons : public QQuickImageProvider {
    QCache<QString,QImage> cache_{512};
    QMutex mutex_;
public:
    AppIcons():QQuickImageProvider(QQuickImageProvider::Image,ForceAsynchronousImageLoading){}
    QImage requestImage(const QString& id,QSize* size,const QSize& requested)override;
};
QString applicationName(const QString& path);
}
