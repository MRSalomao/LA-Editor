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
    pointerTriangle << QPoint(-5,  1)  << QPoint( 5,  1)  << QPoint(  0, -4);
    scaleArrowLeft  << QPoint( 0, 0) << QPoint( 0,  videoTimelineHeight) << QPoint(-18,  videoTimelineHeight/2);
    scaleArrowRight << QPoint( 0, 0) << QPoint( 0,  videoTimelineHeight) << QPoint( 18,  videoTimelineHeight/2);

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
#ifdef Q_OS_MAC
    audioInput->suspend();
#else
    audioInput->stop();
#endif

    addPointerEndEvent();

    isRecording = false;

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

    // Paint current event
    if (currentEvent != NULL) if (currentEvent->type == Event::STROKE_EVENT) paintVideoPixmap();

    // Move with damping towards target timeline position
    if ( (isPlaying || isRecording) && !mouseMiddleDown )
    {
        windowStartMSec += ( std::max(0.0f, timeCursorMSec - 21500.0f) - windowStartMSec) * 0.05;
    }
    timelineStartPos = windowStartMSec * pixelsPerMSec;

    // Draw timeline ruler
    windowEndMSec = windowStartMSec + width() * mSecsPerPixel ;

    float firstMarkMSec = mSecsBetweenMarks * (windowStartMSec / mSecsBetweenMarks);
    float lastMarkMSec = mSecsBetweenMarks * (windowEndMSec / mSecsBetweenMarks);

    float markPos = (firstMarkMSec - windowStartMSec) * pixelsPerMSec;
    for (float markTimeMSec = firstMarkMSec; markTimeMSec <= lastMarkMSec; markTimeMSec += mSecsBetweenMarks)
    {
        QString minutesAndSeconds;
        minutesAndSeconds.sprintf( "%d:%02d", (int) markTimeMSec / 60000, ( (int) markTimeMSec % 60000 ) / 1000 );

        // Draw ruler marks
        painter.drawLine(markPos, height(), markPos, height()-16);

        // Draw timestamps
        painter.drawText(markPos+3, height()-6, minutesAndSeconds);

        for (float subdivisionPos = markPos + pixelsPerSubmark;
                   subdivisionPos < markPos + pixelsPerMark;
                   subdivisionPos += pixelsPerSubmark)
        {
            // draw ruler submarks
            painter.drawLine(subdivisionPos, height(), subdivisionPos, height()-4);
        }

        markPos += pixelsPerMark;
    }

    QRectF videoTimelineTarget(0,                videoTimelineStart, width(), videoTimelineHeight);
    QRectF videoTimelineSource(timelineStartPos, 0,                  width(), videolineRealHeight);

    QRectF audioTimelineTarget(0,                audioTimelineStart, width(), audioTimelineHeight);
    QRectF audioTimelineSource(timelineStartPos, 0,                  width(), audiolineRealHeight);

    // Draw both video and audio pixmaps
    painter.drawPixmap(videoTimelineTarget, *videoPixmap, videoTimelineSource);
    painter.drawPixmap(audioTimelineTarget, *audioPixmap, audioTimelineSource);

    // Start selection painting
    if (videoSelected)
    {
        videoSelectionRect(selectionStart - timelineStartPos, videoTimelineStart, selectionEnd-selectionStart+1, videoTimelineHeight);

        painter.setPen(QPen(Qt::transparent));
        painter.setBrush(QBrush(selectionColor));
        painter.drawRect(videoSelectionRect);

        if (!mouseLeftDownSelect)
        {
            scaleArrowLeftVideo = QPolygon(scaleArrowLeft.translated(selectionStart - timelineStartPos, videoTimelineStart));
            scaleArrowRightVideo = QPolygon(scaleArrowRight.translated(selectionEnd + 1 - timelineStartPos, videoTimelineStart));

            if (videoSelectionRect.contains(mousePos) ||
                scaleArrowLeftVideo.containsPoint(mousePos, Qt::OddEvenFill) ||
                scaleArrowRightVideo.containsPoint(mousePos, Qt::OddEvenFill))
            {
                videoScaleArrowAlpha += (targetAlpha - videoScaleArrowAlpha) * arrowFadeIn;
            }
            else
            {
                videoScaleArrowAlpha += (0 - videoScaleArrowAlpha) * arrowFadeOut;
            }

            painter.setPen(QPen(Qt::transparent));
            painter.setBrush(QBrush(QColor(103,169,217, videoScaleArrowAlpha)));
            painter.setRenderHint(QPainter::Antialiasing);

            painter.drawConvexPolygon(scaleArrowLeftVideo);
            painter.drawConvexPolygon(scaleArrowRightVideo);
        }
    }
    if (audioSelected)
    {
        painter.setPen(QPen(Qt::transparent));
        painter.setBrush(QBrush(selectionColor));
        audioSelectionRect(selectionStart - timelineStartPos, audioTimelineStart, selectionEnd-selectionStart+1, audioTimelineHeight);
        painter.drawRect(audioSelectionRect);

        if (!mouseLeftDownSelect)
        {
            scaleArrowLeftAudio = QPolygon(scaleArrowLeft.translated(selectionStart - timelineStartPos, audioTimelineStart));
            scaleArrowRightAudio = QPolygon(scaleArrowRight.translated(selectionEnd + 1 - timelineStartPos, audioTimelineStart));

            if (audioSelectionRect.contains(mousePos) ||
                scaleArrowLeftAudio.containsPoint(mousePos, Qt::OddEvenFill) ||
                scaleArrowRightAudio.containsPoint(mousePos, Qt::OddEvenFill))
            {
                videoScaleArrowAlpha += (targetAlpha - videoScaleArrowAlpha) * arrowFadeIn;
            }
            else
            {
                videoScaleArrowAlpha += (0 - videoScaleArrowAlpha) * arrowFadeOut;
            }

            painter.setPen(QPen(Qt::transparent));
            painter.setBrush(QBrush(QColor(103,169,217, videoScaleArrowAlpha)));
            painter.setRenderHint(QPainter::Antialiasing);

            painter.drawConvexPolygon(scaleArrowLeftAudio);
            painter.drawConvexPolygon(scaleArrowRightAudio);
        }

        // Draw timecursor
        float cursorPos = (timeCursorMSec - windowStartMSec) * pixelsPerMSec;
        painter.drawLine(cursorPos, 0, cursorPos, height());
    }

    if (mouseOver)
    {
        painter.setPen(QPen(QColor(0,0,0, 150)));
        painter.setBrush(QBrush(QColor(0,0,0, 40)));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawConvexPolygon( pointerTriangle.translated(mousePos.x(), height()) ); // make it optional
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

    if (selectedItem->text() == "Cut")
    {
        copyVideo();
        deleteSelectedVideo();
    }
    else if (selectedItem->text() == "Paste")
    {
        pasteVideo(timeCursorMSec);
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
    if (selectionStartIdx == events.size())
    {
        audioSelected = false;
        videoSelected = false;
        selectionEnd = selectionStart;
        return;
    }
    selectionStart = events[selectionStartIdx]->startTime * pixelsPerMSec;

    value.endTime = selectionEnd * mSecsPerPixel;
    i = qUpperBound(events.begin(), events.end(), &value, endTimeLessThan);

    selectionEndIdx = i - events.begin() - 1;
    if (selectionEndIdx == events.size() || selectionStartIdx > selectionEndIdx || selectionStartIdx == -1)
    {
        audioSelected = false;
        videoSelected = false;
        selectionEnd = selectionStart;
        return;
    }
    selectionEnd = events[selectionEndIdx]->endTime * pixelsPerMSec;
}

void Timeline::selectAudio()
{
    if (selectionStart > totalTimeRecorded * pixelsPerMSec) selectionStart = totalTimeRecorded * pixelsPerMSec;
    if (selectionEnd > totalTimeRecorded * pixelsPerMSec) selectionEnd = totalTimeRecorded * pixelsPerMSec;
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

            // Stop incrementing (break the loop) the event index for now, if the timecursor was hit
            if (hitLimit)
            {
                if (isPlaying) cursorPosition = eventToDraw->getCursorPos(timeCursorMSec);

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
                cursorPosition = ev->getCursorPos(timeCursorMSec);

                break;
            }
        }
    }
}


void Timeline::mousePressEvent(QMouseEvent *event)
{
    mousePos = event->pos();

    if(event->button() == Qt::LeftButton && !isRecording)
    {
        if (videoSelectionRect.contains(mousePos) ||
            scaleArrowLeftTmp.containsPoint(mousePos, Qt::OddEvenFill) ||
            scaleArrowRightTmp.containsPoint(mousePos, Qt::OddEvenFill))
        {

        }

        if (mousePos.y() > timeRulerStart)
        {
            mouseLeftDownSetPos = true;
            setCursorAt( mousePos.x() );
            if (isPlaying)
            {
                stopPlaying();
                wasPlaying = true;
            }
        }
        else
        {
            mouseLeftDownSelect = true;
            audioSelected = false;
            videoSelected = false;
            selectionStart = mousePos.x();
            selectionEnd = mousePos.x();

            if (mousePos.y() > audioTimelineStart)
            {
                audioSelected = true;
            }
            else if (mousePos.y() > videoTimelineStart + videoTimelineHeight)
            {
                audioSelected = true;
                videoSelected = true;
            }
            else if (mousePos.y() > videoTimelineStart)
            {
                videoSelected = true;
            }
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

    if (mouseLeftDownSetPos)
    {
        setCursorAt( mousePos.x() );
    }
    else if (mouseLeftDownSelect)
    {
        selectionEnd = mousePos.x();
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
        if (mouseLeftDownSetPos)
        {
            mouseLeftDownSetPos = false;
            setCursorAt( mousePos.x() );
            if (wasPlaying) startPlaying();
            wasPlaying = false;
        }
        else
        {
            mouseLeftDownSelect = false;
            selectionEnd = mousePos.x();
            if (selectionStart == selectionEnd)
            {
                audioSelected = false;
                videoSelected = false;
            }
            else
            {
                if (selectionStart > selectionEnd)
                {
                    int tmp = selectionStart;
                    selectionStart = selectionEnd;
                    selectionEnd = tmp;
                }

                if (videoSelected) selectVideo();
                if (audioSelected) selectAudio();
            }
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
