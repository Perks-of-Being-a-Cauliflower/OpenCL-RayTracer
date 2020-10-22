

typedef struct Point
{
	float3 xyz;
} Point;


// vectors consist of three coordinates and represent a direction from (an implied) origin
typedef struct Vector
{
	float3 xyz;

} Vector;


// rays are cast from a starting point in a direction
typedef struct Ray
{
	Point start;
	Vector dir;
} Ray;


