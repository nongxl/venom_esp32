#ifndef CONTAINER_H
#define CONTAINER_H

#include <M5Unified.h>
#include "config.h"
#include "physics/Skeleton.h"

class Container {
public:
    Container();
    
    Face currentFace;
    
    void projectPoint(float x, float y, float z, Face face, float& outX, float& outY, float& outZ, float ax, float ay) const;
    void projectToFace(const Node& n, Face face, float& outX, float& outY, float& outZ, float ax, float ay) const;
    
    void drawContainer(M5Canvas* canvas, float ax, float ay, const Skeleton& skeleton) const;
};

#endif // CONTAINER_H
