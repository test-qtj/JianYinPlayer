#include "music.h"
#include <QUuid>
#include <QMediaPlayer>
#include <QDebug>
#include <QCoreApplication>
#include <QFileInfo>

#include <QSqlQuery>
#include <QSqlError>

Music::Music()
    : duration(0)
    , isLike(false)
    , isHistory(false)
{

}

Music::Music(QUrl url)
    : duration(0)
    , isLike(false)
    , isHistory(false)
    , musicUrl(url)
{
    // 读取url对应的歌曲文件的信息，解析出元数据
    // 歌曲名称、歌曲作者、歌曲专辑、歌曲持续时长
    musicId = QUuid::createUuid().toString();
    parseMediaMetaMusic();
}

void Music::setMusicId(const QString &musicId)
{
    this->musicId = musicId;
}

void Music::setMusicName(const QString& musicName)
{
    this->musicName = musicName;
}

void Music::setMusicSinger(const QString& musicSinger)
{
    this->musicSinger = musicSinger;
}

void Music::setMusicAlbum(const QString& musicAlbumn)
{
    this->musicAlbumn = musicAlbumn;
}

void Music::setMusicDuration(qint64 duration)
{
    this->duration = duration;
}
void Music::setIsLike(bool isLike)
{
    this->isLike = isLike;
}

void Music::setIsHistory(bool isHistory)
{
    this->isHistory = isHistory;
}

void Music::setMusicUrl(QUrl musicUrl)
{
    this->musicUrl = musicUrl;
}

QString Music::getMusicName()const
{
    return musicName;
}

QString Music::getMusicSinger()const
{
    return musicSinger;
}

QString Music::getMusicAlbum()const
{
    return musicAlbumn;
}

qint64 Music::getMusicDuration()const
{
    return duration;
}

bool Music::getIsLike()const
{
    return isLike;
}

bool Music::getIsHistory()const
{
    return isHistory;
}

QUrl Music::getMusicUrl()const
{
    return musicUrl;
}

QString Music::getMusicId() const
{
    return musicId;
}

QString Music::getLrcFilePath() const
{
    const QFileInfo audioFile(musicUrl.toLocalFile());
    return audioFile.path() + QLatin1Char('/') + audioFile.completeBaseName()
            + QStringLiteral(".lrc");
}

void Music::InsertMusicToDB()
{
    // 1. 检测music是否在数据库中存在
    QSqlQuery query;
    // query.prepare("SELECT * FROM MusicInfo WHERE musicId = ?");   // 效率低，因为要返回复合条件记录的所有列

    // 现在只需要检测musicId的歌曲是否存在
    // query.prepare("SELECT id FROM MusicInfo WHERE musicId = ?");    // 原表中有9列，现在只需要返回1列：id


    // 在MusicInfo表中查询MusicId等于指定值的歌曲，如果歌曲存在，返回记录中只包含一个1
    // 假设musicId的歌曲在MusicInfo中存在3个
    // SELECT 1 FROM MusicInfo WHERE musicId = ? 返回结果：
    // 1
    // 1
    // 1
    // query.prepare("SELECT 1 FROM MusicInfo WHERE musicId = ?");

    // 当SELECT 1...查询到结果后，我们需要知道是否存在
    // SELECT EXISTS(子查询) ： 子查询中如果有记录，SELECT EXISTS返回TRUE
    //                       如果子查询中没有满足条件的记录， SELECT EXISTS返回FALSE

    query.prepare("SELECT EXISTS (SELECT 1 FROM MusicInfo WHERE musicId = ?)");
    query.addBindValue(musicId);
    if(!query.exec())
    {
        qDebug()<<"查询失败: "<<query.lastError().text();
        return;
    }

    if(query.next())
    {
        bool isExists = query.value(0).toBool();
        if(isExists)
        {
            // musicId的歌曲已经存在
            // 2. Refresh the stored metadata too, including corrected artist/title
            // values reconstructed from "artist - title" filenames.
            query.prepare("UPDATE MusicInfo SET musicName = ?, musicSinger = ?, albumName = ?, musicUrl = ?,\
                          duration = ?, isLike = ?, isHistory = ? WHERE musicId = ?");
            query.addBindValue(musicName);
            query.addBindValue(musicSinger);
            query.addBindValue(musicAlbumn);
            query.addBindValue(musicUrl.toLocalFile());
            query.addBindValue(duration);
            query.addBindValue(isLike? 1 : 0);
            query.addBindValue(isHistory? 1 : 0);
            query.addBindValue(musicId);
            if(!query.exec())
            {
                qDebug()<<"更新失败: "<<query.lastError().text();
            }

            qDebug()<<"更新music信息: "<<musicName<<" "<<musicId;
        }
        else
        {
            // 3. 不存在：直接将music的属性信息插入数据库
            query.prepare("INSERT INTO MusicInfo(musicId, musicName, musicSinger, albumName, musicUrl,\
                          duration, isLike, isHistory)\
                    VALUES(?,?,?,?,?,?,?,?)");
            query.addBindValue(musicId);
            query.addBindValue(musicName);
            query.addBindValue(musicSinger);
            query.addBindValue(musicAlbumn);
            query.addBindValue(musicUrl.toLocalFile());
            query.addBindValue(duration);
            query.addBindValue(isLike ? 1 : 0);
            query.addBindValue(isHistory? 1 : 0);

            if(!query.exec())
            {
                qDebug()<<"插入失败: "<<query.lastError().text();
                return;
            }

            qDebug()<<"插入music信息: "<<musicName<<" "<<musicId;
        }
    }

}

void Music::parseMediaMetaMusic()
{
    // Metadata loading through QMediaPlayer is asynchronous. A busy wait here
    // can freeze the whole app for unsupported or damaged files, so use the
    // filename as a safe initial fallback and let playback provide duration.
    const QString baseName = QFileInfo(musicUrl.toLocalFile()).completeBaseName().trimmed();
    const int separator = baseName.lastIndexOf(QStringLiteral(" - "));
    if (separator > 0) {
        // Netease downloads are named "artist - title".
        musicSinger = baseName.left(separator).trimmed();
        musicName = baseName.mid(separator + 3).trimmed();
    } else {
        musicName = baseName;
        musicSinger = QStringLiteral("未知歌手");
    }
    if (musicName.isEmpty()) musicName = QStringLiteral("未知歌曲");
    if (musicSinger.isEmpty()) musicSinger = QStringLiteral("未知歌手");
    musicAlbumn = QStringLiteral("未知专辑");
    duration = 0;
}





