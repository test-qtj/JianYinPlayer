#ifndef LRCPAGE_H
#define LRCPAGE_H

#include <QWidget>
#include <QPropertyAnimation>
#include <QPixmap>


// LRC行歌词结构
struct LrcWordLine
{
    LrcWordLine(qint64 lrcTime, QString lrcText)
    {
        this->lrcTime = lrcTime;
        this->lrcText = lrcText;
    }

    qint64 lrcTime;
    QString lrcText;
};

namespace Ui {
class LrcPage;
}

class QLabel;

class LrcPage : public QWidget
{
    Q_OBJECT

public:
    explicit LrcPage(QWidget *parent = nullptr);
    ~LrcPage();

    void setTrackInfo(const QString &title, const QString &artist);
    void clearLyrics();
    bool parseLrcFile(const QString& lrcFilePath);
    void setBackgroundCover(const QPixmap &cover);
    void showLrcWordLine(qint64 time);
    int getLrcWordLineIndex(qint64 time);
    QString getLrcWordByIndex(int index);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void updateBackgroundCover();

    Ui::LrcPage *ui;
    QLabel *coverBackground;
    QPixmap backgroundCover;

    QPropertyAnimation* animal;

    QVector<LrcWordLine> lrcWordLines;
};

#endif // LRCPAGE_H
