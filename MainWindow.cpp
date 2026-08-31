#include "MainWindow.h"
#include "Camera.h"
#include "Logger.h"
#include "ui_MainWindow.h"
#include "Renderer.h"
#include "Engine.h"
#include <QKeyEvent>
#include <QColorDialog>
#include "GameObject.h"
#include "Transform.h"

MainWindow::MainWindow(QWidget *parent) :
        QMainWindow(parent),
        ui(new Ui::MainWindow),
        mLogger(Logger::getInstance())
{
    ui->setupUi(this);

    // Logger is the OutputLog at the bottom of the program
    // feed in MainWindow to the logger - has to be done, else the logger will crash the program
    mLogger.setMainWindow(this);

    //MainWindow size:
    resize(1300, 850);
    //Main app title
    setWindowTitle("Vulkan 26");

    // VulkanWindow is the part of the program that renders the Vulkan graphic
    mVulkanWindow = new Renderer();
    mVulkanWindow->setTitle("Renderer");    //Render window title - not shown
    mVulkanWindow->setMainWindow(this);     //Tells the Renderer of this MainWindow

    // Have to set the size of the Vulkan window here,
    // before initVulkan()
    // otherwise it can not set up the Vulkan swapchain correctly
    mVulkanWindow->setWidth(1000);
    mVulkanWindow->setHeight(800);
    mVulkanWindow->initVulkan();

    // Wrap VulkanWindow (QWindow) into a QWidget
    // This way the VulkanWindow can be integrated into the main window
    QWidget* vulkanWidget = QWidget::createWindowContainer(mVulkanWindow, this);
    // This will be the smallest you can scale the program window
    vulkanWidget->setMinimumSize(1000, 600);

    // Adding the Vulkan renderer to the main programs UI:
    // In the MainWindow.ui file we have a QVBoxLayout called VulkanLayout
    ui->VulkanLayout->addWidget(vulkanWidget);

    mCamera = mVulkanWindow->camera();

    // Set keyboard and mouse focus to the renderer, so the camera works without clicking.
    vulkanWidget->setFocus();

    // You can also send messages to the statusbar at the very bottom:
    statusBar()->showMessage(" Everybody loves Vulkan! ");
}

MainWindow::~MainWindow()
{
    if(mVulkanWindow)
    {
        delete mVulkanWindow;
        mVulkanWindow = nullptr;
    }
    delete ui;
}

void MainWindow::start()
{
    LOGW("Start is called");

    // Have to feed the other classes into Engine,
    // since it is a singleton and can not be fed in through the constructor
    Engine::getInstance()->setMainWindow(this);

    // This starts the GameLoop!
    Engine::getInstance()->update();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
        quitApp();

    if (event->key() == Qt::Key_Space)
        start();

    // For editor camera
    if(event->key() == Qt::Key_W)
        mInput.W = true;
    if(event->key() == Qt::Key_S)
        mInput.S = true;
    if(event->key() == Qt::Key_D)
        mInput.D = true;
    if(event->key() == Qt::Key_A)
        mInput.A = true;
    if(event->key() == Qt::Key_Q)
        mInput.Q = true;
    if(event->key() == Qt::Key_E)
        mInput.E = true;
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    // For editor camera
    if(event->key() == Qt::Key_W)
        mInput.W = false;
    if(event->key() == Qt::Key_S)
        mInput.S = false;
    if(event->key() == Qt::Key_D)
        mInput.D = false;
    if(event->key() == Qt::Key_A)
        mInput.A = false;
    if(event->key() == Qt::Key_Q)
        mInput.Q = false;
    if(event->key() == Qt::Key_E)
        mInput.E = false;
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
        mInput.RMB = true;
    if (event->button() == Qt::LeftButton)
        mInput.LMB = true;
    if (event->button() == Qt::MiddleButton)
        mInput.MMB = true;
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
        mInput.RMB = false;
    if (event->button() == Qt::LeftButton)
        mInput.LMB = false;
    if (event->button() == Qt::MiddleButton)
        mInput.MMB = false;
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (mInput.RMB)
    {
        //Using mMouseXYlast as deltaXY so we don't need extra variables
        mMouseXlast = event->pos().x() - mMouseXlast;
        mMouseYlast = event->pos().y() - mMouseYlast;

        mCamera->mYaw += mMouseXlast * mCameraRotateSpeed;
        mCamera->mPitch += mMouseYlast * mCameraRotateSpeed;

    }
    mMouseXlast = event->pos().x();
    mMouseYlast = event->pos().y();
}

void MainWindow::wheelEvent(QWheelEvent *event)
{
    QPoint numDegrees = event->angleDelta() / 8;

    // if RMB, change the speed of the camera
    // The values here could be set in a config-file instead of being hardcoded
    if (mInput.RMB)
    {
        if (numDegrees.y() > 1)
        {
            mCameraSpeed += 0.05f;
            if (mCameraSpeed > 2.5f)    //test to not go to high
                mCameraSpeed = 2.5f;
        }
        if (numDegrees.y() < 1)
        {
            mCameraSpeed -= 0.05f;    //test to not go to low / negative
            if (mCameraSpeed < 0.05f)
                mCameraSpeed = 0.05f;
        }
    }
    event->accept();
}

void MainWindow::handleInput(float deltaTime)
{
    //If camera is not set, don't try to update it!
    if (!mCamera)
        return;

    mCamera->resetMovement();  //reset last frame movement

    if (mInput.RMB)
    {
        if (mInput.W)
            mCamera->mCameraMovement.z += mCameraSpeed * deltaTime; //forward
        if (mInput.S)
            mCamera->mCameraMovement.z -= mCameraSpeed * deltaTime; //backward
        if (mInput.D)
            mCamera->mCameraMovement.x += mCameraSpeed * deltaTime; //right
        if (mInput.A)
            mCamera->mCameraMovement.x -= mCameraSpeed * deltaTime; //left
        if (mInput.Q)
            mCamera->mCameraMovement.y -= mCameraSpeed * deltaTime; //down
        if (mInput.E)
            mCamera->mCameraMovement.y += mCameraSpeed * deltaTime; //up
    }
    else
    {
        float moveSpeed = 1.5f;
        if (mInput.W)
            Engine::getInstance()->mPlayer->mTransform->position.z -= moveSpeed * deltaTime;//forward
        if (mInput.S)
            Engine::getInstance()->mPlayer->mTransform->position.z += moveSpeed * deltaTime;//backward
        if (mInput.D)
            Engine::getInstance()->mPlayer->mTransform->position.x += moveSpeed * deltaTime;//right
        if (mInput.A)
            Engine::getInstance()->mPlayer->mTransform->position.x -= moveSpeed * deltaTime;//left
	}
    mCamera->update();
    statusBar()->showMessage(QString("Camera position ") + "x: " + QString::number(mCamera->mPosition.x) + ", " +
                             "y: " + QString::number(mCamera->mPosition.y) + ", " +
                             "z: " + QString::number(mCamera->mPosition.z) + "     " +
                            + "Camera speed: " + QString::number(mCameraSpeed));
}

void MainWindow::on_action_BackgroundColor_triggered()
{
    // Here you should get the current color from the Renderer class
    QColor currentColor = QColor::fromRgbF(1.0, 0.0, 0.0); // Red as default for now

    //QColor has a strange format, so have to fix the string a bit for logging
    QString colorString = QString("currentColor: %1").arg(currentColor.name());
    LOGH(colorString.toStdString());

    // Open color dialog - with currentColor selected
    QColor color = QColorDialog::getColor(currentColor, this, "Choose Color");
    colorString = QString("new Color: %1").arg(color.name());
    LOGW(colorString.toStdString());

    //Now you can push this color to the Renderer, to use this as the VkClearValue clearColor!
}

void MainWindow::on_action_Quit_triggered()
{
    quitApp();
}

void MainWindow::quitApp()
{
    delete mVulkanWindow;
    mVulkanWindow = nullptr;
    close(); // calls Qt to close the app
}

void MainWindow::on_actionLog_to_Console_toggled(bool arg)
{
    mLogger.setPrintToConsole( arg );
}

void MainWindow::on_actionDelete_Logfile_at_start_toggled(bool arg)
{
    mLogger.setDeleteLogFileAtStart( arg );
}

void MainWindow::on_actionLog_to_File_toggled(bool arg)
{
    mLogger.setLogToFile( arg );
}


void MainWindow::on_actionRender_Lines_toggled(bool arg)
{
    mVulkanWindow->mRenderLines = arg;
}

