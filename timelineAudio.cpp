#include "timeline.h"

Timeline* Timeline::si;

Timeline::Timeline(QWidget *parent) :
    QWidget(parent), audioTempBuffer(14096, 0)
{
    si = this;

    // Create audio pixmap
    audioPixmap = new QPixmap(pixmapLenght, audioPixmapHeight);
    audioPixmap->fill(timelineColor);

    // Create video pixmap
    videoPixmap = new QPixmap(pixmapLenght, 1);
    videoPixmap->fill(timelineColor);

    // Create arrows for timeline
    pointerTriangle << QPoint(-5, 1)   << QPoint( 5,  1)                   << QPoint(  0, -4);
    scaleArrowLeft  << QPoint( 0, 0)   << QPoint( 0,  videoTimelineHeight) << QPoint(-18,  videoTimelineHeight/2);
    scaleArrowRight << QPoint( 0, 0)   << QPoint( 0,  videoTimelineHeight) << QPoint( 18,  videoTimelineHeight/2);

    // Initialize context menu settings
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),this, SLOT(ShowContextMenu(const QPoint&)));

    // Create and open audio file
    #ifndef Q_OS_MAC
    rawAudioFile = new QFile("rawAudioFile.wav");
    #else
    rawAudioFile = new QFile("./../../../rawAudioFile.raw");
    #endif
    rawAudioFile->open(QIODevice::ReadWrite | QIODevice::Truncate);

    initializeAudio();
}


Timeline::~Timeline()
{
    // Delete event objects
    Event::deleteAllEvents();

    // Delete audio objects
    audioInput->stop();
    audioOutput->stop();

    writeWavHeader(rawAudioFile);
    rawAudioFile->close();

    QProcess *process = new QProcess(this);
    #ifdef Q_OS_UNIX
    QString command = "opusenc";
    #else
    QString command = "opusenc.exe";
    #endif
    process->execute( command, QStringList({"--bitrate", "24", "rawAudioFile.wav", "FinalAudio.opus"}) );
}


void Timeline::initializeAudio()
{
    // Request a good format
    format.setSampleRate(samplingFrequency);
    format.setChannelCount(1);
    format.setSampleSize(8 * sampleSize);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setCodec("audio/pcm");


    // Get input device information
    if(QSettings().value("audioInputDevice").isValid())
    {
        for(const QAudioDeviceInfo& deviceInfo : QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
        {
             if (deviceInfo.deviceName() == QSettings().value("audioInputDevice").toString())
             {
                 infoIn = deviceInfo;
             }
        }
    }
    else
    {
        infoIn = QAudioDeviceInfo::defaultInputDevice();
    }


    // Get output device information
    if(QSettings().value("audioOutputDevice").isValid())
    {
        for(const QAudioDeviceInfo& deviceInfo : QAudioDeviceInfo::availableDevices(QAudio::AudioOutput))
        {
             if (deviceInfo.deviceName() == QSettings().value("audioOutputDevice").toString())
             {
                 infoOut = deviceInfo;
             }
        }
    }
    else
    {
        infoOut = QAudioDeviceInfo::defaultOutputDevice();
    }


    // Log devices actually being used
    qDebug() << "Using audio input device: " + infoIn.deviceName();
    qDebug() << "Using audio output device: " + infoOut.deviceName();


    // Get final input format
    if (!infoIn.isFormatSupported(format))
    {
       // Get the best available format, if the one requested isn't available
       format = infoIn.nearestFormat(format);

       if (format.sampleSize() % 8 != 0) format.setSampleSize(8);
       sampleSize = format.sampleSize() / 8;
       samplingFrequency = format.sampleRate();

       qWarning() << "Audio Input: Requested format not supported. Getting the nearest:";
       qWarning() << "Sample rate:" << samplingFrequency << "samples / second";
       qWarning() << "Sample size:" << sampleSize << "bytes / sample";
       qWarning() << "Sample type:" << format.sampleType();
       qWarning() << "Byte order: " << format.byteOrder();
    }


    // Get final output format
    if (!infoOut.isFormatSupported(format))
    {
       // Get the best available format, if the one requested isn't available
       format = infoOut.nearestFormat(format);

       if (format.sampleSize() % 8 != 0) format.setSampleSize(8);
       sampleSize = format.sampleSize() / 8;
       samplingFrequency = format.sampleRate();

       qWarning() << "Audio Output: Requested format not supported. Getting the nearest:";
       qWarning() << "Sample rate:" << samplingFrequency << "samples / second";
       qWarning() << "Sample size:" << sampleSize << "bytes / sample";
    }


    // Create audio input and output
    audioInput = new QAudioInput(infoIn, format, this);
    audioOutput = new QAudioOutput(infoOut, format, this);


    // Update the samplingInterval dependent variables
    samplingInterval = 1.0 / samplingFrequency;
    barsPerMSec = samplingFrequency / (samplesPerBar * 1000.0);
    pixelsPerBar = samplesPerBar * samplingInterval * pixelsPerSecond;


    // Start audio capture device and connect to apropriate SLOTs
    inputDevice = audioInput->start();
    connect(inputDevice, SIGNAL(readyRead()), this, SLOT(readAudioFromMic()));
    connect(audioInput, SIGNAL(stateChanged(QAudio::State)), this, SLOT(stateChanged(QAudio::State)));
}


void Timeline::startRecording()
{
//    int seekPos = sampleSize * samplingFrequency * timeCursorMSec / 1000.0;

//    if ( seekPos > rawAudioFile->size() )
//    {
//        seekPos = rawAudioFile->size();
//        timeCursorMSec = seekPos / (sampleSize * samplingFrequency / 1000.0);
//    }
    int seekPos = rawAudioFile->size();
    timeCursorMSec = seekPos / (sampleSize * samplingFrequency / 1000.0);

    lastRecordStartLocalTime = timeCursorMSec;
    barCursor = timeCursorMSec * barsPerMSec;
    lastAudioPixelX = floor(barCursor);
    rawAudioFile->seek(seekPos);
    isRecording = true;

    timer.start();

    Canvas::si->redrawRequested = true;
}


void Timeline::pauseRecording()
{
    canvasHoverEnd();

    isRecording = false;

    // Update video upper limit
    if (rawAudioFile->size() != 0)
        totalTimeRecorded = rawAudioFile->size() / sampleSize / samplingFrequency * 1000.0;
    else
        totalTimeRecorded = timeCursorMSec;

    // Fill the 1-2 pixels gap after recording to the middle of an existing audio piece
    if (rawAudioFile->pos() != rawAudioFile->size())
    {
        float x = barCursor * pixelsPerBar - 1;
        QPixmap tmpPixmap = audioPixmap->copy(x, 0, 1, audioPixmapHeight);

        QPainter painter(audioPixmap);
        painter.drawPixmap(x+1, 0, 2, audioPixmapHeight, tmpPixmap);
    }
}


void Timeline::startPlaying()
{
    isPlaying = true;

    // Reset timecursor if the video is about to end
    if (timeCursorMSec > totalTimeRecorded - 100)
    {
        rawAudioFile->seek(0);
        lastRecordStartLocalTime = 0;
        timeCursorMSec = 0;
    }
    else
    {
        rawAudioFile->seek( sampleSize * samplingFrequency * timeCursorMSec / 1000.0 );
        lastRecordStartLocalTime = timeCursorMSec;
    }

    timer.start();

    playerThread.start();

    Canvas::si->redrawRequested = true;
}


void Timeline::stopPlaying()
{
    isPlaying = false;

    playerThread.exit();
}

void Timeline::readAudioFromMic()
{
    if(!audioInput) return;

    QByteArray audioTempBuffer(inputDevice->readAll());
    int totalSamplesRead = audioTempBuffer.size() / sampleSize;

    if (!isRecording || totalSamplesRead <= 0) return;

    int barsToAdd = (totalSamplesRead + accumulatedSamples) / samplesPerBar;

    short* resultingData = (short*) audioTempBuffer.data(); //TODO: support char data (8bit samples)

    QPainter painter(audioPixmap);
    painter.setPen(QPen(audioColor));

    for (int i = 0; i < barsToAdd; i++)
    {
        int index = i * samplesPerBar - accumulatedSamples;

        int barIntensity = abs( resultingData[index] * audioPixmapHeight / SHRT_MAX );

        float x = (i + barCursor) * pixelsPerBar;
        lastAudioPixelX = floor(x);

        if (x > lastAudioPixelX)
        {
            painter.setPen(QPen(timelineColor));
            painter.drawLine(x+1, audioPixmapHeight, x+1, 0);
            painter.setPen(QPen(audioColor));
        }

        painter.drawLine(x, audioPixmapHeight, x, audioPixmapHeight - barIntensity);
    }

    barCursor += barsToAdd;

    accumulatedSamples = totalSamplesRead + accumulatedSamples - barsToAdd * samplesPerBar;

    rawAudioFile->write( audioTempBuffer, totalSamplesRead * sampleSize );

}


int Timeline::getCurrentTime()
{
    if (isRecording || isPlaying)
    {
        return lastRecordStartLocalTime + timer.elapsed();
    }
    else
    {
        return timeCursorMSec;
    }
}


void Timeline::stateChanged(QAudio::State newState)
{
    switch(newState)
    {
    case QAudio::StoppedState:
        if (audioInput->error() != QAudio::NoError)
        {
            qDebug() << "Audio error:" << audioInput->error();
        }
        break;
    }
}

void PlayerThread::run()
{
    QAudioOutput* audioOutput = new QAudioOutput(QAudioDeviceInfo::defaultOutputDevice(), Timeline::si->format);

    audioOutput->start(Timeline::si->rawAudioFile);

    QEventLoop loop;
    loop.exec();

    audioOutput->stop();
}


void Timeline::scaleAudio()
{
    // Calculate the tempo alteration
    float tempo = (1.0f - audioScaling) * 100.0;
    if (audioScaling > 0) tempo = 100.0f * (1.0f / audioScaling - 1.0f);

    // Create an audio file for interfacing with SoundStretch (used as an external program so far)
    QFile* tmpFileOut;
    #ifndef Q_OS_MAC
    tmpFileOut = new QFile("tmp.wav");
    #else
    tmpFileOut = new QFile("./../../../tmp.raw");
    #endif
    tmpFileOut->open(QIODevice::WriteOnly | QIODevice::Truncate);

    // Write the audioClipboard content to the temporary file
    int bytesToWrite = audioClipboard.size();
    while (bytesToWrite != 0)
    {
        bytesToWrite -= tmpFileOut->write(audioClipboard);
    }

    // Write the temporary file's wave header and close it
    qDebug() << tmpFileOut->size() << "SIZE";
    writeWavHeader(tmpFileOut);
    tmpFileOut->close();

    // Use SoundStretch to compress or expand the audio
    QProcess *process = new QProcess(this);
    #ifdef Q_OS_UNIX
    QString command = "soundstretch";
    #else
    QString command = "soundstretch.exe";
    #endif
    process->execute( command, QStringList({"tmp.wav", "tmpResult.wav", QString("-tempo=") + QString::number(tempo)}) );

    // Open the results
    QFile* tmpFileIn;
    #ifndef Q_OS_MAC
    tmpFileIn = new QFile("tmpResult.wav");
    #else
    tmpFileIn = new QFile("./../../../tmpResult.raw");
    #endif
    tmpFileIn->open(QIODevice::ReadOnly | QIODevice::Truncate);

    // Read back the result
    QByteArray tmpArray;
    audioClipboard.clear();
    int bytesToRead = tmpFileIn->size();
    while (bytesToRead != 0)
    {
        tmpArray = tmpFileIn->read(bytesToRead);
        bytesToRead -= tmpArray.size();
        audioClipboard.append(tmpArray);
    }

    // Close it after reading
    tmpFileIn->close();
}


void Timeline::writeWavHeader(QFile* audioFile)
{
    audioFile->seek(0);

    CombinedHeader header;

    const int HeaderLength = sizeof(CombinedHeader);

    memset(&header, 0, HeaderLength);

    // RIFF header
    memcpy(header.riff.descriptor.id,"RIFF",4);
    qToLittleEndian<quint32>(quint32(audioFile->size() - 8),
                             reinterpret_cast<unsigned char*>(&header.riff.descriptor.size));
    memcpy(header.riff.type, "WAVE",4);

    // WAVE header
    memcpy(header.wave.descriptor.id,"fmt ",4);
    qToLittleEndian<quint32>(quint32(16),
                             reinterpret_cast<unsigned char*>(&header.wave.descriptor.size));
    qToLittleEndian<quint16>(quint16(1),
                             reinterpret_cast<unsigned char*>(&header.wave.audioFormat));
    qToLittleEndian<quint16>(quint16(1),
                             reinterpret_cast<unsigned char*>(&header.wave.numChannels)); int sf = samplingFrequency;
    qToLittleEndian<quint32>(quint32(sf),
                             reinterpret_cast<unsigned char*>(&header.wave.sampleRate)); int f = samplingFrequency * 2;
    qToLittleEndian<quint32>(quint32(f),
                             reinterpret_cast<unsigned char*>(&header.wave.byteRate));
    qToLittleEndian<quint16>(quint16(2),
                             reinterpret_cast<unsigned char*>(&header.wave.blockAlign));
    qToLittleEndian<quint16>(quint16(16),
                             reinterpret_cast<unsigned char*>(&header.wave.bitsPerSample));

    // DATA header
    memcpy(header.data.descriptor.id,"data",4);
    qToLittleEndian<quint32>(quint32(audioFile->size() - 44),
                             reinterpret_cast<unsigned char*>(&header.data.descriptor.size));

    if (audioFile->write(reinterpret_cast<const char *>(&header), HeaderLength) == HeaderLength)
    {
        qDebug() << "Wave header written sucessfully.";
    }
    else
    {
        qDebug() << "Error writting wave header.";
    }
}
