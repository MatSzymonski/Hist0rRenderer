//OpenGL project that draws a tetrahedron on black background and rotates it using matrix muliplied by position vector of tetrahedron in geometry shader, this matrix is being updated in main loop
//Additionally tetrahedron is colorful due to interpolation based on vertex average of vertex positions
//Camera movement and rotation is implemented

//Standard libraries
#define STB_IMAGE_IMPLEMENTATION

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <vector>

//OpenGL must-have libraries
#include <GL\glew.h>
#include <GLFW\glfw3.h>

//OpenGL math libraries
#include <glm\mat4x4.hpp>
#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\type_ptr.hpp>

//Project headers
#include "CommonValues.h"
#include "Lights/PointLight.h"
#include "Lights/DirectionalLight.h"
#include "Lights/SpotLight.h"

#include "Mesh.h"
#include "Shader.h"
#include "Window.h"
#include "Camera.h"
#include "Texture.h"
#include "Material.h"
#include "Model.h"
#include "Terrain.h"

using namespace std;

const float toRadians = 3.14159265f / 180.0f;

GLuint uniformProjection = 0, uniformModel = 0, uniformView = 0; //Initialize projection matrices
GLuint uniformAmbientIntensity = 0, uniformAmbientColour = 0; //Initialize ambient light matrices
GLuint uniformDiffuseDirection = 0, uniformDiffuseIntensity = 0; //Initialize diffuse light matrices
GLuint uniformSpecularIntensity = 0, uniformShininess = 0; //Initialize specular light matrices
GLuint uniformCameraPosition = 0; //Initialize camera position matrix

Window mainWindow;

vector<Mesh*> meshList; //List of Mesh pointers

vector<Shader> shaderList; //List of Shader 
Shader directionalShadowShader;

Camera camera; //Camera class

Model testModel_Human;
Model testModel_Cube;

Texture brickTexture;
Texture dirtTexture;

Material shinyMaterial;
Material dullMaterial;

DirectionalLight directionalLight;
PointLight pointLights[MAX_POINT_LIGHTS];
SpotLight spotLights[MAX_SPOT_LIGHTS];
unsigned int pointLightCount = 0;
unsigned int spotLightCount = 0;

//To maintain same speed on every CPU
GLfloat deltaTime = 0.0f;
GLfloat lastTime = 0.0f;

//---------------------Moving Mesh--------------------------------
float movementCurrentTranslation = 0;
float movementDirection = true;

float currentAngle = 0.0f;
float angleIncrement = 1.0f;

bool sizeDirection = true;
float currentSize = 0.4f;
float maxSize = 0.8f;
float minSize = 0.1f;
//---------------------------------------------------------------

//Vertex shader (vertexColor is interpolated value based on position of other vertices)
static const char* vShader = "Shaders/vertex.shader";

//Fragment shader
static const char* fShader = "Shaders/fragment.shader";

//Function that calculates average normal vectors for each index
void calcAverageNormals(unsigned int * indices, unsigned int indiceCount, GLfloat * vertices, unsigned int verticeCount, unsigned int vLength, unsigned int normalOffset)
{
	for (size_t i = 0; i < indiceCount; i += 3)
	{
		unsigned int in0 = indices[i] * vLength;
		unsigned int in1 = indices[i + 1] * vLength;
		unsigned int in2 = indices[i + 2] * vLength;
		glm::vec3 v1(vertices[in1] - vertices[in0], vertices[in1 + 1] - vertices[in0 + 1], vertices[in1 + 2] - vertices[in0 + 2]);
		glm::vec3 v2(vertices[in2] - vertices[in0], vertices[in2 + 1] - vertices[in0 + 1], vertices[in2 + 2] - vertices[in0 + 2]);
		glm::vec3 normal = glm::cross(v1, v2);
		normal = glm::normalize(normal);

		in0 += normalOffset; in1 += normalOffset; in2 += normalOffset;
		vertices[in0] += normal.x; vertices[in0 + 1] += normal.y; vertices[in0 + 2] += normal.z;
		vertices[in1] += normal.x; vertices[in1 + 1] += normal.y; vertices[in1 + 2] += normal.z;
		vertices[in2] += normal.x; vertices[in2 + 1] += normal.y; vertices[in2 + 2] += normal.z;
	}

	for (size_t i = 0; i < verticeCount / vLength; i++)
	{
		unsigned int nOffset = i * vLength + normalOffset;
		glm::vec3 vec(vertices[nOffset], vertices[nOffset + 1], vertices[nOffset + 2]);
		vec = glm::normalize(vec);
		vertices[nOffset] = vec.x; vertices[nOffset + 1] = vec.y; vertices[nOffset + 2] = vec.z;
	}
}

void CreateObject()
{
	//Index draws (order of points), so we have 12 indices and only 4 vertices because program is creating 3 vertices in each location (with same parameters)
	unsigned int indices[] = {
		0, 3, 1, //First triangle with 3 points
		1, 3, 2, //Second triangle with 3 points
		2, 3, 0, //Third triangle with 3 points
		0, 1, 2 //Fourth triangle with 3 points
	};

	//Four vertices: (-1-1,0), (0,-1,1), (1,-1,0), (0,1,0) + (UV.x, UV.y) to each
	GLfloat vertices[] = {
		// x      y      z        u     v       normal vec (x,y,z) 
		-1.0f, -1.0f,  0.0f,     0.0f, 0.0f,    0.0f, 0.0f, 0.0f, //Point 0 
		0.0f,  -1.0f,  1.0f,     0.5f, 0.0f,    0.0f, 0.0f, 0.0f, //Point 1
		1.0f,  -1.0f,  0.0f,     1.0f, 0.0f,    0.0f, 0.0f, 0.0f, //Point 2
		0.0f,   1.0f,  0.0f,     0.5f, 1.0f,    0.0f, 0.0f, 0.0f  //Point 3
	};

	calcAverageNormals(indices, 12, vertices, 32, 8, 5); //Calculate average normal vectors for each vertex (Phong shading)

	Mesh *obj1 = new Mesh(); //From mesh.h
	obj1->CreateMesh(vertices, indices, 32, 12);
	meshList.push_back(obj1); //Add to the end of the list




	//Create floor
	unsigned int floorIndices[] = {
		0, 2, 1,
		1, 2, 3
	};

	GLfloat floorVertices[] = {
		-10.0f, 0.0f, -10.0f,	0.0f, 0.0f,		0.0f, -1.0f, 0.0f,
		10.0f, 0.0f, -10.0f,	10.0f, 0.0f,	0.0f, -1.0f, 0.0f,
		-10.0f, 0.0f, 10.0f,	0.0f, 10.0f,	0.0f, -1.0f, 0.0f,
		10.0f, 0.0f, 10.0f,		10.0f, 10.0f,	0.0f, -1.0f, 0.0f
	};

	Mesh *obj2 = new Mesh();
	obj2->CreateMesh(floorVertices, floorIndices, 32, 6);
	meshList.push_back(obj2);

	//Terrain
	Terrain *terrain = new Terrain("Textures/height4.png");
	meshList.push_back(terrain->CreateTerrain());
}

void CreateShaders()
{
	Shader *shader1 = new Shader();
	shader1->CreateFromFiles(vShader, fShader);
	shaderList.push_back(*shader1);

	directionalShadowShader.CreateFromFiles("Shaders/directional_shadow_map_vertex.shader", "Shaders/directional_shadow_fragment.shader");
}

void RenderScene()
{
	//SPECIFIC MODEL OPERATIONS
	glm::mat4 model;

	//Terrain
	model = glm::mat4();
	model = glm::scale(model, glm::vec3(4.0f, 4.0f, 4.0f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model)); //Set uniform variable value in shader (uniform variable in shader, count, transpose?, pointer to matrix)	
	brickTexture.UseTexture();
	shinyMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	meshList[2]->RenderMesh();


	//Tetrahedron
	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(0.0f, 4.0f, -2.5f)); //Transforming identity matrix into translation matrix
	model = glm::rotate(model, currentAngle * toRadians, glm::vec3(0.0f, 1.0f, 0.0f)); //Tranforming matrix of translation into matrix of translation and rotation
	model = glm::scale(model, glm::vec3(currentSize, currentSize, 1.0f)); //Tranforming matrix of translation and rotation into matrix of translation, rotation and scale 
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model)); //Set uniform variable value in shader (uniform variable in shader, count, transpose?, pointer to matrix)	
	brickTexture.UseTexture();
	shinyMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	meshList[0]->RenderMesh();

	//Floor
	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, -0.0f)); //Transforming identity matrix into translation matrix													
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model)); //Set uniform variable value in shader (uniform variable in shader, count, transpose?, pointer to matrix)	
	dirtTexture.UseTexture();
	shinyMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	meshList[1]->RenderMesh();


	//Test Human
	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(0.0f + movementCurrentTranslation, 0.0f, -0.0f)); //Transforming identity matrix into translation matrix
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model)); //Set uniform variable value in shader (uniform variable in shader, count, transpose?, pointer to matrix)	
	shinyMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	testModel_Human.RenderModel();

	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(0.0f, 0.5f, 3.0f)); //Transforming identity matrix into translation matrix
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model)); //Set uniform variable value in shader (uniform variable in shader, count, transpose?, pointer to matrix)	
	shinyMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	testModel_Cube.RenderModel();
}

void DirectionalShadowMapPass(DirectionalLight* light) //Pass that renders shadow map
{
	directionalShadowShader.UseShader(); //Set directional shadow shader active

	glViewport(0, 0, light->GetShadowMap()->GetShadowWidth(), light->GetShadowMap()->GetShadowHeight()); //Set viewport size to the same dimensions as framebuffer

	light->GetShadowMap()->Write();
	glClear(GL_DEPTH_BUFFER_BIT);

	
	uniformModel = directionalShadowShader.GetModelLocation(); //Get uniform variables in shader
	directionalShadowShader.SetDirectionalLightTransform(&light->CalculateLightTransform()); //Set uniform variable in shader to view matrix from light source point of view
	
	RenderScene();

	glBindFramebuffer(GL_FRAMEBUFFER, 0); //Bind default framebuffer that will draw scene
}

void RenderPass(glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
{
	shaderList[0].UseShader(); //Set default shader active

	//Get uniform variables in shader
	uniformModel = shaderList[0].GetModelLocation();
	uniformProjection = shaderList[0].GetProjectionLocation();
	uniformView = shaderList[0].GetViewLocation();
	uniformCameraPosition = shaderList[0].GetCameraPositionLocation();
	uniformSpecularIntensity = shaderList[0].GetSpecularIntensityLocation();
	uniformShininess = shaderList[0].GetShininessLocation();

	glViewport(0, 0, 800, 600);

	//Clear window
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f); //Clears all frame and sets color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Clear colors and depth (Each pixel stores more information than only color (depth, stencil, alpha, etc))

	glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projectionMatrix)); //Set uniform variable value in shader
	glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(viewMatrix)); //Set uniform variable value in shader
	glUniform3f(uniformCameraPosition, camera.getCameraPosition().x, camera.getCameraPosition().y, camera.getCameraPosition().z); //Set uniform variable value in shader

	//Set lights (set light properties to shader uniform variables)
	shaderList[0].SetDirectionalLight(&directionalLight);
	shaderList[0].SetPointLights(pointLights, pointLightCount);
	shaderList[0].SetSpotLights(spotLights, spotLightCount);
	shaderList[0].SetDirectionalLightTransform(&directionalLight.CalculateLightTransform());

	directionalLight.GetShadowMap()->Read(GL_TEXTURE1); //Make texture unit 1(second) active and bind shadow map texture to it
	shaderList[0].SetTexture(0); //Set uniform variable in shader that holds texture for mesh to 0(first) texture unit
	shaderList[0].SetDirectionalShadowMap(1); //Set uniform variable in shader that holds shadow map texture 1(second) texture unit

	//glm::vec3 lowerLight = camera.getCameraPosition();
	//lowerLight.y -= 0.3f;
	//spotLights[0].SetFlash(lowerLight, camera.getCameraDirection()); //Flashlight

	RenderScene();
}

int main()
{
	mainWindow = Window(800, 600);
	mainWindow.Initialise();

	CreateObject();
	CreateShaders();

	camera = Camera(glm::vec3(5.0f, 2.0f, -5.0f), glm::vec3(0.0f, 1.0f, 0.0f), 135.0f, 0.0f, 5.0f, 0.4f); //Creating camera

	//Loading models
	testModel_Human = Model();
	testModel_Human.LoadModel("Models/Gary.fbx");
	testModel_Cube = Model();
	testModel_Cube.LoadModel("Models/Cube.fbx");


	//Loading textures
	brickTexture = Texture("Textures/brick.png");
	brickTexture.LoadTexture();
	dirtTexture = Texture("Textures/dirt.png");
	dirtTexture.LoadTexture();
	
	//brickTexture.UseTexture(); //This texture will be used for the rest of the code

	//Creating materials
	shinyMaterial = Material(1.0f, 32);
	dullMaterial = Material(0.3f, 4);

	//Create directional light
	directionalLight = DirectionalLight(2048, 2048,
								1.0f, 1.0f, 1.0f, 
								0.2, 0.3f,
								-10.0f, -15.0f, -10.0f); 

	//Create pointlights
	pointLights[0] = PointLight(0.0f, 0.0f, 1.0f,
								0.0f, 1.0f,
								-8.0f, 2.0f, -8.0f,
								0.3f, 0.2f, 0.1f);
	pointLightCount++;

	pointLights[1] = PointLight(0.0f, 1.0f, 0.0f,
								0.0f, 1.0f,
								8.0f, 2.0f, 8.0f,
								0.3f, 0.1f, 0.1f);
	pointLightCount++;

	//Create spotlights
	spotLights[0] = SpotLight(1.0f, 1.0f, 1.0f,
							  0.0f, 5.0f,
						      -8.0f, 1.0f, -4.0f,
							  45.0f, 0.0f, 0.0f,
							  0.50f, 0.0f, 0.0f,
							  10.0f);
	spotLightCount++;

	spotLights[1] = SpotLight(1.0f, 0.0f, 0.0f,
							  0.0f, 5.0f,
							  0.0f, 1.0f, 0.0f,
							  0.0f, 0.0f, 0.0f,
							  0.40f, 0.10f, 0.10f,
							  15.0f);
	spotLightCount++;


	glm::mat4 projection = glm::perspective(45.0f, (GLfloat)mainWindow.getBufferWidth() / (GLfloat)mainWindow.getBufferHeight(), 0.1f, 100.0f); //(field of view, aspect ratio, draw distance min, draw distance max)

	//Loop until window closed
	while (!mainWindow.getShouldClose()) //If will loop until this function detects that the window should be closed based on variable hidden inside GLFW 
	{
		GLfloat now = glfwGetTime(); // = SDL_GetPerformanceCounter(); - if using STL instead of GLFW
		deltaTime = now - lastTime; // = (now - lastTime) * 1000/SDL_GetPerformanceFrequency(); - if using STL instead of GLFW
		lastTime = now;

		//Get and handle user input events
		glfwPollEvents();
		camera.keyControl(mainWindow.getKeys(), deltaTime);
		camera.mouseControl(mainWindow.getXChange(), mainWindow.getYChange());

		//---------------------Moving Mesh---------------------------


		//Human movement
		if (movementCurrentTranslation >= 3 || movementCurrentTranslation <= -3)
		{
			movementDirection = !movementDirection;
		}
		if (movementDirection)
		{
			movementCurrentTranslation += 0.01f;
		}
		else
		{
			movementCurrentTranslation -= 0.01f;
		}
		

		//Rotating tetrahedron
		currentAngle += angleIncrement;
		if (currentAngle >= 360.0f)
		{
			currentAngle -= 360.0f;
		}
		if (currentSize >= maxSize || currentSize <= minSize)
		{
			sizeDirection = !sizeDirection;
		}
		if (sizeDirection)
		{
			currentSize += 0.001f;
		}
		else
		{
			currentSize -= 0.001f;
		}
	
		
		//-------------------------DRAW---------------------------

		DirectionalShadowMapPass(&directionalLight);
		RenderPass(camera.calculateViewMatrix(), projection);

		glUseProgram(0);

		//---------------------------------------------------------

		mainWindow.swapBuffers();//Swap back buffer and front buffer
	}

	return 0;
}

