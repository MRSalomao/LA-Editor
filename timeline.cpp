#include <QDebug>
#include <QPainter>
#include <qmath.h>
#include <limits>
#include <QMouseEvent>
#include <QMenu>
#include <QIcon>
#include <QtAlgorithms>

#include "timeline.h"
#include "strokerenderer.h"
#include "canvas.h"

Timeline* Timeline::si;

Timeline::Timeline(QWidget *parent) :
    QWidget(parent), audioTempBuffer(14096, 0)
{
    si = this;

    // Create audio pixmap
    audioPixmap = new QPixmap(pixmapLenght, audiolineRealHeight);
    audioPixmap->fill(timelineColor);

    // Create video pixmap
    videoPixmap = new QPixmap(pixmapLenght, 1);
    videoPixmap->fill(timelineColor);

    // Create arrows for timeline
    pointerTriangle << QPoint(-5,  1)  << QPoint( 5,  1)  << QPoint( 0, -4);
    scaleArrowLeft  << QPoint( 0, -10) << QPoint( 0,  10) << QPoint(-15,  0);
    scaleArrowRight << QPoint( 0, -10) << QPoint( 0,  10) << QPoint( 15,  0);

    // Initialize context menu settings
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),this, SLOT(ShowContextMenu(const QPoint&)));

    // Create and open audio file
#ifndef Q_OS_MAC
    rawAudioFile = new QFile("rawAudioFile.raw");
#else
    rawAudioFile = new QFile("./../../../rawAudioFile.raw");
#endif
    rawAudioFile->open(QIODevice::ReadWrite | QIODevice::Truncate);

    // Start up audio
    initializeAudio();
}

Timeline::~Timeline()
{
    // Delete event objects
    qDeleteAll(events);

    audioInput->stop();
    rawAudioFile->close();
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


    // Get input format source information
    QAudioDeviceInfo infoIn;

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


    // Get final input format
    if (!infoIn.isFormatSupported(format))
    {
       // Get the best available format, if the one requested isn't possible
       format = infoIn.nearestFormat(format);

       if (format.sampleSize() % 8 != 0) format.setSampleSize(8);
       sampleSize = format.sampleSize() / 8;
       samplingFrequency = format.sampleRate();

       qWarning() << "Audio Input: Requested format not supported. Getting the nearest:";
       qWarning() << "Sample rate:" << samplingFrequency << "samples / second";
       qWarning() << "Sample size:" << sampleSize << "bytes / sample";
       qWarning() << "Sample type:" << format.sampleType();
       qWarning() << "Byte order:" << format.byteOrder();
    }

    // Create audio input
    audioInput = new QAudioInput(infoIn, format, this);


    // Get output format source information
    QAudioDeviceInfo infoOut;

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

    qDebug() << "Using audio input device: " + infoIn.deviceName();
    qDebug() << "Using audio output device: " + infoOut.deviceName();

    // Get final output format
    if (!infoOut.isFormatSupported(format))
    {
       // Get the best available format, if the one requested isn't possible
       format = infoOut.nearestFormat(format);

       if (format.sampleSize() % 8 != 0) format.setSampleSize(8);
       sampleSize = format.sampleSize() / 8;
       samplingFrequency = format.sampleRate();

       qWarning() << "Audio Output: Requested format not supported. Getting the nearest:";
       qWarning() << "Sample rate:" << samplingFrequency << "samples / second";
       qWarning() << "Sample size:" << sampleSize << "bytes / sample";
    }

    // Create audio output
    audioOutput = new QAudioOutput(infoOut, format, this);

    samplingInterval = 1.0 / samplingFrequency;
    barsPerMSec = samplingFrequency / (samplesPerBar * 1000.0);
    pixelsPerBar = samplesPerBar * samplingInterval * pixelsPerSecond;

#ifdef Q_OS_MAC
    inputDevice = audioInput->start();

    connect(inputDevice, SIGNAL(readyRead()), this, SLOT(readAudioFromMic()));

    connect(audioInput, SIGNAL(stateChanged(QAudio::State)), this, SLOT(stateChanged(QAudio::State)));

    audioInput->suspend();
#endif
}


void Timeline::startRecording()
{
    int seekPos = sampleSize * samplingFrequency * timeCursorMSec / 1000.0;

    if ( seekPos > rawAudioFile->size() )
    {
        seekPos = rawAudioFile->size();
        timeCursorMSec = seekPos / (sampleSize * samplingFrequency / 1000.0);
    }

    lastRecordStartLocalTime = timeCursorMSec;
    barCursor = timeCursorMSec * barsPerMSec;
    rawAudioFile->seek(seekPos);
    isRecording = true;

#ifdef Q_OS_MAC
    audioInput->resume();
#else
    inputDevice = audioInput->start();

    connect(inputDevice, SIGNAL(readyRead()), this, SLOT(readAudioFromMic()));

    connect(audioInput, SIGNAL(stateChanged(QAudio::State)), this, SLOT(stateChanged(QAudio::State)));
#endif

    timer.start();
}


void Timeline::pauseRecording()
{
    isRecording = false;

#ifdef Q_OS_MAC
    audioInput->suspend();
#else
    audioInput->stop();
#endif

    totalTimeRecorded = rawAudioFile->size() / sampleSize / samplingFrequency * 1000.0;
}


void Timeline::startPlaying()
{
    isPlaying = true;

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

    pt.start();

    Canvas::si->redrawRequested = true;
}


void Timeline::stopPlaying()
{
    isPlaying = false;

    pt.exit();
}

void Timeline::readAudioFromMic()
{
    if(!audioInput || !isRecording) return;

    QByteArray audioTempBuffer(inputDevice->readAll());
    int totalSamplesRead = audioTempBuffer.size() / sampleSize;
    qDebug() << totalSamplesRead;
    if (totalSamplesRead > 0)
    {
        int barsToAdd = (totalSamplesRead + accumulatedSamples) / samplesPerBar;

        short* resultingData = (short*) audioTempBuffer.data();

        QPainter painter(audioPixmap);
        painter.setPen(QPen(QColor(0,0,0,audioOpacity)));

        for (int i = 0; i < barsToAdd; i++)
        {
            int index = i * samplesPerBar - accumulatedSamples;

            int barIntensity = abs( resultingData[index] * audiolineRealHeight / SHRT_MAX );

            float x = (i + barCursor) * pixelsPerBar;

            painter.drawLine(x, audiolineRealHeight, x, audiolineRealHeight - barIntensity);
        }

        barCursor += barsToAdd;

        accumulatedSamples = totalSamplesRead + accumulatedSamples - barsToAdd * samplesPerBar;

        rawAudioFile->write( audioTempBuffer, totalSamplesRead * sampleSize );
    }
}

void Timeline::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    timeCursorMSec = getCurrentTime();

    if (currentEvent != NULL) if (currentEvent->type == Event::STROKE_EVENT) paintVideoPixmap();

    if ( (isPlaying || isRecording) && !mouseMiddleDown )
    {
        windowStartMSec += ( std::max(0.0f, timeCursorMSec - 21500.0f) - windowStartMSec) * 0.05;
    }

    windowEndMSec = windowStartMSec + width() * mSecsPerPixel ;

    float firstMarkMSec = mSecsBetweenMarks * (windowStartMSec / mSecsBetweenMarks);
    float lastMarkMSec = mSecsBetweenMarks * (windowEndMSec / mSecsBetweenMarks);

    float markPos = (firstMarkMSec - windowStartMSec) * pixelsPerMSec;
    for (float markTimeMSec = firstMarkMSec; markTimeMSec <= lastMarkMSec; markTimeMSec += mSecsBetweenMarks)
    {
        QString minutesAndSeconds;
        minutesAndSeconds.sprintf( "%d:%02d", (int) markTimeMSec / 60000, ( (int) markTimeMSec % 60000 ) / 1000 );

        painter.drawLine(markPos, height(), markPos, height()-16);

        painter.drawText(markPos+3, height()-6, minutesAndSeconds);

        for (float subdivisionPos = markPos + pixelsPerSubmark;
                   subdivisionPos < markPos + pixelsPerMark;
                   subdivisionPos += pixelsPerSubmark)
        {
            painter.drawLine(subdivisionPos, height(), subdivisionPos, height()-4);
        }

        markPos += pixelsPerMark;
    }

    float cursorPos = (timeCursorMSec - windowStartMSec) * pixelsPerMSec;
    float timelineStartPos = windowStartMSec * pixelsPerMSec;

    QRectF videoTimelineTarget(0,                videoTimelineStart, width(), videoTimelineHeight);
    QRectF videoTimelineSource(timelineStartPos, 0,                  width(), videolineRealHeight);

    QRectF audioTimelineTarget(0,                audioTimelineStart, width(), audioTimelineHeight);
    QRectF audioTimelineSource(timelineStartPos, 0,                  width(), audiolineRealHeight);

    painter.drawPixmap(videoTimelineTarget, *videoPixmap, videoTimelineSource);
    painter.drawPixmap(audioTimelineTarget, *audioPixmap, audioTimelineSource);


    painter.setPen(QPen(QColor(0,0,0,250), 1, Qt::DotLine));
    painter.drawLine(cursorPos, 0, cursorPos, height());

    painter.setPen(QPen(QColor(103,169,217,139)));
    painter.setBrush(QBrush(QColor(103,169,217,60)));

    if (selecting && selectionStart != selectionEnd)
    {
        painter.drawRect(selectionStart, 0, selectionEnd-selectionStart, 33);

//        painter.drawRect(selectionStart, 78, selectionEnd-selectionStart, 20);


        if (!mouseRightDown)
        {
//            painter.drawRect(selectionStart-14, 90, 14, 33);
//            painter.drawRect(selectionEnd, 90, 14, 33);

            painter.setPen(QPen(QColor(103,169,217,239)));
            painter.setBrush(QBrush(QColor(103,169,217,80)));
            painter.setRenderHint(QPainter::Antialiasing);

            painter.drawConvexPolygon( scaleArrowLeft.translated(selectionStart - 0, 88) );
            painter.drawConvexPolygon( scaleArrowRight.translated(selectionEnd + 1, 88) );
        }
        painter.setPen(QPen(QColor(103,169,217,139), 1, Qt::DotLine));
        painter.drawLine(selectionEnd, 0, selectionEnd, 99);
        painter.drawLine(selectionStart, 0, selectionStart, 99);
//        painter.drawRect(selectionStart, height()-60, selectionEnd-selectionStart, 33);
    }

    if (mouseOver)
    {
        painter.setPen(QPen(QColor(0,0,0, 150)));
        painter.setBrush(QBrush(QColor(0,0,0, 40)));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawConvexPolygon( pointerTriangle.translated(mousePos.x(), height()) );
    }

    if (!MainWindow::si->fileDialogOpen) update();

    if (rawAudioFile->pos() == rawAudioFile->size()) MainWindow::si->stopPlaying();
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


void Timeline::ShowContextMenu(const QPoint& pos)
{
    QPoint globalPos = mapToGlobal(pos);

    QMenu myMenu;
    myMenu.addAction(QIcon(":/icons/icons/icon-record-l.png"), "Record")->setIconVisibleInMenu(true);
    myMenu.addAction("")->setSeparator(true);
    myMenu.addAction(QIcon::fromTheme("edit-cut"), "Cut")->setIconVisibleInMenu(true);
    myMenu.addAction(QIcon::fromTheme("edit-copy"), "Copy")->setIconVisibleInMenu(true);
    myMenu.addAction(QIcon::fromTheme("edit-paste"), "Paste")->setIconVisibleInMenu(true);
    myMenu.addAction(QIcon::fromTheme("edit-delete"), "Erase")->setIconVisibleInMenu(true);
    myMenu.addAction("")->setSeparator(true);
    myMenu.addAction(QIcon::fromTheme("edit-undo"), "Undo")->setIconVisibleInMenu(true);
    myMenu.addAction(QIcon::fromTheme("edit-redo"), "Redo")->setIconVisibleInMenu(true);

    QAction* selectedItem = myMenu.exec(globalPos);
    if (!selectedItem) return;

    if (selectedItem->text() == "Erase")
    {
        deleteSelectedVideo();
    }

}

void Timeline::paintVideoPixmap()
{
    QPainter painter(videoPixmap);

    int x1 = currentEvent->startTime * pixelsPerMSec;
    int x2;

    if(currentEvent->endTime == -1)
    {
        x2 = timeCursorMSec * pixelsPerMSec;
    }
    else
    {
        x2 = currentEvent->endTime * pixelsPerMSec;
    }
    painter.setPen(QPen( QColor( ((PenStroke*)currentEvent)->r*255, ((PenStroke*)currentEvent)->g*255, ((PenStroke*)currentEvent)->b*255)) );
    painter.drawLine(x1, 0, x2, 0);
}


void Timeline::paintVideoPixmapRange()
{

}


bool startTimeLessThan(const Event* e1, const Event* e2)
{
    return e1->startTime < e2->startTime;
}


bool endTimeLessThan(const Event* e1, const Event* e2)
{
    return e1->endTime < e2->endTime;
}


void Timeline::selectVideo()
{
    Event value(selectionStart * mSecsPerPixel);

    QVector<Event*>::iterator i = qLowerBound(events.begin(), events.end(), &value, startTimeLessThan);

    selectionStartIdx = i - events.begin();
    qDebug() << "selectionStartIdx" << selectionStartIdx;
    if (selectionStartIdx == events.size())
    {
        selectionEnd = selectionStart;
        return;
    }
    selectionStart = events[selectionStartIdx]->startTime * pixelsPerMSec;

    value.startTime = selectionEnd * mSecsPerPixel;
    i = qLowerBound(events.begin(), events.end(), &value, startTimeLessThan);

    selectionEndIdx = i - events.begin() - 2;
    qDebug() << "selectionEndIdx" << selectionEndIdx;
    if (selectionEndIdx == events.size() || selectionStartIdx > selectionEndIdx)
    {
        selectionEnd = selectionStart;
        return;
    }
    selectionEnd = events[selectionEndIdx]->endTime * pixelsPerMSec;
}

void Timeline::deleteSelectedVideo()
{
    int x1 = events[selectionStartIdx]->startTime * pixelsPerMSec - 1;
    int x2 = events[selectionEndIdx]->endTime * pixelsPerMSec + 1;

    //TODO: delete
    events.erase(events.begin() + selectionStartIdx, events.begin() + selectionEndIdx + 1);

    QPainter painter(videoPixmap);

    painter.fillRect(x1, 0, x2-x1, 1, timelineColor);

    Canvas::si->redrawRequested = true;
}

void Timeline::scaleAndMoveSelectedVideo(float scale, int timeShiftMSec)
{
    int pivot = events[selectionStartIdx]->startTime;

    for (int i = selectionStartIdx; i < selectionEndIdx + 1; i++)
    {
        events[i]->scale(scale, (events[i]->startTime - pivot) * scale + timeShiftMSec);
    }
}

void Timeline::copyVideo()
{
    for (int i = selectionStartIdx; i < selectionEndIdx + 1; i++)
    {
        eventsClipboard << events[i];
    }
}


void Timeline::pasteVideo(int atTimeMSec)
{
    Event value(atTimeMSec);

    int insertIdx = qLowerBound(events.begin(), events.end(), &value, startTimeLessThan) - events.begin() - 1;

    int timeShiftMSec = atTimeMSec - eventsClipboard.first()->startTime;

    QPainter painter(videoPixmap);
    painter.setPen(QPen( QColor( ((PenStroke*)currentEvent)->r*255, ((PenStroke*)currentEvent)->g*255, ((PenStroke*)currentEvent)->b*255)) );

    for (Event* ev : eventsClipboard)
    {
        ev->timeShift(timeShiftMSec);

        int x1 = ev->startTime * pixelsPerMSec;
        int x2 = ev->endTime * pixelsPerMSec;

        painter.drawLine(x1, 0, x2, 0);
    }

//    QVector<Event*> newEvents(events.size() + eventsClipboard.size());

    events = events.mid(0, insertIdx) + eventsClipboard + events.mid(insertIdx); //TODO - improve performance
}


void Timeline::resizeEvent(QResizeEvent *event)
{
    pixelsPerMark = pixelsPerMSec * mSecsBetweenMarks;
    pixelsPerSubmark = pixelsPerMSec * mSecsBetweenSubdivisions;
}


void Timeline::setCursorAt(int x)
{
    timeCursorMSec = x * mSecsPerPixel + windowStartMSec;

    if (timeCursorMSec > totalTimeRecorded) timeCursorMSec = totalTimeRecorded;
    if (timeCursorMSec < 0) timeCursorMSec = 0;

    Canvas::si->redrawRequested = true;
}

// Redraw the entire screen from time 0 to the current timeCursor position
void Timeline::redrawScreen()
{
    eventToDrawIdx = 0;
    Event::setSubeventIndex(0);

    bool hitCursor = false;

    for(; eventToDrawIdx < events.size(); eventToDrawIdx++)
    {
        eventToDraw = events[eventToDrawIdx];

        if (eventToDraw->type == Event::STROKE_EVENT)
        {
            hitCursor = ((PenStroke*)eventToDraw)->drawUntil(timeCursorMSec);

            if (hitCursor) break;
        }
    }
}


// Draw from the last indexes to the current timeCursor position
void Timeline::incrementalDraw()
{
    timeCursorMSec = getCurrentTime();

    bool hitLimit = false;

    for(; eventToDrawIdx < events.size(); eventToDrawIdx++)
    {
        eventToDraw = events[eventToDrawIdx];

        if (eventToDraw->startTime > timeCursorMSec) break;

        if (eventToDraw->type == Event::STROKE_EVENT)
        {
            hitLimit = ((PenStroke*)eventToDraw)->drawFromIndexUntil(timeCursorMSec);

            if (hitLimit)
            {
                if (isPlaying) cursorPosition = ((PenStroke*)eventToDraw)->getPos(timeCursorMSec);

                break;
            }
        }
    }

    if (!hitLimit)
    {
        Event* ev;

        for (int i = eventToDrawIdx - 1; i >= 0; i--)
        {
            ev = events[i];

            if (ev->type == Event::STROKE_EVENT || ev->type == Event::POINTER_MOVEMENT_EVENT)
            {
                cursorPosition = ev->getPos(timeCursorMSec);

                break;
            }
        }
    }
}


void Timeline::mousePressEvent(QMouseEvent *event)
{
    mousePos = event->pos();

    if(event->button() == Qt::RightButton)
    {
        mouseRightDown = true;
        selecting = true;
        selectionStart = mousePos.x();
        selectionEnd = mousePos.x();
    }
    else if(event->button() == Qt::LeftButton && !isRecording)
    {
        mouseLeftDown = true;
        setCursorAt( mousePos.x() );
        if (isPlaying)
        {
            stopPlaying();
            wasPlaying = true;
        }
    }
    else if(event->button() == Qt::MiddleButton)
    {
        mouseMiddleDown = true;
        dragStartX = event->x();
        lastWindowStartMSec = windowStartMSec;
    }
}

void Timeline::mouseMoveEvent(QMouseEvent *event)
{
    mousePos = event->pos();

    if ( this->rect().contains(mousePos) )
    {
        mouseOver = true;
    }
    else
    {
        mouseOver = false;
    }

    if (mouseRightDown)
    {
        selectionEnd = mousePos.x();
    }
    else if(mouseLeftDown)
    {
        setCursorAt( mousePos.x() );
    }
    else if(mouseMiddleDown)
    {
        windowStartMSec = lastWindowStartMSec + ( dragStartX - event->x() ) * mSecsPerPixel;

        if (windowStartMSec < 0) windowStartMSec = 0;
    }
}

void Timeline::mouseReleaseEvent(QMouseEvent *event)
{
    mousePos = event->pos();

    if(event->button() == Qt::LeftButton)
    {
        mouseLeftDown = false;
        setCursorAt( mousePos.x() );
        if (wasPlaying) startPlaying();
        wasPlaying = false;
    }
    else if(event->button() == Qt::RightButton)
    {
        mouseRightDown = false;
        selectionEnd = mousePos.x();
        if (selectionStart == selectionEnd) selecting = false;
        else
        {
            if (selectionStart > selectionEnd)
            {
                int tmp = selectionStart;
                selectionStart = selectionEnd;
                selectionEnd = tmp;
            }

            selectVideo();
        }
    }
    else if(event->button() == Qt::MiddleButton)
    {
        mouseMiddleDown = false;
    }
}

void Timeline::wheelEvent(QWheelEvent *event)
{
    windowStartMSec -= event->delta() * 8.0;

    if (windowStartMSec < 0) windowStartMSec = 0;
}


void Timeline::addStrokeBeginEvent(QPointF penPos, int pboStart, int pbo)
{
    int timestamp = getCurrentTime();

    switch (MainWindow::si->activeTool)
    {
    case MainWindow::PEN_TOOL:
        events.append(new PenStroke(pboStart, timestamp));
        break;

    case MainWindow::ERASER_TOOL:
        events.append(new EraserStroke(pboStart, timestamp));
        break;

    default:
        break;
    }

    currentEvent = events.back();

    dynamic_cast<PenStroke*>(currentEvent)->addStrokeEvent(timestamp, penPos.x(), penPos.y(), pbo);

    Canvas::si->incrementalDrawRequested = true;
}

void Timeline::addStrokeMoveEvent(QPointF penPos, int pbo)
{
    int timestamp = getCurrentTime();

    ((PenStroke*)currentEvent)->addStrokeEvent(timestamp, penPos.x(), penPos.y(), pbo);

    Canvas::si->incrementalDrawRequested = true;
}

void Timeline::addStrokeEndEvent()
{
    int timestamp = getCurrentTime();

    dynamic_cast<PenStroke*>(currentEvent)->closeStrokeEvent(timestamp);
}


void Timeline::addPointerBeginEvent(QPointF penPos)
{
    if (!isRecording) return;

    int timestamp = getCurrentTime();

    events.append(new PointerMovement(timestamp));

    currentEvent = events.back();

    dynamic_cast<PointerMovement*>(currentEvent)->addPointerEvent(timestamp, penPos.x(), penPos.y());
}

void Timeline::addPointerMoveEvent(QPointF penPos)
{
    if (!isRecording) return;

    int timestamp = getCurrentTime();

    if (currentEvent == NULL)
    {
        addPointerBeginEvent(penPos);
    }
    else if (currentEvent->type != Event::POINTER_MOVEMENT_EVENT)
    {
        addPointerBeginEvent(penPos);
    }
    else
    {
        dynamic_cast<PointerMovement*>(currentEvent)->addPointerEvent(timestamp, penPos.x(), penPos.y());
    }
}

void Timeline::addPointerEndEvent()
{
    if (!isRecording) return;

    int timestamp = getCurrentTime();

    if (currentEvent == NULL)
    {
        addPointerBeginEvent(QPoint(0,0));
    }
    else
    {
        dynamic_cast<PointerMovement*>(currentEvent)->closePointerEvent(timestamp);
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
