#include <iostream>
#include <string>
#include <stdio.h>

#include "SDL.h"
#include <GL/glew.h>

#include "glwindow.h"
#include "geometry.h"

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <common/shader.hpp>

using namespace std;

const char* glGetErrorString(GLenum error)
{
    switch(error)
    {
    case GL_NO_ERROR:
        return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    default:
        return "UNRECOGNIZED";
    }
}

void glPrintError(const char* label="Unlabelled Error Checkpoint", bool alwaysPrint=false)
{
    GLenum error = glGetError();
    if(alwaysPrint || (error != GL_NO_ERROR))
    {
        printf("%s: OpenGL error flag is %s\n", label, glGetErrorString(error));
    }
}

GLuint loadShader(const char* shaderFilename, GLenum shaderType)
{
    FILE* shaderFile = fopen(shaderFilename, "r");
    if(!shaderFile)
    {
        return 0;
    }

    fseek(shaderFile, 0, SEEK_END);
    long shaderSize = ftell(shaderFile);
    fseek(shaderFile, 0, SEEK_SET);

    char* shaderText = new char[shaderSize+1];
    size_t readCount = fread(shaderText, 1, shaderSize, shaderFile);
    shaderText[readCount] = '\0';
    fclose(shaderFile);

    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, (const char**)&shaderText, NULL);
    glCompileShader(shader);

    delete[] shaderText;

    return shader;
}

GLuint loadShaderProgram(const char* vertShaderFilename, const char* fragShaderFilename)
{
    GLuint vertShader = loadShader(vertShaderFilename, GL_VERTEX_SHADER);
    GLuint fragShader = loadShader(fragShaderFilename, GL_FRAGMENT_SHADER);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if(linkStatus != GL_TRUE)
    {
        GLsizei logLength = 0;
        GLchar message[1024];
        glGetProgramInfoLog(program, 1024, &logLength, message);
        cout << "Shader load error: " << message << endl;
        return 0;
    }

    return program;
}

OpenGLWindow::OpenGLWindow()
{
}

void OpenGLWindow::initGL()
{
    // We need to first specify what type of OpenGL context we need before we can create the window
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    sdlWin = SDL_CreateWindow("OpenGL Prac 1",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              640, 480, SDL_WINDOW_OPENGL);
    if(!sdlWin)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Error", "Unable to create window", 0);
    }
    SDL_GLContext glc = SDL_GL_CreateContext(sdlWin);
    SDL_GL_MakeCurrent(sdlWin, glc);
    SDL_GL_SetSwapInterval(1);

    glewExperimental = true;
    GLenum glewInitResult = glewInit();
    glGetError(); // Consume the error erroneously set by glewInit()
    if(glewInitResult != GLEW_OK)
    {
        const GLubyte* errorString = glewGetErrorString(glewInitResult);
        cout << "Unable to initialize glew: " << errorString;
    }

    int glMajorVersion;
    int glMinorVersion;
    glGetIntegerv(GL_MAJOR_VERSION, &glMajorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &glMinorVersion);
    cout << "Loaded OpenGL " << glMajorVersion << "." << glMinorVersion << " with:" << endl;
    cout << "\tVendor: " << glGetString(GL_VENDOR) << endl;
    cout << "\tRenderer: " << glGetString(GL_RENDERER) << endl;
    cout << "\tVersion: " << glGetString(GL_VERSION) << endl;
    cout << "\tGLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Dark grey background
    glClearColor(0.1f, 0.1f, 0.1f, 0.0f);

    // generate and bind VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // load and use shader
    shader = loadShaderProgram("build/simple.vert", "build/simple.frag");
    glUseProgram(shader);

    // Get a handle for our "MVP" uniform
    MatrixID = glGetUniformLocation(shader, "MVP");

    // Projection matrix : 45° Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
    Projection = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 100.0f);

    // Camera matrix
    View       = glm::lookAt(
                    glm::vec3(0,0,-5), // Camera is at (0, 0, 5), in World Space
                    glm::vec3(0,0, 0), // and looks at the origin
                    glm::vec3(0,1, 0)  // Head is up (set to 0,-1,0 to look upside-down)
                );

    // Model matrix : an identity matrix (model will be at the origin)
    Model      = glm::mat4(1.0f);

    // Our ModelViewProjection : multiplication of our 3 matrices
    MVP        = Projection * View * Model;

    // set object color to white
    colorLoc = glGetUniformLocation(shader, "objectColor");
    glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);                

    // Load the model that we want to use and buffer the vertex attributes
    cout << "Enter the model path to import: ";
    cin >> filename1;
    
    GeometryData geometry;
    geometry.loadFromOBJFile(filename1);
    vertexCount = geometry.vertexCount();

    int vertexLoc = glGetAttribLocation(shader, "position");

    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * sizeof(float), geometry.vertexData(), GL_STATIC_DRAW);
    glVertexAttribPointer(vertexLoc, 3, GL_FLOAT, false, 0, 0);
    glEnableVertexAttribArray(vertexLoc);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glPrintError("Setup complete!", true);
}

void OpenGLWindow::spawnNewObject() { // loads a second model
    GeometryData geometry, geometry2;

    cout << "Enter the model path to import: ";
    cin >> filename2;

    // load models
    geometry.loadFromOBJFile(filename1);
    vertexCount = geometry.vertexCount();

    geometry2.loadFromOBJFile(filename2);
    vertexCount2 = geometry2.vertexCount();

    cout << "Total vertex count: " << geometry.vertexCount() + geometry2.vertexCount() << endl;

    float * firstModel = ( (float*) geometry.vertexData() );
    float * secondModel = ( (float*) geometry2.vertexData() );

    // calculate maximum x coordinate of first model
    float firstBound = 0;

    for (int vertex = 0; vertex < vertexCount * 3; vertex += 3) {
        if (firstModel[vertex] > firstBound) {
            firstBound = firstModel[vertex];
        }
    }  

    // calculate minimum x coordinate of second model
    float secondBound = FLT_MAX;

    for (int vertex = 0; vertex < vertexCount2 * 3; vertex += 3) {
        if (secondModel[vertex] < secondBound) {
            secondBound = secondModel[vertex];
        }
    }

    // calculate shift value
    float shift = std::abs(secondBound - firstBound) + 0.3f;
    cout << "Shift value: " << shift << endl;

    // offset the second model's vertices using bounding value
    for (int vertex = 0; vertex < vertexCount2 * 3; vertex += 3) {
        secondModel[vertex] += shift;
    }

    // combine vertex arrays
    vector<float> combinedVertices;
    for (int i = 0; i < vertexCount * 3; i++ ) {
        combinedVertices.push_back( firstModel[i] );
    }
    for (int i = 0; i < vertexCount2 * 3; i++ ) {
        combinedVertices.push_back( secondModel[i] );
    }
    cout << "combined vertices into one vector" << endl;

    // buffer vertices
    int vertexLoc = glGetAttribLocation(shader, "position");

    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (vertexCount + vertexCount2) * 3 * sizeof(float), &combinedVertices[0], GL_STATIC_DRAW);
    glVertexAttribPointer(vertexLoc, 3, GL_FLOAT, false, 0, 0);
    glEnableVertexAttribArray(vertexLoc);

    spawnedSecondObj = true;
    glPrintError("Second model loading complete!", true);
}

void OpenGLWindow::render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shader);

    if (partyMode)
        glUniform3f(colorLoc, (float) rand()/RAND_MAX, (float) rand()/RAND_MAX, (float) rand()/RAND_MAX);

    // send transformation to shader
    glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

    // draw objects
    glDrawArrays(GL_TRIANGLES, 0, vertexCount + vertexCount2);

    // Swap the front and back buffers
    SDL_GL_SwapWindow(sdlWin);
}

void OpenGLWindow::changeAxis() { // switches between transform axis
    if (transformationAxis == X) {
        transformationAxis = Y;
        cout << "Axis: Y" << endl;
    }
    else if (transformationAxis == Y) {
        transformationAxis = Z;
        cout << "Axis: Z" << endl;
    }
    else if (transformationAxis == Z) {
        transformationAxis = X;
        cout << "Axis: X" << endl;
    }
}

bool OpenGLWindow::handleEvent(SDL_Event e) { // event handler for SDL key presses and mouse motion

    if(e.type == SDL_KEYDOWN)
    {
        if(e.key.keysym.sym == SDLK_ESCAPE) // 'Esc' = exit
        {
            return false;
        }

        if(e.key.keysym.sym == SDLK_a) // 'A' = scale all
        {
            transformationMode = SCALEALL;
            cout << "Scaling object uniformly." << endl;
        }

        if(e.key.keysym.sym == SDLK_s) // 'S' = scale
        {
            if (transformationMode == SCALE) {
                changeAxis();
            }
            else {
                cout << "Scaling object on individual axis." << endl;
                cout << "Axis: X" << endl;
                transformationMode = SCALE;
                transformationAxis = X;
            }   

            return true;
        }

        if(e.key.keysym.sym == SDLK_t) // 'T' = translate
        {
            if (transformationMode == TRANSLATE) {
                changeAxis();
            }
            else {
                cout << "Translating object." << endl;
                cout << "Axis: X" << endl;
                transformationMode = TRANSLATE;
                transformationAxis = X;
            }      
            return true;
        }
        
        if(e.key.keysym.sym == SDLK_r) // 'R' = rotate
        {
            if (transformationMode == ROTATE) {
                changeAxis();
            }
            else {
                cout << "Rotating object." << endl;
                cout << "Axis: X" << endl;
                transformationMode = ROTATE;
                transformationAxis = X;
            }        
            return true;
        }

        if(e.key.keysym.sym == SDLK_v) // 'V' = view
        {
            if (transformationMode != VIEW) {
                cout << "Viewing object." << endl;
                transformationMode = VIEW;
            }               
        }
        if(e.key.keysym.sym == SDLK_p) // 'P' = party mode (color change)
        {
            if (!partyMode) {
                cout << "Party mode enabled! :D" << endl;
                partyMode = true;   
            }
            else {
                cout << "Party mode disabled... :(" << endl;
                partyMode = false;
                glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);                
            }      
        }
        if(e.key.keysym.sym == SDLK_n) // 'N' = load second object
        {
            if (!spawnedSecondObj) {
                spawnNewObject();  
                cout << "Spawning second object!" << endl;
            }
            else cout << "Already have two models in the scene, sorry!" << endl;
        }
    }
    
    else if (e.type == SDL_MOUSEMOTION) { // handle mouse motion to drive transformations

        // figure out if movement is up or down, and change sign accordingly
        int sign = 0;
        if (e.motion.yrel < 0)
            sign = 1;
        else
            sign = -1;


        if (transformationMode == ROTATE) { // apply rotation
            glm::mat4 trans;
            trans = glm::translate(trans, translation);

            if (transformationAxis == X) {
                trans = glm::rotate(trans, glm::radians(sign * 5.0f), glm::vec3(1.0, 0.0, 0.0));
            }
            else if (transformationAxis == Y) {
                trans = glm::rotate(trans, glm::radians(sign * 5.0f), glm::vec3(0.0, 1.0, 0.0));
            }
            else if (transformationAxis == Z) {
                trans = glm::rotate(trans, glm::radians(sign * 5.0f), glm::vec3(0.0, 0.0, 1.0));             
            }

            trans = glm::translate(trans, -translation);
            Model *= trans;
            MVP = Projection * View * Model;            
            return true;
        }

        else if (transformationMode == SCALE) { //apply scale
            if (transformationAxis == X) {
                glm::mat4 trans;
                trans = glm::scale(trans, glm::vec3(1.0 + (sign * 0.1), 1.0, 1.0));
                Model *= trans;
            }
            else if (transformationAxis == Y) {
                glm::mat4 trans;
                trans = glm::scale(trans, glm::vec3(1.0, 1.0 + (sign * 0.1), 1.0));
                Model *= trans;                
            }
            else if (transformationAxis == Z) {
                glm::mat4 trans;
                trans = glm::scale(trans, glm::vec3(1.0, 1.0, 1.0 + (sign * 0.1)));
                Model *= trans;                
            }
            MVP = Projection * View * Model;            
            return true;
        } 

        else if (transformationMode == SCALEALL) { // apply uniform scale
            glm::mat4 trans;
            trans = glm::scale(trans, glm::vec3(1.0 + (sign * 0.1), 1.0 + (sign * 0.1), 1.0 + (sign * 0.1)));
            Model *= trans;   
            MVP = Projection * View * Model;            
            return true;
        } 

        else if (transformationMode == TRANSLATE) { // apply translation
            if (transformationAxis == X) {
                glm::mat4 trans;
                trans = glm::translate(trans, glm::vec3(sign * 0.1, 0.0, 0.0));
                translation.x += sign * 0.1;
                Model *= trans;
            }
            else if (transformationAxis == Y) {
                glm::mat4 trans;
                trans = glm::translate(trans, glm::vec3(0.0, sign * 0.1, 0.0));
                translation.y += sign * 0.1;
                Model *= trans;                
            }
            else if (transformationAxis == Z) {
                glm::mat4 trans;
                trans = glm::translate(trans, glm::vec3(0.0, 0.0, sign * 0.1));
                translation.z += sign * 0.1;
                Model *= trans;                
            }
            cout << "Oject translation: (" << translation.x << ", " << translation.y << ", " << translation.z << ")" << endl;
            MVP = Projection * View * Model;            
            return true;
        } 

        else if (transformationMode == VIEW) { // do nothing
            return true;
        } 
    }
    return true;
}

void OpenGLWindow::cleanup()
{
    glDeleteBuffers(1, &vertexBuffer);
    glDeleteVertexArrays(1, &vao);
    SDL_DestroyWindow(sdlWin);
}