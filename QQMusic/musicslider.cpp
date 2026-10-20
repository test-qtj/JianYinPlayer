#include "musicslider.h"
#include "ui_musicslider.h"

#include <QMouseEvent>
#include <QDebug>


MusicSlider::MusicSlider(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MusicSlider)
{
    ui->setupUi(this);
    maxWidth = this->width();

    currentPos = 0;
    moveSlider();
}

MusicSlider::~MusicSlider()
{
    delete ui;
}

void MusicSlider::setStep(float ratio)
{
    currentPos = maxWidth*ratio;
    moveSlider();
}

void MusicSlider::mousePressEvent(QMouseEvent *event)
{
    currentPos = event->pos().x();
    moveSlider();
}

void MusicSlider::mouseMoveEvent(QMouseEvent *event)
{
    // 注意：鼠标移动的时候，event->pos()必须在MusciSlider窗口内
    //      才能支持拖拽功能
    // 检测：event->pos()是否在MusicSlider的窗口矩形范围内
    QRect musicSliderRect = QRect(0, 0, geometry().width(), geometry().height());
    if(!musicSliderRect.contains(event->pos()))
    {
         qDebug()<<musicSliderRect<<"-"<<event->pos();
        return;
    }


    if(event->buttons() == Qt::LeftButton)
    {
        currentPos = event->pos().x();
        if(currentPos < 0)
        {
            currentPos = 0;
        }

        if(currentPos > maxWidth)
        {
            currentPos = maxWidth;
        }

        moveSlider();
    }
}

void MusicSlider::mouseReleaseEvent(QMouseEvent *event)
{
    currentPos = event->pos().x();
    moveSlider();

    emit setMusicSliderPositon(ui->outLine->width()/(float)maxWidth);
}

void MusicSlider::moveSlider()
{
    ui->outLine->setGeometry(ui->outLine->x(), ui->outLine->y(), currentPos, ui->outLine->height());
}
