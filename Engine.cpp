#include "Engine.h"
#include "MainWindow.h"
#include "Mesh.h"
#include "Texture.h"
#include "GameObject.h"
#include "Transform.h"
#include "Terrain.h"
#include "Collider.h"
#include "Renderer.h"
#include "Logger.h"
#include "Mesh.h"
#include "Terrain.h"
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
    mMeshes.push_back(new Mesh(mRenderer, Mesh::MeshType::TRIANGLE));
    mMeshes.push_back(new Mesh(mRenderer, Mesh::MeshType::OBJ, "Suzanne.obj"));
    mMeshes.push_back(new Mesh(mRenderer, Mesh::MeshType::OBJ, "Sphere.obj"));
    mMeshes.push_back(new Mesh(mRenderer, Mesh::MeshType::QUAD));
    mMeshes.push_back(new Terrain(mRenderer, 16, 1));
    mTerrain = static_cast<Terrain*>(mMeshes.back());
    LOGP("No of Meshes %d", mMeshes.size());
}

void Engine::loadScene()
{
    // hacky way to add game objects
    GameObject* temp = new GameObject();
    temp->mTransform = new Transform{glm::vec3(1.0f, 0.0f, 1.f)};
    temp->mTransform->scale = glm::vec3(0.3f, 0.3f, 0.3f);
    temp->mMesh = 1; //Monkey
    temp->mTexture = 0;
    temp->mCollider = new Collider(0.2f, temp);// A collider for our player
    mGameObjects.push_back(temp);
    mPlayer = mGameObjects.back(); //This is our player!

    temp = new GameObject();
    temp->mTransform = new Transform{ glm::vec3(12.0f, 0.0f, 8.f) };
    temp->mTransform->scale = glm::vec3(0.7f, 0.7f, 0.7f);
    temp->mMesh = 2; //Triangle
    temp->mTexture = 2;
    temp->mCollider = new Collider(0.2f, temp);// A collider for our NPC
    mGameObjects.push_back(temp);
    mNPC = mGameObjects.back(); //This is our NPC!

    temp = new GameObject();
    temp->mTransform = new Transform{ glm::vec3(0.f, 0.f, 0.f) };
    temp->mMesh = 4; //Terrain
    temp->mTexture = 1; //hund
    mGameObjects.push_back(temp);
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

    // 2. update game objects

    // Simple example of NPC movement
    // this should rather be done inside an NPC class update() function
    glm::vec3 npcPos = mNPC->mTransform->position;
	float xMax = 12.f;
	float xMin = 0.f;
	static bool movingRight = true;
	float moveSpeed = 1.1f;
    if (movingRight)
    {
        npcPos.x += moveSpeed * deltaTime;
        if (npcPos.x >= xMax)
            movingRight = false;
    }
    else
    {
        npcPos.x -= moveSpeed * deltaTime;
        if (npcPos.x <= xMin)
            movingRight = true;
	}

    // Player movement
    // this should rather be done inside a Player class update() function
    glm::vec3 playerPos = mPlayer->mTransform->position;

    // check height against terrain
    if (mTerrain)
    {
        playerPos.y = mTerrain->heightAtPoint(glm::vec2(playerPos.x, playerPos.z)) + 0.3f; // add 0.3 to put the player slightly above the terrain
        npcPos.y = mTerrain->heightAtPoint(glm::vec2(npcPos.x, npcPos.z)) + 0.3f; // add 0.3 to put the player slightly above the terrain
    }
    mPlayer->mTransform->position = playerPos;
    mNPC->mTransform->position = npcPos;

    // collisions
    if(mPlayer->mCollider->isColliding(*mNPC->mCollider))
        mRenderer->mLogger.logText("Player is colliding with NPC!",Logger::LogType::HIGHLIGHT);

    // 3. call the renderer to draw a frame
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
