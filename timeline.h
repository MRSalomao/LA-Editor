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

struct chunk
{
    char        id[4];
    quint32     size;
};

struct RIFFHeader
{
    chunk       descriptor;     // "RIFF"
    char        type[4];        // "WAVE"
};

struct WAVEHeader
{
    chunk       descriptor;
    quint16     audioFormat;
    quint16     numChannels;
    quint32     sampleRate;
    quint32     byteRate;
    quint16     blockAlign;
    quint16     bitsPerSample;
};

struct DATAHeader
{
    chunk       descriptor;
};

struct CombinedHeader
{
    RIFFHeader  riff;
    WAVEHeader  wave;
    DATAHeader  data;
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
    double pixelsPerSecond = 20.0;
    double pixelsPerMSec = 20.0 / 1000.0;
    double mSecsPerPixel = 1.0 / pixelsPerMSec;
    float pixmapLenght = 30.0 * 60.0 * 1000.0 * pixelsPerMSec;
    float samplingFrequency = 40000.0;
    float sampleSize = 2.0;
    float samplingInterval = 1.0 / samplingFrequency;
    float samplesPerBar = 10.0;
    double barsPerMSec = samplingFrequency / (samplesPerBar * 1000.0);
    double pixelsPerBar = samplesPerBar * samplingInterval * pixelsPerSecond;
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
    const float arrowFadeOut = 0.06;

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
    int eventTimeExpansionLeftMSec = 0;
    int eventTimeExpansionRightMSec = 0;
    void checkForCollision();
    QPixmap tmpVideo;
    QPixmap tmpAudio;

    // Handle dragging and scaling both video and audio
    bool handleSelectionPressed(QRect &selectionRect, QPolygon &leftArrow, QPolygon &rightArrow);

    // Timeline sizing and positioning
    const int videoTimelineStart = 0;
    const int videoTimelineHeight = 34;
    const int audioTimelineStart = 40;
    const int audioTimelineHeight = 34;
    const int audioPixmapHeight = 34;
    const int videoPixmapHeight = 1;
    const int timeRulerStart = audioTimelineStart + audioPixmapHeight;

    // Write wav header
    void writeWavHeader(QFile *audioFile);
public:

    // Objects used for audio sampling
    PlayerThread playerThread;
    QAudioFormat format;
    QAudioDeviceInfo infoIn;
    QAudioDeviceInfo infoOut;
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
    int audioSelectionStart=0;
    int audioSelectionSize=0;
    float audioScaling=1.0f;

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
    int lastSelectionStartPos, lastSelectionEndPos;
    int lastSelectionStartTime, lastSelectionEndTime;

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

    // Sets the cursor at a given position in widget coordinates - must redraw the scene
    void setCursorAt(int x);

    // Video handling methods
    void copyVideo();
    void copyAudio();
    QByteArray audioClipboard;
    void pasteVideo();
    void pasteAudio();
    void deleteVideo(int fromTime, int toTime);
    void deleteAudio(int fromTime, int toTime);
    void scaleAndMoveSelectedVideo();
    void scaleAndMoveSelectedAudio();
    void scaleAudio();
    bool shiftPressed = false;
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

    // Basic operations
    void paste();
    void apply();
    void erase();
    void cut();
    void copy();
    void recordToSlot();
    void redo();
    void undo();
    void unselect();


protected:

    // Overrides
    void paintEvent(QPaintEvent *event);

    void mouseMoveEvent( QMouseEvent * event );
    void mousePressEvent( QMouseEvent * event );
    void mouseReleaseEvent( QMouseEvent * event );
    void enterEvent( QEvent * event );
    void leaveEvent( QEvent * event );
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
