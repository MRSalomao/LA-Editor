#ifndef HOTKEYS_H
#define HOTKEYS_H

#include <QDialog>

namespace Ui {
class Hotkeys;
}

class Hotkeys : public QDialog
{
    Q_OBJECT

public:
    explicit Hotkeys(QWidget *parent = 0);
    ~Hotkeys();

private:
    Ui::Hotkeys *ui;
};

#endif // HOTKEYS_H
