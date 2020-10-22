
#include "Stage5/Primitives.cl"
#include "Stage5/Colour.cl"


typedef struct Material
{
	// type of colouring/texturing
	enum { GOURAUD, CHECKERBOARD, CIRCLES, WOOD } type;

	__declspec(align(16)) Colour diffuse;				// diffuse colour
	__declspec(align(16)) Colour diffuse2;			// second diffuse colour, only for checkerboard types

	__declspec(align(16)) Vector offset;				// offset of generated texture
	float size;					// size of generated texture

	__declspec(align(16)) Colour specular;			// colour of specular lighting
	float power;				// power of specular reflection

	float reflection;			// reflection amount
	float refraction;			// refraction amount
	float density;				// density of material (affects amount of defraction)
} Material;


// light object
typedef struct Light
{
	__declspec(align(16)) Point pos;					// location
	__declspec(align(16)) Colour intensity;			// brightness and colour
} Light;


// sphere object
typedef struct Sphere
{
	__declspec(align(16)) Point pos;					// a point on the plane
	float size;					// radius of sphere
	unsigned int materialId;	// material id
} Sphere;


// (axis-aligned) (bounding) box object
typedef struct Box
{
	__declspec(align(16)) Point p1, p2;				// two points to define opposite corners of the box
	unsigned int materialId;	// material id
} Box;