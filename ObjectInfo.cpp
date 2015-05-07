#include <iostream>
#include <exception>
#include <string>
#include "OGRE/Ogre.h"
#include "ObjectInfo.h"

namespace std {

	ObjectInfo::ObjectInfo(Ogre::Vector3 pos, const string& objName, Ogre::SceneNode *rootNode) 
	{
		position = pos;
		name = objName;
		node = rootNode->getChild(objName);
	}

	std::string ObjectInfo::getObjectName() {
		return name;
	}

}