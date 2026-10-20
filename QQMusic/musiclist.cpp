#include "musiclist.h"
#include <QMimeDatabase>
#include <QFileInfo>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

MusicList::MusicList()
{

}

void MusicList::addMusicsByUrl(const QList<QUrl> &musicUrls)
{
    // 将所有的音乐放置到musicList
    for(auto e : musicUrls)
    {
        // 文件重复过滤
        // 如果该文件已经在musicList中，就不在添加
        // 过滤方式：将当前将要添加到musicList中的文件的路径，与musicList中已经存在的
        //          歌曲文件路劲对比，如果相同则歌曲已经存在，如果不同说明还没有添加过，则添加
        // 下面查找的算法：时间复杂度O(n)
        #if 0
        auto it = begin();
        for(; it != end(); ++it)
        {
            if(it->getMusicUrl() == e)
            {
                break;
            }
        }

        if(it != end())
        {
            continue;
        }

        #endif

        // 查找：最快的方式就是哈希  O(1)
        QString musicPath = e.toLocalFile();
        if(musicPaths.contains(musicPath))
            continue;

        // 歌曲不存在
        musicPaths.insert(musicPath);

        // 如果musicUrl是一个有效的歌曲文件，再将其添加到歌曲列表中
        // 检测歌曲文件的MIME类型
        const QString suffix = QFileInfo(musicPath).suffix().toLower();
        const QStringList supportedExtensions = QStringList() << "mp3" << "flac" << "wav" << "m4a" << "aac" << "ogg" << "wma";

        // mime 和 audio/mepg(表示mp3文件)   audio/flac(:无损音乐)  audio/wav
        if(supportedExtensions.contains(suffix))
        {
            // 需要将url创建一个Music对象，添加到musicList中
            Music music(e);
            musicList.push_back(music);
        }
        else
        {
            musicPaths.remove(musicPath);
        }
    }
}

iterator MusicList::findMusicByMusicId(const QString &musicId)
{
    for(auto it = begin(); it != end(); ++it)
    {
        if(it->getMusicId() == musicId)
            return it;
    }

    return end();
}

bool MusicList::removeMusicById(const QString &musicId)
{
    auto it = findMusicByMusicId(musicId);
    if (it == end()) return false;

    QSqlQuery query;
    query.prepare("DELETE FROM MusicInfo WHERE musicId = ?");
    query.addBindValue(musicId);
    if (!query.exec()) {
        qDebug() << "删除歌曲记录失败:" << query.lastError().text();
        return false;
    }

    musicPaths.remove(it->getMusicUrl().toLocalFile());
    musicList.erase(it);
    return true;
}

iterator MusicList::begin()
{
    return musicList.begin();
}

iterator MusicList::end()
{
    return musicList.end();
}

void MusicList::readFromDB()
{
    musicList.clear();
    musicPaths.clear();
    QSqlQuery query;
    query.prepare("SELECT musicId, musicName, musicSinger, albumName, musicUrl,\
                          duration, isLike, isHistory FROM MusicInfo");
    if(!query.exec())
    {
        qDebug()<<"数据库查询失败:"<<query.lastError().text();
        return;
    }

    while(query.next())
    {
        const QString audioPath = query.value(4).toString();
        // Re-derive artist/title from the original "artist - title" filename.
        // Earlier versions stored these two fields in reverse order.
        Music music(QUrl::fromLocalFile(audioPath));
        music.setMusicId(query.value(0).toString());
        if (QFileInfo(audioPath).completeBaseName().lastIndexOf(QStringLiteral(" - ")) <= 0) {
            music.setMusicName(query.value(1).toString());
            music.setMusicSinger(query.value(2).toString());
        }
        music.setMusicAlbum(query.value(3).toString());
        music.setMusicDuration(query.value(5).toLongLong());
        music.setIsLike(query.value(6).toBool());
        music.setIsHistory(query.value(7).toBool());
        musicList.push_back(music);

        // 恢复musicPaths，目的防止相同目录下的歌曲被重复加载
        musicPaths.insert(music.getMusicUrl().toLocalFile());
    }
}

void MusicList::writeToDB()
{
    for(auto music : musicList)
    {
        // 拿到每个musci对象之后，将music的所有属性写入到数据库
        // 具体写入单个Music对象的操作也将其封装成一个函数
        music.InsertMusicToDB();
    }
}


