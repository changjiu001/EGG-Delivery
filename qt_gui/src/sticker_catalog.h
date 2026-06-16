#ifndef STICKER_CATALOG_H
#define STICKER_CATALOG_H

#include <QList>
#include <QString>

struct StickerMeta {
    QString id;
    QString resourcePath;
};

inline QList<StickerMeta> allStickers()
{
    return {
        {QStringLiteral("sticker_01"), QStringLiteral(":/assets/stickers/sticker_01.xpm")},
        {QStringLiteral("sticker_02"), QStringLiteral(":/assets/stickers/sticker_02.xpm")},
        {QStringLiteral("sticker_03"), QStringLiteral(":/assets/stickers/sticker_03.xpm")},
    };
}

inline QString stickerPathById(const QString &id)
{
    const auto stickers = allStickers();
    for (const auto &sticker : stickers) {
        if (sticker.id == id) {
            return sticker.resourcePath;
        }
    }
    return QString();
}

#endif // STICKER_CATALOG_H
