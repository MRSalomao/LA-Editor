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
    audioPixmap = new QPixmap(pixmapLenght, audioPixmapRealHeight);
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

    // Delete audio objects
    audioInput->stop();
    audioOutput->stop();
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

    // Start audio capture device
    inputDevice = audioInput->start();

    connect(inputDevice, SIGNAL(readyRead()), this, SLOT(readAudioFromMic()));

    connect(audioInput, SIGNAL(stateChanged(QAudio::State)), this, SLOT(stateChanged(QAudio::State)));
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
    lastAudioPixelX = floor(barCursor);
    rawAudioFile->seek(seekPos);
    isRecording = true;

    timer.start();
}


void Timeline::pauseRecording()
{
    addPointerEndEvent();

    isRecording = false;

    // If recording over
    if (rawAudioFile->pos() != rawAudioFile->size())
    {
        // Fill the 1-2 pixels gap after recording to the middle of an existing audio piece
        totalTimeRecorded = rawAudioFile->size() / sampleSize / samplingFrequency * 1000.0;

        float x = barCursor * pixelsPerBar - 1;
        QPixmap tmpPixmap = audioPixmap->copy(x, 0, 1, audioPixmapRealHeight);

        QPainter painter(audioPixmap);
        painter.drawPixmap(x+1, 0, 2, audioPixmapRealHeight, tmpPixmap);
    }
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

    if(!isRecording) return;

    int totalSamplesRead = audioTempBuffer.size() / sampleSize;

    if (totalSamplesRead > 0)
    {
        int barsToAdd = (totalSamplesRead + accumulatedSamples) / samplesPerBar;

        short* resultingData = (short*) audioTempBuffer.data();

        QPainter painter(audioPixmap);
        painter.setPen(QPen(audioColor));

        for (int i = 0; i < barsToAdd; i++)
        {
            int index = i * samplesPerBar - accumulatedSamples;

            int barIntensity = abs( resultingData[index] * audioPixmapRealHeight / SHRT_MAX );

            float x = (i + barCursor) * pixelsPerBar;
            lastAudioPixelX = floor(x);

            if (x > lastAudioPixelX)
            {
                painter.setPen(QPen(timelineColor));
                painter.drawLine(x+1, audioPixmapRealHeight, x+1, 0);
                painter.setPen(QPen(audioColor));
            }

            painter.drawLine(x, audioPixmapRealHeight, x, audioPixmapRealHeight - barIntensity);
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

    // Get video pixmap source and target coords
    QRectF videoTimelineTarget(0,                videoTimelineStart, width(), videoTimelineHeight);
    QRectF videoTimelineSource(timelineStartPos, 0,                  width(), videoPixmapHeight);

    // Get audio pixmap source and target coords
    QRectF audioTimelineTarget(0,                audioTimelineStart, width(), audioPixmapHeight);
    QRectF audioTimelineSource(timelineStartPos, 0,                  width(), audioPixmapRealHeight);

    // Draw both video and audio pixmaps
    painter.drawPixmap(videoTimelineTarget, *videoPixmap, videoTimelineSource);
    painter.drawPixmap(audioTimelineTarget, *audioPixmap, audioTimelineSource);

    // Start selection painting
     if (videoSelected)
     {
         videoSelectionRect.setRect(selectionStartPos - timelineStartPos,    videoTimelineStart,
                                    selectionEndPos - selectionStartPos + 1, videoTimelineHeight);

         painter.setPen(QPen(Qt::transparent));
         painter.setBrush(QBrush(selectionColor));
//             if (draggingEvents || scalingEventsLeft || scalingEventsRight)
         painter.drawRect(videoSelectionRect);

         if (!isSelecting)
         {
             videoLeftScaleArrow =  QPolygon(scaleArrowLeft.translated( videoSelectionRect.left(),      videoTimelineStart));
             videoRightScaleArrow = QPolygon(scaleArrowRight.translated(videoSelectionRect.right() + 1, videoTimelineStart));

             if (videoSelectionRect.contains(mousePos) ||
                 videoLeftScaleArrow.containsPoint(mousePos, Qt::OddEvenFill) ||
                 videoRightScaleArrow.containsPoint(mousePos, Qt::OddEvenFill))
             {
                 videoScaleArrowAlpha += (targetAlpha - videoScaleArrowAlpha) * arrowFadeIn;
             }
             else
             {
                 videoScaleArrowAlpha -= videoScaleArrowAlpha * arrowFadeOut;
             }

             QColor selectionColorArrow = selectionColor;
             selectionColorArrow.setAlpha(videoScaleArrowAlpha);
             painter.setBrush(QBrush(selectionColorArrow));
             painter.setRenderHint(QPainter::Antialiasing);

             painter.drawConvexPolygon(videoLeftScaleArrow);
             painter.drawConvexPolygon(videoRightScaleArrow);
         }
     }
     if (audioSelected)
     {
         audioSelectionRect.setRect(selectionStartPos - timelineStartPos,    audioTimelineStart,
                                    selectionEndPos - selectionStartPos + 1, audioPixmapHeight);

         painter.setPen(QPen(Qt::transparent));
         painter.setBrush(QBrush(selectionColor));
         painter.drawRect(audioSelectionRect);

         if (!isSelecting)
         {
             audioLeftScaleArrow =  QPolygon(scaleArrowLeft.translated( audioSelectionRect.left(),      audioTimelineStart));
             audioRightScaleArrow = QPolygon(scaleArrowRight.translated(audioSelectionRect.right() + 1, audioTimelineStart));

             if (audioSelectionRect.contains(mousePos) ||
                 audioLeftScaleArrow.containsPoint(mousePos, Qt::OddEvenFill) ||
                 audioRightScaleArrow.containsPoint(mousePos, Qt::OddEvenFill))
             {
                 audioScaleArrowAlpha += (targetAlpha - audioScaleArrowAlpha) * arrowFadeIn;
             }
             else
             {
                 audioScaleArrowAlpha -= audioScaleArrowAlpha * arrowFadeOut;
             }

             QColor selectionColorArrow = selectionColor;
             selectionColorArrow.setAlpha(audioScaleArrowAlpha);
             painter.setBrush(QBrush(selectionColorArrow));
             painter.setRenderHint(QPainter::Antialiasing);

             painter.drawConvexPolygon(audioLeftScaleArrow);
             painter.drawConvexPolygon(audioRightScaleArrow);
         }
     }

     // Draw timecursor
     float cursorPos = (timeCursorMSec - windowStartMSec) * pixelsPerMSec;
     painter.setPen(QPen(QColor(0,0,0,250), 1, Qt::DotLine));
     painter.drawLine((int)cursorPos, 0, (int)cursorPos, height());

     // Draw timeline pointer triangle
     if ( mouseOver && !(isRecording || isPlaying) )
     {
         painter.setPen(QPen(QColor(0,0,0, 150)));
         painter.setBrush(QBrush(QColor(0,0,0, 40)));
         painter.setRenderHint(QPainter::Antialiasing);
         painter.drawConvexPolygon( pointerTriangle.translated(mousePos.x(), height()) ); // make it optional
     }

     if (!MainWindow::si->childWindowOpen) update();

     // Stop playing if end of file is reached
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
    if (isRecording || isPlaying) return;

    QPoint globalPos = mapToGlobal(pos);

    if (eventModified)
    {
        QMenu myMenu;
        myMenu.addAction("Apply")->setIconVisibleInMenu(true);
        myMenu.addAction("Cancel")->setIconVisibleInMenu(true);

        QAction* selectedItem = myMenu.exec(globalPos);
        if (!selectedItem) return;

        if (selectedItem->text() == "Apply")
        {
            pasteVideo(selectionStartTime);
        }
    }
    else
    {
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
    Event value(selectionStartPos * mSecsPerPixel);

    QVector<Event*>::iterator i = qLowerBound(events.begin(), events.end(), &value, startTimeLessThan);

    selectionStartIdx = i - events.begin();
    if (selectionStartIdx == events.size())
    {
        audioSelected = false;
        videoSelected = false;
        selectionEndPos = selectionStartPos;
        return;
    }
    selectionStartPos = events[selectionStartIdx]->startTime * pixelsPerMSec;

    value.endTime = selectionEndPos * mSecsPerPixel;
    i = qUpperBound(events.begin(), events.end(), &value, endTimeLessThan);

    selectionEndIdx = i - events.begin() - 1;
    if (selectionEndIdx == events.size() || selectionStartIdx > selectionEndIdx || selectionStartIdx == -1)
    {
        audioSelected = false;
        videoSelected = false;
        selectionEndPos = selectionStartPos;
        return;
    }
    selectionEndPos = events[selectionEndIdx]->endTime * pixelsPerMSec;
}


void Timeline::selectAudio()
{
    if (selectionStartPos > totalTimeRecorded * pixelsPerMSec) selectionStartPos = totalTimeRecorded * pixelsPerMSec;
    if (selectionEndPos > totalTimeRecorded * pixelsPerMSec) selectionEndPos = totalTimeRecorded * pixelsPerMSec;
    if (selectionEndPos == selectionStartPos) audioSelected = false;
}


void Timeline::checkForCollision()
{
    // Check if selection is below zero seconds
    if (selectionStartTime < 0)
    {
        // If so, apply corrections
        eventTimeShiftMSec -= selectionStartTime;
        selectionStartTime -= selectionStartTime;
        selectionEndTime   -= selectionStartTime;
    }

    // Check if selection is above the total recorded time
    if (selectionEndTime > totalTimeRecorded)
    {
        // If so, apply corrections
        int adjustment = totalTimeRecorded - selectionEndTime;

        eventTimeShiftMSec += adjustment;
        selectionStartTime += adjustment;
        selectionEndTime   += adjustment;
    }

    Event value(selectionStartTime);
    QVector<Event*>::iterator i = qLowerBound(events.begin(), events.end(), &value, startTimeLessThan);
    int newSelectionStartIdx = i - events.begin() - 1;
    if (newSelectionStartIdx >= events.size())
    {
        return;
    }
    if (newSelectionStartIdx < 0)
    {
        newSelectionStartIdx = 0;
    }

    value.endTime = selectionEndTime;
    i = qLowerBound(events.begin(), events.end(), &value, endTimeLessThan);
    int newSelectionEndIdx = i - events.begin() + 1;
    if (newSelectionEndIdx >= events.size())
    {
        newSelectionEndIdx = events.size() - 1;
    }
    if (newSelectionEndIdx < 0)
    {
        return;
    }

    for (int i=newSelectionStartIdx; i<newSelectionEndIdx; i++)
    {
        if (events[i]->type != Event::POINTER_MOVEMENT_EVENT)
        {
            selectionColor = selectionColorCollision;
            return;
        }
    }

    selectionColor = selectionColorNoCollision;
}


void Timeline::deleteSelectedVideo()
{
    int x1 = selectionStartTime - 1;
    int x2 = selectionEndTime + 1;

    //TODO: delete
    events.erase(events.begin() + selectionStartIdx, events.begin() + selectionEndIdx + 1);

    QPainter painter(videoPixmap);

    painter.fillRect(x1, 0, x2-x1, 1, timelineColor);

    Canvas::si->redrawRequested = true;
}


void Timeline::deleteSelectedAudio()
{
    int x1 = selectionStartTime - 1;
    int x2 = selectionEndTime + 1;

    QPainter painter(audioPixmap);
    painter.fillRect(x1, 0, x2-x1, 1, timelineColor);

    int oldSeekPos = rawAudioFile->pos();
    int selectionTime = events[selectionEndIdx]->endTime - events[selectionStartIdx]->startTime;
    int deletionSize = sampleSize * samplingFrequency * selectionTime / 1000.0;
    int selectionStart = sampleSize * samplingFrequency * events[selectionStartIdx]->startTime / 1000.0;
    QByteArray zeros(deletionSize, 0);
    rawAudioFile->seek(selectionStart);
    while (deletionSize != 0) deletionSize -= rawAudioFile->write(zeros);
    rawAudioFile->seek(oldSeekPos);
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
    tmpVideo = videoPixmap->copy(selectionStartPos - 1, 0,
                                 selectionEndPos - selectionStartPos, 1);

    for (int i = selectionStartIdx; i < selectionEndIdx + 1; i++)
    {
        eventsClipboard << events[i];
    }
}


void Timeline::copyAudio()
{
    tmpAudio.copy(selectionStartPos - 1, 0,
                  selectionEndPos - selectionStartPos, 1);

    int oldSeekPos = rawAudioFile->pos();
    int selectionTime = events[selectionEndIdx]->endTime - events[selectionStartIdx]->startTime;
    int copySize = sampleSize * samplingFrequency * selectionTime / 1000.0;
    int selectionStart = sampleSize * samplingFrequency * events[selectionStartIdx]->startTime / 1000.0;
    QByteArray tmpArray;
    rawAudioFile->seek(selectionStart);
    while (copySize != 0)
    {
        tmpArray = rawAudioFile->read(copySize);
        copySize -= tmpArray.size();
        audioClipboard.append(tmpArray);
    }
    rawAudioFile->seek(oldSeekPos);
}


void Timeline::pasteVideo(int atTimeMSec)
{
    Event value(atTimeMSec);

    int insertIdx = qLowerBound(events.begin(), events.end(), &value, startTimeLessThan) - events.begin() - 1;

    int timeShiftMSec = atTimeMSec - eventsClipboard.first()->startTime;

    QPainter painter(videoPixmap);
    painter.drawPixmap(selectionStartPos, 0,
                       selectionEndPos - selectionStartPos, audioPixmapHeight, tmpVideo);
//    painter.setPen(QPen( QColor( ((PenStroke*)currentEvent)->r*255, ((PenStroke*)currentEvent)->g*255, ((PenStroke*)currentEvent)->b*255)) );

    for (Event* ev : eventsClipboard)
    {
        ev->timeShift(timeShiftMSec);

//        int x1 = ev->startTime * pixelsPerMSec;
//        int x2 = ev->endTime * pixelsPerMSec;

//        painter.drawLine(x1, 0, x2, 0);
    }

//    QVector<Event*> newEvents(events.size() + eventsClipboard.size());

    events = events.mid(0, insertIdx) + eventsClipboard + events.mid(insertIdx); //TODO - improve performance
}


void Timeline::pasteAudio(int atTimeMSec)
{
    int timeShiftMSec = atTimeMSec - eventsClipboard.first()->startTime;

    QPainter painter(audioPixmap);
    painter.drawPixmap(selectionStartPos, 0,
                       selectionEndPos - selectionStartPos, videoPixmapHeight, tmpAudio);

    int oldSeekPos = rawAudioFile->pos();
    int selectionTime = events[selectionEndIdx]->endTime - events[selectionStartIdx]->startTime;
    int deletionSize = sampleSize * samplingFrequency * selectionTime / 1000.0;
    int selectionStart = sampleSize * samplingFrequency * events[selectionStartIdx]->startTime / 1000.0;
    QByteArray zeros(deletionSize, 0);
    rawAudioFile->seek(selectionStart);
    while (deletionSize != 0) deletionSize -= rawAudioFile->write(zeros);
    rawAudioFile->seek(oldSeekPos);
}


void Timeline::resizeEvent(QResizeEvent *event)
{

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
    if (isRecording || isPlaying) return;

    mousePos = event->pos();


    if(event->button() == Qt::MiddleButton)
    {
        mouseMiddleDown = true;

        // Panning the timeline from anchor
        mouseDragStartX = mousePos.x();

        // Save the last "windowStartMSec"
        lastWindowStartMSec = windowStartMSec;
    }

    else if(event->button() == Qt::LeftButton)
    {
        // If the time ruler was pressed
        if (mousePos.y() > timeRulerStart)
        {
            isSettingCursor = true;
            setCursorAt( mousePos.x() );
            if (isPlaying)
            {
                stopPlaying();
                wasPlaying = true;
            }
        }

        // Handle video event manipulation
        else if (videoSelected || audioSelected)
        {
            // We may alter one of them at a time or both audio and video events at the same time
            if (videoSelected)
            {
                handleSelectionPressed(videoSelectionRect, videoLeftScaleArrow, videoRightScaleArrow);
            }
            if (audioSelected)
            {
                handleSelectionPressed(audioSelectionRect, audioLeftScaleArrow, audioRightScaleArrow);
            }
        }

        // Return if already handled the left button press
        if (draggingEvents || scalingEventsLeft || scalingEventsRight) return;

        // Apply modifications if any was made
        if (eventModified)
        {
            // Reset event modification variables
            draggingEvents = false;
            eventModified = false;
            scalingEventsLeft = false;
            scalingEventsRight = false;

            // Apply changes
            pasteVideo(selectionStartTime);
        }

        // If we reached this point, it means we started creating a selection
        isSelecting = true;

        // Reset selection color
        selectionColor = selectionColorNormal;

        // Setup initial selection limits
        selectionStartPos = mousePos.x();
        selectionEndPos = mousePos.x();

        // Selecting just the audio
        if (mousePos.y() > audioTimelineStart)
        {
            videoSelected = false;
            audioSelected = true;
        }

        // Selecting both audio and video
        else if (mousePos.y() > videoTimelineStart + videoTimelineHeight)
        {
            videoSelected = true;
            audioSelected = true;
        }

        // Selecting just the video
        else if (mousePos.y() > videoTimelineStart)
        {
            videoSelected = true;
            audioSelected = false;
        }
    }
}

void Timeline::mouseMoveEvent(QMouseEvent *event)
{
    if (isRecording || isPlaying) return;

    mousePos = event->pos();

    // Update mouse hover variable
    if ( this->rect().contains(mousePos) )
    {
        mouseOver = true;
    }
    else
    {
        mouseOver = false;
    }

    // Case dragging events
    if (draggingEvents)
    {
        eventTimeShiftMSec = (mousePos.x() - mouseDragStartX) * mSecsPerPixel;

        selectionStartTime = events[selectionStartIdx]->startTime + eventTimeShiftMSec;
        selectionEndTime = events[selectionStartIdx]->endTime + eventTimeShiftMSec;
        selectionStartPos = selectionStartTime * pixelsPerMSec;
        selectionEndPos = selectionEndTime * pixelsPerMSec;

        checkForCollision();
    }

    // Case scaling DOWN
    else if (scalingEventsLeft)
    {
        float originalSelectionTimeMSec = events[selectionStartIdx]->endTime - events[selectionStartIdx]->startTime;

        eventTimeShiftMSec = -(mousePos.x() - mouseDragStartX) * mSecsPerPixel;

        eventScaling = (originalSelectionTimeMSec + eventTimeShiftMSec) / originalSelectionTimeMSec;

        selectionStartTime = events[selectionStartIdx]->startTime + eventTimeShiftMSec;
        selectionEndTime = events[selectionStartIdx]->endTime + eventTimeShiftMSec;

        selectionEndTime += (selectionEndTime - selectionStartTime) * eventScaling;

        selectionStartPos = selectionStartTime * pixelsPerMSec;
        selectionEndPos = selectionEndTime * pixelsPerMSec;

        checkForCollision();
    }

    // Case scaling UP
    else if (scalingEventsRight)
    {
        float originalSelectionTimeMSec = events[selectionStartIdx]->endTime - events[selectionStartIdx]->startTime;

        eventTimeShiftMSec = -(mousePos.x() - mouseDragStartX) * mSecsPerPixel;

        eventScaling = (originalSelectionTimeMSec + eventTimeShiftMSec) / originalSelectionTimeMSec;

        selectionStartTime = events[selectionStartIdx]->startTime + eventTimeShiftMSec;
        selectionEndTime = events[selectionStartIdx]->endTime + eventTimeShiftMSec;

        selectionEndTime -= (selectionEndTime - selectionStartTime) * eventScaling;

        selectionStartPos = selectionStartTime * pixelsPerMSec;
        selectionEndPos = selectionEndTime * pixelsPerMSec;

        checkForCollision();
    }

    else if (isSettingCursor)
    {
        setCursorAt( mousePos.x() );
    }
    else if (isSelecting)
    {
        selectionEndPos = mousePos.x();
    }

    else if(mouseMiddleDown)
    {
        windowStartMSec = lastWindowStartMSec + ( mouseDragStartX - event->x() ) * mSecsPerPixel;

        if (windowStartMSec < 0) windowStartMSec = 0;
    }
}

void Timeline::mouseReleaseEvent(QMouseEvent *event)
{
    if (isRecording || isPlaying) return;

    mousePos = event->pos();

    // Case dragging events
    if (draggingEvents)
    {
        draggingEvents = false;
        checkForCollision();

        //Restore normal cursor
        setCursor(Qt::ArrowCursor);
    }

    // Case scaling DOWN
    else if (scalingEventsLeft)
    {
        scalingEventsLeft = false;
        checkForCollision();

        //Restore normal cursor
        setCursor(Qt::ArrowCursor);
    }

    // Case scaling UP
    else if (scalingEventsRight)
    {
        scalingEventsRight = false;
        checkForCollision();

        //Restore normal cursor
        setCursor(Qt::ArrowCursor);
    }

    else if(event->button() == Qt::LeftButton)
    {
        if (isSettingCursor)
        {
            isSettingCursor = false;
            setCursorAt( mousePos.x() );
            if (wasPlaying) startPlaying();
            wasPlaying = false;
        }
        else if (isSelecting)
        {
            isSelecting = false;
            selectionEndPos = mousePos.x();
            if (selectionStartPos == selectionEndPos)
            {
                audioSelected = false;
                videoSelected = false;
            }
            else
            {
                if (selectionStartPos > selectionEndPos)
                {
                    int tmp = selectionStartPos;
                    selectionStartPos = selectionEndPos;
                    selectionEndPos = tmp;
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


void Timeline::handleSelectionPressed(QRect& selectionRect, QPolygon& leftArrow, QPolygon& rightArrow)
{
    // User dragging the event through time
    if (selectionRect.contains(mousePos))
    {
        // Was already selected and being moved around
        if (eventModified)
        {
            // We started dragging again
            draggingEvents = true;

            // Dragging from a new anchor point
            mouseDragStartX = mousePos.x();
        }
        // Just selected it and started moving it around
        else
        {
            // Moving for the first time, after selecting
            eventModified = true;

            // Reset timeshift
            eventTimeShiftMSec = 0;

            // Start dragging
            draggingEvents = true;

            // Dragging from a new anchor point
            mouseDragStartX = mousePos.x();

            // Move the video segment to the clipboard (copy + delete)
            copyVideo();
            deleteSelectedVideo();
        }

        // Change cursor type to "dragging cursor"
        setCursor(Qt::SizeAllCursor);
    }

    // User scaling DOWN the event through time
    else if (leftArrow.containsPoint(mousePos, Qt::OddEvenFill))
    {
        // Was already selected and being scaled
        if (eventModified)
        {
            // We scaling left again
            scalingEventsLeft = true;

            // Scaling from a new anchor point
            mouseDragStartX = mousePos.x();
        }
        // Just selected it and started scaling it around
        else
        {
            // Scaled for the first time, after selecting
            eventModified = true;

            // Reset scale
            eventScaling = 1.0f;

            // Start scaling
            scalingEventsLeft = true;

            // Scaling from a new anchor point
            mouseDragStartX = mousePos.x();

            // Move the video segment to the clipboard (copy + delete)
            copyVideo();
            deleteSelectedVideo();
        }

        // Change cursor type to "scaling cursor"
        setCursor(Qt::SizeHorCursor);
    }

    // User scaling UP the event through time
    else if (rightArrow.containsPoint(mousePos, Qt::OddEvenFill))
    {
        // Was already selected and being scaled
        if (eventModified)
        {
            // We scaling left again
            scalingEventsLeft = true;

            // Scaling from a new anchor point
            mouseDragStartX = mousePos.x();
        }
        // Just selected it and started scaling it around
        else
        {
            // Scaled for the first time, after selecting
            eventModified = true;

            // Reset scale
            eventScaling = 1.0f;

            // Start scaling
            scalingEventsLeft = true;

            // Scaling from a new anchor point
            mouseDragStartX = mousePos.x();

            // Move the video segment to the clipboard (copy + delete)
            copyVideo();
            deleteSelectedVideo();
        }

        // Change cursor type to "scaling cursor"
        setCursor(Qt::SizeHorCursor);
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
