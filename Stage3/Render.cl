
#include "Stage3/Scene.cl"
#include "Stage3/Constants.cl"
#include "Stage3/Intersection.cl"
#include "Stage3/Lighting.cl"

// output a bunch of info about the contents of the scene
void outputInfo(const Scene* scene)
{
	__global Box* boxes = scene->boxContainer;
	__global Sphere* spheres = scene->sphereContainer;
	__global Light* lights = scene->lightContainer;
	__global Material* materials = scene->materialContainer;

	printf("\n---- GPU --------\n");
	printf("sizeof(Point):    %ld\n", sizeof(Point));
	printf("sizeof(Vector):   %ld\n", sizeof(Vector));
	printf("sizeof(Colour):   %ld\n", sizeof(Colour));
	printf("sizeof(Ray):      %ld\n", sizeof(Ray));
	printf("sizeof(Light):    %ld\n", sizeof(Light));
	printf("sizeof(Sphere):   %ld\n", sizeof(Sphere));
	printf("sizeof(Box):      %ld\n", sizeof(Box));
	printf("sizeof(Material): %ld\n", sizeof(Material));
	printf("sizeof(Scene):    %ld\n", sizeof(Scene));

	printf("\n--- Scene:\n");;
	printf("pos: %.1f %.1f %.1f\n", scene->cameraPosition.x, scene->cameraPosition.y, scene->cameraPosition.z);
	printf("rot: %.1f\n", scene->cameraRotation);
	printf("fov: %.1f\n", scene->cameraFieldOfView);
	printf("exp: %.1f\n", scene->exposure);
	printf("sky: %d\n", scene->skyboxMaterialId);

	printf("\n--- Spheres (%d):\n", scene->numSpheres);;
	for (unsigned int i = 0; i < scene->numSpheres; ++i)
	{
		if (scene->numSpheres > 10 && i >= 3 && i < scene->numSpheres - 3)
		{
			printf(" ... \n");
			i = scene->numSpheres - 3;
			continue;
		}

		printf("Sphere %d: %.1f %.1f %.1f, %.1f -- %d\n", i, spheres[i].pos.x, spheres[i].pos.y, spheres[i].pos.z, spheres[i].size, spheres[i].materialId);
	}

	printf("\n--- Boxes (%d):\n", scene->numBoxes);
	for (unsigned int i = 0; i < scene->numBoxes; ++i)
	{
		if (scene->numBoxes > 10 && i >= 3 && i < scene->numBoxes - 3)
		{
			printf(" ... \n");
			i = scene->numBoxes - 3;
			continue;
		}

		printf("Box %d: %.1f %.1f %.1f, %.1f %.1f %.1f -- %d\n", i,
			boxes[i].p1.x, boxes[i].p1.y, boxes[i].p1.z,
			boxes[i].p2.x, boxes[i].p2.y, boxes[i].p2.z,
			boxes[i].materialId
		);
	}

	printf("\n--- Lights (%d):\n", scene->numLights);
	for (unsigned int i = 0; i < scene->numLights; ++i)
	{
		if (scene->numLights > 10 && i >= 3 && i < scene->numLights - 3)
		{
			printf(" ... \n");
			i = scene->numLights - 3;
			continue;
		}

		printf("Light %d: %.1f %.1f %.1f -- %.1f %.1f %.1f\n", i,
			lights[i].pos.x, lights[i].pos.y, lights[i].pos.z,
			lights[i].intensity.x, lights[i].intensity.y, lights[i].intensity.z);
	}

	printf("\n--- Materials (%d):\n", scene->numMaterials);
	for (unsigned int i = 0; i < scene->numMaterials; ++i)
	{
		if (scene->numMaterials > 10 && i >= 3 && i < scene->numMaterials - 3)
		{
			printf(" ... \n");
			i = scene->numMaterials - 3;
			continue;
		}

		printf("Material %d: %d %.1f %.1f %.1f ... %.1f %.1f %.1f\n", i,
			materials[i].type,
			materials[i].diffuse.x, materials[i].diffuse.y, materials[i].diffuse.z,
			materials[i].reflection,
			materials[i].refraction,
			materials[i].density);
	}
}

// follow a single ray until it's final destination (or maximum number of steps reached)
Colour traceRay(const Scene* scene, Ray viewRay)
{
	Colour output = { 0.0f, 0.0f, 0.0f }; 								// colour value to be output
	float currentRefractiveIndex = DEFAULT_REFRACTIVE_INDEX;		// current refractive index
	float coef = 1.0f;												// amount of ray left to transmit
	Intersection intersect;											// properties of current intersection

																	// loop until reached maximum ray cast limit (unless loop is broken out of)
	for (int level = 0; level < MAX_RAYS_CAST; ++level)
	{
		// check for intersections between the view ray and any of the objects in the scene
		// exit the loop if no intersection found
		if (!objectIntersection(scene, &viewRay, &intersect)) break;

		// calculate response to collision: ie. get normal at point of collision and material of object
		calculateIntersectionResponse(scene, &viewRay, &intersect);


		// apply the diffuse and specular lighting 
		if (!intersect.insideObject) output += coef * applyLighting(scene, &viewRay, &intersect);
		return output;

	}

	return output;
}

// Data struct of data passed thought the kernel
typedef struct kernelPass {
	unsigned int aaLevel;					// aaLevel
	int testMode;							// testMode
	float3 cameraPositions;					// camera location
	float cameraRotation;					// direction camera points
	float cameraFieldOfView;				// field of view for the camera

	float exposure;							// image exposure

	unsigned int skyboxMaterialId;			// skybox materialID
	unsigned int numMaterials;				// num materials
	unsigned int numLights;					// numLight
	unsigned int numSpheres;				// numSphere
	unsigned int numBoxes;					// numBoxes
}kernelPass;

__kernel void render(struct kernelPass data, __global struct Material* materialContainer, __global struct Light* lightContainer, __global struct Sphere* sphereContainer, __global struct Box* boxContainer, __global unsigned int* out)
{
	// get the j (x) and i (y) values from the global ID
	unsigned int i = get_global_id(0);
	unsigned int j = get_global_id(1);

	// Create new scene struct to link data too
	Scene clScene;

	//set aaLevel and testMode 
	unsigned int aaLevel = data.aaLevel;
	int testMode = data.testMode;

	// set cl scene camera positions to the scene camera positions passed through to the kernel.
	clScene.cameraPosition.x = data.cameraPositions.x;
	clScene.cameraPosition.y = data.cameraPositions.y;
	clScene.cameraPosition.z = data.cameraPositions.z;

	// set other struct data and containers through to the cl scene struct
	clScene.cameraRotation = data.cameraRotation;
	clScene.cameraFieldOfView = data.cameraFieldOfView;
	clScene.exposure = data.exposure;
	clScene.skyboxMaterialId = data.skyboxMaterialId;
	clScene.numMaterials = data.numMaterials;
	clScene.numLights = data.numLights;
	clScene.numSpheres = data.numSpheres;
	clScene.numBoxes = data.numBoxes;

	clScene.materialContainer = materialContainer;
	clScene.lightContainer = lightContainer;
	clScene.sphereContainer = sphereContainer;
	clScene.boxContainer = boxContainer;


	// get the total height and width of the scene
	unsigned int width = get_global_size(0);
	unsigned int height = get_global_size(1);


	// angle between each successive ray cast (per pixel, anti-aliasing uses a fraction of this)
	float dirStepSize = 1.0f / (0.5f * width / tan(PIOVER180 * 0.5f * clScene.cameraFieldOfView));


	// count of samples rendered
	unsigned int samplesRendered = 0;

	// loop through all the pixels

	int y = j - height / 2;
	int x = i - width / 2;


	Colour output = { 0.0f, 0.0f, 0.0f };

	// calculate multiple samples for each pixel
	float sampleStep = 1.0f / aaLevel, sampleRatio = 1.0f / (aaLevel * aaLevel);

	// loop through all sub-locations within the pixel
	for (float fragmentx = (float)x; fragmentx < x + 1.0f; fragmentx += sampleStep)
	{
		for (float fragmenty = (float)y; fragmenty < y + 1.0f; fragmenty += sampleStep)
		{
			// direction of default forward facing ray
			Vector dir = { fragmentx * dirStepSize, (fragmenty * dirStepSize), 1.0f };

			// rotated direction of ray
			Vector rotatedDir = {
				dir.x * cos(clScene.cameraRotation) - dir.z * sin(clScene.cameraRotation),
				dir.y,
				dir.x * sin(clScene.cameraRotation) + dir.z * cos(clScene.cameraRotation) };

			// view ray starting from camera position and heading in rotated (normalised) direction
			Ray viewRay = { clScene.cameraPosition, normalize(rotatedDir) };

			// follow ray and add proportional of the result to the final pixel colour
			output += sampleRatio * traceRay(&clScene, viewRay);

			// count this sample
			samplesRendered++;
		}
	}

	if (!testMode)
	{
		// set the out to be either white or black depending on if there is an intersect
		unsigned int returnColour = ((unsigned char)(255 * output.x) << 0) +
			((unsigned char)(255 * output.y) << 8) +
			((unsigned char)(255 * output.z) << 16);


		// store colour (calculated from x,y coordinates) in image buffer 
		out[(j * width + i)] = returnColour;

	}
	else
	{
		// store saturated final colour value in image buffer
		//*out++ = output.convertToPixel(clScene->exposure);
	}


}

