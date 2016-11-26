/* The MIT License (MIT)
 *
 * Copyright (c) 2015 Giovanni Ortolani, Taneli Mikkonen, Pingjiang Li, Tommi Puolamaa, Mitra Vahida
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE. */

#include <iostream>
#include <exception>
#include <string>
#include <stdlib.h>
#include "ObjectInfo.h"
#include <vector>
#include "OGRE/Ogre.h"
#include "PSphere.h"
#include <OgreMeshSerializer.h>
#include <OgreDataStream.h>
#include <OgreException.h>
#include "OgreConfigFile.h"
#include "Common.h"
#include "ResourceParameter.h"
#include <qdebug.h>
#define FREEIMAGE_LIB
#include "FreeImage.h"
#include <assert.h>

using namespace std;

#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4

#define TESTVECS 40000  // Number of vectors to get height statistics
#define BRACKETS 100    // Number of histogram-slots between min and max height

PSphere::PSphere(Ogre::uint32 iters, Ogre::uint32 gridSize, Ogre::uint16 textureWidth, Ogre::uint16 textureHeight, ResourceParameter resourceParameter){
	vertexes =	NULL;
	vNorms =	NULL;
	texCoords =	NULL;
	indexes =	NULL;
	surfaceTexture =NULL;
	surfaceTextureWidth = textureWidth;
	surfaceTextureHeight = textureHeight;
	exportImage =	NULL;
	observer =	Ogre::Vector3(0.0f, 0.0f, 0.0f);

	create(iters, gridSize, resourceParameter);
}

PSphere::~PSphere()
{
    delete[] indexes;
    delete[] texCoords;
    delete[] vertexes;
    delete[] vNorms;
    delete[] surfaceTexture;
    delete[] exportImage;

    delete faceXM;
    delete faceXP;
    delete faceYM;
    delete faceYP;
    delete faceZM;
    delete faceZP;

    delete gridXM;
    delete gridXP;
    delete gridYM;
    delete gridYP;
    delete gridZM;
    delete gridZP;
}

void PSphere::create(Ogre::uint32 iters, Ogre::uint32 gridSize, ResourceParameter resourceParameter)
{
    RParameter = resourceParameter;
    float waterFraction = resourceParameter.getWaterFraction();
    radius = resourceParameter.getRadius();
    vertexCount = 0;
    indexCount = 0;

    // Iters less than 3 are pointless
    if(iters < 3)
    {
        iters = 3;
        std::cout << "Sphere needs atleast 3 iters" << std::endl;
    }
    // Creating 2D texture with zeros would fail when creating texture with Ogre
    if (surfaceTextureWidth == 0)
    {
        surfaceTextureWidth = 1;
    }
    if (surfaceTextureHeight == 0)
    {
        surfaceTextureHeight = 1;
    }
    /* Make grid big enough, so that so that grid-depending code doesn't make
     * anything nasty. Probably need to be tested. */
    if (gridSize < 2)
        gridSize = 2;

    /* Calling Sphere::create more than once would cause memory leak if we
     * wouldn't delete old allocations first */
    if(vertexes != NULL)
        delete[] vertexes;
    if(vNorms != NULL)
        delete[] vNorms;
    if(texCoords != NULL)
        delete[] texCoords;
    if(indexes != NULL)
        delete[] indexes;

    /* +iter*8 is for texture seam fix, duplicating some vertexes.
     * Approximate, but should be on a safe side */
    vertexes =	new Ogre::Vector3[iters*iters*6 + iters*8];
    vNorms =	new Ogre::Vector3[iters*iters*6 + iters*8];
    texCoords =	new Ogre::Vector2[iters*iters*6 + iters*8];
    indexes =	new Ogre::uint32[(iters-1)*(iters-1)*6*6];

    Ogre::Matrix3 noRot, rotZ_90, rotZ_180, rotZ_270, rotX_90, rotX_270;

    noRot = Ogre::Matrix3(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    rotZ_90 = Ogre::Matrix3(0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    rotZ_180 = Ogre::Matrix3(-1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    rotZ_270 = Ogre::Matrix3(0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    rotX_90 = Ogre::Matrix3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);
    rotX_270 = Ogre::Matrix3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f);

    srand(RParameter.getSeed());
    randomTranslate.x = (float)((rand() % 1000)-500)/100.0f;
    randomTranslate.y = (float)((rand() % 1000)-500)/100.0f;
    randomTranslate.z = (float)((rand() % 1000)-500)/100.0f;

    calculateSeaLevel(minimumHeight, maximumHeight, waterFraction);

    // No rotation
    faceYP = new HeightMap(iters, noRot, &RParameter, seaHeight);
    gridYP = new Grid(gridSize, noRot);
    // 90 degrees through z-axis
    faceXM = new HeightMap(iters, rotZ_90, &RParameter, seaHeight);
    gridXM = new Grid(gridSize, rotZ_90);
    // 180 degrees through z-axis
    faceYM = new HeightMap(iters, rotZ_180, &RParameter, seaHeight);
    gridYM = new Grid(gridSize, rotZ_180);
    // 270 degrees through z-axis
    faceXP = new HeightMap(iters, rotZ_270, &RParameter, seaHeight);
    gridXP = new Grid(gridSize, rotZ_270);
    // 90 degrees through x-axis
    faceZP = new HeightMap(iters, rotX_90, &RParameter, seaHeight);
    gridZP = new Grid(gridSize, rotX_90);
    // 270 degrees through x-axis
    faceZM = new HeightMap(iters, rotX_270, &RParameter, seaHeight);
    gridZM = new Grid(gridSize, rotX_270);

    gridYP->setNeighbours(gridXM, gridXP, gridZP, gridZM);
    gridXM->setNeighbours(gridYM, gridYP, gridZP, gridZM);
    gridYM->setNeighbours(gridXP, gridXM, gridZP, gridZM);
    gridXP->setNeighbours(gridYP, gridYM, gridZP, gridZM);
    gridZP->setNeighbours(gridXM, gridXP, gridYM, gridYP);
    gridZM->setNeighbours(gridXM, gridXP, gridYP, gridYM);


    surfaceTexture = new unsigned char[surfaceTextureWidth*surfaceTextureHeight*3];
    generateImage(surfaceTextureWidth, surfaceTextureHeight, surfaceTexture);//take longtime

    // Requires variable seaHeight that is set by calculateSeaLevel
    setGridLandInfo(gridYP);
    setGridLandInfo(gridXM);
    setGridLandInfo(gridYM);
    setGridLandInfo(gridXP);
    setGridLandInfo(gridZP);
    setGridLandInfo(gridZM);

}

void PSphere::calculateSeaLevel(float &minElev, float &maxElev, float seaFraction)
{
    Ogre::uint32 i, accumulator=0;

    unsigned int histogram[BRACKETS]={0};
    Ogre::Real testHeight[TESTVECS];
    Ogre::Vector3 testVec;

    vector <float> frequency = RParameter.getFrequency();
    vector <float> amplitude = RParameter.getAmplitude();

    minElev = 1e6f;
    maxElev = -1e6f;

    /* Create random (hopefully evenly distributed) test-vectors to gather
     * statistics for height-histogram */
    for(i=0; i < TESTVECS; i++)
    {
        testVec = Ogre::Vector3(static_cast<float>((rand() % 65536)-32768),
                                static_cast<float>((rand() % 65536)-32768),
                                static_cast<float>((rand() % 65536)-32768));
        testVec.normalise();
        testHeight[i] = heightNoise(amplitude, frequency, testVec + randomTranslate);
        if (minElev > testHeight[i])
            minElev = testHeight[i];
        if (maxElev < testHeight[i])
            maxElev = testHeight[i];
    }

    float mult = static_cast<float>(BRACKETS-1) + 0.5f;
    unsigned int slot;
    /* Divide height variations into slots */
    for(i=0; i < TESTVECS; i++)
    {
        slot = static_cast<unsigned int>((testHeight[i]-minElev)
                                         / (maxElev-minElev) * mult);

        // Let's be sure
        assert(slot < BRACKETS);
        histogram[slot] += 1;
    }

    /* Go through slots, until it accumulates more samples than all
     * samples * seaFraction */
    for(i=0; i < BRACKETS; i++)
    {
        accumulator += histogram[i];
        if(Ogre::Real(accumulator) > Ogre::Real(TESTVECS)*seaFraction)
            break;
    }
    // Figure out offset with i
    seaHeight = Ogre::Real(i) / static_cast<float>(BRACKETS-1)
                                 * (maxElev-minElev) + minElev;

}

void PSphere::generateImage(unsigned short textureWidth, unsigned short textureHeight, unsigned char *image)
{
    Ogre::Vector3 spherePoint;
    Ogre::Real latitude, longitude;
    Ogre::Real height;
    Ogre::uint32 x, y;
    Ogre::ColourValue water1st, water2nd, terrain1st, terrain2nd, mountain1st, mountain2nd, Pixel;
    unsigned char red, green, blue;
    vector <float> frequency = RParameter.getFrequency();
    vector <float> amplitude = RParameter.getAmplitude();


    RParameter.getWaterFirstColor(red, green, blue);
    water1st.r = red;
    water1st.g = green;
    water1st.b = blue;
    RParameter.getWaterSecondColor(red, green, blue);
    water2nd.r = red;
    water2nd.g = green;
    water2nd.b = blue;

    RParameter.getTerrainFirstColor(red, green, blue);
    terrain1st.r = red;
    terrain1st.g = green;
    terrain1st.b = blue;
    RParameter.getTerrainSecondColor(red, green, blue);
    terrain2nd.r = red;
    terrain2nd.g = green;
    terrain2nd.b = blue;

    RParameter.getMountainFirstColor(red, green, blue);
    mountain1st.r = red;
    mountain1st.g = green;
    mountain1st.b = blue;
    RParameter.getMountainSecondColor(red, green, blue);
    mountain2nd.r = red;
    mountain2nd.g = green;
    mountain2nd.b = blue;

    for(y=0; y < textureHeight; y++)
    {
        for(x=0; x < textureWidth; x++)
        {
            longitude = (Ogre::Real(x)+0.5f)/textureWidth*360.0f;
            latitude = 90.0f - (Ogre::Real(y)+0.5f)/textureHeight*180.0f;

            // Get a point that corresponds to a given pixel
            spherePoint = convertSphericalToCartesian(latitude, longitude);

            // Get height of a point
            height = heightNoise(amplitude, frequency, spherePoint + randomTranslate);

            Pixel = generatePixel(height,
                                  seaHeight,
                                  minimumHeight,
                                  maximumHeight,
                          water1st,
                          water2nd,
                          terrain1st,
                          terrain2nd,
                          mountain1st,
                          mountain2nd);

            image[((textureHeight-1-y)*textureWidth+x)*3] = Pixel.r;
            image[((textureHeight-1-y)*textureWidth+x)*3+1] = Pixel.g;
            image[((textureHeight-1-y)*textureWidth+x)*3+2] = Pixel.b;
        }
    }
}

void PSphere::setGridLandInfo(Grid *grid)
{
    unsigned int x, y;
    Ogre::Vector3 spherePos;
    Ogre::Real height;

    vector <float> frequency = RParameter.getFrequency();
    vector <float> amplitude = RParameter.getAmplitude();

    for(x=0; x < grid->getSize(); x++)
    {
        for(y=0; y < grid->getSize(); y++)
        {
            spherePos = grid->projectToSphere(x, y);
            height = heightNoise(amplitude, frequency, spherePos + randomTranslate);

            if (height > seaHeight)
                grid->setValue(x, y, 1);
            else
                grid->setValue(x, y, 0);
        }
    }
}

void PSphere::setObserverPosition(Ogre::Vector3 position)
{
	observer = position;
}

void PSphere::fixTextureSeam()
{
	Ogre::uint32 i, j, affectedTriangleCount=0, vCntBeforeFix;
	Ogre::Real absDiff1, absDiff2, absDiff3;

	vCntBeforeFix = vertexCount;

	for(i=0; i < indexCount; i = i + 3)
	{
		absDiff1 = Ogre::Math::Abs(texCoords[indexes[i]].x - texCoords[indexes[i+1]].x);
		absDiff2 = Ogre::Math::Abs(texCoords[indexes[i]].x - texCoords[indexes[i+2]].x);
		absDiff3 = Ogre::Math::Abs(texCoords[indexes[i+1]].x - texCoords[indexes[i+2]].x);

		/* Check for an abrupt change in triangles u-coordinates
		 * (texture-coordinate(u, v)). */
		if(absDiff1 > 0.3f || absDiff2 > 0.3f || absDiff3 > 0.3f)
		{
			for(j=0; j < 3; j++)
			{
				if(texCoords[indexes[i+j]].x < 0.3f)
				{
					// Duplicate offending vertex data
					vertexes[vertexCount] = vertexes[indexes[i+j]];
					vNorms[vertexCount] = vNorms[indexes[i+j]];
					texCoords[vertexCount] = texCoords[indexes[i+j]];

					// Give correct u
					texCoords[vertexCount].x += 1.0f;

					// update index to point to the new vertex
					indexes[i+j] = vertexCount;

					vertexCount++;

				}
			}
			affectedTriangleCount++;
		}
	}

	/* FIXME: Might still have some problems in the poles. Revise if necessary */

	std::cout << "fixTextureSeam:" << std::endl
			  << "  number of fixed triangles "
			  << affectedTriangleCount << std::endl
			  << "  number of individual vertexes duplicated "
			  << vertexCount - vCntBeforeFix << std::endl;
}

Ogre::Real PSphere::getObserverDistanceToSurface()
{
	Ogre::Real height;
	Ogre::Vector3 direction, surfacePos;
	Ogre::Real distance;

	// Hardcode these values for now, waiting for parameter class.
	vector <float> frequency = RParameter.getFrequency();
	vector <float> amplitude = RParameter.getAmplitude();

	// normal vector that points from the origo to the observer
	direction = observer.normalisedCopy();
	/* Get position of the surface along the line that goes from
	 * the planet origo to the observer */
	height = heightNoise(amplitude, frequency, direction + randomTranslate);
	surfacePos = direction*(height*radius + radius);

	distance = fabsf(observer.length()) - fabsf(surfacePos.length());

	return distance;
}

Ogre::Real PSphere::getSurfaceHeight(Ogre::Vector3 Position)
{
	Ogre::Real height;
	Ogre::Vector3 direction, surfacePos;

	// Hardcode these values for now, waiting for parameter class.
	vector <float> frequency = RParameter.getFrequency();
	vector <float> amplitude = RParameter.getAmplitude();

	// normal vector that points from the origo to the observer
	direction = Position.normalisedCopy();
	/* Get position of the surface along the line that goes from
	 * the planet origo to the observer */
	height = heightNoise(amplitude, frequency, direction + randomTranslate);
	surfacePos = direction*(height*radius + radius);

	//distance = fabsf(observer.length()) - fabsf(surfacePos.length());

	return surfacePos.length();
}

Ogre::Real PSphere::getRadius()
{
	return radius;
}

string PSphere::getMeshName()
{
    return meshName[0];
}

string PSphere::getTextureName()
{
    return textureName[0];
}

PSphere* PSphere::getAstroChild(const std::string &objectName)
{
    for (vector<PSphere*>::iterator it = astroObjectsChild.begin() ; it != astroObjectsChild.end(); ++it)
    {
        if ((*it)->getMeshName().compare(objectName) == 0)
        {
            return (*it);
        }
    }
    return NULL;
}

void PSphere::load(Ogre::SceneNode *parent, Ogre::SceneManager *scene, const std::string &planetName, const std::string &textureName)
{
    this->node = parent->createChildSceneNode(planetName);

    faceYP->load(this->node, scene, planetName+"_YP", radius);
    faceXM->load(this->node, scene, planetName+"_XM", radius);
    faceYM->load(this->node, scene, planetName+"_YM", radius);
    faceXP->load(this->node, scene, planetName+"_XP", radius);
    faceZP->load(this->node, scene, planetName+"_ZP", radius);
    faceZM->load(this->node, scene, planetName+"_ZM", radius);
}

void PSphere::unload(Ogre::SceneManager *scene)
{
    faceYP->unload(this->node, scene);
    faceXM->unload(this->node, scene);
    faceYM->unload(this->node, scene);
    faceXP->unload(this->node, scene);
    faceZP->unload(this->node, scene);
    faceZM->unload(this->node, scene);

    scene->destroySceneNode(this->node);
}

void PSphere::loadMeshFile(const std::string &path, const std::string &meshName) {
	Ogre::String source;
	source = path;
	FILE* pFile = fopen( source.c_str(), "rb" );
	if (!pFile)
		OGRE_EXCEPT(Ogre::Exception::ERR_FILE_NOT_FOUND,"File " + source + " not found.", "OgreMeshLoaded");
	struct stat tagStat;
	stat( source.c_str(), &tagStat );
	Ogre::MemoryDataStream* memstream = new Ogre::MemoryDataStream(source, tagStat.st_size, true);
	fread( (void*)memstream->getPtr(), tagStat.st_size, 1, pFile );
	fclose( pFile );
	Ogre::MeshPtr pMesh = Ogre::MeshManager::getSingleton().createManual(meshName,Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
	Ogre::MeshSerializer meshSerializer;
	Ogre::DataStreamPtr stream(memstream);
	meshSerializer.importMesh(stream, pMesh.getPointer());
}

bool PSphere::checkIfObjectIsIn (std::string &objectName) {
	// Check if the object if already attached to the planet. Or at least is in the list of objects (the vector objects)
	for (vector<ObjectInfo>::iterator it = objects.begin() ; it != objects.end(); ++it) {
		ObjectInfo objTemp = *it;
		if (objTemp.getObjectName().compare(objectName) == 0) {
			return true;
		}
	}
	return false;
}

void PSphere::attachMeshSphereCoord(Ogre::SceneNode *node, Ogre::SceneManager *scene, const std::string &meshName, Ogre::Real x, Ogre::Real y, Ogre::Real z) {
    int temp_int = 0;
    string newName = meshName;
    string result;
    string delimiter = ".";
    string sec_node = "sec_node_";
    string nameWithoutFormat = newName.substr(0, newName.find(delimiter)); // Remove the format from the name (the part of the name after the ".")
    string finalName = nameWithoutFormat;
    sec_node = sec_node+finalName;
    while (checkIfObjectIsIn(finalName)) {
        // If the name has already been used it change it adding an auto-increased number in the end
        temp_int++;
        ostringstream convert;
        convert << temp_int;
        string result = convert.str();
        finalName = nameWithoutFormat+result;
        sec_node = sec_node+finalName;
    }

    Ogre::Vector3 position = Ogre::Vector3(x, y, z);
    Ogre::Entity *entity = scene->createEntity(finalName, meshName);
    Ogre::SceneNode *node_secondary = node->createChildSceneNode(sec_node);
    Ogre::SceneNode *node_satellite = node_secondary->createChildSceneNode(finalName, position);
    ObjectInfo object = ObjectInfo(position, finalName, node_secondary);
    objects.push_back(object);
    node_satellite->attachObject(entity);

}

void PSphere::attachMeshSphereCoord(Ogre::SceneNode *node, Ogre::SceneManager *scene, const std::string &meshName, const std::string &objectName, Ogre::Real x, Ogre::Real y, Ogre::Real z) {
	int temp_int = 0;
	string newName = objectName;
	string result;
	string delimiter = ".";
    string sec_node = "sec_node_";
	string nameWithoutFormat = newName.substr(0, newName.find(delimiter)); // Remove the format from the name (the part of the name after the ".")
	string finalName = nameWithoutFormat;
    sec_node = sec_node+finalName;
	while (checkIfObjectIsIn(finalName)) { 
		// If the name has already been used it change it adding an auto-increased number in the end
		temp_int++;
		ostringstream convert;
		convert << temp_int;
		string result = convert.str();
		finalName = nameWithoutFormat+result;
        sec_node = sec_node+finalName;
	}

    Ogre::Vector3 position = Ogre::Vector3(x, y, z);
    Ogre::Entity *entity = scene->createEntity(finalName, meshName);
    Ogre::SceneNode *node_secondary = node->createChildSceneNode(sec_node);
    Ogre::SceneNode *node_satellite = node_secondary->createChildSceneNode(finalName, position);
    ObjectInfo object = ObjectInfo(position, finalName, node_secondary);
    objects.push_back(object);
    node_satellite->attachObject(entity);

}

void PSphere::attachMesh(Ogre::SceneNode *node, Ogre::SceneManager *scene, const std::string &meshName, const std::string &objectName, Ogre::Real latitude, Ogre::Real longitude, Ogre::Real dist) {
    if (dist == 0.0f)
        dist = radius;
    Ogre::Vector3 cart_coord = convertSphericalToCartesian(latitude, longitude);
    Ogre::Real x = dist*2*cart_coord.x;
    Ogre::Real y = dist*2*cart_coord.y;
    Ogre::Real z = dist*2*cart_coord.z;

    this->attachMeshSphereCoord(node, scene, meshName, objectName, x, y, z);
}

void PSphere::attachMesh(Ogre::SceneNode *node, Ogre::SceneManager *scene, const std::string &meshName, Ogre::Real latitude, Ogre::Real longitude, Ogre::Real dist) {
    if (dist == 0.0f)
        dist = radius;
    Ogre::Vector3 cart_coord = convertSphericalToCartesian(latitude, longitude);
	Ogre::Real x = radius*1.2*cart_coord.x;
	Ogre::Real y = radius*1.2*cart_coord.y;
	Ogre::Real z = radius*1.2*cart_coord.z;
    this->attachMeshSphereCoord(node, scene, meshName, x, y, z);

}

void PSphere::attachMeshOnGround(Ogre::SceneNode *node, Ogre::SceneManager *scene, const std::string &meshName, const std::string &objectName, Ogre::Real latitude, Ogre::Real longitude) {
	Ogre::Vector3 cart_coord = convertSphericalToCartesian(latitude, longitude);
	Ogre::Real x = radius*cart_coord.x;
	Ogre::Real y = radius*cart_coord.y;
	Ogre::Real z = radius*cart_coord.z;

	int temp_int = 0;
	string newName = objectName;
	string result;
	string delimiter = ".";
	string nameWithoutFormat = newName.substr(0, newName.find(delimiter)); // Remove the format from the name (the part of the name after the ".")
	string finalName = nameWithoutFormat;
	while (checkIfObjectIsIn(finalName)) { 
		// If the name has already been used it change it adding an auto-increased number in the end
		temp_int++;
		ostringstream convert;
		convert << temp_int;
		string result = convert.str();
		finalName = nameWithoutFormat+result;
	}

	Ogre::Vector3 position = Ogre::Vector3(x, y, z);
	Ogre::Real surfaceHeight = getSurfaceHeight(position);
	Ogre::Entity *entity = scene->createEntity(finalName, meshName);
	Ogre::SceneNode *cube = node->createChildSceneNode(finalName);
	cube->attachObject(entity);

	//cube->_updateBounds();
	//float objectSize=cube->_getWorldAABB().getSize().length();
	//float ratio = (surfaceHeight+objectSize/2 )/position.length();
	float ratio = (surfaceHeight )/position.length();
	position = position*ratio;
	cube->setPosition( position );

	//change orientation
	Ogre::Quaternion q = Ogre::Vector3::UNIT_Y.getRotationTo(position);
	cube->setOrientation( q );

	ObjectInfo object = ObjectInfo(position, finalName, node);
	objects.push_back(object);
}

void PSphere::attachAstroParent(PSphere *object)
{
    astroObjectsParent.push_back(object);
}

void PSphere::attachAstroChild(PSphere *object, Ogre::Real x, Ogre::Real y, Ogre::Real z)
{
    string objectMeshName = object->getMeshName();
    astroObjectsChild.push_back(object);
    object->attachAstroParent(this);
//    Ogre::Entity* entity = object->getEntity();

    string secNodeName = "sec_node_";
    string nodeObjectName = "node_";
    secNodeName = secNodeName + objectMeshName;
    nodeObjectName = nodeObjectName + objectMeshName;
    Ogre::SceneNode *nodeSecondary = this->node->createChildSceneNode(secNodeName);
    Ogre::SceneNode *nodeAstroChild = nodeSecondary->createChildSceneNode(objectMeshName);

//    nodeAstroChild->attachObject(entity);
    object->setNode(nodeAstroChild);
    nodeAstroChild->setPosition(x, y, z);
}

void PSphere::setNode(Ogre::SceneNode *node)
{
    this->node = node;
}

Ogre::SceneNode* PSphere::getNode()
{
    return node;
}

bool PSphere::getGridLocation(Ogre::Vector3 location, Grid **face,
							  unsigned int &ix, unsigned int &iy)
{
	Grid *grid;
	Ogre::Real x, y, z, x_f, y_f;

	x = Ogre::Math::Abs(location.x);
	y = Ogre::Math::Abs(location.y);
	z = Ogre::Math::Abs(location.z);

	/* If two or three vector elements equal to each other, they are on
	 * cube edges. This slightly shortens y compared to x and z compared to y.
	 *  This results in ix and iy to fall within a correct range. */
	if (x == y)
	{
		location.y *= 0.9999;

	}
	if (x == z)
	{
		location.z *= 0.9999;
	}
	if (y == z)
	{
		location.z *= 0.9999;
	}

	// reassign absolut values in case location vector was changed
	x = Ogre::Math::Abs(location.x);
	y = Ogre::Math::Abs(location.y);
	z = Ogre::Math::Abs(location.z);

	if (x > y && x > z)
	{
		// Scale longest component to unit length
		location *= (1.0f/x);
		// Set grid y-component
		y_f = location.z;
		// Check if this is positive or negative face
		if (location.x < 0.0f)
		{
			// grid x-component
			x_f = -location.y;
			grid = gridXM;
		}
		else
		{
			x_f = location.y;
			grid = gridXP;
		}
	}
	else if (y > x && y > z)
	{
		location *= (1.0f/y);
		y_f = location.z;
		if (location.y < 0.0f)
		{
			x_f = location.x;
			grid = gridYM;
		}
		else
		{
			x_f = -location.x;
			grid = gridYP;
		}
	}
	else if (z > x && z > y)
	{
		location *= (1.0f/z);
		x_f = -location.x;
		if (location.z < 0.0f)
		{
			y_f = location.y;
			grid = gridZM;
		}
		else
		{
			y_f = -location.y;
			grid = gridZP;
		}
	}
	else
	{
		return false;
	}

	iy = (unsigned short)((1.0f+y_f)/2.0f*grid->getSize());
	ix = (unsigned short)((1.0f+x_f)/2.0f*grid->getSize());

	(*face) = grid;

	return true;
}

bool PSphere::checkAccessibility(Ogre::Vector3 location)
{
	Grid *grid, *gridObj;
	unsigned int i, ix, iy, Obj_x, Obj_y;
	Ogre::Vector3 ObjPos;

	if (getGridLocation(location, &grid, ix, iy))
	{
		// Check if location to check has already an object
		for(i=0; i < objects.size(); i++)
		{
			ObjPos = objects[i].getPosition();
			if (!getGridLocation(ObjPos, &gridObj, Obj_x, Obj_y))
				return false;

			// Checks if location and object is on a same grid
			if (grid == gridObj)
			{
				if ( (ix == Obj_x) && (iy == Obj_y) )
					return false;
			}
		}

		// If land mask is nonzero, it is accessible.
		if (grid->getValue(ix, iy) != 0)
			return true;
		else
			return false;
	}
	else
	{
		return false;
	}
}

Ogre::Vector3 PSphere::nextPosition(Ogre::Vector3 location, PSphere::Direction dir)
{
	Ogre::Vector3 newPos;
	Grid *grid;
	unsigned int int_x, int_y;

	/* Using 3D cartesian position figures out which face of the 6 cubefaces it
	 * resides, and gives integer grid-coordinates x and y for it. */
	if (!getGridLocation(location, &grid, int_x, int_y))
		return Ogre::Vector3(0.0f, 0.0f, 0.0f);

	// Going y+
	if (dir == PSPHERE_GRID_YPLUS)
	{
		// Handles migrating from one grid to the next
		if (int_y == grid->getSize()-1)
		{
			// Outputs adjacent x and y on neighboring grid by using current grid x and y
			grid->getNeighbourEntryCoordinates(Grid::neighbour_YP, int_x, int_y);
			// Set neighbour as a grid
			grid = grid->getNeighbourPtr(Grid::neighbour_YP);
		}
		else
			int_y++;
	}
	else if (dir == PSPHERE_GRID_YMINUS)
	{
		if (int_y == 0)
		{
			grid->getNeighbourEntryCoordinates(Grid::neighbour_YM, int_x, int_y);
			grid = grid->getNeighbourPtr(Grid::neighbour_YM);
		}
		else
			int_y--;
	}
	else if (dir == PSPHERE_GRID_XPLUS)
	{
		if (int_x == grid->getSize()-1)
		{
			grid->getNeighbourEntryCoordinates(Grid::neighbour_XP, int_x, int_y);
			grid = grid->getNeighbourPtr(Grid::neighbour_XP);
		}
		else
			int_x++;
	}
	else if (dir == PSPHERE_GRID_XMINUS)
	{
		if (int_x == 0)
		{
			grid->getNeighbourEntryCoordinates(Grid::neighbour_XM, int_x, int_y);
			grid = grid->getNeighbourPtr(Grid::neighbour_XM);
		}
		else
			int_x--;
	}

	// Project 2D grid-location back to 3D cartesian coordinate
	newPos = grid->projectToSphere(int_x, int_y);

	return newPos;
}

vector<ObjectInfo> *PSphere::getObjects()
{
	return &objects;
}

void PSphere::setCollisionManager(CollisionManager	*CDM)
{
	CollisionDetectionManager = CDM;
}

unsigned char *PSphere::exportMap(unsigned short width, unsigned short height, MapType type) {

	// Check if exportImage is already allocated
	if (exportImage != NULL)
		delete[] exportImage;

	if (type == MAP_EQUIRECTANGULAR)
	{
		exportImage = new unsigned char[width*height*3];

		// Generates the map. exportImage points to it.
		generateImage(width, height, exportImage);
	}
	else if (type == MAP_CUBE)
	{
		unsigned short x, y, i, gSize;
		unsigned char red, green, blue;
		Ogre::Real elev;
		Grid *temp[6];
		Ogre::ColourValue water1st, water2nd, Output;

		RParameter.getWaterFirstColor(red, green, blue);
		water1st.r = red;
		water1st.g = green;
		water1st.b = blue;
		RParameter.getWaterSecondColor(red, green, blue);
		water2nd.r = red;
		water2nd.g = green;
		water2nd.b = blue;

		Ogre::ColourValue terrain1st, terrain2nd;

		RParameter.getTerrainFirstColor(red, green, blue);
		terrain1st.r = red;
		terrain1st.g = green;
		terrain1st.b = blue;
		RParameter.getTerrainSecondColor(red, green, blue);
		terrain2nd.r = red;
		terrain2nd.g = green;
		terrain2nd.b = blue;

		Ogre::ColourValue mountain1st, mountain2nd;

		RParameter.getMountainFirstColor(red, green, blue);
		mountain1st.r = red;
		mountain1st.g = green;
		mountain1st.b = blue;
		RParameter.getMountainSecondColor(red, green, blue);
		mountain2nd.r = red;
		mountain2nd.g = green;
		mountain2nd.b = blue;

		exportImage = new unsigned char[width*(width/4*3)*3];

		/* Initialize memory. Silences valgrind warning that happens because we
		 * don't set pixel values for every pixel in cubemap, but FreeImage
		 * reads uninitialized values when saving the image. */
		memset(exportImage, 0, width*(width/4*3)*3);

		gSize = width/4;
		temp[0] = new Grid(gSize, gridYP->getOrientation());
		temp[1] = new Grid(gSize, gridXM->getOrientation());
		temp[2] = new Grid(gSize, gridYM->getOrientation());
		temp[3] = new Grid(gSize, gridXP->getOrientation());
		temp[4] = new Grid(gSize, gridZP->getOrientation());
		temp[5] = new Grid(gSize, gridZM->getOrientation());

		// 4 equatorial tiles
		for(i=0; i < 4; i++)
		{
			for(y=0; y < gSize; y++)
			{
				for(x=0; x < gSize; x++)
				{
					elev = heightNoise(RParameter.getAmplitude(), RParameter.getFrequency(), temp[i]->projectToSphere(x, y)+randomTranslate);
					Output = generatePixel(elev,
                                           seaHeight,
                                           minimumHeight,
                                           maximumHeight,
								  water1st,
								  water2nd,
								  terrain1st,
								  terrain2nd,
								  mountain1st,
								  mountain2nd);

					exportImage[((gSize+y)*width+x+i*gSize)*3] = Output.r;
					exportImage[((gSize+y)*width+x+i*gSize)*3+1] = Output.g;
					exportImage[((gSize+y)*width+x+i*gSize)*3+2] = Output.b;
				}
			}
		}
		// +Z tile
		for(y=0; y < gSize; y++)
		{
			for(x=0; x < gSize; x++)
			{
				elev = heightNoise(RParameter.getAmplitude(), RParameter.getFrequency(), temp[4]->projectToSphere(x, y)+randomTranslate);
				Output = generatePixel(elev,
                                       seaHeight,
                                       minimumHeight,
                                       maximumHeight,
							  water1st,
							  water2nd,
							  terrain1st,
							  terrain2nd,
							  mountain1st,
							  mountain2nd);

				exportImage[((gSize*2+y)*width+x)*3] = Output.r;
				exportImage[((gSize*2+y)*width+x)*3+1] = Output.g;
				exportImage[((gSize*2+y)*width+x)*3+2] = Output.b;
			}
		}
		// -Z tile
		for(y=0; y < gSize; y++)
		{
			for(x=0; x < gSize; x++)
			{
				elev = heightNoise(RParameter.getAmplitude(), RParameter.getFrequency(), temp[5]->projectToSphere(x, y)+randomTranslate);
				Output = generatePixel(elev,
                                       seaHeight,
                                       minimumHeight,
                                       maximumHeight,
							  water1st,
							  water2nd,
							  terrain1st,
							  terrain2nd,
							  mountain1st,
							  mountain2nd);

				exportImage[((y)*width+x)*3] = Output.r;
				exportImage[((y)*width+x)*3+1] = Output.g;
				exportImage[((y)*width+x)*3+2] = Output.b;
			}
		}
		// Delete temporary grids
		for(i=0; i < 6; i++)
		{
			delete temp[i];
		}
	}
	else
	{
		std::cerr << "Type not recognized!" << std::endl;
		// memory was deleted.
		exportImage = NULL;
	}

	return exportImage;
}

bool PSphere::exportMap(unsigned short width, unsigned short height, std::string fileName, MapType type) {
	RGBQUAD color;

	/* Create map to memory location pointed by exportImage.
	 * It is class private variable. */
	exportMap(width, height, type);

	if (exportImage == NULL)
	{
		std::cerr << "Map not created!" << std::endl;
		return false;
	}

	// Ignore given height with Cubemap
	if (type == MAP_CUBE)
		height = width/4*3;

	// Use freeimage to save the map as a file
	FreeImage_Initialise();
	FIBITMAP *bitmap = FreeImage_Allocate(width, height, 24);
	for(int i=0; i < width; i++) {
		for (int j=0; j < height; j++) {
			color.rgbRed = exportImage[((width*j)+i)*3];
			color.rgbGreen = exportImage[((width*j)+i)*3+1];
			color.rgbBlue = exportImage[((width*j)+i)*3+2];
			FreeImage_SetPixelColor(bitmap, i, j, &color);
		}
	}
	if ( !FreeImage_Save(FIF_PNG, bitmap, fileName.c_str(), 0) )
	{
		std::cerr << "Saving image " << fileName << " failed!" << std::endl;

		FreeImage_Unload(bitmap);
		FreeImage_DeInitialise();

		return false;
	}
	FreeImage_Unload(bitmap);
	FreeImage_DeInitialise();

	return true;
}

void PSphere::moveObject(const std::string &objectName, int direction, float pace) {
	for (vector<ObjectInfo>::iterator it = objects.begin() ; it != objects.end(); ++it) {
		//ObjectInfo objTemp = *it;
		if (it->getObjectName().compare(objectName) == 0) {
			Ogre::Node *node = it->getNode();
			Ogre::Vector3 oldPosition = node->getPosition();

			Ogre::Vector3 newPosition(oldPosition.x, oldPosition.y, oldPosition.z);

			Ogre::Vector3 oldPositionNormalised = oldPosition.normalisedCopy();

			Ogre::Vector2 cartesianCoord;
			Ogre::Vector3 cart_coord;

			switch (direction) {
				case (UP):
					
					cartesianCoord = Ogre::Vector2(asin(oldPositionNormalised.z ), atan2(oldPositionNormalised.y, oldPositionNormalised.x));
					cartesianCoord = Ogre::Vector2(cartesianCoord.x*(180/Ogre::Math::PI)+pace, 360+cartesianCoord.y*(180/Ogre::Math::PI)); // Convertion from radians to degrees
					if(cartesianCoord.x > 90.0f)//prevent shaking
						break;
					cart_coord = convertSphericalToCartesian(cartesianCoord.x, cartesianCoord.y);

					//set on the ground
					newPosition = cart_coord * ( getSurfaceHeight(cart_coord) / cart_coord.length()) ;

					node->setPosition(newPosition);
					it->setPosition(newPosition);

					//Collision Detection
					if(CollisionDetectionManager->checkCollisionAABB(*it).collided)//collided,move back
					{	
						node->setPosition(oldPosition);
						it->setPosition(oldPosition);
					}else{//not collided, change orientataion and position
						
						//change orientation
						Ogre::Quaternion q;
						q = Ogre::Vector3::UNIT_Y.getRotationTo(newPosition);
						Ogre::Quaternion a;
						node->setOrientation( q );
						//node->yaw ( ( (newPosition-oldPosition).getRotationTo(q*Ogre::Vector3::UNIT_Z).getYaw() ) );
						node->yaw ( Ogre::Math::Abs( (newPosition-oldPosition).getRotationTo(q*Ogre::Vector3::UNIT_Z).getYaw() ) );
					}
					break;
				case (DOWN):

					cartesianCoord=Ogre::Vector2(asin(oldPositionNormalised.z ), atan2(oldPositionNormalised.y, oldPositionNormalised.x));
					cartesianCoord = Ogre::Vector2(cartesianCoord.x*(180/Ogre::Math::PI)-pace, 360+cartesianCoord.y*(180/Ogre::Math::PI)); // Convertion from radians to degrees
					
					if(cartesianCoord.x < -90.0f) //prevent shaking
						break;
					cart_coord = convertSphericalToCartesian(cartesianCoord.x, cartesianCoord.y);
					
					//set on the ground
					newPosition = cart_coord * ( getSurfaceHeight(cart_coord) / cart_coord.length()) ;
					
					node->setPosition(newPosition);
					it->setPosition(newPosition);

					//Collision Detection
					if(CollisionDetectionManager->checkCollisionAABB(*it).collided)//collided,move back
					{	
						node->setPosition(oldPosition);
						it->setPosition(oldPosition);
					}else{//not collided, change orientataion and position
						
						//change orientation
						Ogre::Quaternion q;
						q = Ogre::Vector3::UNIT_Y.getRotationTo(newPosition);
						Ogre::Quaternion a;
						node->setOrientation( q );
						//node->yaw ( ( (newPosition-oldPosition).getRotationTo(q*Ogre::Vector3::UNIT_Z).getYaw() ) );
						
					}
					break;
					
				case (LEFT):

					cartesianCoord = Ogre::Vector2(asin(oldPositionNormalised.z ), atan2(oldPositionNormalised.y, oldPositionNormalised.x));
					cartesianCoord = Ogre::Vector2(cartesianCoord.x*(180/Ogre::Math::PI), 360+cartesianCoord.y*(180/Ogre::Math::PI)-pace); // Convertion from radians to degrees

					cart_coord = convertSphericalToCartesian(cartesianCoord.x, cartesianCoord.y);

					//set on the ground
					newPosition = cart_coord * ( getSurfaceHeight(cart_coord) / cart_coord.length()) ;

					node->setPosition(newPosition);
					it->setPosition(newPosition);

					//Collision Detection
					if(CollisionDetectionManager->checkCollisionAABB(*it).collided)//collided,move back
					{	
						node->setPosition(oldPosition);
						it->setPosition(oldPosition);
					}else{//not collided, change orientataion and position	
						//change orientation
						Ogre::Quaternion q;
						q = Ogre::Vector3::UNIT_Y.getRotationTo(newPosition);
						Ogre::Quaternion a;
						node->setOrientation( q );
						//node->yaw ( Ogre::Math::Abs( (newPosition-oldPosition).getRotationTo(q*Ogre::Vector3::UNIT_Z).getYaw() ) );
					}
					
					break;
				case (RIGHT):

					cartesianCoord = Ogre::Vector2(asin(oldPositionNormalised.z ), atan2(oldPositionNormalised.y, oldPositionNormalised.x));

					cartesianCoord = Ogre::Vector2(cartesianCoord.x*(180.0f/Ogre::Math::PI), 360.0f+cartesianCoord.y*(180.0f/Ogre::Math::PI)+pace); // Convertion from radians to degrees
					
					cart_coord = convertSphericalToCartesian(cartesianCoord.x, cartesianCoord.y);
					
					//set on the ground
					newPosition = cart_coord * ( getSurfaceHeight(cart_coord) / cart_coord.length()) ;

					node->setPosition(newPosition);
					it->setPosition(newPosition);

					//Collision Detection
					if(CollisionDetectionManager->checkCollisionAABB(*it).collided)//collided,move back
					{	
						node->setPosition(oldPosition);
						it->setPosition(oldPosition);
					}else{//not collided, change orientataion and position	
						//change orientation
						Ogre::Quaternion q;
						q = Ogre::Vector3::UNIT_Y.getRotationTo(newPosition);
						Ogre::Quaternion a;
						node->setOrientation( q );
						//node->yaw ( -1*Ogre::Math::Abs( (newPosition-oldPosition).getRotationTo(q*Ogre::Vector3::UNIT_Z).getYaw() ) );
					}
					
					break;
			}
		}
	}
}

void PSphere::moveObjectRevolution(const std::string &objectName, int direction, float pace) {
    for (vector<ObjectInfo>::iterator it = objects.begin() ; it != objects.end(); ++it) {
        //ObjectInfo objTemp = *it;
        if (it->getObjectName().compare(objectName) == 0) {
            Ogre::Node *node = it->getNode();
            Ogre::Vector3 oldPosition = node->getPosition();

            Ogre::Vector3 newPosition(oldPosition.x, oldPosition.y, oldPosition.z);

            Ogre::Vector3 oldPositionNormalised = oldPosition.normalisedCopy();

            Ogre::Vector2 cartesianCoord;
            Ogre::Vector3 cart_coord;

            switch (direction) {
                case (UP):

                    cartesianCoord = Ogre::Vector2(asin(oldPositionNormalised.z ), atan2(oldPositionNormalised.y, oldPositionNormalised.x));
                    cartesianCoord = Ogre::Vector2(cartesianCoord.x*(180/Ogre::Math::PI)+pace, 360+cartesianCoord.y*(180/Ogre::Math::PI)); // Convertion from radians to degrees
                    if(cartesianCoord.x > 90.0f)//prevent shaking
                        break;
                    cart_coord = convertSphericalToCartesian(cartesianCoord.x, cartesianCoord.y);

                    //set on the ground
                    newPosition = cart_coord * ( getSurfaceHeight(cart_coord) / cart_coord.length()) ;

                    node->setPosition(newPosition);
                    it->setPosition(newPosition);
                    break;
                case (DOWN):
                    break;
            }
        }
    }
}

void PSphere::moveAstroChild(const std::string &objectName, Ogre::Real pitch, Ogre::Real yaw, Ogre::Real roll)
{
    string sec_node;
    sec_node = "sec_node_" + objectName;
    Ogre::Node* nodeSecondary;
    nodeSecondary = node->getChild(sec_node);
    if (pitch != 0.0f)
    {
        nodeSecondary->pitch(Ogre::Radian(pitch));
    }
    if (yaw != 0.0f)
    {
        nodeSecondary->yaw(Ogre::Radian(yaw));
    }
    if (roll != 0.0f)
    {
        nodeSecondary->roll(Ogre::Radian(roll));
    }

}

ResourceParameter *PSphere::getParameters() {
	return &RParameter;
}
