/**
 * @file   Ambient.h
 * @author Odysseus Georgoudis
 * @date   01/09/2016
 * @brief  
 *
 */

#ifndef RAYTRACING_AMBIENT_H
#define RAYTRACING_AMBIENT_H

#include "Light.h"

/**
 * Ambient light definition.
 * Ambient light has no direction.
 */
class AmbientLight: public Light
{
public:
    /**
     * Constructors
     */
    AmbientLight(const Color& intensity)
        : Light { intensity, Point { 0, 0, 0} }
    {}

    /**
     * Return true, since light is ambient
     * @return
     */
    bool IsAmbient() const override { return true; }
};

#endif //RAYTRACING_AMBIENT_H
