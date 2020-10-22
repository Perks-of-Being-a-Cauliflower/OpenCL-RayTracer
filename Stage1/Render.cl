
#include "Stage1/Scene.cl"

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
	printf("pos: %.1f %.1f %.1f\n", scene->cameraPosition.xyz.x, scene->cameraPosition.xyz.y, scene->cameraPosition.xyz.z);
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

		printf("Sphere %d: %.1f %.1f %.1f, %.1f -- %d\n", i, spheres[i].pos.xyz.x, spheres[i].pos.xyz.y, spheres[i].pos.xyz.z, spheres[i].size, spheres[i].materialId);
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
			boxes[i].p1.xyz.x, boxes[i].p1.xyz.y, boxes[i].p1.xyz.z,
			boxes[i].p2.xyz.x, boxes[i].p2.xyz.y, boxes[i].p2.xyz.z,
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
			lights[i].pos.xyz.x, lights[i].pos.xyz.y, lights[i].pos.xyz.z,
			lights[i].intensity.RGB.x, lights[i].intensity.RGB.y, lights[i].intensity.RGB.z);
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
			materials[i].diffuse.RGB.x, materials[i].diffuse.RGB.y, materials[i].diffuse.RGB.z,
			materials[i].reflection,
			materials[i].refraction,
			materials[i].density);
	}
}

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
	// get x (i) and y (j) values
	unsigned int i = get_global_id(0);
	unsigned int j = get_global_id(1);

	// create scene object
	Scene clScene;

	// set scene variables in kernel data struct to be values passed through from CPU
	clScene.cameraPosition.xyz.x = data.cameraPositions.x;
	clScene.cameraPosition.xyz.y = data.cameraPositions.y;
	clScene.cameraPosition.xyz.z = data.cameraPositions.z;

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

	// print only at first position
	if (i == 0 && j == 0)
	{
		printf("---------------\nGPU-side data\n");

		outputInfo(&clScene);

		printf("---------------\nGPU-side data end\n");
	}
	// get the total width and height
	unsigned int width = get_global_size(0);
	unsigned int height = get_global_size(1);

	// set colour
	unsigned int output = (((j % 256) << 0) + 0.0f + ((i % 256) << 16));

	// add colour to out array
	out[(j * width + i)] = output;

}

