#include "platform.hpp"
#include "app_icons.hpp"

#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QPainter>

#include <commctrl.h>
#include <commoncontrols.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace muse {
namespace {

QImage takeIcon(HICON icon) {
    if (!icon) return {};
    auto image = QImage::fromHICON(icon);
    DestroyIcon(icon);
    return image;
}

// Reads the icon group straight out of the executable's resources at a usable
// size. The shell equivalent (SHGetFileInfo) hands back the generic document icon
// while its icon cache is cold or busy, which looked like a blank card.
QImage iconFromResources(const std::wstring& path) {
    for (UINT size : {256u, 48u, 32u}) {
        HICON icon = nullptr;
        if (PrivateExtractIconsW(path.c_str(), 0, static_cast<int>(size), static_cast<int>(size), &icon, nullptr, 1,
                                 LR_DEFAULTCOLOR) == 1 &&
            icon)
            return takeIcon(icon);
    }
    return {};
}

// Modern .ico files often carry only a 256 px entry, so the legacy 32 px shell
// call returns a blank or badly letterboxed icon. 48 px from the extra-large list
// is plenty for the 56 px card slot and much cheaper than asking for jumbo.
QImage shellIcon(const std::wstring& path) {
    if (auto resources = iconFromResources(path); !resources.isNull()) return resources;
    SHFILEINFOW info{};
    if (SHGetFileInfoW(path.c_str(), FILE_ATTRIBUTE_NORMAL, &info, sizeof(info), SHGFI_SYSICONINDEX)) {
        for (int size : {SHIL_EXTRALARGE, SHIL_LARGE}) {
            IImageList* list = nullptr;
            if (FAILED(SHGetImageList(size, IID_IImageList, reinterpret_cast<void**>(&list))) || !list) continue;
            HICON icon = nullptr;
            const auto hr = list->GetIcon(info.iIcon, ILD_TRANSPARENT, &icon);
            list->Release();
            if (SUCCEEDED(hr) && icon) {
                auto image = takeIcon(icon);
                if (!image.isNull()) return image;
            }
        }
    }
    SHFILEINFOW legacy{};
    if (SHGetFileInfoW(path.c_str(), FILE_ATTRIBUTE_NORMAL, &legacy, sizeof(legacy), SHGFI_ICON | SHGFI_LARGEICON))
        return takeIcon(legacy.hIcon);
    return {};
}

// A fully transparent - or pure white - extraction is invisible on the white card,
// so it is treated as "no icon" and the drawn placeholder is used instead.
bool looksBlank(const QImage& image) {
    if (image.isNull() || image.width() < 4 || image.height() < 4) return true;
    const auto sample = image.scaled(16, 16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < sample.height(); ++y) {
        for (int x = 0; x < sample.width(); ++x) {
            const auto pixel = sample.pixel(x, y);
            if (qAlpha(pixel) <= 8) continue;
            if (qRed(pixel) < 240 || qGreen(pixel) < 240 || qBlue(pixel) < 240) return false;
        }
    }
    return true;
}

QImage placeholderIcon() {
    QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor("#e1eee7"));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(6, 6, 52, 52, 12, 12);
    p.setPen(QPen(QColor("#699782"), 3));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(19, 19, 26, 26, 4, 4);
    p.drawLine(19, 27, 45, 27);
    return image;
}

// Large image-list entries are often a small icon centred in a big transparent
// canvas. Cropping to the visible pixels keeps the card icon readable instead of
// shrinking it to a stamp.
QImage trimmedIcon(QImage image) {
    if (image.isNull()) return image;
    image = image.convertToFormat(QImage::Format_ARGB32);
    int left = image.width(), top = image.height(), right = -1, bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) <= 8) continue;
            if (x < left) left = x;
            if (x > right) right = x;
            if (y < top) top = y;
            bottom = y;
        }
    }
    if (right < left || bottom < top) return image;
    QRect content(left, top, right - left + 1, bottom - top + 1);
    if (content.size() == image.size()) return image;
    // Keep the crop square so aspect-preserving scaling does not distort it.
    const int side = std::max(content.width(), content.height());
    QRect square(0, 0, side, side);
    square.moveCenter(content.center());
    square = square.intersected(QRect(0, 0, image.width(), image.height()));
    return image.copy(square);
}

} // namespace

QImage AppIcons::requestImage(const QString& id, QSize* size, const QSize& requested) {
    QImage image;
    {
        QMutexLocker lock(&mutex_);
        if (auto* cached = cache_.object(id)) image = *cached;
    }
    if (image.isNull()) {
        // Shell icon extraction reads the executable's resources without starting
        // it. It can block for unusual installers, so it never runs under the
        // cache lock: one slow icon must not stall every other card.
        const auto path = QString::fromUtf8(QByteArray::fromHex(id.toLatin1()));
        const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const auto started = std::chrono::steady_clock::now();
        const auto raw = shellIcon(path.toStdWString());
        const auto cropped = trimmedIcon(raw);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - started).count();
        // Both S_OK and S_FALSE increment COM's per-thread initialization count.
        if (SUCCEEDED(com)) CoUninitialize();
        const bool blank = looksBlank(cropped);
        image = blank ? placeholderIcon() : cropped;
        if (GetEnvironmentVariableW(L"MUSE_TRACE", nullptr, 0) > 0) {
            const auto name = QFileInfo(path).fileName().toStdString();
            const auto message = "icon " + name + " " + std::to_string(elapsed) + "ms raw=" +
                                 std::to_string(raw.width()) + "x" + std::to_string(raw.height()) + " used=" +
                                 std::to_string(image.width()) + "x" + std::to_string(image.height()) + " " +
                                 (blank ? "blank" : "ok");
            trace(message.c_str());
            const auto dumpDir = QString::fromStdWString((exeDir() / L"muse-icon-dump").wstring());
            QDir().mkpath(dumpDir);
            image.save(dumpDir + "/" + QFileInfo(path).fileName() + ".png");
        }
        QMutexLocker lock(&mutex_);
        cache_.insert(id, new QImage(image));
    }
    // Return the decoded image at its native size and report that same size: Qt
    // scales it to sourceSize itself, so the texture never disagrees with the
    // reported dimensions (which left some cards blank on some runs).
    Q_UNUSED(requested);
    if (size) *size = image.size();
    return image;
}

QString applicationName(const QString& path) {
    const auto fallback = QFileInfo(path).completeBaseName();
    const auto widePath = path.toStdWString();
    DWORD unused = 0;
    const DWORD length = GetFileVersionInfoSizeW(widePath.c_str(), &unused);
    if (length == 0 || length > 4 * 1024 * 1024) return fallback;
    std::vector<BYTE> data(length);
    if (!GetFileVersionInfoW(widePath.c_str(), 0, length, data.data())) return fallback;
    struct Translation { WORD language, codepage; };
    Translation* table = nullptr;
    UINT bytes = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&table), &bytes) ||
        bytes < sizeof(Translation))
        return fallback;
    wchar_t query[128];
    swprintf_s(query, L"\\StringFileInfo\\%04x%04x\\FileDescription", table[0].language, table[0].codepage);
    wchar_t* description = nullptr;
    UINT chars = 0;
    if (VerQueryValueW(data.data(), query, reinterpret_cast<void**>(&description), &chars) && chars > 1) {
        const auto name = QString::fromWCharArray(description).trimmed();
        if (!name.isEmpty()) return name;
    }
    return fallback;
}
}
