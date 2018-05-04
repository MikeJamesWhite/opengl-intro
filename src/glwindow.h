#ifndef GL_WINDOW_H
#define GL_WINDOW_H

#include <GL/glew.h>

#include "geometry.h"

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <common/shader.hpp>

class OpenGLWindow
{
public:
    OpenGLWindow();

    void initGL();
    void render();
    bool handleEvent(SDL_Event e);
    void cleanup();

    void loadModel(std::string filename);
    void spawnNewObject();

private:
    enum Axis {X, Y, Z};
    enum Transformation {VIEW, SCALEALL, SCALE, ROTATE, TRANSLATE};

    void changeAxis();

    Transformation transformationMode = VIEW;
    Axis transformationAxis = X;

    SDL_Window* sdlWin;

    GLuint vao;
    GLuint shader;
    GLuint vertexBuffer;
    GLuint vertexBuffer2;
    GLuint vertexCount;
    GLuint vertexCount2;
    GLuint MatrixID;
    int colorLoc;

    bool partyMode = false;
    bool spawnedSecondObj = false;

    glm::vec3 translation;
	glm::mat4 Model;
	glm::mat4 View;
	glm::mat4 Projection;
	glm::mat4 MVP;
};

#endif