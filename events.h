#ifndef EVENTS_H
#define EVENTS_H

#include <QMatrix4x4>
#include <QPixmap>
#include <iostream>
#include <QColor>
#include <mainwindow.h>

#include "timeline.h"
#include "strokerenderer.h"

class Event
{
public:

    static int subeventToDrawIdx;
    static QPointF cursorPos;
    static Event* activeEvent;
    static QVector<Event*> allEvents;

    int startTime = -1, endTime = -1;

    int type = -1, ID = -1;

    QRectF selectionRect;
    QMatrix4x4 transform;

    enum { STROKE_EVENT,
           POINTER_MOVEMENT_EVENT,
           CTRL_Z_EVENT,
           SCROLL_EVENT };


    Event(int startT, bool isLocal=true);

    Event();

    static void setActiveID(int ID, QPointF pressPos);

    static void handleDrag(QPointF deltaPos);

    static void setSubeventIndex(int idx);

    static int getSubeventIndex();

    static void deleteAllEvents();

    virtual void mouseDragged(QPointF deltaPos) {}

    virtual QPointF getCursorPos(int time) {return QPointF(100,100);}

    virtual void timeShift(int time) {}

    virtual void scaleAndMove(float scale, int timeShiftMSec, int pivot) {}

    virtual void trimRange(int from, int to, int insertIdx, QVector<Event*> &events) {}

    virtual void trimFrom(int from) {}

    virtual void trimUntil(int to) {}

    virtual Event* clone() const {}

    virtual ~Event() {}

    virtual QRectF getSelectionRect()
    {
        QPointF corner1 = transform * selectionRect.topLeft();
        QPointF corner2 = transform * selectionRect.bottomRight();

        return QRectF(corner1, corner2);
    }

    void init();
};

class PenStroke : public Event
{
public:
    struct Subevent
    {
        int t,pbIdx; float x,y;
        Subevent() {}
        Subevent(int t, float x, float y, int pbIdx) : t(t), x(x), y(y), pbIdx(pbIdx) {}
    };

    int pbStart;
    float r=0, g=0, b=0;
    float ptSize = 3;
    QVector<Subevent> subevents;
    float selectionSpacing = 10.0f;

    ~PenStroke() {}

    PenStroke(int pbStart, int startT) :
        Event(startT, false),
        pbStart(pbStart-1) // First event is always a point and pbStart is where the pbIdx is AFTER its been inserted in the pb
    {
        r = MainWindow::si->activeColor.redF();
        g = MainWindow::si->activeColor.greenF();
        b = MainWindow::si->activeColor.blueF();

        type = STROKE_EVENT;
    }

    virtual PenStroke* clone() const;

    void mouseDragged(QPointF deltaPos);

    void closeStrokeEvent(int endT)
    {
        endTime = endT;
    }

    void addStrokeEvent(int t, float x, float y, int pbo)
    {
        subevents << Subevent(t, x, y, pbo);

        if (subevents.size() == 1)
        {
            selectionRect.setCoords(x,y,x,y);
        }
        else
        {
            if ( y > selectionRect.top()    ) selectionRect.setTop   (y);
            if ( y < selectionRect.bottom() ) selectionRect.setBottom(y);
            if ( x > selectionRect.right()  ) selectionRect.setRight (x);
            if ( x < selectionRect.left()   ) selectionRect.setLeft  (x);
        }
    }

    void scaleAndMove(float scale, int timeShiftMSec, int pivot);

    QPointF getCursorPos(int time)
    {
        return cursorPos;
    }

    void timeShift(int time);

    static bool timeLessThan(const Subevent se1, const Subevent se2)
    {
        return se1.t < se2.t;
    }

    void trimRange(int from, int to, int insertIdx, QVector<Event*> &events);

    void trimFrom(int from);

    void trimUntil(int to);

    bool drawUntil(int time);

    bool drawFromIndexUntil(int limitTime);
};

class EraserStroke : public PenStroke
{
public:

    EraserStroke(int pbStart, int startT) :
        PenStroke(pbStart, startT)
    {
        r = 1; g = 1; b = 1;

        ptSize = 20;
    }
};

class PointerMovement : public Event
{
public:

    struct Subevent
    {
        int t; float x,y;
        Subevent() {}
        Subevent(int t, float x, float y) : t(t), x(x), y(y) {}
    };

    QVector<Subevent> subevents;

    ~PointerMovement() {}

    PointerMovement(int startT) :
        Event(startT, false)
    {
        type = POINTER_MOVEMENT_EVENT;
    }

    PointerMovement(int startT, int endT, QVector<Subevent> subevents) :
        Event(startT, false)
    {
        endTime = endT;

        this->subevents = subevents;

        type = POINTER_MOVEMENT_EVENT;
    }

    virtual PointerMovement* clone() const
    {
        return new PointerMovement(*this);
    }

    static bool timeLessThan(const Subevent se1, const Subevent se2)
    {
        return se1.t < se2.t;
    }

    void trimRange(int from, int to, int insertIdx, QVector<Event*> &events);

    void trimFrom(int time);

    void trimUntil(int time);

    void scaleAndMove(float scale, int timeShiftMSec, int pivot);

    void timeShift(int time);

    QPointF getCursorPos(int time);

    void addPointerEvent(int t, float x, float y)
    {
        subevents << Subevent(t, x, y);
    }

    void closePointerEvent(int endT)
    {
        endTime = endT;
    }
};

class ViewportScroll : public Event
{
    ~ViewportScroll() {}

    ViewportScroll(int t, float y) :
        Event(t)
    {
        type = SCROLL_EVENT;

        endTime = t;
    }
};

class CtrlZ : public Event
{
public:
    QColor eventColor = QColor(70, 70, 70);

    ~CtrlZ() {}

    CtrlZ(int t) :
        Event(t)
    {
        type = CTRL_Z_EVENT;

        endTime = t;
    }
};

//    events.append((unsigned char)(timestamp >> 0 ));
//    events.append((unsigned char)(timestamp >> 8 ));
//    events.append((unsigned char)(timestamp >> 16));

#endif
