#include "listitembox.h"
#include "ui_listitembox.h"
#include <QDebug>
#include <QSizePolicy>


ListItemBox::ListItemBox(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ListItemBox),
    isLike(false)
{
    ui->setupUi(this);

    // This spacer had a fixed 210 px hint inside a 380 px song-name column,
    // leaving too little room for the title and pushing text into the singer
    // column. Let the title label use the remaining width instead.
    if (ui->musicNameBox->layout() && ui->musicNameBox->layout()->count() > 0) {
        QLayoutItem *spacer = ui->musicNameBox->layout()->itemAt(
            ui->musicNameBox->layout()->count() - 1);
        if (spacer && spacer->spacerItem()) {
            spacer->spacerItem()->changeSize(0, 0, QSizePolicy::Fixed,
                                             QSizePolicy::Fixed);
            ui->musicNameBox->layout()->invalidate();
        }
    }
    ui->musicNameLabel->setMinimumWidth(0);
    ui->musicNameLabel->setWordWrap(false);

    connect(ui->likeBtn, &QPushButton::clicked, this, &ListItemBox::onLikeBtnClicked);
}

ListItemBox::~ListItemBox()
{
    qDebug()<<"ListItemBox被销毁";
    delete ui;
}

void ListItemBox::enterEvent(QEvent *event)
{
    (void)event;
    setStyleSheet("background-color:#EFEFEF");
}

void ListItemBox::leaveEvent(QEvent *event)
{
    (void)event;
    setStyleSheet("");
}

void ListItemBox::setMusicName(const QString &musicName)
{
    ui->musicNameLabel->setText(musicName);
    ui->musicNameLabel->setToolTip(musicName);
}

void ListItemBox::setMusicSinger(const QString &musicSinger)
{
    ui->musicSingerLabel->setText(musicSinger);
}

void ListItemBox::setMusicAlbum(const QString &musicAlbum)
{
    ui->musicAlbumLabel->setText(musicAlbum);
}

void ListItemBox::setLikeMusic(bool isLike)
{
    this->isLike = isLike;
    if(isLike)
    {
        ui->likeBtn->setIcon(QIcon(":/images/like_2.png"));
    }
    else
    {
        ui->likeBtn->setIcon(QIcon(":/images/like_3.png"));
    }
}

void ListItemBox::onLikeBtnClicked()
{
    isLike = !isLike;
    setLikeMusic(isLike);

    // 现在收藏或不收藏的状态改变,应该通知修改Music对象中的isLike属性
    emit setIsLike(isLike);
}



