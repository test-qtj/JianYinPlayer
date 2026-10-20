#include "btform.h"
#include "ui_btform.h"



BtForm::BtForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BtForm)
{
    ui->setupUi(this);

    ui->lineBox->hide();

    // 设置line1的动画效果
    line1Animal = new QPropertyAnimation(ui->line1, "geometry", this);
    line1Animal->setDuration(1500);
    line1Animal->setKeyValueAt(0, QRect(0, 15, 2, 0));
    line1Animal->setKeyValueAt(0.5, QRect(0, 0, 2, 15));
    line1Animal->setKeyValueAt(1, QRect(0, 15, 2, 0));
    line1Animal->setLoopCount(-1);
    line1Animal->start();

    // 设置line2的动画效果
    line2Animal = new QPropertyAnimation(ui->line2, "geometry", this);
    line2Animal->setDuration(1600);
    line2Animal->setKeyValueAt(0, QRect(7, 15, 2, 0));
    line2Animal->setKeyValueAt(0.5, QRect(7, 0, 2, 15));
    line2Animal->setKeyValueAt(1, QRect(7, 15, 2, 0));
    line2Animal->setLoopCount(-1);
    line2Animal->start();

    // 设置line3的动画效果
    line3Animal = new QPropertyAnimation(ui->line3, "geometry", this);
    line3Animal->setDuration(1700);
    line3Animal->setKeyValueAt(0, QRect(14, 15, 2, 0));
    line3Animal->setKeyValueAt(0.5, QRect(14, 0, 2, 15));
    line3Animal->setKeyValueAt(1, QRect(14, 15, 2, 0));
    line3Animal->setLoopCount(-1);
    line3Animal->start();

    // 设置line4的动画效果
    line4Animal = new QPropertyAnimation(ui->line4, "geometry", this);
    line4Animal->setDuration(1800);
    line4Animal->setKeyValueAt(0, QRect(21, 15, 2, 0));
    line4Animal->setKeyValueAt(0.5, QRect(21, 0, 2, 15));
    line4Animal->setKeyValueAt(1, QRect(21, 15, 2, 0));
    line4Animal->setLoopCount(-1);
    line4Animal->start();
}

BtForm::~BtForm()
{
    delete ui;
}

void BtForm::setIconAndText(const QString &btIcon, const QString &btText, int pageId)
{
    // 设置按钮的图标
    ui->btIcon->setPixmap(QPixmap(btIcon));

    // 设置按钮文本
    ui->btText->setText(btText);

    // 将按钮和QQMusic中的Page页面进行关联
    this->pageId = pageId;
}

int BtForm::getPageId() const
{
    return pageId;
}

void BtForm::clearBackground()
{
    ui->btStyle->setStyleSheet("#btStyle { background:transparent; border-radius:8px; } #btStyle:hover { background:#e8e9eb; } QLabel#btText { color:#55585e; font-weight:400; }");
}

void BtForm::showAnimal(bool isShow)
{
    if(isShow)
    {
        ui->lineBox->show();
    }
    else
    {
        ui->lineBox->hide();
    }
}

void BtForm::mousePressEvent(QMouseEvent *event)
{
    (void)event;
    // 1. 当按钮按下之后，需要修改其背景颜色
    ui->btStyle->setStyleSheet("#btStyle { background:#e6f6ed; border-radius:8px; } QLabel#btText { color:#239b61; font-weight:600; }");

    // 2. 当按钮按钮之后，需要切换bodyRight中的Page页面
    // 问题一：什么时机切换
    // 问题二：由谁完成页面切换
    // 问题三：按钮和Page页面的对应关系？

    emit btClick(pageId);
}
