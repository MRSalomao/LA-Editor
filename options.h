#ifndef OPTIONS_H
#define OPTIONS_H

#include <QDialog>
#include <QAudioDeviceInfo>

namespace Ui { class Options; }

class Options : public QDialog
{
    Q_OBJECT

public:
    explicit Options(QWidget *parent = 0);
    ~Options();

private slots:
    void on_comboBox_activated(const QString &arg1);

    void on_audioOutputComboBox_activated(const QString &arg1);

private:
    Ui::Options *ui;
};

#endif
