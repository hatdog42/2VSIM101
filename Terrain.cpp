#include "Terrain.h"
#include "Renderer.h"
#include <glm/glm.hpp>
#include <cmath>
#include "External/stb_image.h"

Terrain::Terrain(Renderer* render, int gridSize, int scale) : Mesh(render), mScale(scale)
{
    // Clear any data initialized by the base Mesh constructor
    mVertices.clear();
    mIndices.clear();

    // getting the heightmap texture for the terrain
    // The terrain texture should be square, that is the only tested format
    int width;           //Width - x-axis
    int depth;           //Depth - z-axis
    int channels;

    stbi_uc* pixels = stbi_load(std::string(PATH + "Assets/Heightmap.jpg").c_str(),
                                &width, &depth, &channels, STBI_rgb_alpha);

    if (pixels == nullptr) {
        LOGE("Heightmap not found - making default mesh ! ");
        makeTriangle();
        mFileName = "Terrain";
        return;
    }

    int channelsPerPixel = 4; // We requested RGBA (STBI_rgb_alpha), so we expect 4 channels per pixel

    //scaling the height read from the heightmap. 0 + 255 meters if this is set to 1
    float heightSpacing{.3f};
    //offset the whole terrain in y (height) axis
    float heightPlacement{-18.f};

    // number of vertices per axis is vertsPerAxis (gridSize squares-1 -> gridSize vertices)
    int vertsPerAxis = width;
    float texDen = static_cast<float>(vertsPerAxis); // divide by vertsPerAxis to get texture coords in [0,1]

    // create vertices
    for (int z = 0; z < vertsPerAxis; ++z)
    {
        for (int x = 0; x < vertsPerAxis; ++x)
        {
            float h = pixels[(z * width + x) * channelsPerPixel] * heightSpacing + heightPlacement;
            Vertex v;
            v.position = {(float)x * mScale, h, (float)z * mScale };
            v.color = {0.0f, 1.0f, 0.0f};
            v.textureCoordinate = {(texDen > 0.0f) ? (float)x / texDen : 0.0f, (texDen > 0.0f) ? (float)z / texDen : 0.0f};
            mVertices.push_back(v);
        }
    }

    LOGE("Vertices in HeightMap: " + std::to_string(mVertices.size()));

    // create indices: two triangles per square
    for (int z = 0; z < vertsPerAxis-1; ++z)
    {
        for (int x = 0; x < vertsPerAxis-1; ++x)
        {
            uint32_t topLeft = z * vertsPerAxis + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (z + 1) * vertsPerAxis + x;
            uint32_t bottomRight = bottomLeft + 1;

            // triangle 1
            mIndices.push_back(topLeft);
            mIndices.push_back(bottomLeft);
            mIndices.push_back(topRight);

            // triangle 2
            mIndices.push_back(topRight);
            mIndices.push_back(bottomLeft);
            mIndices.push_back(bottomRight);
        }
    }

    LOGE("Indices in HeightMap: " + std::to_string(mIndices.size()));

    createBuffers();

    calculateHeighMapNormals(width, depth);

    // Free the image memory after creating the mesh
    stbi_image_free(pixels);

    mFileName = "Terrain";
}

float Terrain::heightAtPoint(const glm::vec2& point)
{
    if (mVertices.empty())
        return 0.0f;

    // Compute which square the world point lies in. Use floor to handle positions correctly.
    int xSquare = std::floor(point.x / mScale);
    int zSquare = std::floor(point.y / mScale);

    // Compute verts per axis from vertex count
    int vertsPerAxis = static_cast<int>(std::sqrt(mVertices.size()));
    if (vertsPerAxis < 2)
        return 0.0f;

    // If point is outside the grid, return 0
    if (xSquare < 0 || xSquare >= vertsPerAxis - 1 || zSquare < 0 || zSquare >= vertsPerAxis - 1)
    {
        LOGE("Mesh not on the heightmap terrain");
        return 0.0f;
    }

    // Indices of the square's vertices
    uint32_t topLeft = zSquare * vertsPerAxis + xSquare;
    uint32_t topRight = topLeft + 1;
    uint32_t bottomLeft = (zSquare + 1) * vertsPerAxis + xSquare;
    uint32_t bottomRight = bottomLeft + 1;

    // Compute local coordinates inside the square (origin at top-left corner of the square)
    glm::vec2 squareOrigin = glm::vec2(xSquare * mScale, zSquare * mScale);
    glm::vec2 localPoint = point - squareOrigin;

    // Build triangle vertex positions in the same local coordinate space (x,z)
    glm::vec2 pTL = glm::vec2(mVertices[topLeft].position.x, mVertices[topLeft].position.z) - squareOrigin;
    glm::vec2 pBL = glm::vec2(mVertices[bottomLeft].position.x, mVertices[bottomLeft].position.z) - squareOrigin;
    glm::vec2 pTR = glm::vec2(mVertices[topRight].position.x, mVertices[topRight].position.z) - squareOrigin;
    glm::vec2 pBR = glm::vec2(mVertices[bottomRight].position.x, mVertices[bottomRight].position.z) - squareOrigin;

    // First triangle: topLeft, bottomLeft, topRight
    glm::vec3 baryc = barycentricCoordinates(pTL, pBL, pTR, localPoint);
    if (baryc.x >= 0.0f && baryc.y >= 0.0f && baryc.z >= 0.0f)
    {
        return baryc.x * mVertices[topLeft].position.y + baryc.y * mVertices[bottomLeft].position.y + baryc.z * mVertices[topRight].position.y;
    }

    // Second triangle: bottomLeft, bottomRight, topRight
    baryc = barycentricCoordinates(pBL, pBR, pTR, localPoint);
    if (baryc.x >= 0.0f && baryc.y >= 0.0f && baryc.z >= 0.0f)
    {
        return baryc.x * mVertices[bottomLeft].position.y + baryc.y * mVertices[bottomRight].position.y + baryc.z * mVertices[topRight].position.y;
    }

    // Not inside either triangle (shouldn't happen if point inside grid), return 0
    LOGE("Mesh not in a triangle even when it is inside the terrain!!!! What????");
    return 0.0f;
}

glm::vec3 Terrain::barycentricCoordinates(const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, 
    glm::vec2 testPoint)
{
	// The testPoint in is the point we want to find the barycentric coordinates of, 
    // with respect to the triangle defined by p1, p2, and p3.

	// Compute the area of the whole triangle (p1, p2, p3) using the cross product method
    glm::vec2 p12 = p2 - p1;
    glm::vec2 p13 = p3 - p1;
    glm::vec3 n = glm::cross(glm::vec3(p12, 0.0f), glm::vec3(p13, 0.0f)); // p12 ^ p13;
    float areal_123 = n.z; //  Need to keep the sign here - lenght gives absolute value - n.length(); // double area

    glm::vec3 baryc; // for return. Remember
    
	// Compute the area of the sub-triangles formed by the testPoint and each edge of the triangle,
    // u
    glm::vec2 p = p2 - testPoint; // *this;
    glm::vec2 q = p3 - testPoint; // *this;
    n = glm::cross(glm::vec3(p, 0.0f), glm::vec3(q, 0.0f)); //p ^ q;
    baryc.x = n.z / areal_123;
    // v
    p = p3 - testPoint; // *this;
    q = p1 - testPoint; //*this;
    n = glm::cross(glm::vec3(p, 0.0f), glm::vec3(q, 0.0f)); // p ^ q;
    baryc.y = n.z / areal_123;
    // w
    p = p1 - testPoint; //*this;
    q = p2 - testPoint; //*this;
    n = glm::cross(glm::vec3(p, 0.0f), glm::vec3(q, 0.0f));  //p ^ q;
    baryc.z = n.z / areal_123;
    return baryc;
}

void Terrain::calculateHeighMapNormals(int width, int depth)
{
    // TODO: This can be optimized and calculated while heightMap is read in

    // Not tested for non-square textures:
    if (width != depth)
    {
        LOG("Normals for ground not calclulated! Use square texture!");
        return;
    }

    int resolutionSquared = width * depth;

    // terrain has the diagonal of the quads pointing up and left, like |\| (not |/|)
    // OEF: not sure I take this into account correctly in the normal calculation!

    // NB: Special case - bottom row has no vertices below
    // NB: Special case - top row has no vertices above
    // NB: Special case - left column has no vertices to the left
    // NB: Special case - right column has no vertices to the right

    for( int i{0}; i < width; i++)
    {
        //temporary normals for each of the 6 surrounding triangles
        glm::vec3 surroundingNormals[6]{};
        std::fill_n(surroundingNormals, 6, glm::vec3{ 0, 1, 0 });

        //the center vertex that we calculate the normal for
        glm::vec3 center = mVertices.at(i).position;

        glm::vec3 first{};  //first vector will be crossed with ...
        glm::vec3 second{}; //second vector - check right hand rule!

        // a: first = north, second = west
        //check if we are to the left, if so - skip:
        //If first vertex - skip - already (0, 1, 0)
        if(i != 0)
        {
            first = mVertices.at(i + width).position - center;
            second = mVertices.at(i - 1).position - center;
            surroundingNormals[0] = glm::cross(first, second);
        }
        // b: first and second flips so we don't have to calculate a vector we already have!!
        // second = north east, first = north
        //check if we are at left, if so - skip
        if( ((i + 1) % width) == false)
        {
            second = mVertices.at(i + width + 1).position - center;
            //first = mVertices.at(i+ mWidth).mXYZ - center;
            surroundingNormals[1] = glm::cross(second, first);

            //c: first = east, second = north east
            first = mVertices.at(i + 1).position - center;
            //second = mVertices.at(i + mWidth + 1).mXYZ - center;
            surroundingNormals[2] = glm::cross(first, second);
        }

        //add all vectors and normalize
        glm::vec3 result{};
        for(int i{0}; i < 6; i++)
        {
            surroundingNormals[i] = glm::normalize(surroundingNormals[i]);
            result += surroundingNormals[i];
        }
        result = glm::normalize(result);

        //put the normal into the vertex
        //        qDebug() << result;
        mVertices.at(i).color = result;
    }

    //calculate each of the triangles normal - omitting the outer vertices
    //starting av mWidth + 1 == the second vertex on the second row
    //ending each row at second to last vertex, to not hit the outer edge
    //ending at resolutionSquared - mWidth - 2 == second to last vertex on the second to last row
    for(int i = width + 1 ; i < resolutionSquared - width - 2; i++)
    {
        //if at the end of a row, jump to next
        if( (i + 2) % width == 0)
        {   i += 2; //have to add 2 to get to the next correct index
            continue;
        }
        //goes in a counter clockwise direction
        //terrain has the diagonal of the quads pointing up and right, like |/| (not |\|)

        //temporary normals for each of the 6 surrounding triangles
        glm::vec3 surroundingNormals[6]{};

        //the center vertex that we calculate the normal for
        glm::vec3 center = mVertices.at(i).position;

        //first and second vector flips each time because second
        //is the "first" at the next calculation.
        //We don't want to calculate again!
        glm::vec3 first{};
        glm::vec3 second{};

        // a 0: first = north, second = west
        //check if we are to the left, if so - skip:
        first = mVertices.at(i + width).position - center;
        second = mVertices.at(i - 1).position - center;
        surroundingNormals[0] = glm::cross(first, second);

        // b 1: first = south west, second = west
        first = mVertices.at(i - width - 1).position - center;
        surroundingNormals[1] = glm::cross(second, first);

        //c 2: first = south west, second = south
        second = mVertices.at(i - width).position - center;
        surroundingNormals[2] = glm::cross(first, second);

        //d 3: second = south, first = east
        //check if we are to the left, if so - skip
        first = mVertices.at(i + 1).position - center;
        surroundingNormals[3] = glm::cross(second, first);

        //e 4: first = east, second = north east
        second = mVertices.at(i + width +1).position - center;
        surroundingNormals[4] = glm::cross(first, second);

        //f 5: second = north east, first = north
        first = mVertices.at(i + width).position - center;
        surroundingNormals[5] = glm::cross(second, first);

        //add all vectors and normalize
        glm::vec3 result{};
        for(int i{0}; i < 6; i++)
        {
            surroundingNormals[i] = glm::normalize(surroundingNormals[i]);
            result += surroundingNormals[i];
        }
        result = glm::normalize(result);

        //put the normal into the vertex
        //        qDebug() << result;
        mVertices.at(i).color = result;
    }
    LOG("Normals for ground calclulated");
}

