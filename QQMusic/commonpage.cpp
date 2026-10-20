#include "commonpage.h"
#include "ui_commonpage.h"

#include "listitembox.h"
#include <QDebug>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QColor>
#include <QMenu>


CommonPage::CommonPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CommonPage)
{
    ui->setupUi(this);
    ui->pageMusicList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->musicPlayBox->setMinimumHeight(78);
    ui->musicPlayBox->setMaximumHeight(78);
    ui->musicImageLabel->setMinimumSize(60, 60);
    ui->musicImageLabel->setMaximumSize(60, 60);
    ui->musicImageLabel->setStyleSheet("border-radius:8px; background:#e9f1ec; color:#5b9c76;");
    if (ui->playAll->layout() && ui->playAll->layout()->count() > 0) {
        QLayoutItem* spacer = ui->playAll->layout()->itemAt(0);
        if (spacer && spacer->spacerItem()) {
            spacer->spacerItem()->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
            ui->playAll->layout()->invalidate();
        }
    }
    ui->playAll->setMinimumWidth(130);
    ui->playAll->setMaximumWidth(130);
    ui->playAllBtn->setText(QStringLiteral("▶  播放全部"));
    ui->playAllBtn->setMinimumSize(116, 36);
    ui->playAllBtn->setMaximumSize(116, 36);
    ui->playAllBtn->setStyleSheet("QPushButton { background:#5b9c76; color:white; border:0; border-radius:18px; font-weight:600; } QPushButton:hover { background:#4d8b68; }");
    ui->listLabelBox->setStyleSheet("QWidget#listLabelBox { background:#f7f8f7; border-bottom:1px solid #e9edea; } QLabel { color:#89928d; font-size:11px; }");
    ui->pageMusicList->setStyleSheet(
        "QListWidget { background:#ffffff; border:0; outline:0; color:#36423b; }"
        "QListWidget::item { border-bottom:1px solid #f3f5f3; padding:4px; }"
        "QListWidget::item:hover { background:#f6f8f6; }"
        "QListWidget::item:selected { background:#edf4ef; color:#36423b; }"
    );
    ui->PageTittle->setStyleSheet("font-size:25px; font-weight:600; color:#28332e; padding:4px 0;");

    connect(ui->playAllBtn, &QPushButton::clicked, this, [=](){
        // 播放所有按钮点击之后，通知QQMusic播放当前pageType标记的page页面中的所有歌曲
        emit playAll(pageType);
    });

    connect(ui->pageMusicList, &QListWidget::doubleClicked, this, [=](const QModelIndex &index){
        // 鼠标双击后，发射信号告诉QQMusic，博能放this页面中共被双击的歌曲
        emit playMusicByIndex(this, index.row());
    });
    ui->pageMusicList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->pageMusicList, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint &position) {
        QListWidgetItem *item = ui->pageMusicList->itemAt(position);
        if (!item) return;
        const QString musicId = getMusicIdByIndex(ui->pageMusicList->row(item));
        if (musicId.isEmpty()) return;

        QMenu menu(this);
        QAction *removeAction = menu.addAction(QStringLiteral("从播放器移除"));
        removeAction->setToolTip(QStringLiteral("仅从播放器曲库移除，保留磁盘上的音频文件"));
        if (menu.exec(ui->pageMusicList->mapToGlobal(position)) == removeAction)
            emit removeMusicRequested(musicId);
    });
}

CommonPage::~CommonPage()
{
    delete ui;
}

void CommonPage::setMusicListType(PageType pageType)
{
    this->pageType = pageType;
}

void CommonPage::setSearchText(const QString &text)
{
    searchText = text.trimmed();
}

void CommonPage::setCommonPageUI(const QString &text, const QString &imagePath)
{
    ui->PageTittle->setText(text);

    ui->musicImageLabel->setPixmap(QPixmap(imagePath));
    ui->musicImageLabel->setScaledContents(true);
}

void CommonPage::addMusicToMusicPage(MusicList &musicList)
{
    // 1. 清楚之前的歌曲内容
    // 注意：清楚musicOfPage和界面显示无关
    musicOfPage.clear();

    for(auto music : musicList)
    {
        bool include = false;
        switch(pageType)
        {
        case LIKE_PAGE:
            include = music.getIsLike();
            break;
        case LOCAL_PAGE:
            include = true;
            break;
        case HISTORY_PAGE:
            include = music.getIsHistory();
            break;
        default:
            qDebug()<<"暂未支持";
        }
        if (include && !searchText.isEmpty()) {
            const QString searchable = music.getMusicName() + QLatin1Char(' ') +
                    music.getMusicSinger() + QLatin1Char(' ') + music.getMusicAlbum();
            if (!searchable.contains(searchText, Qt::CaseInsensitive))
                include = false;
        }
        if (include)
            musicOfPage.push_back(music.getMusicId());
    }
}

// 该方法负责将歌曲信息更新到界面
void CommonPage::reFresh(MusicList &musicList)
{
    // 将QListWidget之前已经添加的内容清空
    ui->pageMusicList->clear();

    // 添加新的歌曲
    addMusicToMusicPage(musicList);

    for(auto musicId : musicOfPage)
    {
        auto it = musicList.findMusicByMusicId(musicId);
        if(it == musicList.end())
            continue;

        // 将Music的歌曲名称、作者、专辑名称更新到界面
        // 测试
        // 将ListBoxItem对象放置pageMusicList
        ListItemBox* listItemBox = new ListItemBox(this);
        listItemBox->setFixedHeight(52);
        listItemBox->setStyleSheet("QWidget { background:transparent; } QLabel { color:#3d4943; }");
        // 设置歌曲名称、作者、专辑名称更新到界面
        listItemBox->setMusicName(it->getMusicName());
        listItemBox->setMusicSinger(it->getMusicSinger());
        listItemBox->setMusicAlbum(it->getMusicAlbum());
        listItemBox->setLikeMusic(it->getIsLike());

        QListWidgetItem* item = new QListWidgetItem(ui->pageMusicList);
        item->setSizeHint(QSize(ui->pageMusicList->viewport()->width(), listItemBox->height()));
        ui->pageMusicList->setItemWidget(item, listItemBox);

        const QString itemMusicId = it->getMusicId();
        connect(listItemBox, &ListItemBox::setIsLike, this, [=](bool isLike){
            // 更新歌曲状态
            // 需要通知QQMusic，让其中的likePage、localPage、recentPage更新页面歌曲信息
            emit updataLikeMusic(isLike, itemMusicId);
        });
    }

    if(musicOfPage.isEmpty())
    {
        QListWidgetItem* emptyItem = new QListWidgetItem(
            pageType == HISTORY_PAGE
                ? QStringLiteral("还没有播放记录\n导入本地音乐并播放后，歌曲会显示在这里")
                : QStringLiteral("这里还没有歌曲\n点击左侧“本地下载”添加音乐文件")
        );
        emptyItem->setTextAlignment(Qt::AlignCenter);
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setForeground(QColor("#89928d"));
        ui->pageMusicList->addItem(emptyItem);
    }

    // 触发窗口重绘paintEvent
    // update();   // updata()将paintEvent事件放在事件循环队列中，没有立马处理
    repaint();     // 立马响应paintEvent事件
}

void CommonPage::addMusicToPlaylist(MusicList &musicList, QMediaPlaylist *playList)
{
    for (const QString &musicId : musicOfPage)
    {
        auto it = musicList.findMusicByMusicId(musicId);
        if (it != musicList.end())
            playList->addMedia(it->getMusicUrl());
    }
}

void CommonPage::setMusicImage(QPixmap pixmap)
{
    ui->musicImageLabel->setPixmap(pixmap);
    ui->musicImageLabel->setScaledContents(true);
}

QString CommonPage::getMusicIdByIndex(int index)
{
    if(index < 0 || index >= musicOfPage.size())
    {
        qDebug()<<"无此歌曲";
        return "";
    }
    return musicOfPage[index];
}
