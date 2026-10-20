#include "recboxitem.h"
#include "ui_recboxitem.h"
#include <QDebug>


#include <QPropertyAnimation>


RecBoxItem::RecBoxItem(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RecBoxItem)
{
    ui->setupUi(this);

    ui->musicImageBox->installEventFilter(this);
}

RecBoxItem::~RecBoxItem()
{
    delete ui;
}

void RecBoxItem::setRecText(const QString &text)
{
    ui->recBoxItemText->setText(text);
}

void RecBoxItem::setRecImage(const QString &imagePath)
{
    QString style = "background-image:url("+imagePath+");";
    ui->recMusicImage->setStyleSheet(style);
}

bool RecBoxItem::eventFilter(QObject *watched, QEvent *event)
{
    if(watched == ui->musicImageBox)
    {
        if(QEvent::Enter == event->type())
        {
            // 添加图片上移动画
            QPropertyAnimation* animation = new QPropertyAnimation(ui->musicImageBox, "geometry");
            animation->setDuration(150);
            animation->setStartValue(QRect(9,9, ui->musicImageBox->width(), ui->musicImageBox->height()));
            animation->setEndValue(QRect(9, 0, ui->musicImageBox->width(), ui->musicImageBox->height()));
            animation->start();

            connect(animation, &QPropertyAnimation::finished, this, [=](){
                delete animation;
            });
        }
        else if(QEvent::Leave == event->type())
        {
            // 添加图标下移动画
            QPropertyAnimation* animation = new QPropertyAnimation(ui->musicImageBox, "geometry");
            animation->setDuration(150);
            animation->setStartValue(QRect(9,0, ui->musicImageBox->width(), ui->musicImageBox->height()));
            animation->setEndValue(QRect(9, 9, ui->musicImageBox->width(), ui->musicImageBox->height()));
            animation->start();

            connect(animation, &QPropertyAnimation::finished, this, [=](){
                delete animation;
            });
        }

        return true;
    }

    return QObject::eventFilter(watched, event);
}
