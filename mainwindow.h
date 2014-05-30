#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "canvas.h"
#include "options.h"

#include <QPushButton>
#include <QScrollBar>
#include <QSettings>

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    static MainWindow* si;

    QColor activeColor;

    QScrollBar* getCanvasScrollBar();

    void changeCanvasScrollBar(int delta);

    QPushButton* activeColorButton;
    QPushButton* activeToolButton;

    enum {PEN_TOOL, ERASER_TOOL, IMAGE_TOOL, LINE_TOOL, TEXT_TOOL, POINTER_TOOL};
    int activeTool = PEN_TOOL;

    bool fileDialogOpen = false;

    void stopPlaying();

    QSettings* settings;

private slots:

    void on_recButton_clicked(bool checked);
    void on_playPauseButton_clicked(bool checked);

    void on_foregroundColor_clicked();
    void on_backgroundColor_clicked();
    void on_greyLight_clicked();
    void on_greyDark_clicked();
    void on_greenDark_clicked();
    void on_greenLight_clicked();
    void on_blueLight_clicked();
    void on_blueDark_clicked();
    void on_yellowDark_clicked();
    void on_yellowLight_clicked();
    void on_brownLight_clicked();
    void on_brownDark_clicked();
    void on_redDark_clicked();
    void on_redLight_clicked();
    void on_purpleLight_clicked();
    void on_purpleDark_clicked();

    void on_pen_clicked();
    void on_eraser_clicked();
    void on_image_clicked();
    void on_line_clicked();
    void on_text_clicked();
    void on_pointer_clicked();

    void on_canvasScrollBar_sliderMoved(int position);
    void on_canvasScrollBar_valueChanged(int value);

    void on_openButton_clicked();

    void on_saveButton_clicked();

    void on_optionsButton_clicked();

protected:

    void keyPressEvent( QKeyEvent * event );

private:
    Ui::MainWindow *ui;

    Options* optionsWindow;

    QVector<QPushButton*> colorButtons;
    QVector<QString> cbStyleSheets;
    QMap<QString, QColor> cbColors;

    void toggleColorButton(QPushButton *cb);
    void toggleToolButton(QPushButton *tb);

};

#endif
