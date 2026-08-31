#include "Mesh.h"
#include "sstream"
#include <fstream>
#include "Utilities.h"
#include "Logger.h"
#include "Renderer.h"

Mesh::Mesh(Renderer* render) : mRenderer{render}, mLogger(Logger::getInstance())
{
    //makeTriangle();
    //createBuffers();
}

Mesh::Mesh(Renderer* render, MeshType meshTypeIn, std::string fileNameIn) : mRenderer{render},
            mLogger(Logger::getInstance()), mFileName(fileNameIn)
{
    switch (meshTypeIn)
    {
    case MeshType::TRIANGLE :
        makeTriangle();
        break;
    case MeshType::QUAD :
        makeQuad();
        break;
    case MeshType::OBJ :
        makeObj();
        break;
    default:
        makeTriangle();
    }

    createBuffers();
}

Mesh::~Mesh()
{
    LOG("Mesh Destructor called");
    // Destroy only if handles are valid - prevents calls on uninitialized / already freed handles
    if (mVertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mRenderer->device, mVertexBuffer, nullptr);
        mVertexBuffer = VK_NULL_HANDLE;
    }
    if (mVertexBufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(mRenderer->device, mVertexBufferMemory, nullptr);
        mVertexBufferMemory = VK_NULL_HANDLE;
    }
    if (mIndexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mRenderer->device, mIndexBuffer, nullptr);
        mIndexBuffer = VK_NULL_HANDLE;
    }
    if (mIndexBufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(mRenderer->device, mIndexBufferMemory, nullptr);
        mIndexBufferMemory = VK_NULL_HANDLE;
    }
}

void Mesh::createBuffers()
{
    mRenderer->createVertexBuffer(this);

    // Only create index buffer if we have indices
    if (mIndices.size() > 0)
        mRenderer->createIndexBuffer(this);
}

void Mesh::makeTriangle()
{
    mVertices = {
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.f, 1.f}},
        {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 0.0f}}
    };
}

void Mesh::makeQuad()
{
    // Second parameter is now the surface normal!
    mVertices = {
        {{-0.5f, -0.5f, 0.f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.5f, 0.5f, 0.f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}
    };

    mIndices = {
        0, 1, 2, 2, 3, 0
    };
}

void Mesh::makeObj()
{
    std::ifstream fileIn;
    fileIn.open( PATH + "Assets/" + mFileName, std::ifstream::in);
    if(!fileIn)
    {
        LOGE("ERROR: Could not load mesh!");
        LOGP("     Could not open file for reading: %s", mFileName.c_str());
        LOGW("     Making default triangle instead");
        makeTriangle();
    }

    std::string oneLine;
    std::string oneWord;
    std::vector<glm::vec3> tempVertices;
    std::vector<glm::vec3> tempNormals;
    std::vector<glm::vec2> tempUVs;

    unsigned int temp_index = 0;
    while(std::getline(fileIn,oneLine))
    {
        std::stringstream sStream;
        sStream << oneLine;
        oneWord = "";
        sStream >> oneWord;
        if(oneWord == "#")  // comment
            continue;

        if (oneWord == "")  // empty line
            continue;

        if(oneWord == "v")  // vertex
        {
            glm::vec3 tempVertex;
            sStream >> oneWord;
            tempVertex.x = std::stof(oneWord);
            sStream >>oneWord;
            tempVertex.y = std::stof(oneWord);
            sStream >>oneWord;
            tempVertex.z = std::stof(oneWord);
            tempVertices.push_back(tempVertex);

            continue;
        }
        if(oneWord == "vt") // uv coordinate
        {
            glm::vec2 tempUV;
            sStream >> oneWord;
            tempUV.x = std::stof(oneWord);
            sStream >> oneWord;
            tempUV.y = std::stof(oneWord);
            tempUVs.push_back(tempUV);

            continue;
        }
        if(oneWord == "vn") // normal
        {
            glm::vec3 tempNormal;
            sStream >> oneWord;
            tempNormal.x = std::stof(oneWord);
            sStream >> oneWord;
            tempNormal.y = std::stof(oneWord);
            sStream >> oneWord;
            tempNormal.z = std::stof(oneWord);
            tempNormals.push_back(tempNormal);

            continue;
        }
        if(oneWord == "f")  // face
        {
            int index, normal, uv;
            for(int i = 0; i<3; i++)
            {
                sStream >> oneWord;
                std::stringstream tempWord(oneWord);
                std::string segment;
                std::vector<std::string> segmentArray;

                while(std::getline(tempWord, segment, '/'))
                    segmentArray.push_back(segment);

                index =std::stoi(segmentArray[0]);

                if(segmentArray[1] !="")
                    uv = std::stoi(segmentArray[1]);
                else
                    uv = 0;

                normal = std::stoi(segmentArray[2]);

                // obj indexes starts at 1, C++ starts at 0
                index--;
                uv--;
                normal--;

                if(uv > -1)
                {
                    Vertex tempVertex{tempVertices[index],tempNormals[normal] ,tempUVs[uv]};
                    mVertices.push_back(tempVertex);
                }
                else
                {
                    Vertex tempVertex{tempVertices[index],tempNormals[normal] ,glm::vec2{0,0}};
                    mVertices.push_back(tempVertex);
                }

                mIndices.push_back(temp_index++);
            }
            continue;
        }
    }
    fileIn.close();

    LOGP("obj file %s made into a mesh!", mFileName.c_str());
}




