#include <iostream>
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

    // Dark blue background
    glClearColor(0.1f, 0.1f, 0.1f, 0.0f);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Note that this path is relative to your working directory
    // when running the program (IE if you run from within build
    // then you need to place these files in build as well)
    shader = loadShaderProgram("build/simple.vert", "build/simple.frag");
    glUseProgram(shader);

    // Get a handle for our "MVP" uniform
    MatrixID = glGetUniformLocation(shader, "MVP");

    // Projection matrix : 45° Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
    Projection = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 100.0f);

    // Camera matrix
    View       = glm::lookAt(
                    glm::vec3(0,0,-5), // Camera is at (4,3,3), in World Space
                    glm::vec3(0,0,0), // and looks at the origin
                    glm::vec3(0,1,0)  // Head is up (set to 0,-1,0 to look upside-down)
                );
    // Model matrix : an identity matrix (model will be at the origin)
    Model      = glm::mat4(1.0f);
    // Our ModelViewProjection : multiplication of our 3 matrices
    MVP        = Projection * View * Model; // Remember, matrix multiplication is the other way around

    int colorLoc = glGetUniformLocation(shader, "objectColor");
    glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);

    // Load the model that we want to use and buffer the vertex attributes
    GeometryData geometry;
    geometry.loadFromOBJFile("objects/teapot.obj");
    vertexCount = geometry.vertexCount();
    cout << vertexCount << endl;

    int vertexLoc = glGetAttribLocation(shader, "position");

    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * sizeof(float), geometry.vertexData(), GL_STATIC_DRAW);
    glVertexAttribPointer(vertexLoc, 3, GL_FLOAT, false, 0, 0);
    glEnableVertexAttribArray(vertexLoc);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glPrintError("Setup complete", true);
}

void OpenGLWindow::render()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glUseProgram(shader);

    // send transformation to shader
    glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

    glDrawArrays(GL_TRIANGLES, 0, vertexCount);

    // Swap the front and back buffers on the window, effectively putting what we just "drew"
    // onto the screen (whereas previously it only existed in memory)
    SDL_GL_SwapWindow(sdlWin);
}

void OpenGLWindow::changeAxis() {
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

// The program will exit if this function returns false
bool OpenGLWindow::handleEvent(SDL_Event e)
{
    // A list of keycode constants is available here: https://wiki.libsdl.org/SDL_Keycode
    // Note that SDL provides both Scancodes (which correspond to physical positions on the keyboard)
    // and Keycodes (which correspond to symbols on the keyboard, and might differ across layouts)
    if(e.type == SDL_KEYDOWN)
    {
        if(e.key.keysym.sym == SDLK_ESCAPE)
        {
            return false;
        }

        if(e.key.keysym.sym == SDLK_a) // scale all
        {
            transformationMode = SCALEALL;
            cout << "Scaling object uniformly." << endl;
        }

        if(e.key.keysym.sym == SDLK_s) // scale
        {
            if (transformationMode == SCALE) {
                changeAxis();
            }
            else {
                cout << "Scaling object on individual axis." << endl;
                transformationMode = SCALE;
                transformationAxis = X;
            }   

            return true;
        }

        if(e.key.keysym.sym == SDLK_t) // translate
        {
            if (transformationMode == TRANSLATE) {
                changeAxis();
            }
            else {
                cout << "Translating object." << endl;
                transformationMode = TRANSLATE;
                transformationAxis = X;
            }            
            return true;
        }
        
        if(e.key.keysym.sym == SDLK_r) // rotate
        {
            if (transformationMode == ROTATE) {
                changeAxis();
            }
            else {
                cout << "Rotating object." << endl;
                transformationMode = ROTATE;
                transformationAxis = X;
            }        
            return true;
        }

        if(e.key.keysym.sym == SDLK_v)
        {
            if (transformationMode != VIEW) {
                cout << "Viewing object." << endl;
                transformationMode = VIEW;
            }               
        }

    }
    else if (e.type == SDL_MOUSEMOTION) {
        // figure out if movement is up or down, and change sign accordingly
        int sign = 0;
        if (e.motion.yrel < 0)
            sign = 1;
        else
            sign = -1;
        if (transformationMode == ROTATE) {
            if (transformationAxis == X) {
                glm::mat4 trans;
                trans = glm::rotate(trans, glm::radians(sign * 5.0f), glm::vec3(1.0, 0.0, 0.0));
                Model *= trans;
            }
            else if (transformationAxis == Y) {
                glm::mat4 trans;
                trans = glm::rotate(trans, glm::radians(sign * 5.0f), glm::vec3(0.0, 1.0, 0.0));
                Model *= trans;                
            }
            else if (transformationAxis == Z) {
                glm::mat4 trans;
                trans = glm::rotate(trans, glm::radians(sign * 5.0f), glm::vec3(0.0, 0.0, 1.0));
                Model *= trans;                
            }
            MVP = Projection * View * Model;            
            return true;
        }

        else if (transformationMode == SCALE) {
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

        else if (transformationMode == SCALEALL) {
            glm::mat4 trans;
            trans = glm::scale(trans, glm::vec3(1.0 + (sign * 0.1), 1.0 + (sign * 0.1), 1.0 + (sign * 0.1)));
            Model *= trans;   
            MVP = Projection * View * Model;            
            return true;
        } 

        else if (transformationMode == TRANSLATE) {
            if (transformationAxis == X) {
                glm::mat4 trans;
                trans = glm::translate(trans, glm::vec3(sign * 0.1, 0.0, 0.0));
                Model *= trans;
            }
            else if (transformationAxis == Y) {
                glm::mat4 trans;
                trans = glm::translate(trans, glm::vec3(0.0, sign * 0.1, 0.0));
                Model *= trans;                
            }
            else if (transformationAxis == Z) {
                glm::mat4 trans;
                trans = glm::translate(trans, glm::vec3(0.0, 0.0, sign * 0.1));
                Model *= trans;                
            }
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