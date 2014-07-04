#ifndef EVENTS_H
#define EVENTS_H

#include <QMatrix4x4>
#include <QPixmap>
#include "strokerenderer.h"
#include <iostream>
#include <QColor>
#include <mainwindow.h>
#include "timeline.h"

static int subeventToDrawIdx = 0;
static QPointF cursorPos;

class Event;
static QVector<Event*> allEvents;

class Event
{
public:
    int startTime = -1, endTime = -1;

    int type = -1, ID = -1;

    enum { STROKE_EVENT,
           POINTER_MOVEMENT_EVENT,
           CTRL_Z_EVENT,
           SCROLL_EVENT };


    Event(int startT) : startTime(startT)
    {
        init();
    }

    Event()
    {
        init();
    }

    static void setSubeventIndex(int idx)
    {
        subeventToDrawIdx = idx;
    }

    static int getSubeventIndex()
    {
        return subeventToDrawIdx;
    }

    static void deleteAllEvents()
    {
        qDeleteAll(allEvents);
    }

    virtual QPointF getCursorPos(int time) {return QPointF(100,100);}

    virtual void timeShift(int time) {}

    virtual void scaleAndMove(float scale, int timeShiftMSec, int pivot) {}

    virtual void trimRange(int from, int to, int insertIdx, QVector<Event*> &events) {}

    virtual void trimFrom(int from) {}

    virtual void trimUntil(int to) {}

    virtual Event* clone() const {}

    virtual ~Event() {}

private:

    void init()
    {
        ID = allEvents.size();
        allEvents.push_back(this);
    }
};

class PenStroke : public Event
{
public:
    int pbStart;
    float r=0, g=0, b=0;
    float ptSize = 3;

    QMatrix4x4 transform;

    struct Subevent
    {
        int t,pbIdx; float x,y;
        Subevent() {}
        Subevent(int t, float x, float y, int pbIdx) : t(t), x(x), y(y), pbIdx(pbIdx) {}
    };

    QVector<Subevent> subevents;

    ~PenStroke() {}

    PenStroke(int pbStart, int startT) :
        Event(startT),
        pbStart(pbStart-1) // First event is always a point and pbStart is where the pbIdx is AFTER its been inserted in the pb
    {
        r = MainWindow::si->activeColor.redF();
        g = MainWindow::si->activeColor.greenF();
        b = MainWindow::si->activeColor.blueF();

        type = STROKE_EVENT;
    }

    virtual PenStroke* clone() const
    {
        return new PenStroke(*this);
    }

    void closeStrokeEvent(int endT)
    {
        endTime = endT;
    }

    void addStrokeEvent(int t, float x, float y, int pbo)
    {
        subevents << Subevent(t, x, y, pbo);
    }

    void scaleAndMove(float scale, int timeShiftMSec, int pivot)
    {
        startTime += timeShiftMSec;
        startTime = (startTime - pivot) * scale + pivot;

        for (Subevent& se : subevents)
        {
            se.t += timeShiftMSec;
            se.t = (se.t - pivot) * scale + pivot;
        }

        endTime += timeShiftMSec;
        endTime = (endTime - pivot) * scale + pivot ;
    }

    QPointF getCursorPos(int time)
    {
        return cursorPos;
    }

    void timeShift(int time)
    {
        startTime += time;
        endTime += time;

        for (Subevent& se : subevents)
        {
            se.t += time;
        }
    }

    static bool timeLessThan(const Subevent se1, const Subevent se2)
    {
        return se1.t < se2.t;
    }

    void trimRange(int from, int to, int insertIdx, QVector<Event*> &events)
    {
        Subevent valueFrom(from, 0, 0, 0);

        QVector<Subevent>::iterator f =
                qLowerBound(subevents.begin(), subevents.end(), valueFrom, timeLessThan);

        Subevent valueTo(to, 0, 0, 0);

        QVector<Subevent>::iterator t =
                qLowerBound(subevents.begin(), subevents.end(), valueTo, timeLessThan) + 1;

        if (t > subevents.end()) t--;

        subevents.erase(f, t);

        startTime = subevents.first().t;
        endTime = subevents.last().t;
    }

    void trimFrom(int from)
    {
        Subevent value(from, 0, 0, 0);

        QVector<Subevent>::iterator i =
                qLowerBound(subevents.begin(), subevents.end(), value, timeLessThan);

        subevents.erase(i, subevents.end());

        endTime = subevents.last().t;
    }

    void trimUntil(int to)
    {
        Subevent value(to, 0, 0, 0);

        QVector<Subevent>::iterator i =
                qLowerBound(subevents.begin(), subevents.end(), value, timeLessThan) + 1;

        if (i > subevents.end()) i--;

        subevents.erase(subevents.begin(), i);

        startTime = subevents.first().t;
    }

    bool drawUntil(int time)
    {
        bool reachedTimeCursor = false;
        int to = subevents.back().pbIdx;

        for (int i = 0; i < subevents.size(); i++)
        {
            if (subevents[i].t > time)
            {
                if (i==0)
                {
                    to = pbStart;
                    subeventToDrawIdx = 0;
                }
                else
                {
                    to = subevents[i-1].pbIdx;
                    subeventToDrawIdx = i;
                }

                reachedTimeCursor = true;

                break;
            }
        }

        StrokeRenderer::si->drawStrokeSpritesRange(pbStart, to, r, g, b, ptSize, transform, ID);

        return reachedTimeCursor;
    }

    bool drawFromIndexUntil(int limitTime)
    {
        bool reachedLimit = false;

        // Draw from the index before where we stopped or from the start index, if refering to the first index
        int from = subeventToDrawIdx == 0 ? pbStart : subevents[subeventToDrawIdx-1].pbIdx;

        // Starting off, we don't draw anything
        int to = from;

        // From the current subevent and on...
        for (; subeventToDrawIdx < subevents.size(); subeventToDrawIdx++)
        {
            // If the subevent's timestamp goes over the limitTime
            if (subevents[subeventToDrawIdx].t <= limitTime)
            {
                // Decrease the index, as we want the subevent right before going over the limitTime and get its pointBuffer index
                to = subevents[subeventToDrawIdx].pbIdx;

                // Update the cursor pos
                cursorPos = QPointF(subevents[subeventToDrawIdx].x, subevents[subeventToDrawIdx].y);
            }
            else
            {
                // timeLimit was reached
                reachedLimit = true;

                // Stop looking
                break;
            }
        }

        // If last index limit has been reached
        if (subeventToDrawIdx == subevents.size())
        {
            // If this event is closed (complete) and we reached the last index
            if (endTime >= 0)
            {
                // Reset subeventIndex
                subeventToDrawIdx = 0;

                // Finish drawing the whole event
                to = subevents.back().pbIdx;

                // Update the cursor pos
                cursorPos = QPointF(subevents.back().x, subevents.back().y);
            }
            else
            {
                // Else, it's still being drawn, and therefore we hit the timecursor
                reachedLimit = true;
            }
        }

        if ( !(to == from) ) StrokeRenderer::si->drawStrokeSpritesRange(from, to, r, g, b, ptSize, transform, ID);

        return reachedLimit;
    }
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
        Event(startT)
    {
        type = POINTER_MOVEMENT_EVENT;
    }

    PointerMovement(int startT, int endT, QVector<Subevent> subevents) :
        Event(startT)
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

    void trimRange(int from, int to, int insertIdx, QVector<Event*> &events)
    {
        Subevent valueFrom(from, 0, 0);
        Subevent valueTo(to, 0, 0);

        QVector<Subevent>::iterator f =
                qLowerBound(subevents.begin(), subevents.end(), valueFrom, timeLessThan);
        QVector<Subevent>::iterator t =
                qLowerBound(subevents.begin(), subevents.end(), valueTo,   timeLessThan);

        // From refers to after the subevents
        if (f > subevents.end()-1) return;
        //ou t >end : end é o fim -> trimFrom(from)
        if (t > subevents.end()-1)
        {
            trimFrom(from);
            return;
        }

        if ((*f).t == from) f++;
        if ((*t).t == to  ) t--;

        // Create new Subevent from t to end
        events.insert( insertIdx, (Event*)(new PointerMovement( (*t).t, endTime, subevents.mid( t - subevents.begin() ) )) );

        subevents.erase(f, subevents.end());

        endTime   = subevents.last().t;

        subevents.last().t--;
    }

    void trimFrom(int time)
    {
        Subevent value(time, 0, 0);

        QVector<Subevent>::iterator i =
                qLowerBound(subevents.begin(), subevents.end(), value, timeLessThan);

        subevents.erase(i, subevents.end());

        endTime = subevents.last().t;
    }

    void trimUntil(int time)
    {
        Subevent value(time, 0, 0);

        QVector<Subevent>::iterator i =
                qLowerBound(subevents.begin(), subevents.end(), value, timeLessThan) + 1;

        if (i > subevents.end()-1) i--;

        subevents.erase(subevents.begin(), i);

        startTime = subevents.first().t;
    }

    void scaleAndMove(float scale, int timeShiftMSec, int pivot)
    {
        startTime += timeShiftMSec;
        startTime = (startTime - pivot) * scale + pivot;

        for (Subevent& se : subevents)
        {
            se.t += timeShiftMSec;
            se.t = (se.t - pivot) * scale + pivot;
        }

        endTime += timeShiftMSec;
        endTime = (endTime - pivot) * scale + pivot ;
    }

    void timeShift(int time)
    {
        startTime += time;
        endTime += time;

        for (Subevent& se : subevents)
        {
            se.t += time;
        }
    }

    QPointF getCursorPos(int time)
    {
        for (int i = subevents.size()-1; i >= 0; i--)
        {
            if (subevents[i].t <= time)
            {
                return QPointF(subevents[i].x, subevents[i].y);
            }
        }
    }

    void addPointerEvent(int t, float x, float y)
    {
        subevents << Subevent(t, x, y);
    }

    void closePointerEvent(int endT)
    {
        endTime = endT;
    }

    void resizeTo(int sizeMSecs)
    {

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
