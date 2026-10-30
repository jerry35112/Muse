#pragma once
#include <QString>
namespace muse {
bool backgroundApplication(const QString& path);
QString applicationCategory(const QString& path,const QString& name);
QString nodeRegion(const QString& name);
}
