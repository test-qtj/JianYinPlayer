#include "recbox.h"
#include "ui_recbox.h"

#include "recboxitem.h"
#include <QJsonObject>

RecBox::RecBox(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RecBox),
    row(1),
    col(4)
{
    ui->setupUi(this);
}

RecBox::~RecBox()
{
    delete ui;
}

void RecBox::initRecBoxUi(QJsonArray data, int row)
{
    if(2 == row)
    {
        this->row = row;
        col = 8;  // ???
    }
    else
    {
        ui->recListDown->hide();
    }

    imageList = data;

    currentIndex = 0;

    count = imageList.size()/col;

    // 在RecBox控件添加RecBoxItem
    createRecBoxItem();
}

void RecBox::createRecBoxItem()
{
    // 删除RecBox内部之前的元素
    QList<RecBoxItem*> recUpList = ui->recListUp->findChildren<RecBoxItem*>();
    for(auto e : recUpList)
    {
        ui->recListUpHLayout->removeWidget(e);
        delete e;
    }

    QList<RecBoxItem*> recDownList = ui->recListDown->findChildren<RecBoxItem*>();
    for(auto e : recDownList)
    {
        ui->recListDownHLayout->removeWidget(e);
        delete e;
    }

    // 创建RecBoxItem对象，往RecBox中添加
    // 今日为你推荐：row=1  col=4
    // 音乐补给站：row=2 col=8
    int index = 0;
    for(int i = currentIndex*col; i < col + col*currentIndex; ++i)
    {
        RecBoxItem* item = new RecBoxItem();

        // 设置音乐图片与对应文本
        QJsonObject obj = imageList[i].toObject();
        item->setRecText(obj.value("text").toString());
        item->setRecImage(obj.value("path").toString());

        if(index >= col/2 && 2 == row)
        {
            ui->recListDownHLayout->addWidget(item);
        }
        else
        {
            ui->recListUpHLayout->addWidget(item);
        }

        index++;
    }
}

void RecBox::on_btDown_clicked()
{
    // 点击之后，需要显示下一组图片，如果已经是最后一组图片，显示第0组
    currentIndex++;
    if(currentIndex >= count)
    {
        currentIndex = 0;
    }

    createRecBoxItem();
}

void RecBox::on_btUp_clicked()
{
    // 点击之后，需要显示前一组图片，如果已经是第0组图片，让其显示最后一组
    currentIndex--;
    if(currentIndex < 0)
    {
        currentIndex = count - 1;
    }

    createRecBoxItem();
}
