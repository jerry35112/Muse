#pragma once
#include <QVariantList>
#include <QStringList>
#include <atomic>
namespace muse {
QString shortcutExecutable(const QString& shortcut);
QVariantList scanApplicationLibrary(const QStringList& roots,const QStringList& registered,const QStringList& running,const std::atomic_bool* cancel=nullptr,bool* complete=nullptr);
QVariantList installedApplicationLibrary(const std::atomic_bool* cancel=nullptr,bool* complete=nullptr);
}
