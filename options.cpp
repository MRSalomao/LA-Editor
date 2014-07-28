#include "options.h"
#include "ui_options.h"
#include "timeline.h"

#include "mainwindow.h"

Options* Options::si;

Options::Options(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Options)
{
    ui->setupUi(this);

    si = this;

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

    pb = ui->progressBar;

    #ifndef Q_OS_MAC
    if ( QFile::exists("noise.wav") ) noiseRecorded = true;
    #else
    if ( QFile::exists("./../../../noise.raw") ) noiseRecorded = true;
    #endif

    if (noiseRecorded)
    {
        pb->setValue(100);
    }
    else
    {
        pb->setValue(0);
    }
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

void Options::on_pushButton_clicked()
{
    // Create and open audio file
    #ifndef Q_OS_MAC
    noise = new QFile("noise.wav");
    #else
    noise = new QFile("./../../../noise.raw");
    #endif
    noise->open(QIODevice::WriteOnly | QIODevice::Truncate);

    Timeline::si->audioInput->stop();
    Timeline::si->audioInput->start(noise);

    connect(&st, SIGNAL(valueChanged(int)), pb, SLOT(setValue(int)));

    st.start();
}


void StoperThread::run()
{
    QTime timer;
    timer.start();

    while (timer.elapsed() <= 4000)
    {
        emit valueChanged( (int)(timer.elapsed() / 40.0f) );
    }

    Timeline::si->audioInput->stop();

    Timeline::si->writeWavHeader(Options::si->noise);
    Options::si->noise->close();


    Timeline::si->startMic();
}
