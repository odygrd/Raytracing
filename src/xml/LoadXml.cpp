/**
 * @file   LoadXml.cpp
 * @author Odysseus Georgoudis
 * @date   31/08/2016
 * @brief  
 *
 */

#include "LoadXml.h"

#include <iostream>
#include <string.h>
#include <memory>
#include <exception>

#include "ParseUtils.h"
#include "../../third-party/tinyxml2/tinyxml2.h"
#include "../core/materials/Blinn.h"
#include "../core/materials/Phong.h"
#include "../core/lights/Ambient.h"
#include "../core/lights/Directional.h"
#include "../core/lights/Point.h"

using tinyxml2::XMLDocument;
using tinyxml2::XMLElement;
using tinyxml2::XML_SUCCESS;

const Sphere ParsedXML::aSphere;

void ParsedXML::LoadXml(const char* file)
{
    XMLDocument xmlDoc;
    xmlDoc.LoadFile(file);

    if (xmlDoc.Error())
    {
        throw std::invalid_argument("ERROR: Failed to load XML file " +
            std::string(file) + ". Invalid location or syntax.\n" +
            "Check if the file is inside the scenes folder "
                "and that the syntax is correct.\n");
    }

    XMLElement* xml { xmlDoc.FirstChildElement("xml") };
    if (xml == nullptr)
    {
        throw std::invalid_argument("ERROR: Failed to load XML file " +
            std::string(file) + ". No \"xml\" tag found.\n");
    }

    XMLElement* sceneXMLElem { xml->FirstChildElement("scene") };
    if (sceneXMLElem == nullptr)
    {
        throw std::invalid_argument("ERROR: Failed to load XML file " +
            std::string(file) + ". No \"scene\" tag found.\n");
    }

    XMLElement* camXMLElem { xml->FirstChildElement("camera") };
    if (camXMLElem == nullptr)
    {
        throw std::invalid_argument("ERROR: Failed to load XML file " +
            std::string(file) + ". No \"camera\" tag found.\n");
    }

    LoadScene(sceneXMLElem);

    LoadCamera(camXMLElem);
}

void ParsedXML::LoadScene(tinyxml2::XMLElement* sceneXMLElement)
{
    XMLElement* sceneXMLElementChild = sceneXMLElement->FirstChildElement();

    while(sceneXMLElementChild)
    {
        if(strcmp(sceneXMLElementChild->Value(), "object") == 0)
        {
            LoadNode(&rootNode, sceneXMLElementChild);
        }
        else if(strcmp(sceneXMLElementChild->Value(), "material") == 0)
        {
            LoadMaterial(sceneXMLElementChild);
        }
        else if(strcmp(sceneXMLElementChild->Value(), "light") == 0)
        {
            LoadLight(sceneXMLElementChild);
        }

        sceneXMLElementChild = sceneXMLElementChild->NextSiblingElement();
    }

    for (auto& elem : nodeMaterial)
    {
        // node's material = find the name in materials map
        elem.first.material_ = materialsMap[elem.second].get();
    }
}

void ParsedXML::LoadCamera(XMLElement* cameraXMLElement)
{
    XMLElement* cameraXMLElemChild = cameraXMLElement->FirstChildElement();

    // load all camera values
    while(cameraXMLElemChild)
    {
        if(strcmp(cameraXMLElemChild->Value(), "position") == 0)
        {
            ReadVector(cameraXMLElemChild, camera.position);
        }
        else if(strcmp(cameraXMLElemChild->Value(), "target") == 0)
        {
            ReadVector(cameraXMLElemChild, camera.direction);
        }
        else if(strcmp(cameraXMLElemChild->Value(), "up") == 0)
        {
            ReadVector(cameraXMLElemChild, camera.up);
        }
        else if(strcmp(cameraXMLElemChild->Value(), "fov") == 0)
        {
            ReadFloat(cameraXMLElemChild, camera.fov);
        }
        else if(strcmp(cameraXMLElemChild->Value(), "width") == 0)
        {
            cameraXMLElemChild->QueryIntAttribute("value", &camera.imageWidth);
        }
        else if(strcmp(cameraXMLElemChild->Value(), "height") == 0)
        {
            cameraXMLElemChild->QueryIntAttribute("value", &camera.imageHeight);
        }

        cameraXMLElemChild = cameraXMLElemChild->NextSiblingElement();
    }

    // compute remaining camera values
    camera.setup();
}

// add a node's children from the XML structure
void ParsedXML::LoadNode(SceneNode* node, XMLElement* objectXMLElement, int level /* = 0 */)
{

    // assign a child to the parent node

    // get the name of the parent node
    const char* name = objectXMLElement->Attribute("name");

    // print out object name
    if (printXml_ == Print::PRINT)
    {
        PrintIndent(level);
        printf("Object [");
        if(name)
        {
            printf("%s", name);
        }
        printf("]");
    }

    // get the type of the parent node
    const char* type = objectXMLElement->Attribute("type");
    const IMesh* mesh { nullptr };
    if(type)
    {
        // for sphere
        if(strcmp(type, "sphere") == 0)
        {
            mesh = &aSphere;

            // print out object type
            if (printXml_ == Print::PRINT)
                printf(" - Sphere");

            // for unknown object
        }
        else
        {
            // print out object type
            if (printXml_ == Print::PRINT)
                printf(" - UNKNOWN TYPE");
        }
    }

    // add the node
    node->AddChildNode(name, mesh);
    auto& currNode = node->GetLastInsertedNode();

    // get the material type of the parent node
    const char* materialName = objectXMLElement->Attribute("material");
    if(materialName)
    {
        // print out object material
        if (printXml_ == Print::PRINT)
            printf(" <%s>", materialName);

        //FIXME
        std::pair<SceneNode&, std::string> tempPair {currNode, materialName};
        nodeMaterial.push_back(tempPair);
    }
    if (printXml_ == Print::PRINT)
        printf("\n");

    // load the appropriate transformation information
    LoadTransform(&currNode, objectXMLElement, level);

    // recursively loop through remaining objects
    for(XMLElement* child = objectXMLElement->FirstChildElement(); child != NULL; child = child->NextSiblingElement())
    {
        if(strcmp(child->Value(), "object") == 0)
            LoadNode(&currNode, child, level + 1);
    }
}

// load in the material information for each element
void ParsedXML::LoadMaterial(XMLElement* materialXMLElement)
{

    // get & print material name
    const char* name = materialXMLElement->Attribute("name");
    if (printXml_ == Print::PRINT)
    {
        printf("Material [");
        if(name)
            printf("%s", name);
        printf("]");
    }

    // get material type
    const char* type = materialXMLElement->Attribute("type");
    if(type)
    {
        // blinn-phong material type
        if(strcmp(type, "blinn") == 0)
        {
            auto blinnMaterial = std::make_unique<BlinnMaterial>();

            // print out material type
            if (printXml_ == Print::PRINT)
                printf(" - Blinn\n");

            // check children for material properties
            for(XMLElement *child = materialXMLElement->FirstChildElement(); child != NULL; child = child->NextSiblingElement())
            {
                // initialize values
                Color c(1, 1, 1);
                float f = 1.0;

                // load diffuse color
                if(strcmp(child->Value(), "diffuse") == 0)
                {
                    ReadColor(child, c);
                    blinnMaterial->setDiffuse(c);

                    // print out diffuse color
                    if (printXml_ == Print::PRINT)
                    {
                        printf("  diffuse %f %f %f\n", c.r, c.g, c.b);
                    }

                    // load specular color
                }
                else if(strcmp(child->Value(), "specular") == 0)
                {
                    ReadColor(child, c);
                    blinnMaterial->setSpecular(c);

                    //print out specular color
                    if (printXml_ == Print::PRINT)
                    {
                        printf("  specular %f %f %f\n", c.r, c.g, c.b);
                    }
                    // load shininess value
                }
                else if(strcmp(child->Value(), "shininess") == 0)
                {
                    ReadFloat(child, f);
                    blinnMaterial->setShininess(f);

                    // print out shininess value
                    if (printXml_ == Print::PRINT)
                    {
                        printf("  shininess %f\n", f);
                    }
                }
            }
            materialsMap[name] = std::move(blinnMaterial);
            // phong material type
        }
        else if(strcmp(type, "phong") == 0)
        {
            auto phongMaterial = std::make_unique<PhongMaterial>();

            // print out material type
            if (printXml_ == Print::PRINT)
            {
                printf(" - Phong\n");
            }
            // check children for material properties
            for(XMLElement *child = materialXMLElement->FirstChildElement(); child != NULL; child = child->NextSiblingElement()){

                // initialize values
                Color c(1, 1, 1);
                float f = 1.0;

                // load diffuse color
                if(strcmp(child->Value(), "diffuse") == 0)
                {
                    ReadColor(child, c);
                    phongMaterial->setDiffuse(c);

                    // print out diffuse color
                    if (printXml_ == Print::PRINT)
                    {
                        printf("  diffuse %f %f %f\n", c.r, c.g, c.b);
                    }
                    // load specular color
                }
                else if(strcmp(child->Value(), "specular") == 0)
                {
                    ReadColor(child, c);
                    phongMaterial->setSpecular(c);

                    //print out specular color
                    if (printXml_ == Print::PRINT)
                    {
                        printf("  specular %f %f %f\n", c.r, c.g, c.b);
                    }
                    // load shininess value
                }
                else if(strcmp(child->Value(), "shininess") == 0)
                {
                    ReadFloat(child, f);
                    phongMaterial->setShininess(f);

                    // print out shininess value
                    if (printXml_ == Print::PRINT)
                    {
                        printf("  shininess %f\n", f);
                    }
                }
            }
            materialsMap[name] = std::move(phongMaterial);
            // unknown material type
        }
        else
        {
            if (printXml_ == Print::PRINT)
            {
                printf(" - UNKNOWN MATERIAL\n");
            }
        }
    }

}

// load in the transformation terms for each node
void ParsedXML::LoadTransform(SceneNode* nodeTransformation, XMLElement* objectXMLElement, int level)
{

    // recursively apply transformations to child nodes that have been set already
    for(XMLElement* child = objectXMLElement->FirstChildElement(); child != NULL; child = child->NextSiblingElement())
    {
        // check if child is a scaling term
        if(strcmp(child->Value(), "scale") == 0)
        {
            float v = 1.0;
            Point s(1, 1, 1);
            ReadFloat(child, v);
            ReadVector(child, s);
            s *= v;
            nodeTransformation->Scale(s.x, s.y, s.z);

            // print out scaling term
            if (printXml_ == Print::PRINT)
            {
                PrintIndent(level + 1);
                printf("scale %f %f %f\n", s.x, s.y, s.z);
            }
            std::cout << " Scaling Node " << nodeTransformation->name_ << nodeTransformation->GetTransform().data << std::endl;
            // check if child is a rotation term
        }
        else if(strcmp(child->Value(), "rotate") == 0)
        {
            Point r(0, 0, 0);
            ReadVector(child, r);
            r.Normalize();
            float a;
            ReadFloat(child, a, "angle");
            nodeTransformation->Rotate(r, a);

            // print out rotation term
            if (printXml_ == Print::PRINT)
            {
                PrintIndent(level + 1);
                printf("rotate %f degrees around %f %f %f\n", a, r.x, r.y, r.z);
            }

            // check if child is a translation term
        }
        else if(strcmp(child->Value(), "translate") == 0)
        {
            Point p(0, 0, 0);
            ReadVector(child, p);
            nodeTransformation->Translate(p);

            // print out translation term
            if (printXml_ == Print::PRINT)
            {
                PrintIndent(level + 1);
                printf("translate %f %f %f\n", p.x, p.y, p.z);
            }
        }
    }
}

// load in the light information for each element
void ParsedXML::LoadLight(XMLElement* lightXMLElement)
{
    // get & print light name
    const char* name = lightXMLElement->Attribute("name");
    if (printXml_ == Print::PRINT)
    {
        printf("Light [");
        if(name)
            printf("%s", name);
        printf("]");
    }

    // get light type
    const char* type = lightXMLElement->Attribute("type");
    if(type)
    {
        // ambient light type
        if(strcmp(type, "ambient") == 0)
        {
            auto ambientLight = std::make_unique<AmbientLight>();

            // print out light type
            if (printXml_ == Print::PRINT) {
                printf(" - Ambient\n");
            }
            // check children for light properties
            for(XMLElement *child = lightXMLElement->FirstChildElement(); child != NULL; child = child->NextSiblingElement()){

                // load intensity (color) of light (for all lights)
                if(strcmp(child->Value(), "intensity") == 0 )
                {
                    Color c(1, 1, 1);
                    ReadColor(child, c);
                    ambientLight->setIntensity(c);

                    // print out light intensity color
                    if (printXml_ == Print::PRINT) {
                        printf("  intensity %f %f %f\n", c.r, c.g, c.b);
                    }
                }
            }
            lightsMap[name] = std::move(ambientLight);
            // direct light type
        }
        else if(strcmp(type, "direct") == 0)
        {
            auto directionalLight = std::make_unique<DirectLight>();

            // print out light type
            if (printXml_ == Print::PRINT)
            {
                printf(" - Direct\n");
            }

            // check children for light properties
            for(XMLElement *child = lightXMLElement->FirstChildElement(); child != NULL; child = child->NextSiblingElement())
            {
                // load intensity (color) of light (for all lights)
                if(strcmp(child->Value(), "intensity") == 0)
                {
                    Color c(1, 1, 1);
                    ReadColor(child, c);
                    directionalLight->setIntensity(c);

                    // print out light intensity color
                    if (printXml_ == Print::PRINT) {
                        printf("  intensity %f %f %f\n", c.r, c.g, c.b);
                    }

                    // load direction of light
                }else if(strcmp(child->Value(), "direction") == 0){
                    Point v(1, 1, 1);
                    ReadVector(child, v);
                    directionalLight->setDirection(v);

                    // print out light direction
                    if (printXml_ == Print::PRINT) {
                        printf("  direction %f %f %f\n", v.x, v.y, v.z);
                    }
                }
            }
            lightsMap[name] = std::move(directionalLight);
            // point light type
        }else if(strcmp(type, "point") == 0)
        {
            auto pointLight = std::make_unique<PointLight>();

            // print out light type
            if (printXml_ == Print::PRINT) {
                printf(" - Point\n");
            }
            // check children for light properties
            for(XMLElement *child = lightXMLElement->FirstChildElement(); child != NULL; child = child->NextSiblingElement()){

                // load intensity (color) of light (for all lights)
                if(strcmp(child->Value(), "intensity") == 0){
                    Color c(1, 1, 1);
                    ReadColor(child, c);
                    pointLight->setIntensity(c);

                    // print out light intensity color
                    if (printXml_ == Print::PRINT) {
                        printf("  intensity %f %f %f\n", c.r, c.g, c.b);
                    }

                    // load position of light
                }else if(strcmp(child->Value(), "position") == 0){
                    Point v(0, 0, 0);
                    ReadVector(child, v);
                    pointLight->setPosition(v);

                    // print out light intensity color
                    if (printXml_ == Print::PRINT) {
                        printf("  position %f %f %f\n", v.x, v.y, v.z);
                    }
                }
            }
            lightsMap[name] = std::move(pointLight);
            // unknown light type
        }else{

            if (printXml_ == Print::PRINT) {
                printf(" - UNKNOWN LIGHT\n");
            }
        }
    }
}