#ifndef UPLOAD_H
#define UPLOAD_H

#include <QDialog>

namespace Ui {
class Upload;
}

class Upload : public QDialog
{
    Q_OBJECT

public:
    explicit Upload(QWidget *parent = 0);
    ~Upload();

private slots:

private:
    Ui::Upload *ui;
};

#endif // UPLOAD_H
