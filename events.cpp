#include "events.h"

int Event::subeventToDrawIdx = 0;
QPointF Event::cursorPos;
Event* Event::activeEvent;
QVector<Event*> Event::allEvents;

Event::Event(int startT, bool isLocal) : startTime(startT)
{
    if (!isLocal) init();
}


Event::Event()
{
    init();
}

#define noSelection (256*256*256-1)
void Event::setActiveID(int ID, QPointF pressPos)
{
    if (ID == noSelection)
    {
        activeEvent = NULL;
        return;
    }
    activeEvent = allEvents[ID];
}


void Event::handleDrag(QPointF deltaPos)
{
    if (activeEvent != NULL) activeEvent->mouseDragged(deltaPos);
}


void Event::setSubeventIndex(int idx)
{
    subeventToDrawIdx = idx;
}


int Event::getSubeventIndex()
{
    return subeventToDrawIdx;
}


void Event::deleteAllEvents()
{
    qDeleteAll(allEvents);
}


void Event::init()
{
    ID = allEvents.size();
    allEvents.push_back(this);
    qDebug() << ID;
}


PenStroke* PenStroke::clone() const
{
    PenStroke* ret = new PenStroke(*this);

    ret->init();

    return ret;
}

void PenStroke::writeToStream(QDataStream& out)
{
    out << (qint32)startTime;
    out << (qint8)(STROKE_START);
    out << (quint8)(r * 255.0f);
    out << (quint8)(g * 255.0f);
    out << (quint8)(b * 255.0f);
    qDebug() << (quint8)(r * 255.0f)<< (quint8)(g * 255.0f)<< (quint8)(b * 255.0f);

    for (Subevent se : subevents)
    {
        out << (qint32)se.t;
        out << (qint8)(STROKE_EVENT);
        out << (qint16)se.x;
        out << (qint16)se.y;
    }

    out << (qint32)endTime;
    out << (qint8)(STROKE_END);
}

void PenStroke::mouseDragged(QPointF deltaPos)
{
    transform.translate(deltaPos.x()/SHRT_MAX, deltaPos.y()/SHRT_MAX);
}


void PenStroke::scaleAndMove(float scale, int timeShiftMSec, int pivot)
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


void PenStroke::trimRange(int from, int to, int insertIdx, QVector<Event *> &events)
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


void PenStroke::trimFrom(int from)
{
    Subevent value(from, 0, 0, 0);

    QVector<Subevent>::iterator i =
            qLowerBound(subevents.begin(), subevents.end(), value, timeLessThan);

    subevents.erase(i, subevents.end());

    endTime = subevents.last().t;
}


void PenStroke::trimUntil(int to)
{
    Subevent value(to, 0, 0, 0);

    QVector<Subevent>::iterator i =
            qLowerBound(subevents.begin(), subevents.end(), value, timeLessThan) + 1;

    if (i > subevents.end()) i--;

    subevents.erase(subevents.begin(), i);

    if (subevents.size() == 0) return; //TODO

    startTime = subevents.first().t;
}


bool PenStroke::drawUntil(int time)
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


bool PenStroke::drawFromIndexUntil(int limitTime)
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
            cursorPos = transform * QPointF(subevents[subeventToDrawIdx].x, subevents[subeventToDrawIdx].y);
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
            cursorPos = transform * QPointF(subevents.back().x, subevents.back().y);
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


void PointerMovement::writeToStream(QDataStream &out)
{
    out << (qint32)startTime;
    out << (qint8)(POINTER_MOVEMENT_START);

    for (Subevent se : subevents)
    {
        out << (qint32)se.t;
        out << (qint8)(POINTER_MOVEMENT_EVENT);
        out << (qint16)se.x;
        out << (qint16)se.y;
    }

    out << (qint32)endTime;
    out << (qint8)(POINTER_MOVEMENT_END);
}

void PointerMovement::trimRange(int from, int to, int insertIdx, QVector<Event *> &events)
{
    Subevent valueFrom(from, 0, 0);
    Subevent valueTo(to, 0, 0);

    QVector<Subevent>::iterator f =
            qLowerBound(subevents.begin(), subevents.end(), valueFrom, timeLessThan);
    QVector<Subevent>::iterator t =
            qLowerBound(subevents.begin(), subevents.end(), valueTo,   timeLessThan);

    // From refers to after the subevents
    if (f > subevents.end()-1) return;

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


void PointerMovement::trimFrom(int time)
{
    Subevent value(time, 0, 0);

    QVector<Subevent>::iterator i =
            qLowerBound(subevents.begin(), subevents.end(), value, timeLessThan);

    subevents.erase(i, subevents.end());

    endTime = subevents.last().t;
}


void PointerMovement::trimUntil(int time)
{
    Subevent value(time, 0, 0);

    QVector<Subevent>::iterator i =
            qLowerBound(subevents.begin(), subevents.end(), value, timeLessThan) + 1;

    if (i > subevents.end()-1) i--;

    subevents.erase(subevents.begin(), i);

    if (subevents.size() == 0) return; //TODO

    startTime = subevents.first().t;
}


void PointerMovement::scaleAndMove(float scale, int timeShiftMSec, int pivot)
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


void PointerMovement::timeShift(int time)
{
    startTime += time;
    endTime += time;

    for (Subevent& se : subevents)
    {
        se.t += time;
    }
}


QPointF PointerMovement::getCursorPos(int time)
{
    for (int i = subevents.size()-1; i >= 0; i--)
    {
        if (subevents[i].t <= time)
        {
            return QPointF(subevents[i].x, subevents[i].y);
        }
    }
}


void PenStroke::timeShift(int time)
{
    startTime += time;
    endTime += time;

    for (Subevent& se : subevents)
    {
        se.t += time;
    }
}
