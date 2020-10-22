/*  The following code is a VERY heavily modified from code originally sourced from:
Ray tracing tutorial of http://www.codermind.com/articles/Raytracer-in-C++-Introduction-What-is-ray-tracing.html
It is free to use for educational purpose and cannot be redistributed outside of the tutorial pages. */

#define TARGET_WINDOWS

#pragma warning(disable: 4996)
#include <stdio.h>
#include "Timer.h"
#include "Primitives.h"
#include "Scene.h"
#include "Lighting.h"
#include "Intersection.h"
#include "ImageIO.h"
#include "LoadCL.h"

unsigned int buffer[MAX_WIDTH * MAX_HEIGHT];
unsigned int* out = buffer;
unsigned int buffer2[MAX_WIDTH * MAX_HEIGHT];
unsigned int* out2 = buffer2;
//unsigned int outCopy[MAX_WIDTH * MAX_HEIGHT];

typedef struct kernelPass {
	cl_uint aaLevel;												// aaLevel
	cl_int testMode;												// testMode
	cl_int i;														// totaldivision of workload
	cl_int totWidth;
	cl_int totHeight;
	cl_uint curBlock;
	cl_uint numBW;
	cl_uint numBH;
	__declspec(align(16)) cl_float3 cameraPosition;					// camera location
	cl_float cameraRotation;										// direction camera points
	cl_float cameraFieldOfView;										// field of view for the camera

	cl_float exposure;												// image exposure

	cl_uint skyboxMaterialId;										// Skybox material ID
	cl_uint numMaterials;											// numMaterials
	cl_uint numLights;												// numLights
	cl_uint numSpheres;												// numSpheres
	cl_uint numBoxes;												// numBoxes
} kernelPass;

// reflect the ray from an object
Ray calculateReflection(const Ray* viewRay, const Intersection* intersect)
{
	// reflect the viewRay around the object's normal
	Ray newRay = { intersect->pos, viewRay->dir - (intersect->normal * intersect->viewProjection * 2.0f) };

	return newRay;
}


// refract the ray through an object
Ray calculateRefraction(const Ray* viewRay, const Intersection* intersect, float* currentRefractiveIndex)
{
	// change refractive index depending on whether we are in an object or not
	float oldRefractiveIndex = *currentRefractiveIndex;
	*currentRefractiveIndex = intersect->insideObject ? DEFAULT_REFRACTIVE_INDEX : intersect->material->density;

	// calculate refractive ratio from old index and current index
	float refractiveRatio = oldRefractiveIndex / *currentRefractiveIndex;

	// Here we take into account that the light movement is symmetrical from the observer to the source or from the source to the oberver.
	// We then do the computation of the coefficient by taking into account the ray coming from the viewing point.
	float fCosThetaT;
	float fCosThetaI = fabsf(intersect->viewProjection);

	// glass-like material, we're computing the fresnel coefficient.
	if (fCosThetaI >= 1.0f)
	{
		// In this case the ray is coming parallel to the normal to the surface
		fCosThetaT = 1.0f;
	}
	else
	{
		float fSinThetaT = refractiveRatio * sqrtf(1 - fCosThetaI * fCosThetaI);

		// Beyond the angle (1.0f) all surfaces are purely reflective
		fCosThetaT = (fSinThetaT * fSinThetaT >= 1.0f) ? 0.0f : sqrtf(1 - fSinThetaT * fSinThetaT);
	}

	// Here we compute the transmitted ray with the formula of Snell-Descartes
	Ray newRay = { intersect->pos, (viewRay->dir + intersect->normal * fCosThetaI) * refractiveRatio - (intersect->normal * fCosThetaT) };

	return newRay;
}


// follow a single ray until it's final destination (or maximum number of steps reached)
Colour traceRay(const Scene* scene, Ray viewRay)
{
	Colour output(0.0f, 0.0f, 0.0f); 								// colour value to be output
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

		// if object has reflection or refraction component, adjust the view ray and coefficent of calculation and continue looping
		if (intersect.material->reflection)
		{
			viewRay = calculateReflection(&viewRay, &intersect);
			coef *= intersect.material->reflection;
		}
		else if (intersect.material->refraction)
		{
			viewRay = calculateRefraction(&viewRay, &intersect, &currentRefractiveIndex);
			coef *= intersect.material->refraction;
		}
		else
		{
			// if no reflection or refraction, then finish looping (cast no more rays)
			return output;
		}
	}

	// if the calculation coefficient is non-zero, read from the environment map
	if (coef > 0.0f)
	{
		Material& currentMaterial = scene->materialContainer[scene->skyboxMaterialId];

		output += coef * currentMaterial.diffuse;
	}

	return output;
}

// render scene at given width and height and anti-aliasing level
int render(Scene* scene, const int width, const int height, const int aaLevel, bool testMode)
{
	// angle between each successive ray cast (per pixel, anti-aliasing uses a fraction of this)
	const float dirStepSize = 1.0f / (0.5f * width / tanf(PIOVER180 * 0.5f * scene->cameraFieldOfView));

	// pointer to output buffer
	//unsigned int* out = buffer;

	// count of samples rendered
	unsigned int samplesRendered = 0;

	// loop through all the pixels
	for (int y = -height / 2; y < height / 2; ++y)
	{
		for (int x = -width / 2; x < width / 2; ++x)
		{
			Colour output(0.0f, 0.0f, 0.0f);

			// calculate multiple samples for each pixel
			const float sampleStep = 1.0f / aaLevel, sampleRatio = 1.0f / (aaLevel * aaLevel);

			// loop through all sub-locations within the pixel
			for (float fragmentx = float(x); fragmentx < x + 1.0f; fragmentx += sampleStep)
			{
				for (float fragmenty = float(y); fragmenty < y + 1.0f; fragmenty += sampleStep)
				{
					// direction of default forward facing ray
					Vector dir = { fragmentx * dirStepSize, fragmenty * dirStepSize, 1.0f };

					// rotated direction of ray
					Vector rotatedDir = {
						dir.x * cosf(scene->cameraRotation) - dir.z * sinf(scene->cameraRotation),
						dir.y,
						dir.x * sinf(scene->cameraRotation) + dir.z * cosf(scene->cameraRotation) };

					// view ray starting from camera position and heading in rotated (normalised) direction
					Ray viewRay = { scene->cameraPosition, normalise(rotatedDir) };

					// follow ray and add proportional of the result to the final pixel colour
					output += sampleRatio * traceRay(scene, viewRay);

					// count this sample
					samplesRendered++;
				}
			}

			if (!testMode)
			{
				// store saturated final colour value in image buffer
				*out++ = output.convertToPixel(scene->exposure);
			}
			else
			{
				// store colour (calculated from x,y coordinates) in image buffer 
				*out++ = Colour((x + width / 2) % 256 / 256.0f, 0, (y + height / 2) % 256 / 256.0f).convertToPixel();
			}
		}
	}

	return samplesRendered;
}

// output a bunch of info about the contents of the scene
void outputInfo(const Scene* scene)
{
	Box* boxes = scene->boxContainer;
	Sphere* spheres = scene->sphereContainer;
	Light* lights = scene->lightContainer;
	Material* materials = scene->materialContainer;

	printf("\n---- CPU --------\n");
	printf("sizeof(Point):    %zd\n", sizeof(Point));
	printf("sizeof(Vector):   %zd\n", sizeof(Vector));
	printf("sizeof(Colour):   %zd\n", sizeof(Colour));
	printf("sizeof(Ray):      %zd\n", sizeof(Ray));
	printf("sizeof(Light):    %zd\n", sizeof(Light));
	printf("sizeof(Sphere):   %zd\n", sizeof(Sphere));
	printf("sizeof(Box):      %zd\n", sizeof(Box));
	printf("sizeof(Material): %zd\n", sizeof(Material));
	printf("sizeof(Scene):    %zd\n", sizeof(Scene));

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
			lights[i].intensity.red, lights[i].intensity.green, lights[i].intensity.blue);
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
			materials[i].diffuse.red, materials[i].diffuse.green, materials[i].diffuse.blue,
			materials[i].reflection,
			materials[i].refraction,
			materials[i].density);
	}
}


// read command line arguments, render, and write out BMP file
int main(int argc, char* argv[])
{
	int width = 2048;
	int height = 2048;
	int samples = 1;
	unsigned int blockSize = 512;
	int workDiv = 1;
	unsigned int curBlock = 0;
	unsigned int numBW;
	unsigned int numBH;

	// rendering options
	int times = 1;
	bool testMode = false;

	// default input / output filenames
	const char* inputFilename = "Scenes/cornell.txt";

	char outputFilenameBuffer[1000];
	char* outputFilename = outputFilenameBuffer;

	// do stuff with command line args
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-size") == 0)
		{
			width = atoi(argv[++i]);
			height = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-samples") == 0)
		{
			samples = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-input") == 0)
		{
			inputFilename = argv[++i];
		}
		else if (strcmp(argv[i], "-output") == 0)
		{
			outputFilename = argv[++i];
		}
		else if (strcmp(argv[i], "-runs") == 0)
		{
			times = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-blockSize") == 0)
		{
			blockSize = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-testMode") == 0)
		{
			testMode = true;
		}
		else
		{
			fprintf(stderr, "unknown argument: %s\n", argv[i]);
		}
	}

	// nasty (and fragile) kludge to make an ok-ish default output filename (can be overriden with "-output" command line option)
	sprintf(outputFilenameBuffer, "Outputs/%s_%dx%dx%d_%s.bmp", (strrchr(inputFilename, '/') + 1), width, height, samples, (strrchr(argv[0], '\\') + 1));

	// read scene file
	Scene scene;
	if (!init(inputFilename, scene))
	{
		fprintf(stderr, "Failure when reading the Scene file.\n");
		return -1;
	}


	// display info about the current scene
	//outputInfo(&scene);

	Timer timer;																						// create timer

	// OpenCL setup code goes here

	// first time and total time taken to render all runs (used to calculate average)
	int firstTime = 0;
	int totalTime = 0;
	int samplesRendered = 0;
	for (int i = 0; i < times; i++)
	{
		if (i > 0) timer.start();

		// OpenCL execution code replaces this call to render()

		// cl variables
		cl_int err;
		cl_platform_id platform;
		cl_device_id device;
		cl_context context;
		cl_command_queue queue;
		cl_program program;
		cl_kernel kernel;
		cl_mem clBufferOut;
		cl_mem clbufferInMaterial;
		cl_mem clbufferInLight;
		cl_mem clbufferInSphere;
		cl_mem clbufferInBox;
		cl_mem clbufferInOut;


		unsigned int numBlocksHigh = ((height - 1) / (blockSize + 1));
		unsigned int numBlocksWide = ((width - 1) / (blockSize + 1));

		numBlocksWide++;
		numBlocksHigh++;

		if ((width % blockSize) != 0) {
			//numBlocksWide++;
		}
		if ((height % blockSize) != 0) {
			//numBlocksHigh++;
		}

		unsigned int totalBlocks = (numBlocksHigh) * (numBlocksWide);

		unsigned int jobSizeX = blockSize;
		unsigned int jobSizeY = blockSize;

		for (int j = 0; j < totalBlocks; ++j) {

			// rendering a width x height image

			size_t workOffset[] = { 0, 0 };

			//_putenv_s("CUDA_CACHE_DISABLE", "1");

			// get the platform
			err = clGetPlatformIDs(1, &platform, NULL);
			if (err != CL_SUCCESS)
			{
				printf("\nError calling clGetPlatformIDs. Error code: %d\n", err);
				exit(1);
			}

			// get the device
			err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
			if (err != CL_SUCCESS) {
				printf("Couldn't find any devices\n");
				exit(1);
			}

			//create cl context
			context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
			if (err != CL_SUCCESS) {
				printf("Couldn't create a context\n");
				exit(1);
			}

			// create a commande queue
			queue = clCreateCommandQueue(context, device, 0, &err);
			if (err != CL_SUCCESS) {
				printf("Couldn't create the command queue\n");
				exit(1);
			}

			// use load source to load the main cl file
			program = clLoadSource(context, "Stage5/Render.cl", &err);
			if (err != CL_SUCCESS) {
				printf("Couldn't load/create the program\n");
				exit(1);
			}

			// build the program and check for any errors
			err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
			if (err != CL_SUCCESS) {
				char* program_log;
				size_t log_size;

				clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
				program_log = (char*)malloc(log_size + 1);
				program_log[log_size] = '\0';
				clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size + 1, program_log, NULL);
				printf("%s\n", program_log);
				free(program_log);
				exit(1);
			}

			// create the kernel and run the "render" function
			kernel = clCreateKernel(program, "render", &err);
			if (err != CL_SUCCESS) {
				printf("Couldn't create the kernel\n");
				exit(1);
			}

			// set the out buffer
			clBufferOut = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(*out) * width * height, out, &err);
			if (err != CL_SUCCESS)
			{
				printf("\nError calling clCreateBufferIn. Error code: %d\n", err);
				exit(1);
			}



			// data to pass through the kernel
			struct kernelPass data = { samples,
				int(testMode),
				blockSize,
				width,
				height,
				j,
				numBlocksWide,
				numBlocksHigh,
				{ scene.cameraPosition.x, scene.cameraPosition.y, scene.cameraPosition.z },
				scene.cameraRotation,
				scene.cameraFieldOfView,
				scene.exposure,
				scene.skyboxMaterialId,
				scene.numMaterials,
				scene.numLights,
				scene.numSpheres,
				scene.numBoxes };

			// create buffer for material container
			clbufferInMaterial = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(*scene.materialContainer) * scene.numMaterials, scene.materialContainer, &err);
			if (err != CL_SUCCESS)
			{
				printf("\nError calling clCreateBufferIn1. Error code: %d\n", err);
				exit(1);
			}

			// create buffer for light container
			clbufferInLight = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(*scene.lightContainer) * scene.numLights, scene.lightContainer, &err);
			if (err != CL_SUCCESS)
			{
				printf("\nError calling clCreateBufferIn1. Error code: %d\n", err);
				exit(1);
			}

			// make sure that the sphere buffer does not have size zero
			int temp = scene.numSpheres;
			if (scene.numSpheres == 0) {
				temp = 1;
			}

			// set sphere buffer
			clbufferInSphere = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(Sphere) * temp, scene.sphereContainer, &err);
			if (err != CL_SUCCESS)
			{
				printf("\nError calling clCreateBufferIn1. Error code: %d\n", err);
				exit(1);
			}

			// make sure that the box buffer does not have size zero
			temp = scene.numBoxes;
			if (scene.numBoxes == 0) {
				temp = 1;
			}

			// set box buffer
			clbufferInBox = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(Box) * temp, scene.boxContainer, &err);
			if (err != CL_SUCCESS)
			{
				printf("\nError calling clCreateBufferIn1. Error code: %d\n", err);
				exit(1);
			}

			// set the first argument to be the kernelPass data struct
			err = clSetKernelArg(kernel, 0, sizeof(kernelPass), &data);
			if (err != CL_SUCCESS)
			{
				printf("\nError calling clSetKernelArg1. Error code: %d\n", err);
				exit(1);
			}

			// set the second argument to be the material buffer
			err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &clbufferInMaterial);
			if (err != CL_SUCCESS)
			{
				printf("\nError calling clSetKernelArg2. Error code: %d\n", err);
				exit(1);
			}

			// set the third argument to be the light buffer
			err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &clbufferInLight);
			if (err != CL_SUCCESS)
			{
				printf("\nError calling clSetKernelArg2. Error code: %d\n", err);
				exit(1);
			}

			// set the fourth argument to be the sphere buffer
			err = clSetKernelArg(kernel, 3, sizeof(cl_mem), &clbufferInSphere);
			if (err != CL_SUCCESS)
			{
				printf("\nError calling clSetKernelArg2. Error code: %d\n", err);
				exit(1);
			}

			// set the fith argument to be the box buffer
			err = clSetKernelArg(kernel, 4, sizeof(cl_mem), &clbufferInBox);
			if (err != CL_SUCCESS)
			{
				printf("\nError calling clSetKernelArg2. Error code: %d\n", err);
				exit(1);
			}


			// set the sixth argument to be the out buffer
			err = clSetKernelArg(kernel, 5, sizeof(clBufferOut), &clBufferOut);
			if (err != CL_SUCCESS)
			{
				printf("\nError calling clSetKernelArg2. Error code: %d\n", err);
				exit(1);
			}

			// get default x and y jobsize
			jobSizeX = blockSize;
			jobSizeY = blockSize;

			// check if last block to do on the x axis
			if ((j != 0) && ((j % (numBlocksWide)) == numBlocksWide - 1) && (width % blockSize != 0)) {
				jobSizeX = width % blockSize;
			}


			// check if last block to do on the y axis
			if ((j != 0) && (totalBlocks - j <= numBlocksWide) && height % blockSize != 0) {
				jobSizeY = height % blockSize;
			}

			// set the worksize
			size_t workSize[] = { jobSizeX, jobSizeY };


			// pass the worksize and workoffset
			err = clEnqueueNDRangeKernel(queue, kernel, 2, workOffset, workSize, NULL, 0, NULL, NULL);
			if (err != CL_SUCCESS) {
				printf("Couldn't enqueue the kernel execution command\n");
				exit(1);
			}

			// read out the values to *out
			clEnqueueReadBuffer(queue, clBufferOut, CL_TRUE, 0, sizeof(*out) * width * height, out, 0, NULL, NULL);
			if (err != CL_SUCCESS) {
				printf("Couldn't enqueue the read buffer command\n");
				exit(1);
			}

			// pass the out into final array since the out values do persist between runs
			for (int k = 0; k < (width * height); k++) {

				if (out[k] != 0) {

					out2[k] = out[k];

				}

			}

			// clean up

			clReleaseMemObject(clBufferOut);
			clReleaseMemObject(clbufferInMaterial);
			clReleaseMemObject(clbufferInLight);
			clReleaseMemObject(clbufferInSphere);
			clReleaseMemObject(clbufferInBox);
			clReleaseCommandQueue(queue);
			clReleaseProgram(program);
			clReleaseKernel(kernel);
			clReleaseContext(context);

		}


		timer.end();																					// record end time
		if (i > 0)
		{
			totalTime += timer.getMilliseconds();														// record total time taken
		}
		else
		{
			firstTime = timer.getMilliseconds();														// record first time taken
		}
	}

	// output timing information (first run, times run and average)
	if (times > 1)
	{
		printf("first run time: %dms, subsequent average time taken (%d run(s)): %.1fms\n", firstTime, times - 1, totalTime / (float)(times - 1));
	}
	else
	{
		printf("first run time: %dms, subsequent average time taken (%d run(s)): N/A\n", firstTime, times - 1);
	}
	// output BMP file
	write_bmp(outputFilename, out2, width, height, width);
}
