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
    QRectF audioTimelineTarget(0,                audioTimelineStart, width(), audioTimelineHeight);
    QRectF audioTimelineSource(timelineStartPos, 0,                  width(), audioPixmapHeight);

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

            QRect target(x1 - timelineStartPos, videoTimelineStart, x2-x1, videoTimelineHeight);

            painter.drawPixmap(target, tmpVideo, tmpVideo.rect());
        }

        if (selectionStartPos != selectionEndPos)
        {
            painter.setPen(QPen(Qt::transparent));
            painter.setBrush(QBrush(selectionColor));
            painter.drawRect(videoSelectionRect);
        }

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

        if (eventModified)
        {
            int x1 = selectionStartPos;
            int x2 = selectionEndPos + 1;

            QRect target(x1 - timelineStartPos, audioTimelineStart, x2-x1, audioTimelineHeight);

            painter.drawPixmap(target, tmpAudio, tmpAudio.rect());
        }

        if (selectionStartPos != selectionEndPos)
        {
            painter.setPen(QPen(Qt::transparent));
            painter.setBrush(QBrush(selectionColor));
            painter.drawRect(audioSelectionRect);
        }

        if (!isSelecting)
        {
            audioLeftScaleArrow =  QPolygon(scaleArrowLeft.translated( audioSelectionRect.left(),      audioTimelineStart));
            audioRightScaleArrow = QPolygon(scaleArrowRight.translated(audioSelectionRect.right() + 1, audioTimelineStart));

            if ( (audioSelectionRect.contains(mousePos) ||
                  audioLeftScaleArrow.containsPoint(mousePos, Qt::OddEvenFill) ||
                  audioRightScaleArrow.containsPoint(mousePos, Qt::OddEvenFill)) &&
                 !(draggingEvents || scalingEventsLeft || scalingEventsRight || !mouseOver) )
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
    if (timeCursorMSec > totalTimeRecorded && isPlaying) MainWindow::si->stopPlaying();
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

    // If video is selected, check for collision with other stroke events
    if (videoSelected)
    {
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

    selectionStartPos = selectionStartTime * pixelsPerMSec;
    selectionEndPos = selectionEndTime * pixelsPerMSec;
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

//    if (deleteSelectionEndIdx < deleteSelectionStartIdx) return;
//    if (deleteSelectionEndIdx == events.size() || deleteSelectionStartIdx > deleteSelectionEndIdx || deleteSelectionStartIdx == -1)
//    {
//        audioSelected = false;
//        videoSelected = false;
//        selectionEndPos = selectionStartPos;
//        return;
//    }


    // Trim if necessary
    if (deleteSelectionStartIdx > 0 && deleteSelectionEndIdx < events.size() - 1)
    {
        if (fromTime > events[deleteSelectionStartIdx - 1]->startTime &&
            fromTime < events[deleteSelectionStartIdx - 1]->endTime &&
            toTime   > events[deleteSelectionEndIdx + 1]->startTime &&
            toTime   < events[deleteSelectionEndIdx + 1]->endTime)
        {
            events[deleteSelectionStartIdx - 1]->trimRange(fromTime, toTime, deleteSelectionStartIdx, events);

            return;
        }
    }
    if (deleteSelectionStartIdx > 0)
    {
        if (fromTime > events[deleteSelectionStartIdx - 1]->startTime &&
            fromTime < events[deleteSelectionStartIdx - 1]->endTime)
        {
            events[deleteSelectionStartIdx - 1]->trimFrom(fromTime);
        }
    }

    if (deleteSelectionEndIdx < events.size() - 1)
    {
        if (toTime > events[deleteSelectionEndIdx + 1]->startTime &&
            toTime < events[deleteSelectionEndIdx + 1]->endTime &&
            deleteSelectionStartIdx + 1 < events.size())
        {
            events[deleteSelectionStartIdx + 1]->trimUntil(toTime);
        }
//        else
//        {
//            events.remove(deleteSelectionEndIdx + 1);
//        }
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
    int deletionSizeBytes = sampleSize * samplingFrequency * (toTime - fromTime) / 1000.0;
    int selectionStart = sampleSize * samplingFrequency * fromTime / 1000.0;
    QByteArray zeros(deletionSizeBytes, 0);

    // Write zeros to the specified range
    int oldSeekPos = rawAudioFile->pos();
    rawAudioFile->seek(selectionStart);
    while (deletionSizeBytes != 0) deletionSizeBytes -= rawAudioFile->write(zeros);
    rawAudioFile->seek(oldSeekPos);

    // Erase the specified range of the audio pixmap
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

        scale = (float)(selectionEndTime - selectionStartTime) / (float)(lastSelectionEndTime - lastSelectionStartTime);
    }
    else
    {
        timeShiftMSec = timeCursorMSec - lastSelectionStartTime;
    }

    if (scale == 1.0f)
    {
        for (Event* ev : eventsClipboard)
        {
            ev->timeShift(timeShiftMSec);
        }
    }
    else
    {
        for (Event* ev : eventsClipboard)
        {
            ev->scaleAndMove(scale, timeShiftMSec, selectionStartTime);
        }
    }
}


void Timeline::scaleAndMoveSelectedAudio()
{
    if (eventModified)
    {
        audioSelectionStart = selectionStartTime;
        audioSelectionSize = selectionEndTime - selectionStartTime;
        audioScaling = float(audioSelectionSize) / (float)(lastSelectionEndTime - lastSelectionStartTime);
    }
    else
    {
        audioSelectionStart = sampleSize * samplingFrequency * timeCursorMSec / 1000.0;
    }

    // Run SoundStretch utility to scale the audio, if needed
    if (fabs(audioScaling - 1.0f) > 0.01f) scaleAudio();
}


void Timeline::copyVideo()
{
    // Copy the selected video pixmap
    tmpVideo = videoPixmap->copy(selectionStartPos + 1, 0,
                                 selectionEndPos - selectionStartPos - 1, videoPixmapHeight);

    // Empty clipboard
    eventsClipboard.clear();

    // Clone the selected events and move their clones to the clipboard
    for (int i = selectionStartIdx; i <= selectionEndIdx; i++)
    {
        eventsClipboard << events[i]->clone();
    }
}


void Timeline::copyAudio()
{
    // Copy the selected audio pixmap
    tmpAudio = audioPixmap->copy(selectionStartPos + 1, 0,
                                 selectionEndPos - selectionStartPos - 1, audioPixmapHeight);

    // Empty clipboard
    audioClipboard.clear();

    int bytesToRead = sampleSize * samplingFrequency * (selectionEndTime - selectionStartTime) / 1000.0;
    bytesToRead = (bytesToRead % 2 == 0) ? bytesToRead : bytesToRead - 1;
    int selectionStart = sampleSize * samplingFrequency * selectionStartTime / 1000.0;
    selectionStart = (selectionStart % 2 == 0) ? selectionStart : selectionStart - 1;
    QByteArray tmpArray;

    // Fill audioClipboard with selection
    int oldSeekPos = rawAudioFile->pos();
    rawAudioFile->seek(selectionStart);
    while (bytesToRead != 0)
    {
        tmpArray = rawAudioFile->read(bytesToRead);
        bytesToRead -= tmpArray.size();
        audioClipboard.append(tmpArray);
    }
    rawAudioFile->seek(oldSeekPos);

    // Reset variables
    audioSelectionStart = selectionStartTime;
    audioSelectionSize = selectionEndTime - selectionStartTime;
    audioScaling = 1.0f;
}


// Simply pastes the content of the clipboard to the startTime of its first event
void Timeline::pasteVideo()
{
    int atTimeMSec;
    int timeLength;

    // Free some space before pasting the video
    if (eventModified)
    {
        deleteVideo(selectionStartTime, selectionEndTime);

        atTimeMSec = eventsClipboard.first()->startTime;
        timeLength = eventsClipboard.last()->endTime - atTimeMSec;
    }
    else
    {
        deleteVideo(timeCursorMSec, timeCursorMSec + selectionEndTime - selectionStartTime);

        atTimeMSec = timeCursorMSec;
        timeLength = selectionEndTime - selectionStartTime;
    }

    Event value(atTimeMSec);

    int insertIdx = qLowerBound(events.begin(), events.end(), &value, startTimeLessThan) - events.begin();

    // Paste pixmap
    QPainter painter(videoPixmap);
    painter.drawPixmap(atTimeMSec * pixelsPerMSec, 0, timeLength * pixelsPerMSec, videoPixmapHeight, tmpVideo);

//    QVector<Event*> newEvents(events.size() + eventsClipboard.size());

    events = events.mid(0, insertIdx) + eventsClipboard + events.mid(insertIdx); //TODO - improve performance

    Canvas::si->redrawRequested = true;

    eventsClipboard.clear();
}


void Timeline::pasteAudio()
{
    // Paste pixmap
    QPainter painter(audioPixmap);
    painter.drawPixmap(audioSelectionStart * pixelsPerMSec, 0, audioSelectionSize * pixelsPerMSec, audioPixmapHeight, tmpAudio);

    int pasteSizeBytes = audioClipboard.size();
    int selectionStart = sampleSize * samplingFrequency * audioSelectionStart / 1000.0;

    // Write clipboard's content to the audio file
    int oldSeekPos = rawAudioFile->pos();
    rawAudioFile->seek(selectionStart);
    while (pasteSizeBytes != 0) pasteSizeBytes -= rawAudioFile->write(audioClipboard);
    rawAudioFile->seek(oldSeekPos);

    audioClipboard.clear();
}


void Timeline::paste()
{
    if (!eventsClipboard.empty())
    {
        pasteVideo();
    }

    if (!audioClipboard.isEmpty())
    {
        pasteAudio();
    }

    videoSelected = false;
    audioSelected = false;
    draggingEvents = false;
    eventModified = false;
    scalingEventsLeft = false;
    scalingEventsRight = false;
    eventTimeExpansionRightMSec = 0;
    eventTimeExpansionLeftMSec = 0;
    eventTimeShiftMSec = 0;
}


void Timeline::apply()
{
    if (/*videoSelected*/ !eventsClipboard.empty())
    {
        scaleAndMoveSelectedVideo();
    }
    if (/*audioSelected*/ !audioClipboard.isEmpty())
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
    videoSelected = false;
    audioSelected = false;
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

