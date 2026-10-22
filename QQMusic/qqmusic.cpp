#include "qqmusic.h"
#include "ui_qqmusic.h"

#include <QMouseEvent>
#include <QDebug>
#include <QApplication>

#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QFont>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QResizeEvent>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QTimer>
#include <QImage>
#include <QDirIterator>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

namespace {

quint32 readSynchsafeInteger(const QByteArray &data, int offset)
{
    if (offset < 0 || offset + 4 > data.size()) return 0;
    return (static_cast<quint32>(static_cast<quint8>(data.at(offset)) & 0x7f) << 21)
            | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 1)) & 0x7f) << 14)
            | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 2)) & 0x7f) << 7)
            | static_cast<quint32>(static_cast<quint8>(data.at(offset + 3)) & 0x7f);
}

QPixmap readMp3EmbeddedCover(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QPixmap();

    const QByteArray header = file.read(10);
    if (header.size() != 10 || !header.startsWith("ID3")) return QPixmap();
    const int version = static_cast<quint8>(header.at(3));
    if (version < 2 || version > 4) return QPixmap();

    const quint32 tagSize = readSynchsafeInteger(header, 6);
    if (tagSize == 0 || tagSize > 64 * 1024 * 1024
            || static_cast<qint64>(tagSize) > file.size() - 10)
        return QPixmap();

    const QByteArray tag = file.read(tagSize);
    if (tag.size() != static_cast<int>(tagSize)) return QPixmap();

    int position = 0;
    const quint8 tagFlags = static_cast<quint8>(header.at(5));
    if (tagFlags & 0x40) { // Extended header
        if (version == 2 || tag.size() < 4) return QPixmap();
        const quint32 extendedSize = version == 4
                ? readSynchsafeInteger(tag, 0)
                : (static_cast<quint32>(static_cast<quint8>(tag.at(0))) << 24)
                  | (static_cast<quint32>(static_cast<quint8>(tag.at(1))) << 16)
                  | (static_cast<quint32>(static_cast<quint8>(tag.at(2))) << 8)
                  | static_cast<quint32>(static_cast<quint8>(tag.at(3)));
        position = static_cast<int>(extendedSize) + (version == 3 ? 4 : 0);
        if (position < 0 || position > tag.size()) return QPixmap();
    }

    const int frameHeaderSize = version == 2 ? 6 : 10;
    while (position + frameHeaderSize <= tag.size()) {
        const QByteArray frameId = tag.mid(position, version == 2 ? 3 : 4);
        if (frameId.isEmpty() || frameId.at(0) == '\0') break;

        quint32 frameSize = 0;
        if (version == 2) {
            frameSize = (static_cast<quint32>(static_cast<quint8>(tag.at(position + 3))) << 16)
                    | (static_cast<quint32>(static_cast<quint8>(tag.at(position + 4))) << 8)
                    | static_cast<quint32>(static_cast<quint8>(tag.at(position + 5)));
        } else if (version == 4) {
            frameSize = readSynchsafeInteger(tag, position + 4);
        } else {
            frameSize = (static_cast<quint32>(static_cast<quint8>(tag.at(position + 4))) << 24)
                    | (static_cast<quint32>(static_cast<quint8>(tag.at(position + 5))) << 16)
                    | (static_cast<quint32>(static_cast<quint8>(tag.at(position + 6))) << 8)
                    | static_cast<quint32>(static_cast<quint8>(tag.at(position + 7)));
        }
        const int bodyStart = position + frameHeaderSize;
        if (frameSize == 0 || frameSize > static_cast<quint32>(tag.size() - bodyStart)) break;

        const QByteArray frameBody = tag.mid(bodyStart, static_cast<int>(frameSize));
        const bool isPictureFrame = frameId == "APIC" || frameId == "PIC";
        if (isPictureFrame && frameBody.size() > 5) {
            const int encoding = static_cast<quint8>(frameBody.at(0));
            int descriptionStart = 0;
            if (frameId == "PIC") {
                // ID3v2.2 stores a three-character image format instead of a MIME type.
                descriptionStart = 1 + 3 + 1;
            } else {
                const int mimeEnd = frameBody.indexOf('\0', 1);
                if (mimeEnd < 0 || mimeEnd + 2 >= frameBody.size()) {
                    position = bodyStart + static_cast<int>(frameSize);
                    continue;
                }
                descriptionStart = mimeEnd + 2; // Skip MIME terminator and picture type.
            }

            int imageStart = -1;
            if (encoding == 1 || encoding == 2) {
                for (int i = descriptionStart; i + 1 < frameBody.size(); i += 2) {
                    if (frameBody.at(i) == '\0' && frameBody.at(i + 1) == '\0') {
                        imageStart = i + 2;
                        break;
                    }
                }
            } else {
                const int descriptionEnd = frameBody.indexOf('\0', descriptionStart);
                if (descriptionEnd >= 0) imageStart = descriptionEnd + 1;
            }

            if (imageStart >= 0 && imageStart < frameBody.size()) {
                QImage image;
                if (image.loadFromData(reinterpret_cast<const uchar *>(frameBody.constData() + imageStart),
                                       frameBody.size() - imageStart))
                    return QPixmap::fromImage(image);
            }
        }

        position = bodyStart + static_cast<int>(frameSize);
    }
    return QPixmap();
}

bool isMpegAudioFrameHeader(const QByteArray &data, int offset)
{
    if (offset < 0 || offset + 4 > data.size()) return false;
    const quint32 header = (static_cast<quint8>(data.at(offset)) << 24)
            | (static_cast<quint8>(data.at(offset + 1)) << 16)
            | (static_cast<quint8>(data.at(offset + 2)) << 8)
            | static_cast<quint8>(data.at(offset + 3));
    const quint32 version = (header >> 19) & 0x3;
    const quint32 layer = (header >> 17) & 0x3;
    const quint32 bitrate = (header >> 12) & 0xf;
    const quint32 sampleRate = (header >> 10) & 0x3;
    return (header >> 21) == 0x7ff && version != 0x1 && layer != 0
            && bitrate != 0 && bitrate != 0xf && sampleRate != 0x3;
}

QString createUntaggedMp3PlaybackCopy(const QString &sourcePath)
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) return QString();

    const QByteArray tagHeader = source.read(10);
    if (tagHeader.size() != 10 || !tagHeader.startsWith("ID3")) return QString();

    const quint64 tagSize = (static_cast<quint64>(static_cast<quint8>(tagHeader.at(6)) & 0x7f) << 21)
            | (static_cast<quint64>(static_cast<quint8>(tagHeader.at(7)) & 0x7f) << 14)
            | (static_cast<quint64>(static_cast<quint8>(tagHeader.at(8)) & 0x7f) << 7)
            | (static_cast<quint64>(static_cast<quint8>(tagHeader.at(9)) & 0x7f));
    quint64 audioStart = 10 + tagSize;
    if (static_cast<quint8>(tagHeader.at(3)) == 4
            && (static_cast<quint8>(tagHeader.at(5)) & 0x10))
        audioStart += 10; // ID3v2.4 footer

    if (audioStart >= static_cast<quint64>(source.size())) return QString();
    if (!source.seek(static_cast<qint64>(audioStart))) return QString();

    const QByteArray frameProbe = source.read(4096);
    int frameOffset = -1;
    for (int i = 0; i + 4 <= frameProbe.size(); ++i) {
        if (isMpegAudioFrameHeader(frameProbe, i)) {
            frameOffset = i;
            break;
        }
    }
    if (frameOffset < 0) return QString();
    audioStart += static_cast<quint64>(frameOffset);

    qint64 audioEnd = source.size();
    if (audioEnd >= 128 && source.seek(audioEnd - 128)
            && source.read(3) == "TAG")
        audioEnd -= 128; // ID3v1 trailer is not audio data
    if (audioEnd <= static_cast<qint64>(audioStart)) return QString();

    const QString cacheRoot = QDir::temp().filePath(QStringLiteral("QQMusic/playback-cache"));
    QDir cacheDir;
    if (!cacheDir.mkpath(cacheRoot)) return QString();
    const QByteArray pathHash = QCryptographicHash::hash(
                QFileInfo(sourcePath).absoluteFilePath().toUtf8(), QCryptographicHash::Sha256).toHex();
    const QString playbackPath = QDir(cacheRoot).filePath(QString::fromLatin1(pathHash) + QStringLiteral(".mp3"));
    const qint64 expectedSize = audioEnd - static_cast<qint64>(audioStart);
    if (QFileInfo(playbackPath).size() == expectedSize) return playbackPath;

    QFile playbackCopy(playbackPath);
    if (!playbackCopy.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || !source.seek(static_cast<qint64>(audioStart)))
        return QString();

    qint64 remaining = expectedSize;
    while (remaining > 0) {
        const QByteArray chunk = source.read(qMin<qint64>(remaining, 1024 * 1024));
        if (chunk.isEmpty() || playbackCopy.write(chunk) != chunk.size()) {
            playbackCopy.close();
            QFile::remove(playbackPath);
            return QString();
        }
        remaining -= chunk.size();
    }
    playbackCopy.close();
    return playbackCopy.error() == QFile::NoError ? playbackPath : QString();
}

QString nextAvailableAudioPath(const QString &directory, const QFileInfo &sourceInfo)
{
    const QString suffix = sourceInfo.suffix();
    const QString stem = sourceInfo.completeBaseName();
    QString candidate = QDir(directory).filePath(sourceInfo.fileName());
    int copyNumber = 2;
    while (QFileInfo::exists(candidate)) {
        const QString fileName = suffix.isEmpty()
                ? QStringLiteral("%1 (%2)").arg(stem).arg(copyNumber)
                : QStringLiteral("%1 (%2).%3").arg(stem).arg(copyNumber).arg(suffix);
        candidate = QDir(directory).filePath(fileName);
        ++copyNumber;
    }
    return candidate;
}

} // namespace



QQMusic::QQMusic(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::QQMusic)
    , lrcPage(nullptr)
    , lrcPageAnimal(nullptr)
    , currentIndex(-1)
    , totalTime(0)
    , isDrag(false)
    , ncmProcess(nullptr)
    , ncmProgress(nullptr)
    , ncmConversionCancelled(false)
{
    ui->setupUi(this);

    initUi();

    initSqlite();

    initPlayer();

    initMusicList();

    connectSignalAndSlots();
}

QQMusic::~QQMusic()
{
    if (ncmProcess && ncmProcess->state() != QProcess::NotRunning) {
        ncmProcess->kill();
        ncmProcess->waitForFinished(1500);
    }
    if (sqlite.isOpen()) {
        musicList.writeToDB();
        sqlite.close();
    }
    delete ui;
}

void QQMusic::initUi()
{
    this->setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setWindowIcon(QIcon(":/images/tubiao.png"));
    setWindowTitle(QStringLiteral("简音 · 音乐播放器"));
    resize(1180, 760);
    setMinimumSize(960, 640);
    QFont appFont(QStringLiteral("Microsoft YaHei UI"), 10);
    QApplication::setFont(appFont);
    ui->max->setEnabled(false);

    // A quiet, neutral palette with one muted green accent.
    ui->background->setStyleSheet("QWidget#background { background:#f7f8f7; border:1px solid #e8ece9; border-radius:14px; }");
    ui->headLeft->setStyleSheet("QWidget#headLeft { background:#f1f4f1; border-top-left-radius:14px; }");
    ui->headRight->setStyleSheet("QWidget#headRight { background:#f7f8f7; border-top-right-radius:14px; }");
    ui->bodyLeft->setStyleSheet("QWidget#bodyLeft { background:#f1f4f1; }");
    ui->bodyRight->setStyleSheet("QWidget#bodyRight { background:#f7f8f7; }");
    ui->controlBox->setStyleSheet("QWidget#controlBox { background:#ffffff; border-top:1px solid #e9edea; border-bottom-left-radius:14px; border-bottom-right-radius:14px; }");
    ui->scrollArea->setFrameShape(QFrame::NoFrame);
    // The designer file used a fixed 810x510 geometry here. Put the scroll
    // area in a layout so it follows the available page size and never sits
    // over the bottom player controls.
    if (!ui->recPage->layout()) {
        QVBoxLayout *recommendationLayout = new QVBoxLayout(ui->recPage);
        recommendationLayout->setContentsMargins(0, 0, 0, 0);
        recommendationLayout->setSpacing(0);
        recommendationLayout->addWidget(ui->scrollArea);
    }
    ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->scrollArea->setWidgetResizable(true);
    ui->scrollArea->setStyleSheet("QScrollArea { background:transparent; border:0; } QScrollBar:vertical { background:transparent; width:8px; margin:4px 2px; } QScrollBar::handle:vertical { background:#d5d7da; border-radius:4px; min-height:32px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }");
    ui->scrollAreaWidgetContents_2->setStyleSheet("QWidget#scrollAreaWidgetContents_2 { background:transparent; }");
    ui->recText->setStyleSheet("font-size:26px; font-weight:600; color:#28332e; padding:8px 0 4px 0;");
    // These labels are only 30 px tall. Large vertical padding used to squeeze
    // and clip the glyphs into the cards below them.
    ui->recMusicText->setStyleSheet("font-size:17px; font-weight:600; color:#3d4943; padding:0 0 2px 0;");
    ui->supplyMusicText->setStyleSheet("font-size:17px; font-weight:600; color:#3d4943; padding:0 0 2px 0;");
    ui->recMusicText->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->supplyMusicText->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->lineEdit->setPlaceholderText(QStringLiteral("搜索音乐、歌手、专辑"));
    ui->lineEdit->setStyleSheet("QLineEdit { background:#eef1ef; border:1px solid transparent; border-radius:17px; padding:8px 14px; color:#35423b; selection-background-color:#75ad8b; } QLineEdit:focus { border-color:#b8d4c2; background:#ffffff; }");
    const QString greenButtonStyle = QStringLiteral("QPushButton { background:#5b9c76; color:white; border:0; border-radius:18px; padding:7px 16px; } QPushButton:hover { background:#4d8b68; }");
    ui->play->setStyleSheet(greenButtonStyle);
    const QString chromeButtonStyle = QStringLiteral("QPushButton { background:transparent; color:#68746d; border:0; border-radius:14px; padding:6px; } QPushButton:hover { background:#eef1ef; }");
    ui->playUp->setStyleSheet(chromeButtonStyle);
    ui->playDown->setStyleSheet(chromeButtonStyle);
    ui->playMode->setStyleSheet(chromeButtonStyle);
    ui->volume->setStyleSheet(chromeButtonStyle);
    ui->addLocal->setStyleSheet(chromeButtonStyle);
    ui->addLocal->setToolTip(QStringLiteral("添加音乐或导入 NCM"));
    ui->lrcWord->setStyleSheet(chromeButtonStyle);
    ui->musicCover->setPixmap(QPixmap(QStringLiteral(":/images/rec/001.png")));
    ui->musicCover->setScaledContents(true);
    // QSS backgrounds in the .ui file are replaced by the neutral button
    // style above, so install the icons explicitly to keep every control
    // visible after changing themes.
    ui->playUp->setIcon(QIcon(QStringLiteral(":/images/up.png")));
    ui->playDown->setIcon(QIcon(QStringLiteral(":/images/down.png")));
    ui->volume->setIcon(QIcon(QStringLiteral(":/images/volumn.png")));
    ui->addLocal->setIcon(QIcon(QStringLiteral(":/images/add.png")));
    ui->lrcWord->setIcon(QIcon(QStringLiteral(":/images/ci.png")));
    for (QPushButton *button : {ui->playUp, ui->playDown, ui->playMode,
                                ui->volume, ui->addLocal, ui->lrcWord}) {
        button->setIconSize(QSize(18, 18));
    }
    // The title bar's original spacer reserved 458 px even on narrow windows,
    // pushing the window buttons outside the visible area.
    if (ui->settingBox->layout() && ui->settingBox->layout()->count() > 0) {
        QLayoutItem *spacer = ui->settingBox->layout()->itemAt(0);
        if (spacer && spacer->spacerItem()) {
            spacer->spacerItem()->changeSize(0, 0, QSizePolicy::Expanding,
                                             QSizePolicy::Minimum);
            ui->settingBox->layout()->invalidate();
        }
    }

    // 添加系统托盘
    QSystemTrayIcon* trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/images/tubiao.png"));
    trayIcon->show();
    // 托盘添加菜单
    QMenu* trayMenu = new QMenu();
    trayMenu->addAction("显示", this, &QWidget::showNormal);
    trayMenu->addAction("退出", this, &QQMusic::onQQMusicQuit);
    trayIcon->setContextMenu(trayMenu);

    // 设置BtForm图标 & 文本信息
    ui->rec->setIconAndText(":/images/rec.png", "推荐", 0);
    ui->radio->setIconAndText(":/images/radio.png", "电台", 1);
    ui->music->setIconAndText(":/images/music.png", "音乐馆", 2);
    ui->like->setIconAndText(":/images/like.png", "我喜欢", 3);
    ui->local->setIconAndText(":/images/local.png", "本地下载", 4);
    ui->recent->setIconAndText(":/images/recent.png", "最近播放", 5);

    // Open on the recent-played view, matching the supplied reference layout.
    ui->stackedWidget->setCurrentIndex(5);
    currentPage = ui->recentPage;
    ui->recent->showAnimal(true);

    // 初始化推荐页面
    srand(time(NULL));
    ui->recMusicBox->initRecBoxUi(randomPiction(), 1);
    ui->supplyMuscBox->initRecBoxUi(randomPiction(), 2);

    // 设置CommonPage的信息
    ui->likePage->setCommonPageUI("我喜欢", ":/images/ilikebg.png");
    ui->localPage->setCommonPageUI("本地音乐", ":/images/localbg.png");
    ui->recentPage->setCommonPageUI("最近播放", ":/images/recentbg.png");

    // 播放控制区按钮图标设定
    ui->play->setIcon(QIcon(":/images/play_2.png"));
    ui->playMode->setIcon(QIcon(":/images/shuffle_2.png"));


    volumeTool = new VolumeTool(this);

    // 实例化LrcWord对象
    // The lyrics view is a full-window overlay, covering the navigation,
    // header, content and player controls while it is open.
    lrcPage = new LrcPage(this);
    lrcPage->setGeometry(rect());
    lrcPage->hide();

    // 初始化上移动画对象
    lrcPageAnimal = new QPropertyAnimation(lrcPage, "geometry", this);
    lrcPageAnimal->setDuration(500);
    lrcPageAnimal->setStartValue(QRect(0, height(), width(), height()));
    lrcPageAnimal->setEndValue(rect());
}

void QQMusic::initSqlite()
{
    // 1. 进行数据库驱动加载
    sqlite = QSqlDatabase::addDatabase("QSQLITE");

    // 2. 设置数据库名称
    sqlite.setDatabaseName(QCoreApplication::applicationDirPath() + "/QQMusic.db");

    // 3. 打开
    if(!sqlite.open())
    {
        QMessageBox::critical(this, "QQMusic", "数据库打开失败!!!");
        return;
    }

    qDebug()<<"QQMusic数据库连接成功";

    // 4. 创建表
    QString sql = "CREATE TABLE IF NOT EXISTS MusicInfo(\
                      id INTEGER PRIMARY KEY AUTOINCREMENT,\
                      musicId varchar(50) UNIQUE,\
                      musicName varchar(50),\
                      musicSinger varchar(50), \
                      albumName varchar(50),\
                      musicUrl varchar(256),\
                      duration BIGINT,\
                      isLike INTEGER,\
                      isHistory INTEGER)";

    QSqlQuery query;
    if(!query.exec(sql))
    {
        QMessageBox::critical(this, "QQMusic", "初始化错误!!!");
        return;
    }

    qDebug()<<"QQMusic表创建成功!!!";
}

void QQMusic::initMusicList()
{
    musicList.readFromDB();

    ui->likePage->setMusicListType(PageType::LIKE_PAGE);
    ui->likePage->reFresh(musicList);

    ui->localPage->setMusicListType(PageType::LOCAL_PAGE);
    ui->localPage->reFresh(musicList);
    // 将localPage中的歌曲添加到媒体播放列表中
    ui->localPage->addMusicToPlaylist(musicList, playList);

    ui->recentPage->setMusicListType(PageType::HISTORY_PAGE);
    ui->recentPage->reFresh(musicList);
}

void QQMusic::initPlayer()
{
    // 1. 初始化媒体播相关类对象
    player = new QMediaPlayer(this);
    playList = new QMediaPlaylist(this);

    // 2. 设置默认播放模式
    playList->setPlaybackMode(QMediaPlaylist::Random);

    // 3. 将播放列表设置到播放媒体对象中
    player->setPlaylist(playList);

    // 4. 设置默认的音量大小, 默认设置成20%
    player->setVolume(20);


    // 关联QMediaPlayer的信号
    connect(player, &QMediaPlayer::stateChanged, this, &QQMusic::onPlayerStateChanged);

    // 关联QMediaPlayer::durationChanged信号
    connect(player, &QMediaPlayer::durationChanged, this, &QQMusic::onDurationChanged);

    // 关联QMediaPlayer::positionChanged信号
    connect(player, &QMediaPlayer::positionChanged, this, &QQMusic::onPositionChanged);

    // 关联QMediaPlayer::metaDataAvailableChanged
    connect(player, &QMediaPlayer::metaDataAvailableChanged, this, &QQMusic::onMetaDataAvailableChanged);
    connect(player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this,
            [this](QMediaPlayer::Error error) {
        const int queueIndex = playList->currentIndex();
        const QString musicId = queueIndex >= 0 && queueIndex < currentPlaylistIds.size()
                ? currentPlaylistIds.at(queueIndex) : QString();
        const auto music = musicId.isEmpty() ? musicList.end() : musicList.findMusicByMusicId(musicId);
        const QUrl mediaUrl = player->currentMedia().request().url();
        const QString title = music == musicList.end()
                ? mediaUrl.fileName()
                : music->getMusicName() + QStringLiteral(" - ") + music->getMusicSinger();
        const QString currentPath = mediaUrl.toLocalFile();
        const QString sourcePath = music == musicList.end()
                ? currentPath : music->getMusicUrl().toLocalFile();

        // Qt 5.14's Windows backend can fail to resolve some tagged MP3 files.
        // Retry from the first MPEG frame using a temporary copy, preserving the
        // user's original file and the playlist's song IDs.
        if (error == QMediaPlayer::ResourceError
                && QFileInfo(sourcePath).suffix().compare(QStringLiteral("mp3"), Qt::CaseInsensitive) == 0
                && QFileInfo::exists(sourcePath) && queueIndex >= 0) {
            QString fallbackPath = playbackFallbackPaths.value(sourcePath);
            if (fallbackPath.isEmpty() && !playbackFallbackAttempted.contains(sourcePath)) {
                playbackFallbackAttempted.insert(sourcePath);
                fallbackPath = createUntaggedMp3PlaybackCopy(sourcePath);
                if (!fallbackPath.isEmpty()) playbackFallbackPaths.insert(sourcePath, fallbackPath);
            }

            if (!fallbackPath.isEmpty()
                    && QFileInfo(currentPath).absoluteFilePath().compare(
                        QFileInfo(fallbackPath).absoluteFilePath(), Qt::CaseInsensitive) != 0) {
                const QStringList playlistIds = currentPlaylistIds;
                qDebug() << "Retrying MP3 without ID3 tag:" << sourcePath;
                QTimer::singleShot(0, this, [this, queueIndex, playlistIds, fallbackPath]() {
                    player->stop();
                    playList->clear();
                    currentPlaylistIds = playlistIds;
                    for (int i = 0; i < playlistIds.size(); ++i) {
                        const auto item = musicList.findMusicByMusicId(playlistIds.at(i));
                        if (item == musicList.end()) {
                            playList->addMedia(QUrl());
                        } else if (i == queueIndex) {
                            playList->addMedia(QUrl::fromLocalFile(fallbackPath));
                        } else {
                            playList->addMedia(item->getMusicUrl());
                        }
                    }
                    if (queueIndex < playList->mediaCount()) {
                        playList->setCurrentIndex(queueIndex);
                        player->play();
                    }
                });
                return;
            }
        }

        QString reason = player->errorString();
        if (reason.isEmpty()) {
            switch (error) {
            case QMediaPlayer::ResourceError:
                reason = QStringLiteral("媒体资源无法解析（本地文件存在，但当前播放器后端仍无法读取）");
                break;
            case QMediaPlayer::FormatError:
                reason = QStringLiteral("音频格式或编码不受当前播放器支持");
                break;
            case QMediaPlayer::NetworkError:
                reason = QStringLiteral("网络媒体读取失败");
                break;
            case QMediaPlayer::AccessDeniedError:
                reason = QStringLiteral("没有读取该媒体文件的权限");
                break;
            default:
                reason = QStringLiteral("多媒体错误代码：%1").arg(static_cast<int>(error));
                break;
            }
        }
        QMessageBox::warning(this, QStringLiteral("播放失败"),
                             QStringLiteral("无法播放：%1\n文件：%2\n原因：%3")
                                 .arg(title, sourcePath, reason));
    });

    // 播放列表的模式放生改变时的信号槽关联
    connect(playList, &QMediaPlaylist::playbackModeChanged, this, &QQMusic::onPlaybackModeChanged);

    // 当playlist中播放源发生变化时
    connect(playList, &QMediaPlaylist::currentIndexChanged, this, &QQMusic::onCurrentIndexChanged);
}

QJsonArray QQMusic::randomPiction()
{
    // 推荐文本 + 推荐图片路径
    QVector<QString> vecImageName;
    vecImageName<<"001.png"<<"003.png"<<"004.png"<<"005.png"<<"006.png"<<"007.png"
                <<"008.png"<<"009.png"<<"010.png"<<"011.png"<<"012.png"<<"013.png"
                <<"014.png"<<"015.png"<<"016.png"<<"017.png"<<"018.png"<<"019.png"
                <<"020.png"<<"021.png"<<"022.png"<<"023.png"<<"024.png"<<"025.png"
                <<"026.png"<<"027.png"<<"028.png"<<"029.png"<<"030.png"<<"031.png"
                <<"032.png"<<"033.png"<<"034.png"<<"035.png"<<"036.png"<<"037.png"
                <<"038.png"<<"039.png"<<"040.png";

    std::random_shuffle(vecImageName.begin(), vecImageName.end());

    // 001.png
    // path: ":/images/rec/"+vecImageName[i];
    // text: "推荐-001"
    QJsonArray objArray;
    for(int i = 0; i < vecImageName.size(); ++i)
    {
        QJsonObject obj;
        obj.insert("path", ":/images/rec/"+vecImageName[i]);

        QString strText = QString("推荐-%1").arg(i, 3, 10, QChar('0'));
        obj.insert("text", strText);

        objArray.append(obj);

    }

    return objArray;
}

void QQMusic::connectSignalAndSlots()
{
    connect(ui->lineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        ui->likePage->setSearchText(text);
        ui->localPage->setSearchText(text);
        ui->recentPage->setSearchText(text);
        ui->likePage->reFresh(musicList);
        ui->localPage->reFresh(musicList);
        ui->recentPage->reFresh(musicList);
    });

    // 关联BtForm的信号和处理该信号的槽函数
    connect(ui->rec, &BtForm::btClick, this, &QQMusic::onBtFormClick);
    connect(ui->radio, &BtForm::btClick, this, &QQMusic::onBtFormClick);
    connect(ui->music, &BtForm::btClick, this, &QQMusic::onBtFormClick);
    connect(ui->like, &BtForm::btClick, this, &QQMusic::onBtFormClick);
    connect(ui->local, &BtForm::btClick, this, &QQMusic::onBtFormClick);
    connect(ui->recent, &BtForm::btClick, this, &QQMusic::onBtFormClick);
    // 收藏或者不收藏处理
    connect(ui->likePage, &CommonPage::updataLikeMusic, this, &QQMusic::updateLikeMusicAndPage);
    connect(ui->localPage, &CommonPage::updataLikeMusic, this, &QQMusic::updateLikeMusicAndPage);
    connect(ui->recentPage, &CommonPage::updataLikeMusic, this, &QQMusic::updateLikeMusicAndPage);
    connect(ui->likePage, &CommonPage::removeMusicRequested, this, &QQMusic::removeMusicFromLibrary);
    connect(ui->localPage, &CommonPage::removeMusicRequested, this, &QQMusic::removeMusicFromLibrary);
    connect(ui->recentPage, &CommonPage::removeMusicRequested, this, &QQMusic::removeMusicFromLibrary);


    // 播放控制区的信号和槽函数关联
    connect(ui->play, &QPushButton::clicked, this, &QQMusic::onPlayMusic);
    connect(ui->playUp, &QPushButton::clicked, this, &QQMusic::onPlayUpClicked);
    connect(ui->playDown, &QPushButton::clicked, this, &QQMusic::onPlayDownClicked);
    connect(ui->playMode, &QPushButton::clicked, this, &QQMusic::onPlaybackModeClicked);

    // likePage、localPage、recentPage都有播放所有按钮
    connect(ui->likePage, &CommonPage::playAll, this, &QQMusic::onPlayAll);
    connect(ui->localPage, &CommonPage::playAll, this, &QQMusic::onPlayAll);
    connect(ui->recentPage, &CommonPage::playAll, this, &QQMusic::onPlayAll);

    // 处理likePage、localPage、recentPage中ListItemBox双击
    connect(ui->likePage, &CommonPage::playMusicByIndex, this, &QQMusic::playMusicByIndex);
    connect(ui->localPage, &CommonPage::playMusicByIndex, this, &QQMusic::playMusicByIndex);
    connect(ui->recentPage, &CommonPage::playMusicByIndex, this, &QQMusic::playMusicByIndex);
    // 歌词按钮点击信号和槽函数
    connect(ui->lrcWord, &QPushButton::clicked, this, &QQMusic::onLrcWordClicked);

    // 处理静音
    connect(volumeTool, &VolumeTool::setMusicMuted, this, &QQMusic::setPlayerMuted);

    // 处理音量大小
    connect(volumeTool, &VolumeTool::setMusicVolume, this, &QQMusic::setPlayerVolume);

    // MusicSlider::setMusicSliderPosiont
    connect(ui->progressBar, &MusicSlider::setMusicSliderPositon, this, &QQMusic::onMusicSliderChanged);
}

void QQMusic::updateBtformAnimal()
{
    // 获取currentPage在stackedWidget中的索引
    int index = ui->stackedWidget->indexOf(currentPage);
    if(-1 == index)
    {
        qDebug()<<"该页面不存在";
        return;
    }

    // 获取QQMusci界面上所有的btForm
    QList<BtForm*> btForms = this->findChildren<BtForm*>();
    for(auto btForm : btForms)
    {
        if(btForm->getPageId() == index)
        {
            // 将currentPage对鱼竿的btForm找到了
            btForm->showAnimal(true);
        }
        else
        {
            btForm->showAnimal(false);
        }
    }
}

void QQMusic::onQQMusicQuit()
{
    // 关闭窗口
    close();
}

void QQMusic::closeEvent(QCloseEvent *event)
{
    event->accept();
}

void QQMusic::mouseMoveEvent(QMouseEvent *event)
{
    if(event->buttons() == Qt::LeftButton && isDrag)
    {
        move(event->globalPos() - dragPos);

        qDebug()<<"mouse move";
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void QQMusic::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        isDrag = true;
        // 获取鼠标相对于电脑屏幕左上角的全局坐标
        dragPos = event->globalPos() - geometry().topLeft();
        return;
    }

     QWidget::mousePressEvent(event);
}

void QQMusic::on_quit_clicked()
{
    hide();
}

void QQMusic::onBtFormClick(int pageId)
{
    // 清除之前btForm按钮的颜色背景
    // 获取所有的BtForm按钮
    QList<BtForm*> btFormList = this->findChildren<BtForm*>();
    for(auto btForm : btFormList)
    {
        if(btForm->getPageId() != pageId)
        {
            btForm->clearBackground();
        }
    }

    ui->stackedWidget->setCurrentIndex(pageId);
    qDebug()<<"切换页面"<<pageId;

    isDrag = false;
}

void QQMusic::updateLikeMusicAndPage(bool isLike, const QString &musicId)
{
    // 1. 修改状态
    auto it = musicList.findMusicByMusicId(musicId);
    if(it != musicList.end())
    {
        it->setIsLike(isLike);
        it->InsertMusicToDB();
    }

    // 2. 更新Page页面的歌曲列表
    ui->likePage->reFresh(musicList);
    ui->localPage->reFresh(musicList);
    ui->recentPage->reFresh(musicList);
}

void QQMusic::removeMusicFromLibrary(const QString &musicId)
{
    auto music = musicList.findMusicByMusicId(musicId);
    if (music == musicList.end()) return;

    const QString title = music->getMusicName();
    const QString prompt = QStringLiteral("确定从播放器移除“%1”吗？\n音频文件会保留在磁盘原位置。")
            .arg(title);
    if (QMessageBox::question(this, QStringLiteral("移除歌曲"), prompt,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
        return;

    const int playingIndex = playList->currentIndex();
    const QString playingId = playingIndex >= 0 && playingIndex < currentPlaylistIds.size()
            ? currentPlaylistIds.at(playingIndex) : QString();
    const bool removingCurrentSong = playingId == musicId;

    if (!musicList.removeMusicById(musicId)) {
        QMessageBox::warning(this, QStringLiteral("移除失败"),
                             QStringLiteral("无法从曲库数据库中移除这首歌。"));
        return;
    }

    if (removingCurrentSong) {
        player->stop();
        playList->clear();
        currentPlaylistIds.clear();
        currentIndex = -1;
        ui->musicName->setText(QStringLiteral("暂无播放歌曲"));
        ui->musicSinger->clear();
        ui->currentTime->setText(QStringLiteral("00:00"));
        ui->totalTime->setText(QStringLiteral("00:00"));
        totalTime = 0;
        ui->progressBar->setStep(0);
    } else {
        for (int i = currentPlaylistIds.size() - 1; i >= 0; --i) {
            if (currentPlaylistIds.at(i) == musicId) {
                currentPlaylistIds.removeAt(i);
                playList->removeMedia(i);
            }
        }
    }

    ui->likePage->reFresh(musicList);
    ui->localPage->reFresh(musicList);
    ui->recentPage->reFresh(musicList);
}

void QQMusic::on_volume_clicked()
{
    // 1. 获取ui->volume控件的left-top坐标，并转换为基于屏幕的全局坐标
    QPoint point = ui->volume->mapToGlobal(QPoint(0, 0));

    // 2. 计算volumeTool需要移动到的位置：即ui->volume的正上方偏左一半
    QPoint volumeLeftTop = point - QPoint(volumeTool->width()/2, volumeTool->height());

    volumeLeftTop.setX(volumeLeftTop.x()+15);
    volumeLeftTop.setY(volumeLeftTop.y()+30);

    // 3. 移动volumeTool
    volumeTool->move(volumeLeftTop);

    // 4. 窗口显示
    volumeTool->show();
}

void QQMusic::on_addLocal_clicked()
{
    QMenu menu(this);
    QAction *addAudioAction = menu.addAction(QStringLiteral("添加本地音乐…"));
    QMenu *ncmMenu = menu.addMenu(QStringLiteral("导入 NCM 音乐"));
    QAction *ncmFilesAction = ncmMenu->addAction(QStringLiteral("选择 NCM 文件…"));
    QAction *ncmFolderAction = ncmMenu->addAction(QStringLiteral("扫描 NCM 文件夹…"));
    QAction *selectedAction = menu.exec(ui->addLocal->mapToGlobal(
                                            QPoint(0, -menu.sizeHint().height())));
    if (selectedAction == addAudioAction) {
        addLocalAudioFiles();
    } else if (selectedAction == ncmFilesAction) {
        chooseNcmFiles();
    } else if (selectedAction == ncmFolderAction) {
        chooseNcmFolder();
    }
}

void QQMusic::addLocalAudioFiles()
{
    QFileDialog fileDialog(this);

    // 设置窗口的标题
    fileDialog.setWindowTitle("添加本地下载音乐");

    // 设置文件对话框默认的打开路径
    const QString sampleMusicPath = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../QQMusic/musics");
    if (QDir(sampleMusicPath).exists())
        fileDialog.setDirectory(sampleMusicPath);


    // 设置一次多开多个
    fileDialog.setFileMode(QFileDialog::ExistingFiles);

    // 通过文件后缀
    // fileDialog.setNameFilter("代码文件(*.h *.c *.hpp *.cpp)");
    // fileDialog.setNameFilter("图片文件(*.png *.bmp *.jpg)");

    // 通过文件的MIME类型来过滤
    QStringList mimeTypeFilters;
     mimeTypeFilters << "audio/mpeg" << "audio/flac" << "audio/wav" << "audio/mp4" << "audio/aac" << "audio/ogg";
     fileDialog.setMimeTypeFilters(mimeTypeFilters);

    if(QDialog::Accepted == fileDialog.exec())
    {
        // 获取选中的文件
        QList<QUrl> fileUrls = fileDialog.selectedUrls();

        // fileUrls: 内部存放的是刚刚选中的文件的url路径
        // 需要将文件信息填充到本地下载

        // 将所有音乐添加到音乐列表中，MusicList
        // musicList中管理的是解析之后的Music对象
        musicList.addMusicsByUrl(fileUrls);
        musicList.writeToDB();

        // 将歌曲信息更新到CommonPage页面的的listWidget中
        // 将CommonPage切换到本地下载的页面
        ui->stackedWidget->setCurrentIndex(4);

        ui->localPage->reFresh(musicList);
    }
}

void QQMusic::chooseNcmFiles()
{
    QFileDialog fileDialog(this);
    fileDialog.setWindowTitle(QStringLiteral("选择网易云音乐 NCM 文件"));
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    fileDialog.setNameFilter(QStringLiteral("NCM 文件 (*.ncm *.NCM)"));
    if (fileDialog.exec() != QDialog::Accepted) return;

    QStringList inputPaths;
    const QList<QUrl> urls = fileDialog.selectedUrls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) inputPaths.append(url.toLocalFile());
    }
    startNcmConversion(inputPaths);
}

void QQMusic::chooseNcmFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(
                this, QStringLiteral("选择要批量导入的 NCM 文件夹"),
                QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    if (folder.isEmpty()) return;

    QDirIterator iterator(folder, QStringList() << QStringLiteral("*.ncm")
                                               << QStringLiteral("*.NCM"),
                         QDir::Files, QDirIterator::Subdirectories);
    int fileCount = 0;
    while (iterator.hasNext()) {
        iterator.next();
        ++fileCount;
    }
    if (fileCount == 0) {
        QMessageBox::information(this, QStringLiteral("没有找到 NCM 文件"),
                                 QStringLiteral("所选文件夹及其子文件夹中没有 .ncm 文件。"));
        return;
    }
    startNcmConversion(QStringList() << folder);
}

void QQMusic::startNcmConversion(const QStringList &inputPaths)
{
    if (inputPaths.isEmpty()) return;
    if (ncmProcess && ncmProcess->state() != QProcess::NotRunning) {
        QMessageBox::information(this, QStringLiteral("正在转换"),
                                 QStringLiteral("当前批次转换完成后再开始下一批。"));
        return;
    }

    const QString javaPath = QStandardPaths::findExecutable(QStringLiteral("java"));
    if (javaPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("需要 Java 环境"),
                             QStringLiteral("没有找到 Java。请安装 Java 8 或更高版本，并将 java 加入 PATH 后重试。"));
        return;
    }

    QSettings settings(QStringLiteral("JianYinPlayer"), QStringLiteral("QQMusic"));
    QString jarPath = settings.value(QStringLiteral("ncm2mp3/jarPath")).toString();
    if (!QFileInfo(jarPath).isFile()) {
        const QStringList candidates = QStringList()
                << QDir(QCoreApplication::applicationDirPath()).filePath(
                       QStringLiteral("tools/NCM2MP3-3.1.0-shaded.jar"))
                << QStringLiteral("D:/NCM2MP3/target/NCM2MP3-3.1.0-shaded.jar")
                << QStringLiteral("D:/NCM2MP3/target/NCM2MP3-3.1.0.jar");
        for (const QString &candidate : candidates) {
            if (QFileInfo(candidate).isFile()) {
                jarPath = candidate;
                break;
            }
        }
    }
    if (!QFileInfo(jarPath).isFile()) {
        jarPath = QFileDialog::getOpenFileName(
                    this, QStringLiteral("定位 NCM2MP3 工具"),
                    QStringLiteral("D:/NCM2MP3/target"),
                    QStringLiteral("Java 程序包 (*.jar)"));
        if (jarPath.isEmpty()) return;
        settings.setValue(QStringLiteral("ncm2mp3/jarPath"), jarPath);
    }

    ncmLyricsByBase.clear();
    const auto rememberSidecar = [this](const QString &ncmPath) {
        const QFileInfo ncmInfo(ncmPath);
        const QString lrcPath = ncmInfo.dir().filePath(ncmInfo.completeBaseName() + QStringLiteral(".lrc"));
        if (QFileInfo(lrcPath).isFile())
            ncmLyricsByBase.insert(ncmInfo.completeBaseName().toCaseFolded(), lrcPath);
    };
    for (const QString &inputPath : inputPaths) {
        const QFileInfo inputInfo(inputPath);
        if (inputInfo.isDir()) {
            QDirIterator iterator(inputPath, QStringList() << QStringLiteral("*.ncm")
                                                          << QStringLiteral("*.NCM"),
                                 QDir::Files, QDirIterator::Subdirectories);
            while (iterator.hasNext()) rememberSidecar(iterator.next());
        } else if (inputInfo.isFile()
                   && inputInfo.suffix().compare(QStringLiteral("ncm"), Qt::CaseInsensitive) == 0) {
            rememberSidecar(inputInfo.absoluteFilePath());
        }
    }

    if (!ncmTempDir.isValid()) {
        QMessageBox::warning(this, QStringLiteral("无法开始转换"),
                             QStringLiteral("无法创建临时转换目录，请检查系统临时目录的可用空间和权限。"));
        return;
    }
    ncmOutputDir = QDir(ncmTempDir.path()).filePath(
                QStringLiteral("batch-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!QDir().mkpath(ncmOutputDir)) {
        QMessageBox::warning(this, QStringLiteral("无法开始转换"),
                             QStringLiteral("无法创建临时转换目录。"));
        ncmOutputDir.clear();
        return;
    }

    QStringList arguments;
    arguments << QStringLiteral("-jar") << jarPath << QStringLiteral("-c");
    arguments.append(inputPaths);

    ncmConversionCancelled = false;
    ncmProcess = new QProcess(this);
    ncmProcess->setWorkingDirectory(ncmOutputDir);
    ncmProgress = new QProgressDialog(
                QStringLiteral("正在转换 NCM 文件，请稍候…"), QStringLiteral("取消"), 0, 0, this);
    ncmProgress->setWindowTitle(QStringLiteral("导入 NCM 音乐"));
    ncmProgress->setWindowModality(Qt::WindowModal);
    ncmProgress->setMinimumDuration(0);
    connect(ncmProgress, &QProgressDialog::canceled, this, [this]() {
        ncmConversionCancelled = true;
        if (ncmProcess && ncmProcess->state() != QProcess::NotRunning)
            ncmProcess->kill();
    });
    connect(ncmProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !ncmProcess) return;
        ncmProcess->deleteLater();
        ncmProcess = nullptr;
        if (ncmProgress) {
            ncmProgress->close();
            ncmProgress->deleteLater();
            ncmProgress = nullptr;
        }
        if (!ncmOutputDir.isEmpty()) QDir(ncmOutputDir).removeRecursively();
        ncmOutputDir.clear();
        QMessageBox::warning(this, QStringLiteral("无法启动转换工具"),
                             QStringLiteral("无法启动 Java，请检查 Java 安装和 PATH 设置。"));
    });
    connect(ncmProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &QQMusic::finishNcmConversion);
    ncmProgress->show();
    ncmProcess->start(javaPath, arguments);
}

void QQMusic::finishNcmConversion(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *finishedProcess = ncmProcess;
    if (!finishedProcess) return;
    const QString errorOutput = QString::fromLocal8Bit(finishedProcess->readAllStandardError()).trimmed();
    const QString standardOutput = QString::fromLocal8Bit(finishedProcess->readAllStandardOutput()).trimmed();
    ncmProcess = nullptr;
    finishedProcess->deleteLater();
    if (ncmProgress) {
        ncmProgress->close();
        ncmProgress->deleteLater();
        ncmProgress = nullptr;
    }

    if (ncmConversionCancelled) {
        if (!ncmOutputDir.isEmpty()) QDir(ncmOutputDir).removeRecursively();
        ncmOutputDir.clear();
        QMessageBox::information(this, QStringLiteral("已取消"), QStringLiteral("NCM 转换已取消。"));
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        if (!ncmOutputDir.isEmpty()) QDir(ncmOutputDir).removeRecursively();
        ncmOutputDir.clear();
        const QString diagnostic = errorOutput.isEmpty() ? standardOutput : errorOutput;
        const QString detail = diagnostic.isEmpty() ? QString() : QStringLiteral("\n\n") + diagnostic.left(1800);
        QMessageBox::warning(this, QStringLiteral("NCM 转换失败"),
                             QStringLiteral("转换工具异常退出。请检查 NCM2MP3 工具和所选文件。%1").arg(detail));
        return;
    }

    const QString convertedDir = QDir(ncmOutputDir).filePath(QStringLiteral("output"));
    const QStringList audioFilters = QStringList()
            << QStringLiteral("*.mp3") << QStringLiteral("*.flac") << QStringLiteral("*.wav")
            << QStringLiteral("*.m4a") << QStringLiteral("*.aac") << QStringLiteral("*.ogg")
            << QStringLiteral("*.wma");
    QString musicRoot = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (musicRoot.isEmpty())
        musicRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (musicRoot.isEmpty()) {
        if (!ncmOutputDir.isEmpty()) QDir(ncmOutputDir).removeRecursively();
        ncmOutputDir.clear();
        QMessageBox::warning(this, QStringLiteral("无法保存转换结果"),
                             QStringLiteral("系统没有可用的音乐目录或应用数据目录。"));
        return;
    }
    const QString libraryDir = QDir(musicRoot).filePath(QStringLiteral("简音播放器/转换音乐"));
    QStringList importedPaths;
    int copyFailures = 0;
    int lyricCopyFailures = 0;
    if (!QDir().mkpath(libraryDir)) {
        if (!ncmOutputDir.isEmpty()) QDir(ncmOutputDir).removeRecursively();
        ncmOutputDir.clear();
        QMessageBox::warning(this, QStringLiteral("无法保存转换结果"),
                             QStringLiteral("无法创建音乐目录：%1").arg(QDir::toNativeSeparators(libraryDir)));
        return;
    }
    if (QDir(convertedDir).exists()) {
        QDirIterator iterator(convertedDir, audioFilters, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString sourcePath = iterator.next();
            const QFileInfo sourceInfo(sourcePath);
            const QString destinationPath = nextAvailableAudioPath(libraryDir, sourceInfo);
            if (QFile::copy(sourcePath, destinationPath)) {
                importedPaths.append(destinationPath);
                const QString lyricSource = ncmLyricsByBase.value(sourceInfo.completeBaseName().toCaseFolded());
                if (!lyricSource.isEmpty()) {
                    const QString lyricName = QFileInfo(destinationPath).completeBaseName() + QStringLiteral(".lrc");
                    const QString lyricDestination = QDir(libraryDir).filePath(lyricName);
                    if (!QFileInfo::exists(lyricDestination) && !QFile::copy(lyricSource, lyricDestination))
                        ++lyricCopyFailures;
                }
            } else {
                ++copyFailures;
            }
        }
    }
    ncmLyricsByBase.clear();
    if (!ncmOutputDir.isEmpty()) QDir(ncmOutputDir).removeRecursively();
    ncmOutputDir.clear();

    if (importedPaths.isEmpty()) {
        const QString diagnostic = errorOutput.isEmpty() ? standardOutput : errorOutput;
        const QString detail = diagnostic.isEmpty() ? QString() : QStringLiteral("\n\n") + diagnostic.left(1800);
        QMessageBox::warning(this, QStringLiteral("没有导入歌曲"),
                             QStringLiteral("转换器没有生成可播放的音频文件，请确认所选 NCM 文件有效。%1").arg(detail));
        return;
    }

    QList<QUrl> urls;
    for (const QString &path : importedPaths) urls.append(QUrl::fromLocalFile(path));
    musicList.addMusicsByUrl(urls);
    musicList.writeToDB();
    ui->stackedWidget->setCurrentIndex(4);
    ui->localPage->reFresh(musicList);

    QString summary = QStringLiteral("已转换并导入 %1 首歌曲。\n保存位置：%2")
            .arg(importedPaths.size()).arg(QDir::toNativeSeparators(libraryDir));
    const int conversionFailures = standardOutput.count(QStringLiteral("转换失败文件："));
    if (conversionFailures > 0)
        summary += QStringLiteral("\n转换器报告 %1 个文件转换失败。").arg(conversionFailures);
    if (copyFailures > 0)
        summary += QStringLiteral("\n有 %1 个文件复制失败。请检查磁盘空间或目标目录权限。").arg(copyFailures);
    if (lyricCopyFailures > 0)
        summary += QStringLiteral("\n有 %1 个同名歌词文件未能复制。").arg(lyricCopyFailures);
    QMessageBox::information(this, QStringLiteral("NCM 导入完成"), summary);
}

void QQMusic::onLrcWordClicked()
{
    const QRect pageRect = rect();
    const QRect hiddenRect(pageRect.x(), pageRect.y() + pageRect.height(),
                           pageRect.width(), pageRect.height());
    lrcPageAnimal->stop();
    lrcPage->setGeometry(hiddenRect);
    lrcPageAnimal->setStartValue(hiddenRect);
    lrcPageAnimal->setEndValue(pageRect);
    lrcPage->show();
    lrcPage->raise();
    lrcPageAnimal->start();
}

void QQMusic::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (lrcPage && lrcPageAnimal) {
        if (lrcPageAnimal->state() == QAbstractAnimation::Running)
            lrcPageAnimal->stop();
        lrcPage->setGeometry(rect());
    }
}


///////////////////////////////////////////////////////////////////////
// 播放控制说明

// 播放和暂停
void QQMusic::onPlayMusic()
{
    if(QMediaPlayer::PlayingState == player->state())
    {
        // 播放中点击，应该暂停
        player->pause();
        //ui->play->setIcon(QIcon(":/images/play3.png"));
    }
    else if(QMediaPlayer::PausedState == player->state())
    {
        // 暂停状态中点击按钮，应该继续播放
        player->play();
        //ui->play->setIcon(QIcon(":/images/play_on.png"));
    }
    else if(QMediaPlayer::StoppedState == player->state())
    {
        // The playlist can be populated before a track is selected. In that
        // case QMediaPlayer::play() has no current media and stays at 00:00.
        if (playList->isEmpty()) {
            QMessageBox::information(this, QStringLiteral("没有可播放的歌曲"),
                                     QStringLiteral("请先在“本地下载”页面导入电脑中的音乐文件。"));
            return;
        }
        if (playList->currentIndex() < 0) {
            // Rebuild the playlist from the visible page so currentPlaylistIds
            // and currentPage stay aligned with the selected media.
            playAllMusicOfCommonPage(currentPage ? currentPage : ui->localPage, 0, true);
            return;
        }
        player->play();
        //ui->play->setIcon(QIcon(":/images/play_on.png"));
    }
    else
    {
        qDebug()<<player->errorString();
    }
}

void QQMusic::onPlayUpClicked()
{
    if (playList->isEmpty()) return;
    playList->previous();
}

void QQMusic::onPlayDownClicked()
{
    if (playList->isEmpty()) return;
    playList->next();
}

void QQMusic::onPlaybackModeClicked()
{
    // 1. 根据当前播放模式，切换到下一个播放模式
    // 2. 设置ToolTip提示
    // 列表循环--->随机播放--->单曲循环
    // 当状态发生改变的时候，还需要修改按钮上的图标
    if(playList->playbackMode() == QMediaPlaylist::Loop)
    {
        // 随机播放
        playList->setPlaybackMode(QMediaPlaylist::Random);
        ui->playMode->setToolTip("随机播放");
    }
    else if(playList->playbackMode() == QMediaPlaylist::Random)
    {
        // 单曲循环
        playList->setPlaybackMode(QMediaPlaylist::CurrentItemInLoop);
        ui->playMode->setToolTip("单曲循环");
    }
    else if(playList->playbackMode() == QMediaPlaylist::CurrentItemInLoop)
    {
        playList->setPlaybackMode(QMediaPlaylist::Loop);
        ui->playMode->setToolTip("列表循环");
    }
    else
    {
        // 暂不支持
        qDebug()<<"暂不支持";
    }
}

void QQMusic::onPlayAll(PageType pageType)
{
    qDebug()<<"播放所有";
    // 播放page对应的CommonPage页面中的所有歌曲，默认从第0首开始播放
    // 通过pageType找到对应的CommonPage页面
    CommonPage* page = ui->localPage;
    switch(pageType)
    {
    case PageType::LIKE_PAGE:
        page = ui->likePage;
        break;
    case PageType::LOCAL_PAGE:
        page = ui->localPage;
        break;
    case PageType::HISTORY_PAGE:
        page = ui->recentPage;
        break;
    default:
        qDebug()<<"暂不支持的页面";
    }

    // page中记录的就是要播放的页面
    playAllMusicOfCommonPage(page, 0, true);
}

void QQMusic::playAllMusicOfCommonPage(CommonPage *page, int index, bool skipMissingAtStart)
{
    if (!page || index < 0) return;
    currentPage = page;
    updateBtformAnimal();

    // 清空之前playlist中的歌曲
    playList->clear();
    currentPlaylistIds.clear();

    int targetIndex = -1;
    int firstAvailableIndex = -1;
    int skippedMissing = 0;
    for (int i = 0; ; ++i) {
        const QString musicId = page->getMusicIdByIndex(i);
        if (musicId.isEmpty()) break;
        auto music = musicList.findMusicByMusicId(musicId);
        if (music == musicList.end() || !QFileInfo::exists(music->getMusicUrl().toLocalFile())) {
            ++skippedMissing;
            continue;
        }
        if (firstAvailableIndex < 0) firstAvailableIndex = playList->mediaCount();
        if (i == index) targetIndex = playList->mediaCount();
        playList->addMedia(music->getMusicUrl());
        currentPlaylistIds.append(musicId);
    }

    if (playList->isEmpty()) {
        ui->musicName->setText(QStringLiteral("暂无可播放歌曲"));
        ui->musicSinger->clear();
        ui->currentTime->setText(QStringLiteral("00:00"));
        ui->totalTime->setText(QStringLiteral("00:00"));
        totalTime = 0;
        return;
    }

    if (targetIndex < 0 && skipMissingAtStart)
        targetIndex = firstAvailableIndex;
    if (targetIndex < 0) {
        QMessageBox::information(this, QStringLiteral("歌曲文件不可用"),
                                 QStringLiteral("这首歌的本地文件不存在。请重新添加音频文件，或从播放器移除失效记录。"));
        return;
    }

    // 播放从目标歌曲开始
    playList->setCurrentIndex(targetIndex);

    if (skippedMissing > 0 && skipMissingAtStart)
        QMessageBox::information(this, QStringLiteral("已跳过失效歌曲"),
                                 QStringLiteral("播放列表中有 %1 首本地文件缺失，已跳过。")
                                     .arg(skippedMissing));

    player->play();
}

void QQMusic::playMusicByIndex(CommonPage *page, int index)
{
    playAllMusicOfCommonPage(page, index);
}

void QQMusic::setPlayerMuted(bool isMuted)
{
    player->setMuted(isMuted);
}

void QQMusic::setPlayerVolume(int volume)
{
    player->setVolume(volume);
}

void QQMusic::onPlayerStateChanged()
{
    if(player->state() == QMediaPlayer::PlayingState)
    {
        ui->play->setIcon(QIcon(":/images/play_on.png"));
    }
    else
    {
        ui->play->setIcon(QIcon(":/images/play3.png"));
    }
}

void QQMusic::onPlaybackModeChanged(QMediaPlaylist::PlaybackMode playbackMode)
{
    if(playbackMode == QMediaPlaylist::Loop)
    {
        ui->playMode->setIcon(QIcon(":/images/list_play.png"));
    }
    else if(playbackMode == QMediaPlaylist::Random)
    {
        ui->playMode->setIcon(QIcon(":/images/shuffle_2.png"));
    }
    else if(playbackMode == QMediaPlaylist::CurrentItemInLoop)
    {
        ui->playMode->setIcon(QIcon(":/images/single_play.png"));
    }
    else
    {
        qDebug()<<"暂不支持该模式";
    }
}

void QQMusic::onCurrentIndexChanged(int index)
{
    currentIndex = index;
    refreshCurrentCover(false);
    if (index < 0 || index >= currentPlaylistIds.size()) {
        lrcPage->setTrackInfo(QString(), QString());
        lrcPage->clearLyrics();
        return;
    }
    // 由于CommonPage页面中的歌曲和正在播放的播放列表中的歌曲的先后次序是相同的
    // 知道歌曲在playlist中的索引之后，直接到CommonPage中获取
    // 注意：playlist中的歌曲就是根据CommonPage中的musicOfPage加载
    QString musicId = currentPlaylistIds.at(index);

    // 本意是想要通过索引拿到歌曲，然后修改歌曲的isHistory属性
    auto it = musicList.findMusicByMusicId(musicId);
    if(it != musicList.end())
    {
        const QString musicName = it->getMusicName();
        const QString musicSinger = it->getMusicSinger();
        ui->musicName->setText(musicName);
        ui->musicSinger->setText(musicSinger);
        lrcPage->setTrackInfo(musicName, musicSinger);
        lrcPage->parseLrcFile(it->getLrcFilePath());
        it->setIsHistory(true);
        it->InsertMusicToDB();
    } else {
        lrcPage->setTrackInfo(QString(), QString());
        lrcPage->clearLyrics();
    }

    ui->recentPage->reFresh(musicList);
}

void QQMusic::onDurationChanged(qint64 duration)
{
    totalTime = duration;
    // duration为歌曲的总时长,单位为毫秒
    // 需要将整形的总时长转换为min:sec
    // 分：duration/1000/60;
    // 秒：duration/1000%60;
    ui->totalTime->setText(QString("%1:%2").arg(duration/1000/60, 2, 10, QChar('0'))
                           .arg(duration/1000%60, 2, 10, QChar('0')));
}

void QQMusic::onPositionChanged(qint64 position)
{
    // 更新当前播放时间
    ui->currentTime->setText(QString("%1:%2").arg(position/1000/60, 2, 10, QChar('0'))
                                             .arg(position/1000%60, 2, 10, QChar('0')));

    // 更新进度条的位置
    if (totalTime > 0)
        ui->progressBar->setStep(qBound(0.0f, position / static_cast<float>(totalTime), 1.0f));

    // 在歌词界面同步显示歌词
    if(currentIndex >= 0)
    {
        lrcPage->showLrcWordLine(position);
    }
}

void QQMusic::onMusicSliderChanged(float ration)
{
    if (totalTime <= 0) return;
    ration = qBound(0.0f, ration, 1.0f);
    // 根据进度条与总宽度的比率，更新当前播放时间
    qint64 duration = totalTime*ration;
    ui->currentTime->setText(QString("%1:%2").arg(duration/1000/60, 2, 10, QChar('0'))
                                             .arg(duration/1000%60, 2, 10, QChar('0')));

    // 时间修改了之后，播放时间也需要修改
    player->setPosition(duration);
}

void QQMusic::onMetaDataAvailableChanged(bool available)
{
    if (!available || currentIndex < 0 || !currentPage) return;
    // 歌曲名称、歌曲作者直接到Musci对象中获取
    // 此时需要知道媒体源在播放列表中的索引
    if (currentIndex < 0 || currentIndex >= currentPlaylistIds.size()) return;
    QString musicId = currentPlaylistIds.at(currentIndex);
    auto it = musicList.findMusicByMusicId(musicId);

    QString musicName("未知歌曲");
    QString musicSinger("歌手位置");
    if(it != musicList.end())
    {
        musicName = it->getMusicName();
        musicSinger = it->getMusicSinger();
    }

    ui->musicName->setText(musicName);
    ui->musicSinger->setText(musicSinger);
    lrcPage->setTrackInfo(musicName, musicSinger);
    refreshCurrentCover(true);
}

void QQMusic::refreshCurrentCover(bool preferEmbeddedCover)
{
    QPixmap cover;
    QString currentAudioPath;
    if (currentIndex >= 0 && currentIndex < currentPlaylistIds.size()) {
        const auto music = musicList.findMusicByMusicId(currentPlaylistIds.at(currentIndex));
        if (music != musicList.end())
            currentAudioPath = music->getMusicUrl().toLocalFile();
    }

    // Qt 5's Windows multimedia backend often omits MP3 APIC artwork from
    // QMediaPlayer metadata. Read the embedded ID3 picture directly so each
    // local song keeps its own artwork.
    if (!currentAudioPath.isEmpty())
        cover = readMp3EmbeddedCover(currentAudioPath);

    if (cover.isNull() && preferEmbeddedCover) {
        const QStringList metadataKeys = QStringList()
                << QStringLiteral("CoverArtImage")
                << QStringLiteral("ThumbnailImage")
                << QStringLiteral("PosterImage");
        for (const QString &key : metadataKeys) {
            const QVariant value = player->metaData(key);
            if (!value.isValid()) continue;
            if (value.canConvert<QImage>()) {
                const QImage image = value.value<QImage>();
                if (!image.isNull()) {
                    cover = QPixmap::fromImage(image);
                    break;
                }
            } else if (value.canConvert<QPixmap>()) {
                const QPixmap image = value.value<QPixmap>();
                if (!image.isNull()) {
                    cover = image;
                    break;
                }
            }
        }
    }

    // Some audio files keep their cover beside the audio instead of embedding
    // it in the tags. Check common per-track and album-folder image names.
    if (cover.isNull() && !currentAudioPath.isEmpty()) {
        const QFileInfo audioFile(currentAudioPath);
        const QString folder = audioFile.absolutePath();
        const QString base = audioFile.completeBaseName();
        QStringList candidates;
        const QStringList extensions = QStringList() << QStringLiteral("jpg")
                << QStringLiteral("jpeg") << QStringLiteral("png")
                << QStringLiteral("webp") << QStringLiteral("bmp");
        for (const QString &extension : extensions)
            candidates.append(folder + QLatin1Char('/') + base + QLatin1Char('.') + extension);
        candidates << folder + QStringLiteral("/cover.jpg")
                   << folder + QStringLiteral("/folder.jpg")
                   << folder + QStringLiteral("/Cover.jpg")
                   << folder + QStringLiteral("/AlbumArt.jpg");
        for (const QString &path : candidates) {
            QPixmap image(path);
            if (!image.isNull()) {
                cover = image;
                break;
            }
        }
    }

    if (cover.isNull())
        cover = QPixmap(QStringLiteral(":/images/rec/001.png"));

    ui->musicCover->setPixmap(cover);
    ui->musicCover->setScaledContents(true);
    if (lrcPage)
        lrcPage->setBackgroundCover(cover);
    if (currentPage)
        currentPage->setMusicImage(cover);
}



void QQMusic::on_skin_clicked()
{
    // 换肤
    // 类似：给窗口更新背景颜色 或者 更换背景图片
    QMessageBox::information(this, "温馨提示", "换肤功能小哥哥正在紧急支持中...");
}

void QQMusic::on_min_clicked()
{
    showMinimized();
}

