#ifndef TIMELINE_H
#define TIMELINE_H

#include <QWidget>
#include <QByteArray>
#include <QAudioInput>
#include <QAudioOutput>
#include <QFile>
#include <QTime>
#include <QThread>
#include "events.h"

#if QT_VERSION < 0x050000
    #define setSampleRate(sr) setFrequency(sr);
    #define setChannelCount(cc) setChannels(cc);
#endif

class PlayerThread : public QThread
{
    Q_OBJECT
    void run();
};

class Timeline : public QWidget
{
    Q_OBJECT

    // Timeline subdivisions and time marks
    int mSecsBetweenMarks = 10000;
    int markSubdivisions = 10;
    int mSecsBetweenSubdivisions = mSecsBetweenMarks / markSubdivisions;
    int windowStartMSec = 0;
    int windowEndMSec = 40000;

    // Pixmap sampling variables
    float pixelsPerSecond = 20.0;
    float pixelsPerMSec = 20.0 / 1000.0;
    float mSecsPerPixel = 1.0 / pixelsPerMSec;
    float pixmapLenght = 30.0 * 60.0 * 1000.0 * pixelsPerMSec;
    float samplingFrequency = 40000.0;
    float sampleSize = 2.0;
    float samplingInterval = 1.0 / samplingFrequency;
    float samplesPerBar = 10.0;
    float barsPerMSec = samplingFrequency / (samplesPerBar * 1000.0);
    float pixelsPerBar = samplesPerBar * samplingInterval * pixelsPerSecond;
    float pixelsPerMark = pixelsPerMSec * mSecsBetweenMarks;
    float pixelsPerSubmark = pixelsPerMSec * mSecsBetweenSubdivisions;

    // Cosmetic details
    QColor timelineColor = QColor(202,202,202);
    const int audioOpacity = 6;
    const int videolineRealHeight = 1;
    const int audiolineRealHeight = 34;
    QPolygon pointerTriangle, scaleArrowRight, scaleArrowLeft;

    // Timeline sizing and positioning
    const int videoTimelineStart = 0;
    const int videoTimelineHeight = 34;
    const int audioTimelineStart = 40;
    const int audioTimelineHeight = 34;

public:

    // Objects used for audio sampling
    PlayerThread pt;
    QAudioFormat format;
    QAudioInput *audioInput;
    QAudioOutput *audioOutput;
    QIODevice* inputDevice;
    QByteArray audioTempBuffer;

    // Final output file
    QFile* rawAudioFile;

    // The 2 main parts of the timeline, where audio and video events are displayed, respectively
    QPixmap* audioPixmap;
    QPixmap* videoPixmap;

    // Setup all audio objects initialization
    void initializeAudio();

    // Audio variables
    int accumulatedSamples=0;
    int barCursor=0;

    // Mouse Tracking
    QPoint mousePos;
    bool mouseOver = false;
    bool selecting = false;
    bool mouseLeftDown = false;
    bool mouseRightDown = false;
    bool mouseMiddleDown = false;
    int dragStartX, lastWindowStartMSec;
    int selectionStart, selectionEnd;

    // The vector of events
    QVector<Event*> events;
    QVector<Event*> eventsClipboard;

    // Currently active Event - the one being filled
    Event* currentEvent = NULL;

    // Used for incremental draw
    Event* eventToDraw = NULL;
    int eventToDrawIdx = 0;
    int lastDrawnSubeventIndex = 0;

    // Draw the video part of the timeline
    void paintVideoPixmap();
    void paintVideoPixmapRange();

    // Sets the cursor at a given position in widget coordinates - must redraw the scene
    void setCursorAt(int x);

    // Video handling methods
    void copyVideo();
    void pasteVideo(int atTimeMSec);
    void deleteSelectedVideo();
    void scaleAndMoveSelectedVideo(float scale, int timeShiftMSec);
    void selectVideo();
    int selectionStartIdx, selectionEndIdx;

    // Constructor & destructor
    explicit Timeline(QWidget *parent = 0);
    ~Timeline();

    // Timing variables
    QTime timer;
    int timeCursorMSec = 0;
    long lastRecordStartLocalTime = 0;
    long totalTimeRecorded = 0;
    bool isRecording = false;
    bool isPlaying = false;
    bool wasPlaying = false;

    // Main methods that control its behavior
    void startPlaying();
    void stopPlaying();
    void startRecording();
    void pauseRecording();

    // Gets the current time, given where the time cursor is
    int getCurrentTime();

    // Singleton
    static Timeline* si;

    // Handle stroke events
    void addStrokeBeginEvent(QPointF penPos, int pboStart, int pbo);
    void addStrokeMoveEvent(QPointF penPos, int pbo);
    void addStrokeEndEvent();

    // Handle pointer events
    void addPointerBeginEvent(QPointF penPos);
    void addPointerMoveEvent(QPointF penPos);
    void addPointerEndEvent();

    // Redraw screen until time cursor position
    void redrawScreen();

    // Just draw what changed in the screen
    void incrementalDraw();

    // The current position of the cursor in the video being played
    QPointF cursorPosition;


protected:

    // Overrides
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);

    void mouseMoveEvent( QMouseEvent * event );
    void mousePressEvent( QMouseEvent * event );
    void mouseReleaseEvent( QMouseEvent * event );
    void wheelEvent( QWheelEvent * event);

public slots:

    // Show context menu
    void ShowContextMenu(const QPoint &pos);

    // Get audio errors
    void stateChanged(QAudio::State newState);

    // Read samples from microphone, paint the audioline and output to the audio file
    void readAudioFromMic();
};

#endif
