#include "Engine.h"
#include "MainWindow.h"
#include "Texture.h"
#include "Renderer.h"
#include "Logger.h"
#include "PointCloud.h"
#include "Mesh.h"
#include "GameObject.h"
#include "Transform.h"
#include "Physics.h"
#include <algorithm>
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
    mMeshes.push_back(new Mesh(mRenderer, Mesh::MeshType::OBJ, "Ball.obj"));
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

    mBall = new GameObject();
    mBall->mMesh = 0; // Ball.obj
    mBall->mTexture = 0;
    mBall->mTransform = new Transform{};

    if (mPointCloud && !mPointCloud->getTriangleIndices().empty())
    {
        const auto& vertices = mPointCloud->getVertices();
        const auto& indices = mPointCloud->getTriangleIndices();

        // start at the centre of a triangle halfway through the surface data.
        const std::size_t first = indices.size() / 3 / 2 * 3 + 30;
        const glm::vec3& A = vertices[indices[first]].position;
        const glm::vec3& B = vertices[indices[first + 1]].position;
        const glm::vec3& C = vertices[indices[first + 2]].position;

        glm::vec3 normal = glm::cross(B - A, C - A);
        const float normalLength = glm::length(normal);
        normal = normalLength > 0.00001f ? normal / normalLength : glm::vec3(0, 1, 0);
        if (normal.y < 0.0f) normal = -normal;

        mBall->mTransform->scale = glm::vec3(0.2f);
        mBall->mTransform->position = (A + B + C) / 3.0f + 0.2f * normal; // put the ball on a triangle
        mCurrentBallTriangle = static_cast<int>(first / 3);
    }

    mGameObjects.push_back(mBall);
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

    // Physics
    Physics physics;
    glm::vec3& position = mBall->mTransform->position;

    int triangle = physics.findTriangle(*mPointCloud, position, mCurrentBallTriangle);

    const auto& neighbours = mPointCloud->getNeighbours();
    mCurrentBallTriangle = triangle;

    if (triangle != -1)
    {
        const auto& vertices = mPointCloud->getVertices();
        const auto& indices = mPointCloud->getTriangleIndices();

        const glm::vec3& A = vertices[indices[3 * triangle]].position;
        const glm::vec3& B = vertices[indices[3 * triangle + 1]].position;
        const glm::vec3& C = vertices[indices[3 * triangle + 2]].position;

        glm::vec3 acceleration = physics.calculateSlidingAcceleration(A, B, C);

        physics.Move(position, mBallVelocity, acceleration, deltaTime);

        int newTriangle = physics.findTriangle(*mPointCloud, position, mCurrentBallTriangle);
        mCurrentBallTriangle = newTriangle;

        if (newTriangle != -1)
        {
            const glm::vec3& newA = vertices[indices[3 * newTriangle]].position;
            const glm::vec3& newB = vertices[indices[3 * newTriangle + 1]].position;
            const glm::vec3& newC = vertices[indices[3 * newTriangle + 2]].position;

            physics.ChangeTriangle(position,mBallVelocity,mBall->mTransform->scale.x,newA, newB, newC);
        }
    }

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
