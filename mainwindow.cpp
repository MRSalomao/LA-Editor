#include <QFileDialog>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "strokerenderer.h"

#include "canvas.h"


MainWindow* MainWindow::si;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    si = this;

    QFont* font;

#ifdef Q_OS_LINUX
    font = new QFont("Ubuntu", 10);
#elif defined(Q_OS_WIN)
    font = new QFont("Tahoma", 10);
#elif defined(Q_OS_MAC)
    font = new QFont("Lucida Grande UI", 12);
#endif

    ui->newButton->setFont(*font);
    ui->openButton->setFont(*font);
    ui->saveButton->setFont(*font);
    ui->exportButton->setFont(*font);
    ui->uploadButton->setFont(*font);
    ui->optionsButton->setFont(*font);
    ui->hotkeysButton->setFont(*font);
    ui->playPauseButton->setFont(*font);
    ui->recButton->setFont(*font);

    settings = new QSettings("LiberaAkademio", "LiberaAkademioEditor");

    activeColorButton = ui->foregroundColor;
    toggleColorButton(ui->foregroundColor);

    activeToolButton = ui->pen;
    toggleToolButton(ui->pen);
    ui->canvas->setCursor(QCursor(QPixmap(":/icons/icons/cursor32.png")));

    colorButtons << ui->foregroundColor << ui->backgroundColor << ui->greyLight   << ui->greyDark <<
                    ui->greenDark       << ui->greenLight      << ui->blueLight   << ui->blueDark  <<
                    ui->yellowDark      << ui->yellowLight     << ui->brownLight  << ui->brownDark <<
                    ui->redDark         << ui->redLight        << ui->purpleLight << ui->purpleDark;

    for (QPushButton* cb : colorButtons)
    {
        QStringList colorStr =  cb->styleSheet().remove("background-color: rgb(")
                                                .remove(");")
                                                .split(", ");

        cbColors.insert(cb->objectName(), QColor( colorStr[0].toInt(), colorStr[1].toInt(), colorStr[2].toInt() ));

        cbStyleSheets.append( "QPushButton {" + cb->styleSheet() + "}" +
                              "QPushButton::checked {border-bottom: 9px solid rgba(104, 104, 104, 106);}" +
                              "QPushButton::hover {border: 0px; border-bottom: 9px solid rgba(104, 104, 104, 66);}" +
                              "QPushButton::checked::hover {border-bottom: 9px solid rgba(104, 104, 104, 156);}" );

        cb->setStyleSheet(cbStyleSheets.back());
    }

}


MainWindow::~MainWindow()
{
    delete ui;
}

QScrollBar* MainWindow::getCanvasScrollBar()
{
    return ui->canvasScrollBar;
}

void MainWindow::changeCanvasScrollBar(int delta)
{
    int newPos = ui->canvasScrollBar->sliderPosition() + delta;

    if ( newPos < ui->canvasScrollBar->minimum())
    {
        ui->canvasScrollBar->setSliderPosition(ui->canvasScrollBar->minimum());
        return;
    }

    if (newPos > ui->canvasScrollBar->maximum())
    {
        ui->canvasScrollBar->setSliderPosition(ui->canvasScrollBar->maximum());
        return;
    }

    ui->canvasScrollBar->setSliderPosition(newPos);
}

void MainWindow::on_recButton_clicked(bool checked)
{
    if(checked)
    {
        ui->recButton->setIcon(QIcon(":/icons/icons/icon-pause.png"));

        ui->playPauseButton->setDisabled(true);

        ui->timeline->startRecording();
    }
    else
    {
        ui->recButton->setIcon(QIcon(":/icons/icons/icon-record-l.png"));

        ui->playPauseButton->setDisabled(false);

        ui->timeline->pauseRecording();
    }
}

void MainWindow::on_playPauseButton_clicked(bool checked)
{
    if(checked)
    {
        ui->playPauseButton->setIcon(QIcon(":/icons/icons/icon-pause.png"));

        ui->recButton->setDisabled(true);

        ui->timeline->startPlaying();
    }
    else
    {
        ui->playPauseButton->setIcon(QIcon(":/icons/icons/icon-play.png"));

        ui->recButton->setDisabled(false);

        ui->timeline->stopPlaying();
    }
}

void MainWindow::stopPlaying()
{
    ui->playPauseButton->setIcon(QIcon(":/icons/icons/icon-play.png"));

    ui->recButton->setDisabled(false);

    ui->playPauseButton->setChecked(false);

    ui->timeline->stopPlaying();
}

void MainWindow::on_foregroundColor_clicked()
{
    toggleColorButton(ui->foregroundColor);
}

void MainWindow::on_backgroundColor_clicked()
{
    toggleColorButton(ui->backgroundColor);
}

void MainWindow::on_greyLight_clicked()
{
    toggleColorButton(ui->greyLight);
}

void MainWindow::on_greyDark_clicked()
{
    toggleColorButton(ui->greyDark);
}

void MainWindow::on_greenDark_clicked()
{
    toggleColorButton(ui->greenDark);
}

void MainWindow::on_greenLight_clicked()
{
    toggleColorButton(ui->greenLight);
}

void MainWindow::on_blueLight_clicked()
{
    toggleColorButton(ui->blueLight);
}

void MainWindow::on_blueDark_clicked()
{
    toggleColorButton(ui->blueDark);
}

void MainWindow::on_yellowDark_clicked()
{
    toggleColorButton(ui->yellowDark);
}

void MainWindow::on_yellowLight_clicked()
{
    toggleColorButton(ui->yellowLight);
}

void MainWindow::on_brownLight_clicked()
{
    toggleColorButton(ui->brownLight);
}

void MainWindow::on_brownDark_clicked()
{
    toggleColorButton(ui->brownDark);
}

void MainWindow::on_redDark_clicked()
{
    toggleColorButton(ui->redDark);
}

void MainWindow::on_redLight_clicked()
{
    toggleColorButton(ui->redLight);
}

void MainWindow::on_purpleLight_clicked()
{
    toggleColorButton(ui->purpleLight);
}

void MainWindow::on_purpleDark_clicked()
{
    toggleColorButton(ui->purpleDark);
}

void MainWindow::toggleColorButton(QPushButton* cb)
{
    activeColorButton->setChecked(false);

    cb->setChecked(true);

    activeColorButton = cb;

    activeColor = cbColors.value(cb->objectName());
}

void MainWindow::toggleToolButton(QPushButton* tb)
{
    activeToolButton->setChecked(false);

    tb->setChecked(true);

    activeToolButton = tb;
}

void MainWindow::on_pen_clicked()
{
    toggleToolButton(ui->pen);

    activeTool = PEN_TOOL;

    ui->canvas->setCursor(QCursor(QPixmap(":/icons/icons/cursor32.png")));
}

void MainWindow::on_eraser_clicked()
{
    toggleToolButton(ui->eraser);

    activeTool = ERASER_TOOL;
}

void MainWindow::on_image_clicked()
{
    toggleToolButton(ui->image);

    activeTool = IMAGE_TOOL;
}

void MainWindow::on_line_clicked()
{
    toggleToolButton(ui->line);

    activeTool = LINE_TOOL;
}

void MainWindow::on_text_clicked()
{
    toggleToolButton(ui->text);

    activeTool = TEXT_TOOL;

    ui->canvas->setCursor(QCursor(Qt::IBeamCursor));
}

void MainWindow::on_pointer_clicked()
{
    toggleToolButton(ui->pointer);

    activeTool = POINTER_TOOL;

    ui->canvas->setCursor(QCursor(Qt::ArrowCursor));
}

void MainWindow::on_canvasScrollBar_sliderMoved(int position)
{
    StrokeRenderer::si->setViewportYStart(position);
}

void MainWindow::on_canvasScrollBar_valueChanged(int value)
{
    StrokeRenderer::si->setViewportYStart(value);
}

void MainWindow::on_openButton_clicked()
{
    childWindowOpen = true;
    QString file = QFileDialog::getOpenFileName( this,tr("Select project to open"),
                                                QDir::homePath(), tr("LA-video (*.vvf)") );
    childWindowOpen = false;
}

void MainWindow::on_saveButton_clicked()
{
    childWindowOpen = true;
    QString file = QFileDialog::getSaveFileName( this,tr("Save project as"),
                                                QDir::homePath() + "/untitled.vvf", tr("LA-video (*.vvf)") );
    childWindowOpen = false;
}


void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Delete:
        Timeline::si->erase();
        Timeline::si->unselect();
        break;

    case Qt::Key_X:
        if (event->modifiers().testFlag(Qt::ControlModifier))
        {
            Timeline::si->cut();
            Timeline::si->unselect();
        }
        break;

    case Qt::Key_C:
        if (event->modifiers().testFlag(Qt::ControlModifier))
        {
            Timeline::si->copy();
            Timeline::si->unselect();
        }
        break;

    case Qt::Key_V:
        if (event->modifiers().testFlag(Qt::ControlModifier))
        {
            Timeline::si->apply();
            Timeline::si->paste();
            Timeline::si->unselect();
        }
        break;

    case Qt::Key_Z:
        if (event->modifiers().testFlag(Qt::ControlModifier) && event->modifiers().testFlag(Qt::ShiftModifier))
        {
            Timeline::si->redo();
        }
        else if (event->modifiers().testFlag(Qt::ControlModifier))
        {
            Timeline::si->undo();
        }
        break;

    case Qt::Key_Y:
        if (event->modifiers().testFlag(Qt::ControlModifier))
        {
            Timeline::si->redo();
        }
        break;


    case Qt::Key_Shift:
        Timeline::si->shiftPressed = true;

    default:
        break;
    }
}

void MainWindow::on_optionsButton_clicked()
{
    childWindowOpen = true;

    optionsWindow = new Options(0);

    optionsWindow->exec();

    childWindowOpen = false;
}

void MainWindow::on_newButton_clicked()
{
    childWindowOpen = true;

    newProject = new NewProject(0);

    newProject->exec();

    childWindowOpen = false;
}

void MainWindow::on_uploadButton_clicked()
{
    childWindowOpen = true;

    upload = new Upload(0);

    upload->exec();

    childWindowOpen = false;
}

void MainWindow::on_hotkeysButton_clicked()
{
    childWindowOpen = true;

    hotkeys = new Hotkeys(0);

    hotkeys->exec();

    childWindowOpen = false;
}

void MainWindow::on_exportButton_clicked()
{
    childWindowOpen = true;
    QString file = QFileDialog::getSaveFileName( this,tr("Export video to"),
                                                QDir::homePath() + "/untitled", tr("Portable Network Graphics (*.png);; Joint Photographic Experts Group (*.jpg);; MPEG-4 (.mp4)") );
    childWindowOpen = false;
}


void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Shift) Timeline::si->shiftPressed = false;
}
