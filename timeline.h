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
    int windowEndMSec = 40000;
    int windowStartMSec = 0;
    float timelineStartPos = 0;

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
    const QColor timelineColor = QColor(202,202,202);
    const QColor selectionColorNormal = QColor(103,169,217,70);
    const QColor selectionColorCollision = QColor(217,169,103,110);
    const QColor selectionColorNoCollision = QColor(80,200,139,110);
    QColor selectionColor = selectionColorNormal;
    const QColor audioColor = QColor(0,0,0,6);
    QPolygon pointerTriangle, scaleArrowRight, scaleArrowLeft;
    QPolygon scaleArrowLeftTmp, scaleArrowRightTmp;
    float audioScaleArrowAlpha = 0;
    float videoScaleArrowAlpha = 0;
    const float targetAlpha = 139;
    const float arrowFadeIn = 0.07;
    const float arrowFadeOut = 0.04;

    // Event manipulation
    QRect videoSelectionRect;
    QRect audioSelectionRect;
    QPolygon audioLeftScaleArrow;
    QPolygon audioRightScaleArrow;
    QPolygon videoLeftScaleArrow;
    QPolygon videoRightScaleArrow;
    bool draggingEvents = false;
    bool scalingEventsLeft = false;
    bool scalingEventsRight = false;
    bool eventModified = false;
    int eventTimeShiftMSec = 0;
    float eventScaling = 1;
    void checkForCollision();
    QPixmap tmpVideo;
    QPixmap tmpAudio;

    // Handle dragging and scaling both video and audio
    void handleSelectionPressed(QRect &selectionRect, QPolygon &leftArrow, QPolygon &rightArrow);

    // Timeline sizing and positioning
    const int videoTimelineStart = 0;
    const int videoTimelineHeight = 34;
    const int audioTimelineStart = 40;
    const int audioPixmapHeight = 34;
    const int videoPixmapHeight = 1;
    const int audioPixmapRealHeight = 34;
    const int timeRulerStart = audioTimelineStart + audioPixmapHeight;


public:

    // Objects used for audio sampling
    PlayerThread playerThread;
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
    int lastAudioPixelX=0;

    // Mouse Tracking
    QPoint mousePos;
    bool mouseOver = false;
    bool audioSelected = false, videoSelected = false;
    bool isSettingCursor = false;
    bool isSelecting = false;
    bool mouseMiddleDown = false;
    int mouseDragStartX, lastWindowStartMSec;
    int selectionStartPos, selectionEndPos;
    int selectionStartTime, selectionEndTime;

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
    void copyAudio();
    QByteArray audioClipboard;
    void pasteVideo(int atTimeMSec);
    void pasteAudio(int atTimeMSec);
    void deleteSelectedVideo();
    void deleteSelectedAudio();
    void scaleAndMoveSelectedVideo(float scale, int timeShiftMSec);
    void selectVideo();
    void selectAudio();
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
