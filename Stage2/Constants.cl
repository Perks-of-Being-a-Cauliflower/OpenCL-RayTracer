#ifndef __CONSTANTS_CL
#define __CONSTANTS_CL

// maximum size of image
__constant int MAX_WIDTH = 2048, MAX_HEIGHT = 2048;

// math constants
__constant float PI = 3.14159265358979323846f;
__constant float PIOVER180 = 0.017453292519943295769236907684886f;

// a small value (used to make sure we don't get stuck detecting collision of the same object over and over) 
__constant float EPSILON = 0.01f;

// maximum ray distance
__constant float MAX_RAY_DISTANCE = 2000000.0f;  //** maybe should be maxfloat?!

// the maximum number of rays to cast before giving up on finding final ray destination
__constant int MAX_RAYS_CAST = 10;

// default refractive index (of air effectively)
__constant float DEFAULT_REFRACTIVE_INDEX = 1.0f;

#endif //__CONSTANTS_H