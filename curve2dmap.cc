// curve2dmap is to map curved screen image into the projector
// 4/15/2016 by Yang Yu (yuy@janelia.hhmi.org)
//

// to compile on Mac OS:
// g++ -o curvedproj curvedproj.cc -framework OpenGL -lGLEW -lglfw

//
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
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

// deformation RG32F
string deformFile = "transformation/deform.bin";
char outFile[] = "result/output.bin";

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
    
    scale = (double) fb_width / (double) wnd_width;
    
    x *= scale;
    y *= scale;
    
    // cursor position
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

int loadDeform(char *p, string fn)
{
    ifstream file (fn, ios::in|ios::binary|ios::ate);
    if (file.is_open())
    {
        size_t size = file.tellg();
        
        try
        {
            p = new char [size];
        }
        catch(...)
        {
            std::cout<<"Fail to allocate memory for deformation"<<std::endl;
            return -1;
        }

        file.seekg (0, ios::beg);
        file.read (p, size);
        file.close();
    }
    
    return 0;
}

// draw input image
const char* vertexShader =
"attribute vec2 vPos;"
"uniform mat4 MVP;"
"void main () {"
"  gl_Position = MVP * vec4(vPos, 0.0, 1.0);"
"}";

const char* fragmentShader =
"uniform vec4 color;"
"void main () {"
"  gl_FragColor = color;\n"
"}";

// render to screen
const char* vsScreen =
"attribute vec2 vPos;"
"varying vec2 texcoord;"
"void main () {"
"  gl_Position = vec4(vPos, 0.0, 1.0);"
"  texcoord = (vPos+vec2(1,1))/2.0;"
"}";

const char* fsScreen =
"uniform vec4 color;"
"uniform sampler2D tex0;"
"varying vec2 texcoord;"
"void main () {"
"  gl_FragColor = texture2D(tex0, texcoord);\n"
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
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE); // fixed window
    
    // Open a window and create its OpenGL context
    //window = glfwCreateWindow( width, height, "project image", NULL, NULL);
    window = glfwCreateWindow( dimx/2, dimy/2, "project image", NULL, NULL);
    if( window == NULL ){
        fprintf( stderr, "Failed to open GLFW window.\n" );
        getchar();
        glfwTerminate();
        return -1;
    }
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursorPos_callback);
    glfwSetMouseButtonCallback(window, mouseButton_callback);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    //glfwSetCursorPos(window, width/2, height/2);
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

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
    GLenum g_drawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    
    //
    //---- create input images
    //
    
    // Example Data Generated
    Quad rect;
    rect.add(glm::vec2(650,60), glm::vec2(700,60), glm::vec2(700,300), glm::vec2(650,300));
    rect.setColor(glm::vec4(0,0,0,0));

    //
    GLuint vs;
    GLuint fs;
    GLuint shaderProgram;
  
    // Create the shaders
    vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShader, NULL);
    glCompileShader(vs);
    if(check_shader_compile_status(vs)==false)
    {
        std::cout<<"Fail to compile vertex shader"<<std::endl;
        return -1;
    }
    
    fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShader, NULL);
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
    GLuint vao=0, vbo=0;
    
    glGenVertexArrays(1, &vao);
    glBindVertexArray( vao );
    
    glGenBuffers(1, &vbo);
    glBindBuffer( GL_ARRAY_BUFFER, vbo );
    glBufferData( GL_ARRAY_BUFFER, sizeof(glm::vec2)*rect.positions.size(), &(rect.positions[0]), GL_STATIC_DRAW );
    
    glEnableVertexAttribArray( pos_location );
    glVertexAttribPointer( pos_location, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(0) );
    
    glBindVertexArray(0);
    
    // fb
    GLuint fb=0, db=0;
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[PJTEX]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, dimx, dimy, 0, GL_RED, GL_FLOAT, 0);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[PJTEX], 0);
    
    glGenRenderbuffers(1, &db);
    glBindRenderbuffer(GL_RENDERBUFFER, db);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, dimx, dimy);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, db);
    
    glDrawBuffers(1, g_drawBuffers);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    //
    //---- load deformation
    //
    
    //
    float * deform_img = NULL;
    loadDeform((char*)(deform_img), deformFile);
    
//    glActiveTexture(GL_TEXTURE1);
//    glBindTexture(GL_TEXTURE_2D, textures[DMTEX]);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, width, height, 0, GL_RG, GL_FLOAT, deform_img);
//    //glUniform1i(glGetUniformLocation(shaderProgram, "tex1"), 1);
//    
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    //
    //---- screen
    //
    
    //glReadBuffer(GL_NONE);
    //glDrawBuffer(GL_COLOR_ATTACHMENT0);
    
    //
    GLuint vsScn;
    GLuint fsScn;
    GLuint spScn;
    
    // Create the shaders
    vsScn = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vsScn, 1, &vsScreen, NULL);
    glCompileShader(vsScn);
    if(check_shader_compile_status(vsScn)==false)
    {
        std::cout<<"Fail to compile screen vertex shader"<<std::endl;
        return -1;
    }
    
    fsScn = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fsScn, 1, &fsScreen, NULL);
    glCompileShader(fsScn);
    if(check_shader_compile_status(fsScn)==false)
    {
        std::cout<<"Fail to compile screen fragment shader"<<std::endl;
        return -1;
    }
    
    //
    spScn = glCreateProgram();
    glAttachShader(spScn, fs);
    glAttachShader(spScn, vs);
    glLinkProgram(spScn);
    
    GLuint pos_loc = glGetAttribLocation(spScn, "vPos");
    GLuint tex_loc  = glGetUniformLocation(spScn, "tex0");

    // The fullscreen quad
    static const GLfloat g_quad_vertex_buffer_data[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f,  1.0f,
        -1.0f,  1.0f,
        1.0f, -1.0f,
        1.0f,  1.0f,
    };
    
    GLuint quad_vertexbuffer;
    glGenBuffers(1, &quad_vertexbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_quad_vertex_buffer_data), g_quad_vertex_buffer_data, GL_STATIC_DRAW);
    
    //
    //---- Warp
    //
    while (!glfwWindowShouldClose (window)) {
        
        // 1st Pass: render an input image into an input framebuffer

        //
        glEnable(GL_TEXTURE_2D);
        //glEnable(GL_DEPTH_TEST);
        //glDepthFunc(GL_LESS);
        //glEnable(GL_CULL_FACE);

        //
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
        
        // render to texture
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glViewport(0, 0, dimx, dimy);
        glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shaderProgram);
        
//        glActiveTexture(GL_TEXTURE0);
//        glBindTexture(GL_TEXTURE_2D, textures[PJTEX]);
//        glUniform1i(tex_loc, 0);
        
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        
        // Render to screen
//        glBindFramebuffer(GL_FRAMEBUFFER, 0);
//        glViewport(0, 0, dimx, dimy);
//        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//        
//        //
//        glUseProgram(spScn);
//        glActiveTexture(GL_TEXTURE0);
//        glBindTexture(GL_TEXTURE_2D, textures[PJTEX]);
//        glUniform1i(tex_loc, 0);
//        
//        //
//        glEnableVertexAttribArray(0);
//        glBindBuffer(GL_ARRAY_BUFFER, quad_vertexbuffer);
//        glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
//        glDrawArrays(GL_TRIANGLES, 0, 6);
//        glDisableVertexAttribArray(0);
        
        
        // load deformation into deform texture (sampler2D)
        
        
        // Set the mouse at the center of the screen
        glfwSwapBuffers(window);
        glfwPollEvents();
        
    }
    
    //
    //---- save output image
    //
    GLfloat *texData = new GLfloat[dimx*dimy];
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_R32F, GL_FLOAT, texData);
    
    //
    FILE *fp=NULL;
    fp = fopen(outFile, "w");
    
    if (fp == NULL) {
        std::cout<<"Can't open output file %s!\n";
        return -1;
    }
    
    for(size_t i=0; i<dimx*dimy; i++)
        fprintf(fp, "%f", texData[i]);
    
    fclose(fp);
    
    
    // Delete allocated resources
    glDeleteProgram(shaderProgram);
    glDeleteShader(fs);
    glDeleteShader(vs);
    
    glDeleteProgram(spScn);
    glDeleteShader(fsScn);
    glDeleteShader(vsScn);
    
    glDeleteTextures(2, textures);
    glDeleteFramebuffers(1, &fb);
    glDeleteRenderbuffers(1, &db);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    
    if(deform_img)
    {
        delete []deform_img;
        deform_img = NULL;
    }
    
    
    // Close OpenGL window and terminate GLFW
    glfwTerminate();
    exit(EXIT_SUCCESS);
}

