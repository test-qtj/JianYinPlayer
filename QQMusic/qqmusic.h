#ifndef QQMUSIC_H
#define QQMUSIC_H

#include <QWidget>
#include "volumetool.h"
#include "musiclist.h"
#include "commonpage.h"
#include "lrcpage.h"
#include <QMediaPlayer>
#include <QMediaPlaylist>
#include <QPropertyAnimation>
#include <QSqlDatabase>
#include <QCloseEvent>
#include <QStringList>
#include <QMap>
#include <QSet>
#include <QProcess>
#include <QTemporaryDir>
#include <QProgressDialog>



QT_BEGIN_NAMESPACE
namespace Ui { class QQMusic; }
QT_END_NAMESPACE
class BtForm;

class QQMusic : public QWidget
{
    Q_OBJECT

public:
    QQMusic(QWidget *parent = nullptr);
    ~QQMusic();

    void initUi();

    void initSqlite();

    void initMusicList();

    void initPlayer();

    QJsonArray randomPiction();

    void connectSignalAndSlots();

    void updateBtformAnimal();

    void onQQMusicQuit();

protected:
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void closeEvent(QCloseEvent *event);
    void resizeEvent(QResizeEvent *event);


private slots:
    void on_quit_clicked();
    void onBtFormClick(int pageId);
    void updateLikeMusicAndPage(bool isLike, const QString& musicId);

    void on_volume_clicked();

    void on_addLocal_clicked();
    void addLocalAudioFiles();
    void chooseNcmFiles();
    void chooseNcmFolder();
    void startNcmConversion(const QStringList &inputPaths);
    void finishNcmConversion(int exitCode, QProcess::ExitStatus exitStatus);

    void onLrcWordClicked();

    // 播放控制区的槽函数
    // 播放按钮：歌曲在播放和暂停之间切换
    void onPlayMusic();

    // 上一曲
    void onPlayUpClicked();

    // 下一曲
    void onPlayDownClicked();

    // 播放模式设置
    void onPlaybackModeClicked();

    void onPlayAll(PageType pageType);   // 播放所有按钮发射信号对应的槽函数
    void playAllMusicOfCommonPage(CommonPage* page, int index, bool skipMissingAtStart = false);
    void playMusicByIndex(CommonPage* page, int index);

    // VolumeTool类中setMusicMuted信号对应的槽函数
    void setPlayerMuted(bool isMuted);

    // VolumeTool类中setMusicVolume信号对应的槽函数
    void setPlayerVolume(int volume);


    // QMediaPlayer中stateChanged信号对应槽函数
    void onPlayerStateChanged();
    void onPlaybackModeChanged(QMediaPlaylist::PlaybackMode playbackMode);
    void onCurrentIndexChanged(int index);   // playlist中播放源发生改变，index表示正在播放的媒体索引

    void onDurationChanged(qint64 duration);  // 媒体切换，播放时间也会改变
    void onPositionChanged(qint64 position);  // 播放进度发生改变
    void onMusicSliderChanged(float ration);  // 进度条改变需要播放时间跟着修改
    void onMetaDataAvailableChanged(bool available);  // 元数据发生改变时
    void on_skin_clicked();

    void on_min_clicked();
    void removeMusicFromLibrary(const QString& musicId);

private:
    Ui::QQMusic *ui;

    QPoint  dragPos;

    QSqlDatabase sqlite;
    MusicList musicList;   // 程序用来组织歌曲文件

    VolumeTool* volumeTool;

    LrcPage* lrcPage;
    QPropertyAnimation* lrcPageAnimal;

    QMediaPlayer* player;      // 专门用来播放控制
    QMediaPlaylist* playList;   // 专门用来管理播放源，包含一些播放模式设置等

    CommonPage* currentPage;  // 记录当前正在播放的commonPage页面
    int currentIndex;         // 记录当前正在播放歌曲在媒体列表中的索引
    QStringList currentPlaylistIds;

    qint64 totalTime;         // 记录媒体源的总时间
    bool isDrag;
    QSet<QString> playbackFallbackAttempted;
    QMap<QString, QString> playbackFallbackPaths;
    QProcess *ncmProcess;
    QTemporaryDir ncmTempDir;
    QProgressDialog *ncmProgress;
    QString ncmOutputDir;
    bool ncmConversionCancelled;
    void refreshCurrentCover(bool preferEmbeddedCover);
};
#endif // QQMUSIC_H
