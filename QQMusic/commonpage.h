#ifndef COMMONPAGE_H
#define COMMONPAGE_H

#include <QWidget>
#include <QMediaPlaylist>
#include "musiclist.h"

namespace Ui {
class CommonPage;
}



enum PageType
{
    LIKE_PAGE,      // 我喜欢页面
    LOCAL_PAGE,     // 本地下载页面
    HISTORY_PAGE    // 最近播放页面
};

class CommonPage : public QWidget
{
    Q_OBJECT

public:
    explicit CommonPage(QWidget *parent = nullptr);
    ~CommonPage();

    void setMusicListType(PageType pageType);
    void setCommonPageUI(const QString& text, const QString& imagePath);
    void addMusicToMusicPage(MusicList& musicList);
    void reFresh(MusicList& musicList);
    void setSearchText(const QString& text);

    // 将歌曲添加到播放列表中
    void addMusicToPlaylist(MusicList& musicList, QMediaPlaylist* playList);

    void setMusicImage(QPixmap pixmap);

    QString getMusicIdByIndex(int index);

signals:
    void updataLikeMusic(bool isLike, const QString& musicId);
    void playAll(PageType);
    void playMusicByIndex(CommonPage*, int);
    void removeMusicRequested(const QString& musicId);

private:
    Ui::CommonPage *ui;

    PageType pageType;   // 保存该页面的类别说明
    QVector<QString> musicOfPage;  // 保存pageType对应页面的歌曲ID
    QString searchText;
};

#endif // COMMONPAGE_H
