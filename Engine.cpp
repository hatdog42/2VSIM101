#include "Engine.h"
#include "MainWindow.h"
#include "Texture.h"
#include "Renderer.h"
#include "Logger.h"
#include "PointCloud.h"
#include <chrono>

Engine::Engine() :mLogger( Logger::getInstance() )
{}

void Engine::loadTextures()
{
    readTexture("green.png");
    readTexture("hund.bmp");
    readTexture("orange.jpg");
}

void Engine::readTexture(std::string textureName)
{
    // Texture update: - textures must be made before the Descriptor sets!
    mTextures.push_back(new Texture(textureName));
    mRenderer->createTextureImage(mTextures[mTextures.size()-1]);
    mRenderer->createTextureImageView(mTextures[mTextures.size()-1]);
    mRenderer->createTextureDescriptor(mTextures[mTextures.size()-1]);
}

void Engine::loadMeshes()
{

}

void Engine::loadPointCloud()
{
    const std::string filePath = std::string(PROJECT_SOURCE_PATH) + "/vertices.txt";

    PointCloud* pointCloud = new PointCloud();

    // Load the points and if successful convert them to renderable vertices
    if (!pointCloud->loadFromFile(filePath) || !pointCloud->prepareVertices())
    {
        delete pointCloud;
        LOGE("Could not load and prepare the point cloud");
        return;
    }

    if (!pointCloud->loadTriangulation(std::string(PROJECT_SOURCE_PATH) + "/triangulation.txt"))
    {
        delete pointCloud;
        LOGE("Could not load the terrain triangulation");
        return;
    }

    mPointCloud = pointCloud;
    mRenderer->createPointCloudVertexBuffer(mPointCloud);
    LOGP("Point cloud ready with %zu points", mPointCloud->getPointCount());
}

void Engine::loadScene()
{
    mPlayer = nullptr;
    mNPC = nullptr;
    mTerrain = nullptr;
}

// Main Game Loop function
// called every frame
void Engine::update()
{
    // Compute delta time (seconds) between frames
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastTime).count();
    // Clamp very large delta times (e.g. when debugging or after a pause)
    if (deltaTime > 0.5f) deltaTime = 0.5f;
	lastTime = now; // Update lastTime for the next frame

    // 1. get input from mouse and keyboard
    mMainWindow->handleInput(deltaTime);

    // 2. call the renderer to draw a frame
    mRenderer->update();
}

Engine* Engine::getInstance()
{
    static Engine mInstance;
    return &mInstance;
}

void Engine::setRenderer(Renderer *rendererIn)
{
    mRenderer = rendererIn;
}

void Engine::setMainWindow(MainWindow *mainWindowIn)
{
        mMainWindow = mainWindowIn;
}
