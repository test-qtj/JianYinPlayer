#include "lrcpage.h"
#include "ui_lrcpage.h"

#include <QPushButton>
#include <QFile>
#include <QDebug>
#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QFont>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QRegularExpression>
#include <QTextCodec>
#include <QVBoxLayout>
#include <algorithm>

LrcPage::LrcPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LrcPage),
    coverBackground(nullptr)
{
    ui->setupUi(this);

    // Keep the cover behind the lyric controls and text while allowing the
    // song image to fill the entire lyrics page.
    ui->bgStyle->setStyleSheet("#bgStyle { background-color: transparent; } * { color: #FFFFFF; background-color: transparent; }");
    ui->lrcTop->setFixedHeight(132);
    if (QHBoxLayout *topLayout = qobject_cast<QHBoxLayout *>(ui->lrcTop->layout())) {
        topLayout->addSpacing(76);
        topLayout->setAlignment(ui->hideBtn, Qt::AlignLeft | Qt::AlignTop);
    }
    ui->titleBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    if (QVBoxLayout *titleLayout = qobject_cast<QVBoxLayout *>(ui->titleBox->layout())) {
        titleLayout->setAlignment(Qt::AlignCenter);
        titleLayout->setSpacing(8);
    }
    QFont artistFont = ui->musicSinger->font();
    artistFont.setPointSize(18);
    artistFont.setWeight(QFont::DemiBold);
    ui->musicSinger->setFont(artistFont);
    QFont titleFont = ui->musicName->font();
    titleFont.setPointSize(16);
    ui->musicName->setFont(titleFont);

    coverBackground = new QLabel(ui->bgStyle);
    coverBackground->setObjectName(QStringLiteral("lyricsCoverBackground"));
    coverBackground->setAttribute(Qt::WA_TransparentForMouseEvents);
    coverBackground->setScaledContents(false);
    coverBackground->lower();
    ui->bgStyle->installEventFilter(this);

    animal = new QPropertyAnimation(this, "geometry", this);
    animal->setDuration(500);

    // 下拉按钮关联clicked
    connect(ui->hideBtn, &QPushButton::clicked, this, [=]{
        // This page is a child overlay of QQMusic. Animate in parent
        // coordinates and use its current geometry so resizing the player
        // never makes the overlay shrink or drift while closing.
        const QRect currentGeometry = geometry();
        animal->stop();
        animal->setStartValue(currentGeometry);
        animal->setEndValue(currentGeometry.translated(0, currentGeometry.height()));
        animal->start();
    });

    // 当动画结束之后，窗口需要隐藏起来
    connect(animal, &QPropertyAnimation::finished, this, [=]{
       hide();
    });

    ui->hideBtn->setFixedSize(76, 36);
    ui->hideBtn->setText(QStringLiteral("⌄  收起"));
    ui->hideBtn->setStyleSheet("QPushButton { color:white; background:rgba(0,0,0,90); border:1px solid rgba(255,255,255,55); border-radius:18px; padding:4px 10px; } QPushButton:hover { background:rgba(0,0,0,145); }");

    showLrcWordLine(-1);
}

LrcPage::~LrcPage()
{
    delete ui;
}

void LrcPage::setBackgroundCover(const QPixmap &cover)
{
    backgroundCover = cover;
    updateBackgroundCover();
}

bool LrcPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->bgStyle && event->type() == QEvent::Resize)
        updateBackgroundCover();
    return QWidget::eventFilter(watched, event);
}

void LrcPage::updateBackgroundCover()
{
    if (!coverBackground)
        return;

    const QSize targetSize = ui->bgStyle->size();
    coverBackground->setGeometry(ui->bgStyle->rect());
    if (backgroundCover.isNull() || targetSize.isEmpty()) {
        coverBackground->clear();
        return;
    }

    const QPixmap scaled = backgroundCover.scaled(targetSize, Qt::KeepAspectRatioByExpanding,
                                                   Qt::SmoothTransformation);
    const int x = qMax(0, (scaled.width() - targetSize.width()) / 2);
    const int y = qMax(0, (scaled.height() - targetSize.height()) / 2);
    QPixmap pageCover = scaled.copy(x, y, targetSize.width(), targetSize.height());

    // Darken the artwork so white lyrics remain easy to read across bright covers.
    QPainter painter(&pageCover);
    painter.fillRect(pageCover.rect(), QColor(0, 0, 0, 112));
    painter.end();

    coverBackground->setPixmap(pageCover);
    coverBackground->lower();
}

void LrcPage::setTrackInfo(const QString &title, const QString &artist)
{
    ui->musicName->setText(title.isEmpty() ? QStringLiteral("未知歌曲") : title);
    ui->musicSinger->setText(artist.isEmpty() ? QStringLiteral("未知歌手") : artist);
    ui->musicName->setToolTip(title);
    ui->musicSinger->setToolTip(artist);
}

void LrcPage::clearLyrics()
{
    lrcWordLines.clear();
    showLrcWordLine(-1);
}

bool LrcPage::parseLrcFile(const QString &lrcFilePath)
{
    // Clear the previous track first so a missing or invalid LRC never leaves
    // the old song's lyrics on screen.
    clearLyrics();

    QFile file(lrcFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "打开lrc文件：" << lrcFilePath << " 失败!!!";
        showLrcWordLine(-1);
        return false;
    }

    QByteArray data = file.readAll();
    QString content;
    if (data.startsWith("\xFF\xFE")) {
        content = QTextCodec::codecForName("UTF-16LE")->toUnicode(data.mid(2));
    } else if (data.startsWith("\xFE\xFF")) {
        content = QTextCodec::codecForName("UTF-16BE")->toUnicode(data.mid(2));
    } else {
        if (data.startsWith("\xEF\xBB\xBF")) data.remove(0, 3);
        content = QString::fromUtf8(data);
    }

    // The downloaded NetEase LRC files can begin with JSON credit lines.
    // Only accept actual [mm:ss.xx] lyric timestamps and skip those headers.
    const QRegularExpression timestampPattern(
        QStringLiteral("\\[(\\d{1,3}):(\\d{1,2})(?:[\\.:](\\d{1,3}))?\\]"));
    const QStringList lines = content.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                            Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QRegularExpressionMatchIterator matches = timestampPattern.globalMatch(line);
        if (!matches.hasNext()) continue;

        QVector<qint64> timestamps;
        int lyricStart = 0;
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            const qint64 minutes = match.captured(1).toLongLong();
            const qint64 seconds = match.captured(2).toLongLong();
            QString fraction = match.captured(3);
            while (fraction.size() < 3) fraction.append(QLatin1Char('0'));
            const qint64 milliseconds = fraction.left(3).toLongLong();
            timestamps.append((minutes * 60 + seconds) * 1000 + milliseconds);
            lyricStart = match.capturedEnd();
        }

        const QString lyric = line.mid(lyricStart).trimmed();
        if (lyric.isEmpty()) continue;
        for (qint64 timestamp : timestamps)
            lrcWordLines.append(LrcWordLine(timestamp, lyric));
    }

    std::stable_sort(lrcWordLines.begin(), lrcWordLines.end(),
                     [](const LrcWordLine &left, const LrcWordLine &right) {
        return left.lrcTime < right.lrcTime;
    });

    showLrcWordLine(0);
    return !lrcWordLines.isEmpty();
}

void LrcPage::showLrcWordLine(qint64 time)
{
    // 1. 根据当前所唱歌曲的时间来获取当前所唱歌曲在QVector中的索引
    int index = getLrcWordLineIndex(time);

    // 2. 更新前三行 当前播放行 后三行到界面
    if(-1 == index)
    {
        ui->line1->setText("");
        ui->line2->setText("");
        ui->line3->setText("");
        ui->lineCenter->setText("当前歌曲暂无歌词");
        ui->line4->setText("");
        ui->line5->setText("");
        ui->line6->setText("");
    }
    else
    {
        ui->line1->setText(getLrcWordByIndex(index-3));
        ui->line2->setText(getLrcWordByIndex(index-2));
        ui->line3->setText(getLrcWordByIndex(index-1));
        ui->lineCenter->setText(getLrcWordByIndex(index));
        ui->line4->setText(getLrcWordByIndex(index+1));
        ui->line5->setText(getLrcWordByIndex(index+2));
        ui->line6->setText(getLrcWordByIndex(index+3));
    }
}

int LrcPage::getLrcWordLineIndex(qint64 time)
{
    // 将time和QVector中保存的LrcWordLine中的time进行对比
    // 当前歌曲没有lrc歌词文件
    if(lrcWordLines.isEmpty()){
        return -1;
    }

    if(time <= lrcWordLines[0].lrcTime){
        return 0;
    }

    for(int i = 1; i < lrcWordLines.size(); ++i)
    {
        if(time >= lrcWordLines[i-1].lrcTime && time < lrcWordLines[i].lrcTime)
        {
            // 第i-1行还没有播放完
            return i-1;
        }
    }

    // 最后一行唱完之后，歌曲结束了，但是还有收尾音乐
    // 让歌词界面显示最后一行歌词
    return lrcWordLines.size()-1;
}

QString LrcPage::getLrcWordByIndex(int index)
{
    if(index < 0 || index >= lrcWordLines.size())
    {
        return "";
    }

    return lrcWordLines[index].lrcText;
}



