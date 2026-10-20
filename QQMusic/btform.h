#ifndef BTFORM_H
#define BTFORM_H

#include <QWidget>
#include <QPropertyAnimation>


namespace Ui {
class BtForm;
}

class BtForm : public QWidget
{
    Q_OBJECT

public:
    explicit BtForm(QWidget *parent = nullptr);
    ~BtForm();

    void setIconAndText(const QString& btIcon, const QString& btText, int pageId);
    int getPageId()const;
    void clearBackground();
    void showAnimal(bool isShow);

signals:
    void btClick(int id);

protected:
    void mousePressEvent(QMouseEvent *event);

private:
    Ui::BtForm *ui;
    int pageId;

    QPropertyAnimation* line1Animal;
    QPropertyAnimation* line2Animal;
    QPropertyAnimation* line3Animal;
    QPropertyAnimation* line4Animal;
};

#endif // BTFORM_H
