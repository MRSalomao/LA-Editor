#include "timeline.h"
#include "strokerenderer.h"
#include "canvas.h"


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

        if (eventModified)
        {
            int x1 = selectionStartPos;
            int x2 = selectionEndPos + 1;

            QRect target(x1 - timelineStartPos, 0, x2-x1, videoTimelineHeight);

            painter.drawPixmap(target, tmpVideo, tmpVideo.rect());
        }

        painter.setPen(QPen(Qt::transparent));
        painter.setBrush(QBrush(selectionColor));
        painter.drawRect(videoSelectionRect);

        if (!isSelecting)
        {
            videoLeftScaleArrow =  QPolygon(scaleArrowLeft.translated( videoSelectionRect.left(),      videoTimelineStart));
            videoRightScaleArrow = QPolygon(scaleArrowRight.translated(videoSelectionRect.right() + 1, videoTimelineStart));

            if ( (videoSelectionRect.contains(mousePos) ||
                  videoLeftScaleArrow.containsPoint(mousePos, Qt::OddEvenFill) ||
                  videoRightScaleArrow.containsPoint(mousePos, Qt::OddEvenFill)) &&
                 !(draggingEvents || scalingEventsLeft || scalingEventsRight || !mouseOver) )
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

        // TODO: event modified

        painter.setPen(QPen(Qt::transparent));
        painter.setBrush(QBrush(selectionColor));
        painter.drawRect(audioSelectionRect);

        if (!isSelecting)
        {
            audioLeftScaleArrow =  QPolygon(scaleArrowLeft.translated( audioSelectionRect.left(),      audioTimelineStart));
            audioRightScaleArrow = QPolygon(scaleArrowRight.translated(audioSelectionRect.right() + 1, audioTimelineStart));

            if ( (audioSelectionRect.contains(mousePos) ||
                  audioLeftScaleArrow.containsPoint(mousePos, Qt::OddEvenFill) ||
                  audioRightScaleArrow.containsPoint(mousePos, Qt::OddEvenFill)) &&
                 !(draggingEvents || scalingEventsLeft || scalingEventsRight) )
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

    // Draw timeline pointer triangle
    if ( mouseOver && !(isRecording || isPlaying) )
    {
        painter.setPen(QPen(QColor(0,0,0, 150)));
        painter.setBrush(QBrush(QColor(0,0,0, 40)));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawConvexPolygon( pointerTriangle.translated(mousePos.x(), height()) ); // make it optional
    }

    // Draw timecursor
    float cursorPos = (timeCursorMSec - windowStartMSec) * pixelsPerMSec;
    painter.setPen(QPen(QColor(0,0,0,250), 0, Qt::DotLine));
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.drawLine((int)cursorPos, 0, (int)cursorPos, height());

    if (!MainWindow::si->childWindowOpen) update();

    // Stop playing if end of file is reached
    if (timeCursorMSec > totalTimeRecorded) MainWindow::si->stopPlaying();
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


bool startTimeLessThan(const Event* e1, const Event* e2)
{
    return e1->startTime < e2->startTime;
}


bool endTimeLessThan(const Event* e1, const Event* e2)
{
    return e1->endTime < e2->endTime;
}


void Timeline::checkForCollision()
{
    // Check if selection is below zero seconds
    if (selectionStartTime < 0)
    {
        // If so, apply corrections
        eventTimeShiftMSec -= selectionStartTime;
        selectionEndTime   -= selectionStartTime;
        selectionStartTime  = 0;
        selectionStartPos   = selectionStartTime * pixelsPerMSec;
        selectionEndPos     = selectionEndTime * pixelsPerMSec;
    }

    // Check if selection is above the total recorded time
    if (selectionEndTime > totalTimeRecorded)
    {
        // If so, apply corrections
        int adjustment = totalTimeRecorded - selectionEndTime;

        eventTimeShiftMSec += adjustment;
        selectionStartTime += adjustment;
        selectionEndTime   += adjustment;
        selectionStartPos   = selectionStartTime * pixelsPerMSec;
        selectionEndPos     = selectionEndTime * pixelsPerMSec;
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


void Timeline::selectVideo()
{
    Event value(selectionStartTime);

    QVector<Event*>::iterator i = qLowerBound(events.begin(), events.end(), &value, startTimeLessThan);

    selectionStartIdx = i - events.begin();
    if (selectionStartIdx == events.size())
    {
        audioSelected = false;
        videoSelected = false;
        selectionEndPos = selectionStartPos;
        return;
    }

    value.endTime = selectionEndTime;
    i = qUpperBound(events.begin(), events.end(), &value, endTimeLessThan);

    selectionEndIdx = i - events.begin() - 1;
    if (selectionEndIdx == events.size() || selectionStartIdx > selectionEndIdx || selectionStartIdx == -1)
    {
        audioSelected = false;
        videoSelected = false;
        selectionEndPos = selectionStartPos;
        return;
    }

    if (shiftPressed)
    {
        selectionStartTime = selectionStartPos * mSecsPerPixel;
        selectionEndTime = selectionEndPos * mSecsPerPixel;
    }
    else
    {
        selectionStartTime = events[selectionStartIdx]->startTime;
        selectionEndTime = events[selectionEndIdx]->endTime;
        selectionStartPos = selectionStartTime * pixelsPerMSec;
        selectionEndPos = selectionEndTime * pixelsPerMSec;
    }

    lastSelectionStartTime = selectionStartTime;
    lastSelectionEndTime = selectionEndTime;
    lastSelectionStartPos = selectionStartPos;
    lastSelectionEndPos = selectionEndPos;
}


void Timeline::selectAudio()
{
    if (selectionStartTime > totalTimeRecorded) selectionStartTime = totalTimeRecorded;
    if (selectionEndTime > totalTimeRecorded) selectionEndTime = totalTimeRecorded;
    if (selectionStartTime < 0) selectionStartTime = 0;
    if (selectionStartTime == selectionEndTime) audioSelected = false;

    selectionStartTime = events[selectionStartIdx]->startTime;
    selectionEndTime = events[selectionEndIdx]->endTime;

    if (shiftPressed && videoSelected)
    {
        selectionStartTime = selectionStartPos * mSecsPerPixel;
        selectionEndTime = selectionEndPos * mSecsPerPixel;
    }
    else
    {
        selectionStartTime = events[selectionStartIdx]->startTime;
        selectionEndTime = events[selectionEndIdx]->endTime;
        selectionStartPos = selectionStartTime * pixelsPerMSec;
        selectionEndPos = selectionEndTime * pixelsPerMSec;
    }

    lastSelectionStartTime = selectionStartTime;
    lastSelectionEndTime = selectionEndTime;
    lastSelectionStartPos = selectionStartPos;
    lastSelectionEndPos = selectionEndPos;
}


void Timeline::deleteVideo(int fromTime, int toTime)
{
    Event value(fromTime);

    QVector<Event*>::iterator i = qLowerBound(events.begin(), events.end(), &value, startTimeLessThan);

    int deleteSelectionStartIdx = i - events.begin();
//    if (deleteSelectionStartIdx == events.size())
//    {
//        audioSelected = false;
//        videoSelected = false;
//        selectionEndPos = selectionStartPos;
//        return;
//    }

    value.endTime = toTime;
    i = qUpperBound(events.begin(), events.end(), &value, endTimeLessThan);

    int deleteSelectionEndIdx = i - events.begin() - 1;
//    if (deleteSelectionEndIdx == events.size() || deleteSelectionStartIdx > deleteSelectionEndIdx || deleteSelectionStartIdx == -1)
//    {
//        audioSelected = false;
//        videoSelected = false;
//        selectionEndPos = selectionStartPos;
//        return;
//    }


    // Trim if necessary
    if (deleteSelectionStartIdx > 0)
    {
        if (selectionStartTime > events[deleteSelectionStartIdx - 1]->startTime &&
            selectionStartTime < events[deleteSelectionStartIdx - 1]->endTime)
        {
            events[deleteSelectionStartIdx - 1]->trimFrom(fromTime);
        }
    }

    if (deleteSelectionEndIdx < events.size() - 1)
    {
        if (selectionEndTime > events[deleteSelectionEndIdx + 1]->startTime &&
            selectionEndTime < events[deleteSelectionEndIdx + 1]->endTime)
        {
            events[deleteSelectionStartIdx + 1]->trimUntil(toTime);
        }
        else
        {
//            events.remove(deleteSelectionEndIdx + 1);
        }
    }

    //TODO: delete
    events.erase(events.begin() + deleteSelectionStartIdx, events.begin() + deleteSelectionEndIdx + 1);

    Canvas::si->redrawRequested = true;

    int x1 = fromTime * pixelsPerMSec - 1;
    int x2 = toTime   * pixelsPerMSec + 1;

    QPainter painter(videoPixmap);
    painter.fillRect(x1, 0, x2-x1, videoPixmapHeight, timelineColor);
}


void Timeline::deleteAudio(int fromTime, int toTime)//TODO
{
    int oldSeekPos = rawAudioFile->pos();
    int selectionTime = events[selectionEndIdx]->endTime - events[selectionStartIdx]->startTime;
    int deletionSize = sampleSize * samplingFrequency * selectionTime / 1000.0;
    int selectionStart = sampleSize * samplingFrequency * events[selectionStartIdx]->startTime / 1000.0;
    QByteArray zeros(deletionSize, 0);
    rawAudioFile->seek(selectionStart);
    while (deletionSize != 0) deletionSize -= rawAudioFile->write(zeros);
    rawAudioFile->seek(oldSeekPos);

    int x1 = lastSelectionStartPos - 1;
    int x2 = lastSelectionEndPos + 1;

    QPainter painter(audioPixmap);
    painter.fillRect(x1, 0, x2-x1, audioPixmapHeight, timelineColor);
}


void Timeline::scaleAndMoveSelectedVideo()
{
    int timeShiftMSec;
    float scale = 1.0f;

    if (eventModified)
    {
        timeShiftMSec = selectionStartTime - lastSelectionStartTime;

        scale = (selectionEndTime - selectionStartTime) / (lastSelectionEndTime - lastSelectionStartTime);
    }
    else
    {
        timeShiftMSec = timeCursorMSec - lastSelectionStartTime;
    }

    if (scale == 1.0f)
    {
        for (Event* ev : eventsClipboard)
        {
            qDebug("No scale");
            ev->timeShift(timeShiftMSec);
        }
    }
    else
    {
        for (Event* ev : eventsClipboard)
        {
            qDebug("Scaled");
            ev->scaleAndMove(scale, timeShiftMSec);
        }
    }
}


void Timeline::scaleAndMoveSelectedAudio()
{

}


void Timeline::copyVideo()
{
    tmpVideo = videoPixmap->copy(selectionStartPos + 1, 0,
                                 selectionEndPos - selectionStartPos-1, 1);

    eventsClipboard.clear();

    for (int i = selectionStartIdx; i <= selectionEndIdx; i++)
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


void Timeline::pasteVideo()
{
    int atTimeMSec = eventsClipboard.first()->startTime;
    int timeLength = eventsClipboard.last()->endTime - atTimeMSec;

    Event value(atTimeMSec);

    int insertIdx = qLowerBound(events.begin(), events.end(), &value, startTimeLessThan) - events.begin();

    QPainter painter(videoPixmap);
    painter.drawPixmap(atTimeMSec * pixelsPerMSec, 0, timeLength * pixelsPerMSec, videoPixmapHeight, tmpVideo);

//    QVector<Event*> newEvents(events.size() + eventsClipboard.size());

    events = events.mid(0, insertIdx) + eventsClipboard + events.mid(insertIdx); //TODO - improve performance

    Canvas::si->redrawRequested = true;
}


void Timeline::pasteAudio() //TODO - imitate video pasting
{
//    int timeShiftMSec = atTimeMSec - eventsClipboard.first()->startTime;

//    QPainter painter(audioPixmap);
//    painter.drawPixmap(selectionStartPos, 0, selectionEndPos - selectionStartPos, audioPixmapHeight, tmpAudio);

//    int oldSeekPos = rawAudioFile->pos();
//    int selectionTime = events[selectionEndIdx]->endTime - events[selectionStartIdx]->startTime;
//    int deletionSize = sampleSize * samplingFrequency * selectionTime / 1000.0;
//    int selectionStart = sampleSize * samplingFrequency * events[selectionStartIdx]->startTime / 1000.0;
//    QByteArray zeros(deletionSize, 0);
//    rawAudioFile->seek(selectionStart);
//    while (deletionSize != 0) deletionSize -= rawAudioFile->write(zeros);
//    rawAudioFile->seek(oldSeekPos);
}


void Timeline::paste()
{
    if (videoSelected)
    {
        pasteVideo();
    }

    if (audioSelected)
    {
        pasteAudio();
    }

    videoSelected = false;
    audioSelected = false;
    draggingEvents = false;
    eventModified = false;
    scalingEventsLeft = false;
    scalingEventsRight = false;
}


void Timeline::apply()
{
    if (videoSelected)
    {
        scaleAndMoveSelectedVideo();
    }
    if (audioSelected)
    {
        scaleAndMoveSelectedAudio();
    }
}


void Timeline::copy()
{
    if (videoSelected)
    {
        copyVideo();
    }
    if (audioSelected)
    {
        copyAudio();
    }
}


void Timeline::cut()
{
    copy();
    erase();
}


void Timeline::unselect()
{
//    videoSelected = false;
//    audioSelected = false;
}


void Timeline::erase()
{
    if (videoSelected)
    {
        deleteVideo(selectionStartTime, selectionEndTime);
    }
    if (audioSelected)
    {
        deleteAudio(selectionStartTime, selectionEndTime);
    }
}


void Timeline::undo()
{
    qDebug("TODO implement UNDO");
}


void Timeline::redo()
{
    qDebug("TODO implement REDO");
}

void Timeline::recordToSlot()
{
    qDebug("TODO implement REC");
}

