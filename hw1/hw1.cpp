/*
  CSCI 420 Computer Graphics, Computer Science, USC
  Assignment 1: Height Fields with Shaders.
  Student username: lwanders
*/

#include "openGLHeader.h"
#include "glutHeader.h"
#include "openGLMatrix.h"
#include "imageIO.h"
#include "pipelineProgram.h"
#include "vbo.h"
#include "vao.h"

#include <iostream>
#include <cstring>
#include <memory>

#if defined(WIN32) || defined(_WIN32)
  #ifdef _DEBUG
    #pragma comment(lib, "glew32d.lib")
  #else
    #pragma comment(lib, "glew32.lib")
  #endif
#endif

#if defined(WIN32) || defined(_WIN32)
  char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
  char shaderBasePath[1024] = "../openGLHelper";
#endif

using namespace std;

int mousePos[2]; // x,y screen coordinates of the current mouse position

int leftMouseButton = 0; // 1 if pressed, 0 if not 
int middleMouseButton = 0; // 1 if pressed, 0 if not
int rightMouseButton = 0; // 1 if pressed, 0 if not

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

typedef enum { POINTS, LINES, TRIANGLES, SHADER } DISPLAY_STATE;
DISPLAY_STATE displayState = POINTS;

// Transformations of the terrain.
float terrainRotate[3] = { 0.0f, 0.0f, 0.0f }; 
// terrainRotate[0] gives the rotation around x-axis (in degrees)
// terrainRotate[1] gives the rotation around y-axis (in degrees)
// terrainRotate[2] gives the rotation around z-axis (in degrees)
float terrainTranslate[3] = { 0.0f, 0.0f, 0.0f };
float terrainScale[3] = { 1.0f, 1.0f, 1.0f };

// Width and height of the OpenGL window, in pixels.
int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 Homework 1";


// Number of vertices in each mode
int numVerts[4];

// CSCI 420 helper classes.
OpenGLMatrix matrix;
PipelineProgram pipelineProgram;
PipelineProgram shaderPipelineProgram;

// Mode 1
VBO vboPointVertices;
VBO vboPointColors;
VAO vaoPoint;

// Mode 2
VBO vboLineVertices;
VBO vboLineColors;
VAO vaoLine;

// Mode 3
VBO vboTriangleVertices;
VBO vboTriangleColors;
VAO vaoTriangle;

// Mode 4
VBO vboSmoothedCenterVertices;
VBO vboSmoothedLeftVertices;
VBO vboSmoothedRightVertices;
VBO vboSmoothedUpVertices;
VBO vboSmoothedDownVertices;
VAO vaoSmoothed;
float scale = 1.0f;
float exponent = 1.0f;

// For toggling between translation and scaling (due to mac limitations) - Press 'T'
bool isScalingMode = true;

// For recording animations - can start / stop recording with R (will overwrite animation folder)
bool isRecording = false;
int frameNum = 0;

// Write a screenshot to the specified filename.
void saveScreenshot(const char * filename)
{
  std::unique_ptr<unsigned char[]> screenshotData = std::make_unique<unsigned char[]>(windowWidth * windowHeight * 3);
  glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, screenshotData.get());

  ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData.get());

  if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK)
    cout << "File " << filename << " saved successfully." << endl;
  else cout << "Failed to save file " << filename << '.' << endl;
}

//captures a screenshot and save it to a given foldername named as "00x.jpg"
void autoCaptureScreenshot(const char* foldername)
{

  //Setting path to "foldename/00x.jpg"
  char filename[8];
  std::sprintf(filename, "%03d.jpg", frameNum);

  char* path = new char[strlen(foldername) + strlen(filename) + 2];
  strcpy(path, foldername);
  strcat(path, "/");
  strcat(path, filename);

  //Taking screenshot

  std::unique_ptr<unsigned char[]> screenshotData = std::make_unique<unsigned char[]>(windowWidth * windowHeight * 3);
  glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, screenshotData.get());

  ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData.get());

  if (screenshotImg.save(path, ImageIO::FORMAT_JPEG) == ImageIO::OK)
  {
    cout << "File " << filename << " saved successfully." << endl;
    frameNum++;
  }
  else 
  {
    cout << "Failed to save file " << filename << '.' << endl;
  }
}

void idleFunc()
{
  //For Recording Animations
  if(isRecording)
  {
    static int lastScreenshotTime = 0;
    int currentTime = glutGet(GLUT_ELAPSED_TIME);

    int frameRate = 67; // 15 frames a second
    if (currentTime - lastScreenshotTime >= frameRate) {
        cout << "Taking a screenshot!" << endl;
        autoCaptureScreenshot("Animation");
        lastScreenshotTime = currentTime;
    }
  }

  // Notify GLUT that it should call displayFunc.
  glutPostRedisplay();
}

void reshapeFunc(int w, int h)
{
  glViewport(0, 0, w, h);

  // When the window has been resized, we need to re-set our projection matrix.
  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.LoadIdentity();
  // You need to be careful about setting the zNear and zFar. 
  // Anything closer than zNear, or further than zFar, will be culled.
  const float zNear = 0.1f;
  const float zFar = 10000.0f;
  const float humanFieldOfView = 60.0f;
  matrix.Perspective(humanFieldOfView, 1.0f * w / h, zNear, zFar);
}

void mouseMotionDragFunc(int x, int y)
{
  // Mouse has moved, and one of the mouse buttons is pressed (dragging).

  // the change in mouse position since the last invocation of this function
  int mousePosDelta[2] = { x - mousePos[0], y - mousePos[1] };

  switch (controlState)
  {
    // translate the terrain
    case TRANSLATE:
      if (leftMouseButton)
      {
        // control x,y translation via the left mouse button
        terrainTranslate[0] += mousePosDelta[0] * 0.01f;
        terrainTranslate[1] -= mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z translation via the middle mouse button
        terrainTranslate[2] += mousePosDelta[1] * 0.01f;
      }
      break;

    // rotate the terrain
    case ROTATE:
      if (leftMouseButton)
      {
        // control x,y rotation via the left mouse button
        terrainRotate[0] += mousePosDelta[1];
        terrainRotate[1] += mousePosDelta[0];
      }
      if (middleMouseButton)
      {
        // control z rotation via the middle mouse button
        terrainRotate[2] += mousePosDelta[1];
      }
      break;

    // scale the terrain
    case SCALE:
      if (leftMouseButton)
      {
        // control x,y scaling via the left mouse button
        terrainScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
        terrainScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z scaling via the middle mouse button
        terrainScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseMotionFunc(int x, int y)
{
  // Mouse has moved.
  // Store the new mouse position.
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y)
{
  // A mouse button has has been pressed or depressed.

  // Keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables.
  switch (button)
  {
    case GLUT_LEFT_BUTTON:
      leftMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_MIDDLE_BUTTON:
      middleMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_RIGHT_BUTTON:
      rightMouseButton = (state == GLUT_DOWN);
    break;
  }

  // Keep track of whether CTRL and SHIFT keys are pressed.
  switch (glutGetModifiers())
  {
    case GLUT_ACTIVE_CTRL: // for windows users
      controlState = TRANSLATE;
    break;

    case GLUT_ACTIVE_SHIFT: // alternate between scaling / translate modes due to mac limitations
      controlState = (isScalingMode) ? SCALE : TRANSLATE;
    break;

    // If CTRL and SHIFT are not pressed, we are in rotate mode.
    default:
      controlState = ROTATE;
    break;
  }

  // Store the new mouse position.
  mousePos[0] = x;
  mousePos[1] = y;
}

void keyboardFunc(unsigned char key, int x, int y)
{
  switch (key)
  {
    case 27: // ESC key
      exit(0); // exit the program
    break;

    case ' ':
      cout << "You pressed the spacebar." << endl;
    break;

    case 'x':
      // Take a screenshot.
      saveScreenshot("screenshot.jpg");
    break;

    case '1':
        //Points Mode
        cout << "switching to points mode" << endl;
        displayState = POINTS;
        break;

    case '2':
        //Lines Mode
        cout << "switching to lines mode" << endl;
        displayState = LINES;
        break;

    case '3':
        //Triangles Mode
        cout << "switching to triangles mode" << endl;
        displayState = TRIANGLES;
        break;

    case '4':
        //Smoothed Shader Mode
        cout << "switching to smoothed shader mode" << endl;
        displayState = SHADER;
        break;

    case '9':
        // Multiply scale
        scale *= 2.0f;
        shaderPipelineProgram.SetUniformVariablef("scale", scale);
        cout << "scale is now: " << scale << endl;
        break;

    case '0':
        // Divide scale
        scale /= 2.0f;
        shaderPipelineProgram.SetUniformVariablef("scale", scale);
        cout << "scale is now: " << scale << endl;
        break;

    case '-':
        // Multiply exponent
        exponent *= 2.0f;
        shaderPipelineProgram.SetUniformVariablef("exponent", exponent);
        cout << "exponent is now: " << exponent << endl;
        break;

    case '=':
        // Divide exponent
        exponent /= 2.0f;
        shaderPipelineProgram.SetUniformVariablef("exponent", exponent);
        cout << "exponent is now: " << exponent << endl;
        break;

    case 't':
        // Toggle between scaling and translation control modes
        isScalingMode = !isScalingMode;
        if(isScalingMode)
        {
          cout << "scaling mode enabled" << endl;
        }
        else
        {
          cout << "translation mode enabled" << endl;
        }
        break;

    case 'r':
        // Start / End a recording
        isRecording = !isRecording;
        if(isRecording)
        {
          cout << "Started Recording!" << endl;
        }
        else
        {
          cout << "Stopped Recording!" << endl;
        }
        break;

  }
}

void displayFunc()
{
  // This function performs the actual rendering.

  // First, clear the screen.
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Set up the camera position, focus point, and the up vector.
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.LoadIdentity();
  

  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.LoadIdentity();
  
  matrix.LookAt(0.0, 0.0, 5.0,
                0.0, 0.0, 0.0,
                0.0, 1.0, 0.0);
  
  matrix.Translate(terrainTranslate[0], terrainTranslate[1], terrainTranslate[2]);
  matrix.Rotate(terrainRotate[0], 1.0f, 0.0f, 0.0f);
  matrix.Rotate(terrainRotate[1], 0.0f, 1.0f, 0.0f);
  matrix.Rotate(terrainRotate[2], 0.0f, 0.0f, 1.0f);
  matrix.Scale(terrainScale[0], terrainScale[1], terrainScale[2]);

  // Read the current modelview and projection matrices from our helper class.
  // The matrices are only read here; nothing is actually communicated to OpenGL yet.
  float modelViewMatrix[16];
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.GetMatrix(modelViewMatrix);

  float projectionMatrix[16];
  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.GetMatrix(projectionMatrix);

  // Upload the modelview and projection matrices to the GPU. Note that these are "uniform" variables.
  // Important: these matrices must be uploaded to *all* pipeline programs used.
  // In hw1, there is only one pipeline program, but in hw2 there will be several of them.
  // In such a case, you must separately upload to *each* pipeline program.
  // Important: do not make a typo in the variable name below; otherwise, the program will malfunction.
  pipelineProgram.SetUniformVariableMatrix4fv("modelViewMatrix", GL_FALSE, modelViewMatrix);
  pipelineProgram.SetUniformVariableMatrix4fv("projectionMatrix", GL_FALSE, projectionMatrix);

  shaderPipelineProgram.SetUniformVariableMatrix4fv("modelViewMatrix", GL_FALSE, modelViewMatrix);
  shaderPipelineProgram.SetUniformVariableMatrix4fv("projectionMatrix", GL_FALSE, projectionMatrix);
  
  if(displayState == POINTS)
  {
    vaoPoint.Bind();
    pipelineProgram.Bind();
    glDrawArrays(GL_POINTS, 0, numVerts[0]);

  }
  else if(displayState == LINES)
  {
    vaoLine.Bind();
    pipelineProgram.Bind();
    glDrawArrays(GL_LINES, 0, numVerts[1]);
  }
  else if(displayState == TRIANGLES)
  {
    vaoTriangle.Bind();
    pipelineProgram.Bind();
    glDrawArrays(GL_TRIANGLES, 0, numVerts[2]);
  }
  else if(displayState == SHADER)
  {
    vaoSmoothed.Bind();
    shaderPipelineProgram.Bind();
    glDrawArrays(GL_TRIANGLES, 0, numVerts[3]);
  }

  // Swap the double-buffers.
  glutSwapBuffers();
}

void MakeVertex(int row, int col, std::unique_ptr<float[]>& positions, std::unique_ptr<float[]>& colors, int& vertexIndex, int& colorIndex, int imageWidth, int imageHeight, int vertexSpacing, float heightNormalized)
{
      positions[vertexIndex] = (col - imageWidth / 2) / (vertexSpacing - 1.0f);  // x
      positions[vertexIndex + 1] = heightNormalized * 2.1f;   // y
      positions[vertexIndex + 2] = -(row - imageHeight / 2) / (vertexSpacing - 1.0f); //z
      vertexIndex += 3;

      colors[colorIndex] = heightNormalized;       // r
      colors[colorIndex + 1] = heightNormalized;   // g
      colors[colorIndex + 2] = heightNormalized;   // b
      colors[colorIndex + 3] = 1.0f;               // a
      colorIndex += 4;
}

//Vertex helper function for mode 4
void MakeSmoothedVertex(int row, int col, std::unique_ptr<float[]>& centerPositions, std::unique_ptr<float[]>& leftPositions, std::unique_ptr<float[]>& rightPositions, std::unique_ptr<float[]>& upPositions, std::unique_ptr<float[]>& downPositions, int& vertexIndex, int imageWidth, int imageHeight, int vertexSpacing, std::unique_ptr<float[]>& heightNormalized)
{
      //Center
      centerPositions[vertexIndex] = (col - imageWidth / 2) / (vertexSpacing - 1.0f);  // x
      centerPositions[vertexIndex + 1] = heightNormalized[0] * 2.1f;   // y
      centerPositions[vertexIndex + 2] = -(row - imageHeight / 2) / (vertexSpacing - 1.0f); //z

      //Left
      leftPositions[vertexIndex] = ((col - 1) - imageWidth / 2) / (vertexSpacing - 1.0f);  // x
      leftPositions[vertexIndex + 1] = heightNormalized[1] * 2.1f;   // y
      leftPositions[vertexIndex + 2] = -(row - imageHeight / 2) / (vertexSpacing - 1.0f); //z

      //Right
      rightPositions[vertexIndex] = ((col + 1) - imageWidth / 2) / (vertexSpacing - 1.0f);  // x
      rightPositions[vertexIndex + 1] = heightNormalized[2] * 2.1f;   // y
      rightPositions[vertexIndex + 2] = -(row - imageHeight / 2) / (vertexSpacing - 1.0f); //z

      //Up
      upPositions[vertexIndex] = (col - imageWidth / 2) / (vertexSpacing - 1.0f);  // x
      upPositions[vertexIndex + 1] = heightNormalized[3] * 2.1f;   // y
      upPositions[vertexIndex + 2] = -((row + 1) - imageHeight / 2) / (vertexSpacing - 1.0f); //z

      //Down
      downPositions[vertexIndex] = (col - imageWidth / 2) / (vertexSpacing - 1.0f);  // x
      downPositions[vertexIndex + 1] = heightNormalized[4] * 2.1f;   // y
      downPositions[vertexIndex + 2] = -((row - 1) - imageHeight / 2) / (vertexSpacing - 1.0f); //z
      
      //iterates vertex index for all position arrays
      vertexIndex += 3;
}

void GenerateMode1(std::unique_ptr<ImageIO>& heightmapImage, int vertexSpacing) 
{
    int imageWidth = heightmapImage->getWidth();
    int imageHeight = heightmapImage->getHeight();
    numVerts[0] = imageWidth * imageHeight;

    std::unique_ptr<float[]> positions = std::make_unique<float[]>(numVerts[0] * 3);
    std::unique_ptr<float[]> colors = std::make_unique<float[]>(numVerts[0] * 4);

    int vertexIndex = 0;
    int colorIndex = 0;

    for (int row = 0; row < imageHeight; row++) 
    {
      for (int col = 0; col < imageWidth; col++) 
      {
          float heightNormalized = static_cast<float>(heightmapImage->getPixel(row, col, 0) / 255.0f);
          MakeVertex(row, col, positions, colors, vertexIndex, colorIndex, imageWidth, imageHeight, vertexSpacing, heightNormalized);
      }
    }
      
  vboPointVertices.Gen(numVerts[0], 3, positions.get(), GL_STATIC_DRAW); // 3 values per position
  vboPointColors.Gen(numVerts[0], 4, colors.get(), GL_STATIC_DRAW); // 4 values per color

  vaoPoint.Gen();
  vaoPoint.ConnectPipelineProgramAndVBOAndShaderVariable(&pipelineProgram, &vboPointVertices, "position");
  vaoPoint.ConnectPipelineProgramAndVBOAndShaderVariable(&pipelineProgram, &vboPointColors, "color");
}

void GenerateMode2(std::unique_ptr<ImageIO>& heightmapImage, int vertexSpacing)
{
    int imageWidth = heightmapImage->getWidth();
    int imageHeight = heightmapImage->getHeight();
    numVerts[1] = 2 * ((imageWidth - 1) * imageHeight + (imageWidth * imageHeight - 1));

    std::unique_ptr<float[]> positions = std::make_unique<float[]>(numVerts[1] * 3);
    std::unique_ptr<float[]> colors = std::make_unique<float[]>(numVerts[1] * 4);

    int vertexIndex = 0;
    int colorIndex = 0;

    // Draws Vertical lines
    for (int row = 0; row < imageHeight - 1; row++) 
    {
        for (int col = 0; col < imageWidth; col++) 
        {
            float heightNormalizedStart = static_cast<float>(heightmapImage->getPixel(row, col, 0)) / 255.0f;
            float heightNormalizedEnd = static_cast<float>(heightmapImage->getPixel(row + 1, col, 0)) / 255.0f;

           MakeVertex(row, col, positions, colors, vertexIndex, colorIndex, imageWidth, imageHeight, vertexSpacing, heightNormalizedStart);
           MakeVertex(row + 1, col, positions, colors, vertexIndex, colorIndex, imageWidth, imageHeight, vertexSpacing, heightNormalizedEnd);
        }
    }

    // Draws Horizontal lines
    for (int row = 0; row < imageHeight; row++) 
    {
        for (int col = 0; col < imageWidth - 1; col++) 
        {
          float heightNormalizedStart = static_cast<float>(heightmapImage->getPixel(row, col, 0)) / 255.0f;
          float heightNormalizedEnd = static_cast<float>(heightmapImage->getPixel(row, col + 1, 0)) / 255.0f;

          MakeVertex(row, col, positions, colors, vertexIndex, colorIndex, imageWidth, imageHeight, vertexSpacing, heightNormalizedStart);
          MakeVertex(row, col + 1, positions, colors, vertexIndex, colorIndex, imageWidth, imageHeight, vertexSpacing, heightNormalizedEnd);
        }
    }

    vboLineVertices.Gen(numVerts[1], 3, positions.get(), GL_STATIC_DRAW); // 3 values per position
    vboLineColors.Gen(numVerts[1], 4, colors.get(), GL_STATIC_DRAW); // 4 values per color

    vaoLine.Gen();
    vaoLine.ConnectPipelineProgramAndVBOAndShaderVariable(&pipelineProgram, &vboLineVertices, "position");
    vaoLine.ConnectPipelineProgramAndVBOAndShaderVariable(&pipelineProgram, &vboLineColors, "color");
}

void GenerateMode3(std::unique_ptr<ImageIO>& heightmapImage, int vertexSpacing)
{
    int imageWidth = heightmapImage->getWidth();
    int imageHeight = heightmapImage->getHeight();
    numVerts[2] = 6 * (imageWidth - 1) * (imageHeight -1);

    std::unique_ptr<float[]> positions = std::make_unique<float[]>(numVerts[2] * 3);
    std::unique_ptr<float[]> colors = std::make_unique<float[]>(numVerts[2] * 4);

    int vertexIndex = 0;
    int colorIndex = 0;

  // Draws "Upwards" and "Downwards" Triangles to fill heightmap
    for (int row = 0; row < imageHeight - 1; row++) 
    {
      for (int col = 0; col < imageWidth - 1; col++) 
      {
          // Get height values
          float heightNormalizedBottomLeft = static_cast<float>(heightmapImage->getPixel(row,  col, 0)) / 255.0f;
          float heightNormalizedBottomRight = static_cast<float>(heightmapImage->getPixel(row, col + 1, 0)) / 255.0f;
          float heightNormalizedTopLeft = static_cast<float>(heightmapImage->getPixel(row + 1, col, 0)) / 255.0f;
          float heightNormalizedTopRight = static_cast<float>(heightmapImage->getPixel(row + 1, col + 1, 0)) / 255.0f;

        // Upwards Triangle (Top Left, Bottom Left, Bottom Right)
          MakeVertex(row, col, positions, colors, vertexIndex, colorIndex, imageWidth, imageHeight, vertexSpacing, heightNormalizedBottomLeft);  // Bottom Left
          MakeVertex(row + 1, col, positions, colors, vertexIndex, colorIndex, imageWidth, imageHeight, vertexSpacing, heightNormalizedTopLeft);  // Top Left
          MakeVertex(row, col + 1, positions, colors, vertexIndex, colorIndex, imageWidth, imageHeight, vertexSpacing, heightNormalizedBottomRight);  // Bottom Right

          // Downwards Triangle (Top Left, Top Right, Bottom Right)
          MakeVertex(row + 1, col, positions, colors, vertexIndex, colorIndex, imageWidth, imageHeight, vertexSpacing, heightNormalizedTopLeft);  // Top Left
          MakeVertex(row, col + 1, positions, colors, vertexIndex, colorIndex, imageWidth, imageHeight, vertexSpacing, heightNormalizedBottomRight);  // Bottom Right
          MakeVertex(row + 1, col + 1, positions, colors, vertexIndex, colorIndex, imageWidth, imageHeight, vertexSpacing, heightNormalizedTopRight);  // Top Right

        }
    }


    vboTriangleVertices.Gen(numVerts[2], 3, positions.get(), GL_STATIC_DRAW); // 3 values per position
    vboTriangleColors.Gen(numVerts[2], 4, colors.get(), GL_STATIC_DRAW); // 4 values per color

    vaoTriangle.Gen();
    vaoTriangle.ConnectPipelineProgramAndVBOAndShaderVariable(&pipelineProgram, &vboTriangleVertices, "position");
    vaoTriangle.ConnectPipelineProgramAndVBOAndShaderVariable(&pipelineProgram, &vboTriangleColors, "color");
}

//Helper function for mode 4 (get all normalized heights)
std::unique_ptr<float[]> GetHeightmapArray(std::unique_ptr<ImageIO>& heightmapImage, int row, int col, int imageHeight, int imageWidth)
{
    float heightNormalizedCenter = static_cast<float>(heightmapImage->getPixel(row,  col, 0)) / 255.0f;
    float heightNormalizedLeft = (col - 1 >= 0) ? static_cast<float>(heightmapImage->getPixel(row,  col - 1, 0)) / 255.0f : heightNormalizedCenter;
    float heightNormalizedRight = (col + 1 < imageWidth) ? static_cast<float>(heightmapImage->getPixel(row,  col + 1, 0)) / 255.0f : heightNormalizedCenter;
    float heightNormalizedUp = (row + 1 < imageHeight) ? static_cast<float>(heightmapImage->getPixel(row + 1,  col, 0)) / 255.0f : heightNormalizedCenter;
    float heightNormalizedDown = (row - 1 >= 0) ? static_cast<float>(heightmapImage->getPixel(row - 1,  col, 0)) / 255.0f : heightNormalizedCenter;
    
    std::unique_ptr<float[]> heightMapArray = std::make_unique<float[]>(5);
    heightMapArray[0] = heightNormalizedCenter;
    heightMapArray[1] = heightNormalizedLeft;
    heightMapArray[2] = heightNormalizedRight;
    heightMapArray[3] = heightNormalizedUp;
    heightMapArray[4] = heightNormalizedDown;
    return heightMapArray;    
}

void GenerateMode4(std::unique_ptr<ImageIO>& heightmapImage, int vertexSpacing)
{
    int imageWidth = heightmapImage->getWidth();
    int imageHeight = heightmapImage->getHeight();
    numVerts[3] = 6 * (imageWidth) * (imageHeight);

    std::unique_ptr<float[]> positionsCenter = std::make_unique<float[]>(numVerts[3] * 3);
    std::unique_ptr<float[]> positionsLeft = std::make_unique<float[]>(numVerts[3] * 3);
    std::unique_ptr<float[]> positionsRight = std::make_unique<float[]>(numVerts[3] * 3);
    std::unique_ptr<float[]> positionsDown = std::make_unique<float[]>(numVerts[3] * 3);
    std::unique_ptr<float[]> positionsUp = std::make_unique<float[]>(numVerts[3] * 3);
    std::unique_ptr<float[]> colors = std::make_unique<float[]>(numVerts[3] * 4);

    int vertexIndex = 0;
    int colorIndex = 0;

  // Draws "Upwards" and "Downwards" Triangles to fill heightmap
    for (int row = 0; row < imageHeight - 1; row++) 
    {
      for (int col = 0; col < imageWidth - 1; col++) 
      {
          // Get height arrays
          std::unique_ptr<float[]> heightArrayBottomLeft = GetHeightmapArray(heightmapImage, row, col, imageHeight, imageWidth);
          std::unique_ptr<float[]> heightArrayBottomRight = GetHeightmapArray(heightmapImage, row, col + 1, imageHeight, imageWidth);
          std::unique_ptr<float[]> heightArrayTopLeft = GetHeightmapArray(heightmapImage, row + 1, col, imageHeight, imageWidth);
          std::unique_ptr<float[]> heightArrayTopRight = GetHeightmapArray(heightmapImage, row + 1, col + 1, imageHeight, imageWidth);

         // Upwards Triangle (Top Left, Bottom Left, Bottom Right)
          MakeSmoothedVertex(row, col, positionsCenter, positionsLeft, positionsRight, positionsUp, positionsDown, vertexIndex, imageWidth, imageHeight, vertexSpacing, heightArrayBottomLeft);  // Bottom Left
          MakeSmoothedVertex(row + 1, col, positionsCenter, positionsLeft, positionsRight, positionsUp, positionsDown, vertexIndex, imageWidth, imageHeight, vertexSpacing, heightArrayTopLeft);  // Top Left
          MakeSmoothedVertex(row, col + 1, positionsCenter, positionsLeft, positionsRight, positionsUp, positionsDown, vertexIndex, imageWidth, imageHeight, vertexSpacing, heightArrayBottomRight);  // Bottom Right

          // Downwards Triangle (Top Left, Top Right, Bottom Right)
          MakeSmoothedVertex(row + 1, col, positionsCenter, positionsLeft, positionsRight, positionsUp, positionsDown, vertexIndex, imageWidth, imageHeight, vertexSpacing, heightArrayTopLeft);  // Top Left
          MakeSmoothedVertex(row + 1, col + 1, positionsCenter, positionsLeft, positionsRight, positionsUp, positionsDown, vertexIndex, imageWidth, imageHeight, vertexSpacing, heightArrayTopRight);  // Top Right
          MakeSmoothedVertex(row, col + 1, positionsCenter, positionsLeft, positionsRight, positionsUp, positionsDown, vertexIndex, imageWidth, imageHeight, vertexSpacing, heightArrayBottomRight);  // Bottom Right
        }
    }

    
    vboSmoothedCenterVertices.Gen(numVerts[3], 3, positionsCenter.get(), GL_STATIC_DRAW); // 3 values per position
    vboSmoothedLeftVertices.Gen(numVerts[3], 3, positionsLeft.get(), GL_STATIC_DRAW); // 3 values per position
    vboSmoothedRightVertices.Gen(numVerts[3], 3, positionsRight.get(), GL_STATIC_DRAW); // 3 values per position
    vboSmoothedUpVertices.Gen(numVerts[3], 3, positionsUp.get(), GL_STATIC_DRAW); // 3 values per position
    vboSmoothedDownVertices.Gen(numVerts[3], 3, positionsDown.get(), GL_STATIC_DRAW); // 3 values per position

    vaoSmoothed.Gen();
    vaoSmoothed.ConnectPipelineProgramAndVBOAndShaderVariable(&shaderPipelineProgram, &vboSmoothedCenterVertices, "centerPosition");
    vaoSmoothed.ConnectPipelineProgramAndVBOAndShaderVariable(&shaderPipelineProgram, &vboSmoothedLeftVertices, "leftPosition");
    vaoSmoothed.ConnectPipelineProgramAndVBOAndShaderVariable(&shaderPipelineProgram, &vboSmoothedRightVertices, "rightPosition");
    vaoSmoothed.ConnectPipelineProgramAndVBOAndShaderVariable(&shaderPipelineProgram, &vboSmoothedUpVertices, "upPosition");
    vaoSmoothed.ConnectPipelineProgramAndVBOAndShaderVariable(&shaderPipelineProgram, &vboSmoothedDownVertices, "downPosition");
    
    shaderPipelineProgram.SetUniformVariablef("exponent", exponent);
    shaderPipelineProgram.SetUniformVariablef("scale", scale);
}

void initScene(int argc, char *argv[])
{
  // Load the image from a jpeg disk file into main memory.
  std::unique_ptr<ImageIO> heightmapImage = std::make_unique<ImageIO>();
  if (heightmapImage->loadJPEG(argv[1]) != ImageIO::OK)
  {
    cout << "Error reading image " << argv[1] << "." << endl;
    exit(EXIT_FAILURE);
  }

  // Set the background color.
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black color.

  // Enable z-buffering (i.e., hidden surface removal using the z-buffer algorithm).
  glEnable(GL_DEPTH_TEST);

  // Create a pipeline program. This operation must be performed BEFORE we initialize any VAOs.
  // A pipeline program contains our shaders. Different pipeline programs may contain different shaders.
  // In this homework, we only have one set of shaders, and therefore, there is only one pipeline program.
  // In hw2, we will need to shade different objects with different shaders, and therefore, we will have
  // several pipeline programs (e.g., one for the rails, one for the ground/sky, etc.).
  // Load and set up the pipeline program, including its shaders.
  if (pipelineProgram.BuildShadersFromFiles(shaderBasePath, "vertexShader.glsl", "fragmentShader.glsl") != 0)
  {
    cout << "Failed to build the pipeline program." << endl;
    throw 1;
  } 
  cout << "Successfully built the pipeline program." << endl;

  if (shaderPipelineProgram.BuildShadersFromFiles(shaderBasePath, "vertexSmoothedShader.glsl", "fragmentShader.glsl") != 0)
  {
    cout << "Failed to build the shader pipeline program." << endl;
    throw 1;
  } 
  cout << "Successfully built the shader pipeline program." << endl;
    
  // Bind the pipeline program that we just created. 
  // The purpose of binding a pipeline program is to activate the shaders that it contains, i.e.,
  // any object rendered from that point on, will use those shaders.
  // When the application starts, no pipeline program is bound, which means that rendering is not set up.
  // So, at some point (such as below), we need to bind a pipeline program.
  // From that point on, exactly one pipeline program is bound at any moment of time.
  pipelineProgram.Bind();

  int imageWidth = heightmapImage->getWidth();
  int imageHeight = heightmapImage->getHeight();
  int vertexSpacing = 50;
  float vertexMaxHeight = 1.0f;

  GenerateMode1(heightmapImage, vertexSpacing);
  GenerateMode2(heightmapImage, vertexSpacing);
  GenerateMode3(heightmapImage, vertexSpacing);
  GenerateMode4(heightmapImage, vertexSpacing);

  // Check for any OpenGL errors.
  std::cout << "GL error status is: " << glGetError() << std::endl;

}

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    
    cout << "The arguments are incorrect." << endl;
    cout << "usage: ./hw1 <heightmap file>" << endl;
    exit(EXIT_FAILURE);
  }

  cout << "Initializing GLUT..." << endl;
  glutInit(&argc,argv);

  cout << "Initializing OpenGL..." << endl;

  #ifdef __APPLE__
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
  #else
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
  #endif

  glutInitWindowSize(windowWidth, windowHeight);
  glutInitWindowPosition(0, 0);  
  glutCreateWindow(windowTitle);

  cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
  cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
  cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

  #ifdef __APPLE__
    // This is needed on recent Mac OS X versions to correctly display the window.
    glutReshapeWindow(windowWidth - 1, windowHeight - 1);
  #endif

  // Tells GLUT to use a particular display function to redraw.
  glutDisplayFunc(displayFunc);
  // Perform animation inside idleFunc.
  glutIdleFunc(idleFunc);
  // callback for mouse drags
  glutMotionFunc(mouseMotionDragFunc);
  // callback for idle mouse movement
  glutPassiveMotionFunc(mouseMotionFunc);
  // callback for mouse button changes
  glutMouseFunc(mouseButtonFunc);
  // callback for resizing the window
  glutReshapeFunc(reshapeFunc);
  // callback for pressing the keys on the keyboard
  glutKeyboardFunc(keyboardFunc);

  // init glew
  #ifdef __APPLE__
    // nothing is needed on Apple
  #else
    // Windows, Linux
    GLint result = glewInit();
    if (result != GLEW_OK)
    {
      cout << "error: " << glewGetErrorString(result) << endl;
      exit(EXIT_FAILURE);
    }
  #endif

  // Perform the initialization.
  initScene(argc, argv);

  // Sink forever into the GLUT loop.
  glutMainLoop();
}

