#include "timeline.h"

#include <QMenu>

void Timeline::mousePressEvent(QMouseEvent *event)
{
    mousePos = event->pos();

    if(event->button() == Qt::MiddleButton)
    {
        mouseMiddleDown = true;

        // Panning the timeline from anchor
        mouseDragStartX = mousePos.x();

        // Save the last "windowStartMSec"
        lastWindowStartMSec = windowStartMSec;

        // Change to panning cursor
        setCursor(Qt::ClosedHandCursor);
    }

    else if(event->button() == Qt::LeftButton)
    {
        // If the time ruler was pressed
        if (mousePos.y() > timeRulerStart)
        {
            isSettingCursor = true;
            setCursorAt(mousePos.x());

            // Stop playing and start playing again once LMB is released
            if (isPlaying)
            {
                stopPlaying();
                wasPlaying = true;
            }

            return;
        }

        if (isRecording || isPlaying) return;

        // Handle video event manipulation
        if (videoSelected || audioSelected)
        {
            if ( handleSelectionPressed(videoSelectionRect, videoLeftScaleArrow, videoRightScaleArrow) ) return;
            if ( handleSelectionPressed(audioSelectionRect, audioLeftScaleArrow, audioRightScaleArrow) ) return;
        }

        // Cancel modifications
        if (eventModified) paste();

        // If we reached this point, it means we started creating a selection
        isSelecting = true;

        // Reset selection color
        selectionColor = selectionColorNormal;

        // Setup initial selection limits
        selectionStartPos = mousePos.x() + timelineStartPos;
        selectionEndPos = mousePos.x() + timelineStartPos;
        selectionStartTime = selectionStartPos * mSecsPerPixel;
        selectionEndTime = selectionEndPos * mSecsPerPixel;

        // Selecting just the audio
        if (mousePos.y() > audioTimelineStart)
        {
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
        }
    }
}

void Timeline::mouseMoveEvent(QMouseEvent *event)
{
    mousePos = event->pos();

    if(mouseMiddleDown)
    {
        windowStartMSec = lastWindowStartMSec + ( mouseDragStartX - event->x() ) * mSecsPerPixel;

        if (windowStartMSec < 0) windowStartMSec = 0;
    }

    else if (isSettingCursor)
    {
        setCursorAt( mousePos.x() );
    }

    if (isRecording || isPlaying) return;

    // Case dragging events
    if (draggingEvents)
    {
        eventTimeShiftMSec = (mousePos.x() - mouseDragStartX) * mSecsPerPixel;

        selectionStartTime = lastSelectionStartTime + eventTimeShiftMSec + eventTimeExpansionLeftMSec;
        selectionEndTime = lastSelectionEndTime + eventTimeShiftMSec + eventTimeExpansionRightMSec;
        selectionStartPos = selectionStartTime * pixelsPerMSec;
        selectionEndPos = selectionEndTime * pixelsPerMSec;

        checkForCollision();
    }

    // Case scaling DOWN
    else if (scalingEventsLeft)
    {
        eventTimeExpansionLeftMSec = (mousePos.x() - mouseDragStartX) * mSecsPerPixel;

        selectionStartTime = lastSelectionStartTime + eventTimeShiftMSec + eventTimeExpansionLeftMSec;
        selectionEndTime = lastSelectionEndTime + eventTimeShiftMSec + eventTimeExpansionRightMSec;
        selectionStartPos = selectionStartTime * pixelsPerMSec;
        selectionEndPos = selectionEndTime * pixelsPerMSec;

        checkForCollision();
    }

    // Case scaling UP
    else if (scalingEventsRight)
    {
        eventTimeExpansionRightMSec = (mousePos.x() - mouseDragStartX) * mSecsPerPixel;

        selectionStartTime = lastSelectionStartTime + eventTimeShiftMSec + eventTimeExpansionLeftMSec;
        selectionEndTime = lastSelectionEndTime + eventTimeShiftMSec + eventTimeExpansionRightMSec;
        selectionStartPos = selectionStartTime * pixelsPerMSec;
        selectionEndPos = selectionEndTime * pixelsPerMSec;

        checkForCollision();
    }

    else if (isSelecting)
    {
        selectionEndPos = mousePos.x() + timelineStartPos;
    }
}

void Timeline::mouseReleaseEvent(QMouseEvent *event)
{
    mousePos = event->pos();

    if(event->button() == Qt::MiddleButton)
    {
        mouseMiddleDown = false;

        //Restore normal cursor
        setCursor(Qt::ArrowCursor);
    }

    if(event->button() == Qt::LeftButton)
    {
        if (isSettingCursor)
        {
            isSettingCursor = false;
            setCursorAt( mousePos.x() );
            if (wasPlaying) startPlaying();
            wasPlaying = false;
        }

        if (isRecording || isPlaying) return;

        if (isSelecting && !isSettingCursor)
        {
            isSelecting = false;

            selectionEndPos = mousePos.x() + timelineStartPos;
            selectionEndTime = selectionEndPos * mSecsPerPixel;

            // If nothing was selected, disable selection mode
            if (selectionStartPos == selectionEndPos)
            {
                audioSelected = false;
                videoSelected = false;
            }
            // Else, trim selection
            else
            {
                // Ensure start is greater than end
                if (selectionStartPos > selectionEndPos)
                {
                    int tmp = selectionStartPos;
                    selectionStartPos = selectionEndPos;
                    selectionEndPos = tmp;

                    tmp = selectionStartTime;
                    selectionStartTime = selectionEndTime;
                    selectionEndTime = tmp;
                }

                // Apply trimming
                if (videoSelected) selectVideo();
                if (audioSelected) selectAudio();
            }
        }
    }

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
}


bool Timeline::handleSelectionPressed(QRect& selectionRect, QPolygon& leftArrow, QPolygon& rightArrow)
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
            mouseDragStartX = mousePos.x() - eventTimeShiftMSec * pixelsPerMSec;
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

            // Move the video segment to the clipboard (cut = copy + delete)
            cut();

            // Change cursor type to "scaling cursor"
            setCursor(Qt::SizeHorCursor);
        }

        return true;
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
            mouseDragStartX = mousePos.x() - eventTimeExpansionLeftMSec * pixelsPerMSec;
        }
        // Just selected it and started scaling it around
        else
        {
            // Scaled for the first time, after selecting
            eventModified = true;

            // Reset left expansion
            eventTimeExpansionLeftMSec = 0;

            // Start scaling
            scalingEventsLeft = true;

            // Scaling from a new anchor point
            mouseDragStartX = mousePos.x();

            // Move the video segment to the clipboard (cut = copy + delete)
            cut();

            // Change cursor type to "scaling cursor"
            setCursor(Qt::SizeHorCursor);
        }

        return true;
    }

    // User scaling UP the event through time
    else if (rightArrow.containsPoint(mousePos, Qt::OddEvenFill))
    {
        // Was already selected and being scaled
        if (eventModified)
        {
            // We scaling left again
            scalingEventsRight = true;

            // Scaling from a new anchor point
            mouseDragStartX = mousePos.x() - eventTimeExpansionRightMSec * pixelsPerMSec;
        }
        // Just selected it and started scaling it around
        else
        {
            // Scaled for the first time, after selecting
            eventModified = true;

            // Reset right expansion
            eventTimeExpansionRightMSec = 0;

            // Start scaling
            scalingEventsRight = true;

            // Scaling from a new anchor point
            mouseDragStartX = mousePos.x();

            // Move the video segment to the clipboard (cut = copy + delete)
            cut();

            // Change cursor type to "scaling cursor"
            setCursor(Qt::SizeHorCursor);
        }

        return true;
    }

    return false;
}


void Timeline::wheelEvent(QWheelEvent *event)
{
    windowStartMSec -= event->delta() * 8.0;

    if (windowStartMSec < 0) windowStartMSec = 0;
}


void Timeline::enterEvent(QEvent *event)
{
    mouseOver = true;
}


void Timeline::leaveEvent(QEvent *event)
{
    mouseOver = false;
}


void Timeline::setCursorAt(int x)
{
    timeCursorMSec = x * mSecsPerPixel + windowStartMSec;

    if (timeCursorMSec > totalTimeRecorded) timeCursorMSec = totalTimeRecorded;
    if (timeCursorMSec < 0) timeCursorMSec = 0;

    Canvas::si->redrawRequested = true;
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
            apply();
            paste();
        }
        if (selectedItem->text() == "Cancel")
        {
            paste();
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

        if (selectedItem->text() == "Record")
        {
            recordToSlot();
        }
        else if (selectedItem->text() == "Cut")
        {
            cut();
            unselect();
        }
        else if (selectedItem->text() == "Copy")
        {
            copy();
            unselect();
        }
        else if (selectedItem->text() == "Paste")
        {
            apply();
            paste();
            unselect();
        }
        if (selectedItem->text() == "Erase")
        {
            erase();
            unselect();
        }
        else if (selectedItem->text() == "Undo")
        {
            undo();
        }
        else if (selectedItem->text() == "Redo")
        {
            redo();
        }
    }
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
        else
        {
            if (timeCursorMSec < eventToDraw->endTime) break;
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
