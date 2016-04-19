// curve2dmap is to map curved screen image into the projector
// 4/15/2016 by Yang Yu (yuy@janelia.hhmi.org)
//

// to compile on Mac OS:
// g++ -o curvedproj curvedproj.cc -framework OpenGL -lGLEW -lglfw

//
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>
#include <limits>
using namespace std;

//
#include <GL/glew.h>

//
#include <GLFW/glfw3.h>
GLFWwindow* window;

#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/string_cast.hpp"
using namespace glm;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

// input
const size_t dimx = 1440;
const size_t dimy = 360;

// output
const size_t width = 608;
const size_t height = 684;

// deformation RG16


// Mouse position
static double xpos = 0, ypos = 0;

// check for shader compiler errors
bool check_shader_compile_status(GLuint obj) {
    GLint status;
    glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE) {
        GLint length;
        glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(length);
        glGetShaderInfoLog(obj, length, &length, &log[0]);
        std::cerr << &log[0];
        return false;
    }
    return true;
}

// check for shader linker error
bool check_program_link_status(GLuint obj) {
    GLint status;
    glGetProgramiv(obj, GL_LINK_STATUS, &status);
    if(status == GL_FALSE) {
        GLint length;
        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(length);
        glGetProgramInfoLog(obj, length, &length, &log[0]);
        std::cerr << &log[0];
        return false;
    }
    return true;
}

// check for glfw error
static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

// glfw key callback
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// glfw cursor callback
static void cursorPos_callback(GLFWwindow* window, double x, double y)
{
    int wnd_width, wnd_height, fb_width, fb_height;
    double scale;
    
    glfwGetWindowSize(window, &wnd_width, &wnd_height);
    glfwGetFramebufferSize(window, &fb_width, &fb_height);
    
    std::cout<<"size "<<wnd_width<<" "<<wnd_height<<std::endl;
    std::cout<<"size "<<fb_width<<" "<<fb_height<<std::endl;
    
//    scale = (double) fb_width / (double) wnd_width;
//    
//    x *= scale;
//    y *= scale;
    
    // Remember cursor position
    xpos = x;
    ypos = y;
}

static void mouseButton_callback(GLFWwindow* window, int button, int action, int mods)
{
    if ((button == GLFW_MOUSE_BUTTON_LEFT) && action == GLFW_PRESS)
    {
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
    }
}

//
const char* vertex_shader =
"attribute vec2 vPos;"
"uniform mat4 MVP;"
"void main () {"
"  gl_Position = MVP * vec4(vPos, 0.0, 1.0);"
"}";

//
const char* fragment_shader =
"uniform vec4 color;"
"void main () {"
"  gl_FragColor = color;\n"
"}";

//
GLuint fb[2] = {std::numeric_limits<GLuint>::max(), std::numeric_limits<GLuint>::max()}; //framebuffers
GLuint rb[2] = {std::numeric_limits<GLuint>::max(), std::numeric_limits<GLuint>::max()}; //renderbuffers, color and depth
GLuint textures[2] = {std::numeric_limits<GLuint>::max(), std::numeric_limits<GLuint>::max()};

const int PJTEX = 0; // project texture
const int DMTEX = 1; // deformation texture

//
// Quadrilateral class
//  d_______c
//   |      |
//   |      |
//  a|______|b
//
class Quad
{
public:
    void add(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d)
    {
        positions.push_back(a);
        positions.push_back(b);
        positions.push_back(c);
        
        positions.push_back(a);
        positions.push_back(c);
        positions.push_back(d);
    }
    
    void setColor(glm::vec4 c)
    {
        color = c;
    }
    
public:
    std::vector<glm::vec2> positions;
    glm::vec4 color;
};

std::vector<Quad> rectangles;

//
// main func
//
int main(int argc, char *argv[])
{
    //
    //----- init
    //
    
    // error check
    glfwSetErrorCallback(error_callback);

    // Init GLFW
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        getchar();
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    
    // Open a window and create its OpenGL context
    //window = glfwCreateWindow( width, height, "project image", NULL, NULL);
    window = glfwCreateWindow( dimx, dimy, "project image", NULL, NULL);
    if( window == NULL ){
        fprintf( stderr, "Failed to open GLFW window.\n" );
        getchar();
        glfwTerminate();
        return -1;
    }
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursorPos_callback);
    glfwSetMouseButtonCallback(window, mouseButton_callback);
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetCursorPos(window, width/2, height/2);
    
    // Init GLEW
    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        getchar();
        glfwTerminate();
        return -1;
    }
    
    //
    // projection matrix
    glm::mat4 projectionMatrix = glm::ortho(0.0f,(float)dimx,(float)dimy,0.0f);
    glm::mat4 viewMatrix =  glm::mat4(1.0f);
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    
    glm::mat4 mvp = projectionMatrix * viewMatrix * modelMatrix;
    
    //
    //---- create input images
    //
    
    // Example Data Generated
    Quad rect;
    rect.add(glm::vec2(850,0), glm::vec2(900,0), glm::vec2(900,300), glm::vec2(850,300));
    rect.setColor(glm::vec4(0,0,0,0));

    //
    GLuint vs;
    GLuint fs;
    GLuint shaderProgram;
    GLuint *tex;
  
    // Create the shaders
    vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex_shader, NULL);
    glCompileShader(vs);
    if(check_shader_compile_status(vs)==false)
    {
        std::cout<<"Fail to compile vertex shader"<<std::endl;
        return -1;
    }
    
    fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_shader, NULL);
    glCompileShader(fs);
    if(check_shader_compile_status(fs)==false)
    {
        std::cout<<"Fail to compile fragment shader"<<std::endl;
        return -1;
    }
    
    //
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, fs);
    glAttachShader(shaderProgram, vs);
    glLinkProgram(shaderProgram);
    
    GLuint mvp_location = glGetUniformLocation(shaderProgram, "MVP");
    GLuint pos_location = glGetAttribLocation(shaderProgram, "vPos");
    GLuint col_location = glGetUniformLocation(shaderProgram, "color");
    
    glUniform4fv(col_location, 1, glm::value_ptr(rect.color));
    
    // vao
    GLuint vao, vbo;
    
    glGenVertexArrays(1, &vao);
    glBindVertexArray( vao );
    
    glGenBuffers(1, &vbo);
    glBindBuffer( GL_ARRAY_BUFFER, vbo );
    glBufferData( GL_ARRAY_BUFFER, sizeof(glm::vec2)*rect.positions.size(), &(rect.positions[0]), GL_STATIC_DRAW );
    
    glEnableVertexAttribArray( pos_location );
    glVertexAttribPointer( pos_location, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(0) );
    
    glBindVertexArray(0);
    
    // fb
    GLuint fb, db;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    
    //
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, dimx, dimy, 0, GL_RED, GL_FLOAT, 0);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    glBindRenderbuffer(GL_RENDERBUFFER, db);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, dimx, dimy);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, db);
    
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    //
    //---- load deformation
    //
    
    //
    float * deform_img = NULL;
    unsigned char *deformR = NULL, *deformG = NULL;
    int w=0,h=0,comp=0;
    
    deformR = stbi_load("transformation/deformr.png", &w, &h, &comp, 0);
    stbi_image_free(deformR);
    deformG = stbi_load("transformation/deformg.png", &w, &h, &comp, 0);
    stbi_image_free(deformG);
    
    std::cout <<" size "<<w<<" "<<h<<" "<<comp<<std::endl;
    
    try
    {
        deform_img = new float [w*h*2];
        
        unsigned short *pR = (unsigned short *)(deformR);
        unsigned short *pG = (unsigned short *)(deformG);
        
        int offc = w*h;
        
        for(int j=0; j<h; j++)
        {
            int offy = j*w;
            
            for(int i=0; i<w; i++)
            {
                int idx = offy+i;
                
                deform_img[idx] = (float(pR[idx]) - 1.0)*0.1;
                deform_img[idx+offc] = (float(pG[idx]) - 1.0)*0.1;
            }
        }
        
    }
    catch(...)
    {
        std::cout<<"Fail to allocate memory for deformation"<<std::endl;
        return -1;
    }
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, width, height, 0, GL_RG, GL_FLOAT, deform_img);
    //glUniform1i(glGetUniformLocation(shaderProgram, "tex1"), 1);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    //
    //---- Warp
    //
    while (!glfwWindowShouldClose (window)) {
        
        // 1st Pass: render an input image into an input framebuffer
        glViewport(0, 0, dimx, dimy);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
        
        // Enable depth test
        //glEnable(GL_DEPTH_TEST);
        // Accept fragment if it closer to the camera than the former one
        //glDepthFunc(GL_LESS);
        
        // Cull triangles which normal is not towards the camera
        //glEnable(GL_CULL_FACE);
        
        glUseProgram(shaderProgram);
        
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
        
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        
        
        
        
        // load deformation into deform texture (sampler2D)
        
        
        // Set the mouse at the center of the screen
        glfwSwapBuffers(window);
        glfwPollEvents();
        
    }
    
    //
    //---- save output image
    //
    
    
    // Close OpenGL window and terminate GLFW
    glfwTerminate();
    exit(EXIT_SUCCESS);
}

