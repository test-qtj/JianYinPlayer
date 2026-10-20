#ifndef MUSIC_H
#define MUSIC_H


#include<QUrl>

class Music
{
public:
    Music();
    Music(QUrl url);
    void setMusicId(const QString& musicId);
    void setMusicName(const QString& musicName);
    void setMusicSinger(const QString& musicSinger);
    void setMusicAlbum(const QString& musicAlbumn);
    void setMusicDuration(qint64 duration);
    void setIsLike(bool isLike);
    void setIsHistory(bool isHistory);
    void setMusicUrl(QUrl musicUrl);

    QString getMusicName()const;
    QString getMusicSinger()const;
    QString getMusicAlbum()const;
    qint64 getMusicDuration()const;
    bool getIsLike()const;
    bool getIsHistory()const;
    QUrl getMusicUrl()const;
    QString getMusicId()const;

    QString getLrcFilePath()const;

    // 将歌曲属性写入到数据库
    void InsertMusicToDB();

private:
    void parseMediaMetaMusic();

private:
    // 为了保证歌曲的唯一性，需要给音乐添加id
    QString musicId;
    // 音乐名称
    QString musicName;
    // 音乐作者
    QString musicSinger;
    // 音乐专辑
    QString musicAlbumn;
    // 持续时长--long long   04:30.56--->(4*60+30)*1000+56毫秒=270*1000+56毫秒=270056毫秒
    qint64 duration;
    // 是否收藏(是否喜欢)
    bool isLike;

    // 是否为历史播放
    bool isHistory;

    // 音乐QUrl
    QUrl musicUrl;
};

#endif // MUSIC_H
