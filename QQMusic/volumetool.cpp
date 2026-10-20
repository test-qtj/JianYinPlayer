#include "volumetool.h"
#include "ui_volumetool.h"

#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QDebug>

VolumeTool::VolumeTool(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::VolumeTool),
    isMuted(false),
    volumeRatio(20)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint|Qt::NoDropShadowWindowHint);

    setAttribute(Qt::WA_TranslucentBackground);

    // 窗口增加自定义的阴影效果
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setColor("#646464");
    shadowEffect->setBlurRadius(10);
    shadowEffect->setOffset(0, 0);
    this->setGraphicsEffect(shadowEffect);

    // 设置按钮图标
    ui->silenceBtn->setIcon(QIcon(":/images/volumn.png"));

    // 将默认的音量设置20%
    ui->volumeRatio->setText("20%");

    // 设置out_line尺寸
    QRect rect = ui->outLine->geometry();
    // 窗体原本高度180*20%, 25rect具体父元素上方的距离
    ui->outLine->setGeometry(rect.x(), 180 - 36 + 25, rect.width(), 36);

    // 移动按钮位置
    ui->silderBtn->move(ui->silderBtn->x(), ui->outLine->y() - ui->silderBtn->height()/2);

    connect(ui->silenceBtn, &QPushButton::clicked, this, &VolumeTool::onSilenceBtnClicked);

    ui->volumeBox->installEventFilter(this);
}

VolumeTool::~VolumeTool()
{
    delete ui;
}

void VolumeTool::paintEvent(QPaintEvent *event)
{
    (void)event;
    // 绘制volumeTool界面下的三角
    QPainter painter(this);

    // 1. 设置画笔
    painter.setPen(Qt::NoPen);

    // 2. 设置画刷
    painter.setBrush(QBrush(Qt::white));


    // 3. 绘制三角形
    QPolygon polygon;
    QPoint a(10+10, 300);
    QPoint b(10+10+60, 300);
    QPoint c(10+10+30, 300+20);
    polygon.append(a);
    polygon.append(b);
    polygon.append(c);

    painter.drawPolygon(polygon);

}

bool VolumeTool::eventFilter(QObject *watched, QEvent *event)
{
    if(ui->volumeBox == watched)
    {
        // 事件发生在volumeBox控件上
        if(event->type() == QEvent::MouseButtonPress)
        {
            // 鼠标按下事件
            calcVolume();
        }
        else if(event->type() == QEvent::MouseButtonRelease)
        {
            // 鼠标释放事件
            emit setMusicVolume(volumeRatio);
        }
        else if(event->type() == QEvent::MouseMove)
        {
            // 鼠标移动事件
            calcVolume();
            emit setMusicVolume(volumeRatio);
        }

        return true;
    }

    return QObject::eventFilter(watched, event);
}

void VolumeTool::calcVolume()
{
    // 获取鼠标点击时的y坐标
    int height = ui->volumeBox->mapFromGlobal(QCursor().pos()).y();

    // 验证height的合法性：[25, 205]
    height = height < 25? 25:height;
    height = height > 205? 205 : height;

    // 更新outLine大小
    ui->outLine->setGeometry(ui->outLine->x(), height, ui->outLine->width(), 205-height);

    // 更新sliderBtn的位置
    // sliderBtn实际矩形，设置了圆角效果之后，呈现出来是圆
    // 移动siliderBtn实际就是重新找其矩形左上角的位置，左上角位置就是outLine左上角y-siliderBtn的高度即可
    // silderBtn的圆心刚好和ui->outLine的上边界平齐
    ui->silderBtn->move(ui->silderBtn->x(), ui->outLine->y() - ui->silderBtn->height()/2);

    // 计算音量大小
    volumeRatio = (int)(ui->outLine->height()/(float)180*100);

    // 更新音量百分比
    ui->volumeRatio->setText(QString::number(volumeRatio)+"%");
}

void VolumeTool::onSilenceBtnClicked()
{
    isMuted = !isMuted;
    // 静音图标切换
    if(isMuted)
    {
        // true：表示静音
        ui->silenceBtn->setIcon(QIcon(":/images/silent.png"));
    }
    else
    {
        // false：表示非静音
        ui->silenceBtn->setIcon(QIcon(":/images/volumn.png"));
    }


    // 媒体静音设置，发射信号让QQMusic处理
    emit setMusicMuted(isMuted);
}



