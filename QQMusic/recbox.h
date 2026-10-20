#ifndef RECBOX_H
#define RECBOX_H

#include <QWidget>
#include <QJsonArray>

namespace Ui {
class RecBox;
}

class RecBox : public QWidget
{
    Q_OBJECT

public:
    explicit RecBox(QWidget *parent = nullptr);
    ~RecBox();

        // 初始化RecBoxItem
    void initRecBoxUi(QJsonArray data, int row);

private slots:
    void on_btDown_clicked();

    void on_btUp_clicked();

private:
    void createRecBoxItem();

private:
    Ui::RecBox *ui;

    int row;
    int col;

    int currentIndex;   // 标记当前显示第几组图片
    int count;          // 标记图片总的组数

    QJsonArray imageList;
};

#endif // RECBOX_H
