#ifndef RECBOXITEM_H
#define RECBOXITEM_H

#include <QWidget>

namespace Ui {
class RecBoxItem;
}

class RecBoxItem : public QWidget
{
    Q_OBJECT

public:
    explicit RecBoxItem(QWidget *parent = nullptr);
    ~RecBoxItem();

    // 设置推荐文本
    void setRecText(const QString& text);

    // 设置图片
    void setRecImage(const QString& imagePath);




    bool eventFilter(QObject *watched, QEvent *event);

private:
    Ui::RecBoxItem *ui;
};

#endif // RECBOXITEM_H
