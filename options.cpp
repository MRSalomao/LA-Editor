#include "options.h"
#include "ui_options.h"

#include "mainwindow.h"

Options::Options(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Options)
{
    ui->setupUi(this);

    // Load inputSourcesList for audioInputComboBox
    QStringList inputSourcesList;

    bool audioDeviceSelected = QSettings().value("audioInputDevice").isValid();

    if (audioDeviceSelected) inputSourcesList << QSettings().value("audioInputDevice").toString();

    if (audioDeviceSelected)
    {
        for (QAudioDeviceInfo& deviceInfo : QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
        {
            if ( !(deviceInfo.deviceName() == inputSourcesList[0]) ) inputSourcesList << deviceInfo.deviceName();
        }
    }
    else
    {    for (QAudioDeviceInfo& deviceInfo : QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
        {
            inputSourcesList << deviceInfo.deviceName();
        }
    }

    ui->audioInputComboBox->addItems(inputSourcesList);


    // Load outputSourcesList for audioOutputComboBox
    QStringList outputSourcesList;

    audioDeviceSelected = QSettings().value("audioOutputDevice").isValid();

    if (audioDeviceSelected) outputSourcesList << QSettings().value("audioOutputDevice").toString();

    if (audioDeviceSelected)
    {
        for (QAudioDeviceInfo& deviceInfo : QAudioDeviceInfo::availableDevices(QAudio::AudioOutput))
        {
            if ( !(deviceInfo.deviceName() == outputSourcesList[0]) ) outputSourcesList << deviceInfo.deviceName();
        }
    }
    else
    {    for (QAudioDeviceInfo& deviceInfo : QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
        {
            outputSourcesList << deviceInfo.deviceName();
        }
    }

    ui->audioOutputComboBox->addItems(outputSourcesList);
}

Options::~Options()
{
    delete ui;
}

void Options::on_audioInputComboBox_activated(const QString &arg1)
{
    QSettings().setValue("audioInputDevice", arg1);
}

void Options::on_audioOutputComboBox_activated(const QString &arg1)
{
    QSettings().setValue("audioOutputDevice", arg1);
}
