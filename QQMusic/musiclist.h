#ifndef MUSICLIST_H
#define MUSICLIST_H

#include <QVector>
#include <QList>
#include <QUrl>
#include <QSet>


#include "music.h"

typedef QVector<Music>::iterator iterator;

// 管理所有的歌曲对应Music对象
class MusicList
{
public:
    MusicList();

    void addMusicsByUrl(const QList<QUrl>& musicUrls);

    iterator findMusicByMusicId(const QString& musicId);
    bool removeMusicById(const QString& musicId);

    iterator begin();
    iterator end();

    void readFromDB();
    void writeToDB();
private:
    QVector<Music> musicList;

    QSet<QString> musicPaths;    // 为了防止歌曲文件重复加载
};

#endif // MUSICLIST_H
