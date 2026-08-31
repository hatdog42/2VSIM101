#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Input.h"
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void start();

    // Camera update
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;;

    void handleInput(float deltaTime);

    float mCameraSpeed{1.5f};
    float mCameraRotateSpeed{ -0.1f };
    int mMouseXlast{0}; //for mouse rotate input
    int mMouseYlast{0};

    Input mInput;
    class Camera* mCamera{ nullptr };

private slots:
    void on_action_BackgroundColor_triggered();
    void on_action_Quit_triggered();
    void on_actionLog_to_Console_toggled(bool arg);
    void on_actionDelete_Logfile_at_start_toggled(bool arg);
    void on_actionLog_to_File_toggled(bool arg);

    void on_actionRender_Lines_toggled(bool arg);

private:
    Ui::MainWindow *ui{nullptr};

    class Renderer* mVulkanWindow{nullptr};

    void quitApp();

    // Our own logger in the MainWindow
    class Logger& mLogger;

    //Logger class uses private ui pointer from this class
    //so needs to have access to this class
    friend class Logger;

};
#endif // MAINWINDOW_H
