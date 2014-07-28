#ifndef OPTIONS_H
#define OPTIONS_H

#include <QDialog>
#include <QFile>
#include <QAudioDeviceInfo>
#include <QThread>
#include <QProgressBar>

namespace Ui { class Options; }

class StoperThread : public QThread
{
    Q_OBJECT
    void run();

signals:
    void valueChanged(int value);
};

class Options : public QDialog
{
    Q_OBJECT

public:
    explicit Options(QWidget *parent = 0);
    ~Options();

    static Options* si;

    QFile* noise;

    QProgressBar* pb;

private slots:
    void on_audioInputComboBox_activated(const QString &arg1);

    void on_audioOutputComboBox_activated(const QString &arg1);

    void on_pushButton_clicked();

private:
    Ui::Options *ui;

    bool noiseRecorded = false;

    StoperThread st;

};

#endif
