#ifndef ENGINE_H
#define ENGINE_H

#include "Renderer.h"

class Terrain;
class GameObject;
class Mesh;
class Texture;

// Holds the meshes and textures for the scene
// Also holds the main Game Loop
class Engine
{
public:

    // Getting the instance of this class. This class is a singleton!
    static Engine* getInstance();

    // Main Game Loop update function
    void update();

    void loadTextures();
    void loadMeshes();
    void loadScene();

    void setRenderer(Renderer *rendererIn);
    void setMainWindow(MainWindow *mainWindowIn);

    Terrain* mTerrain{ nullptr };

    GameObject* mPlayer{ nullptr };
    GameObject* mNPC{ nullptr };

private:
    Engine();
    ~Engine() = default;

    // Delete copy constructor and assignment operator
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void readTexture(std::string textureName);

    Renderer* mRenderer{ nullptr };
    MainWindow* mMainWindow { nullptr };

    std::vector<Mesh*> mMeshes;
    std::vector<Texture*> mTextures;

    std::vector<GameObject*> mGameObjects; // Vector to the GameObjects

    Logger& mLogger; // Reference to the Logger, set in the constructor

    friend Renderer; // Allow Renderer to access private members of Engine, e.g. for loading textures and meshes
};

#endif // ENGINE_H
