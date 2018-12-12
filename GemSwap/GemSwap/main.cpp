
#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>       /* time */
#include <vector>
#include <iostream>
#include <cstdlib>
#include <string>



#if defined(__APPLE__)
#include <GLUT/GLUT.h>
#include <OpenGL/gl3.h>
#include <OpenGL/glu.h>
#else
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#endif
#include <GL/glew.h>		// must be downloaded
#include <GL/freeglut.h>	// must be downloaded unless you have an Apple
#endif

int windowWidth = 512, windowHeight = 512;


// OpenGL major and minor versions
int majorVersion = 3, minorVersion = 0;


// row-major matrix 4x4
struct mat4
{
    float m[4][4];
public:
    mat4() {}
    mat4(float m00, float m01, float m02, float m03,
         float m10, float m11, float m12, float m13,
         float m20, float m21, float m22, float m23,
         float m30, float m31, float m32, float m33)
    {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
        m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
    }
    
    mat4 operator*(const mat4& right)
    {
        mat4 result;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                result.m[i][j] = 0;
                for (int k = 0; k < 4; k++) result.m[i][j] += m[i][k] * right.m[k][j];
            }
        }
        return result;
    }
    operator float*() { return &m[0][0]; }
};


// 3D point in homogeneous coordinates
struct vec4
{
    float v[4];
    
    vec4(float x = 0, float y = 0, float z = 0, float w = 1)
    {
        v[0] = x; v[1] = y; v[2] = z; v[3] = w;
    }
    
    vec4 operator*(const mat4& mat)
    {
        vec4 result;
        for (int j = 0; j < 4; j++)
        {
            result.v[j] = 0;
            for (int i = 0; i < 4; i++) result.v[j] += v[i] * mat.m[i][j];
        }
        return result;
    }
    
    vec4 operator+(const vec4& vec)
    {
        vec4 result(v[0] + vec.v[0], v[1] + vec.v[1], v[2] + vec.v[2], v[3] + vec.v[3]);
        return result;
    }
};

// 2D point in Cartesian coordinates
struct vec2
{
    float x, y;
    
    vec2(float x = 0.0, float y = 0.0) : x(x), y(y) {}
    
    vec2 operator+(const vec2& v)
    {
        return vec2(x + v.x, y + v.y);
    }
};

double time_glob = 0;
double move_time = 0.5;
double remove_time = 1.0;



class GameObject {
    int type;
    vec2 position;
    float orientation;
    float rotation_rate;
    bool in_grid;
    bool in_motion;
    
public:
    double scaling;
    double removal_start_t;
    double removal_end_t;


    GameObject(int _type, vec2 _position, float _orientation, float _rotation_rate) {
        type = _type;
        orientation = _orientation;
        position = _position;
        rotation_rate = _rotation_rate;
        in_grid = false;
        in_motion = false;
        scaling = 1.0;
    }
    
    int getType() {
        return type;
    }
    
    vec2 getPosition() {
        return position;
    }
    
    void setPosition(vec2 _position) {
        position = _position;
        in_motion = true;
    }
    
    bool isInMotion() {
        return in_motion;
    }
    
    void stopMotion() {
        in_motion = false;
    }
    
    float getOrientation() {
        return orientation;
    }
    
    float getRotationRate() {
        return rotation_rate;
    }
    void set_in_grid(bool _in_grid) {
        in_grid = _in_grid;
        if(!in_grid) {
            rotation_rate = 150;
            removal_start_t = time_glob;
            removal_end_t = time_glob + remove_time;
        }
    }
    bool is_in_grid() {
        return in_grid;
    }
};




class Camera
{
    vec2 center;
    vec2 size;
    double orientation;
public:
    Camera(vec2 center1, vec2 size1, double _orientation) {
        center = center1;
        size = size1;
        orientation = _orientation;
    }
    mat4 GetViewTransformationMatrix() {
        mat4 C = mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -center.x, -center.y, 0, 1);
        mat4 S = mat4(1/size.x, 0, 0, 0, 0, 1/size.y, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
        
        float alpha = (orientation) / 180.0 * M_PI;
        
        mat4 R = mat4(
                      cos(alpha), sin(alpha), 0.0, 0.0,
                      -sin(alpha), cos(alpha), 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      0.0, 0.0, 0.0, 1.0);
        return C*S*R;
    }
    void SetOrientation(double _orientation) {
        orientation = _orientation;
    }
};

Camera camera = Camera(vec2(0,0), vec2(1.0,1.0), 0);

// handle of the shader program
class Shader {
protected:
    unsigned int shaderProgram;
    
public:
    Shader() {

    }
    
    
    ~Shader() {
        glDeleteProgram(shaderProgram);
        
    }
    
    void Run() {
        glUseProgram(shaderProgram);
    }
    
    virtual void UploadSamplerID() = 0;
    
    virtual void UploadColor(vec4& color) = 0;
    
    void CompileProgram(const char *vertexSource, const char *fragmentSource) {
        // create vertex shader from string
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        if (!vertexShader) { printf("Error in vertex shader creation\n"); exit(1); }
        
        glShaderSource(vertexShader, 1, &vertexSource, NULL);
        glCompileShader(vertexShader);
        checkShader(vertexShader, "Vertex shader error");
        
        // create fragment shader from string
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        if (!fragmentShader) { printf("Error in fragment shader creation\n"); exit(1); }
        
        glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
        glCompileShader(fragmentShader);
        checkShader(fragmentShader, "Fragment shader error");
        
        // attach shaders to a single program
        shaderProgram = glCreateProgram();
        if (!shaderProgram) { printf("Error in shader program creation\n"); exit(1); }
        
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
    }
    
    void LinkProgram() {
        glLinkProgram(shaderProgram);
        checkLinking(shaderProgram);
    }
    
    void UploadM(mat4& M)
    {
        int location = glGetUniformLocation(shaderProgram, "M");
        if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, M);
        else printf("uniform M cannot be set\n");
    }
    
    
    void getErrorInfo(unsigned int handle)
    {
        int logLen;
        glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 0)
        {
            char * log = new char[logLen];
            int written;
            glGetShaderInfoLog(handle, logLen, &written, log);
            printf("Shader log:\n%s", log);
            delete[] log;
        }
    }
    
    // check if shader could be compiled
    void checkShader(unsigned int shader, char * message)
    {
        int OK;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &OK);
        if (!OK)
        {
            printf("%s!\n", message);
            getErrorInfo(shader);
        }
    }
    
    // check if shader could be linked
    void checkLinking(unsigned int program)
    {
        int OK;
        glGetProgramiv(program, GL_LINK_STATUS, &OK);
        if (!OK)
        {
            printf("Failed to link shader program!\n");
            getErrorInfo(program);
        }
    }
};

class StandardShader : public Shader {
public:
    StandardShader()
    {
        // vertex shader in GLSL
        const char *vertexSource = R"(
#version 150
        precision highp float;
        in vec2 vertexPosition;
        uniform vec3 vertexColor;
        uniform mat4 M;
        out vec3 color;
        void main()
        {
            color = vertexColor;
            gl_Position = vec4(vertexPosition.x,
                               vertexPosition.y, 0, 1) * M;
        }
        )";
        
        // fragment shader in GLSL
        const char *fragmentSource = R"(
#version 150
        precision highp float;
        
        in vec3 color;			// variable input: interpolated from the vertex colors
        out vec4 fragmentColor;		// output that goes to the raster memory as told by glBindFragDataLocation
        
        void main()
        {
            fragmentColor = vec4(color, 1); // extend RGB to RGBA
        }
        )";
        
        CompileProgram(vertexSource, fragmentSource);
        
        glBindAttribLocation(shaderProgram, 0, "vertexPosition");
        glBindFragDataLocation(shaderProgram, 0, "fragmentColor");
        
        LinkProgram();
    }
    
    void UploadSamplerID() {
    
    }
    
    void UploadColor(vec4& color) {
        int location = glGetUniformLocation(shaderProgram, "vertexColor");
        if (location >= 0) glUniform3fv(location, 1, &color.v[0]); // set uniform variable vertexColor
        else printf("uniform vertex color cannot be set\n");
    }
};

class TexturedShader : public Shader {
public:
    TexturedShader()
    {
        // vertex shader in GLSL
        const char *vertexSource = R"(
#version 150
        precision highp float;
        
        in vec2 vertexPosition;
        in vec2 vertexTexCoord;
        uniform mat4 M;
        out vec2 texCoord;
        
        void main()
        {
            texCoord = vertexTexCoord;
            gl_Position = vec4(vertexPosition.x, vertexPosition.y, 0, 1) * M;
        }
        )";
        
        // fragment shader in GLSL
        const char *fragmentSource = R"(
#version 150
        precision highp float;
        
        uniform sampler2D samplerUnit;
        in vec2 texCoord;
        out vec4 fragmentColor;
        
        void main()
        {
            fragmentColor = texture(samplerUnit, texCoord);
        }
        )";

        
        CompileProgram(vertexSource, fragmentSource);

        glBindAttribLocation(shaderProgram, 0, "vertexPosition");
        glBindAttribLocation(shaderProgram, 1, "vertexTexCoord");
        glBindFragDataLocation(shaderProgram, 0, "fragmentColor");
        
        LinkProgram();
    }
    
    void UploadSamplerID()
    {
        int samplerUnit = 0;
        int location = glGetUniformLocation(shaderProgram, "samplerUnit");
        glUniform1i(location, samplerUnit);
        glActiveTexture(GL_TEXTURE0 + samplerUnit); 
    }
    
    void UploadColor(vec4& color) {

    }
};



class Geometry
{
protected:
    unsigned int vao;
public:
    Geometry() {
        glGenVertexArrays(1, &vao);
    }
    virtual void Draw() = 0;
    
};



class Star: public Geometry {
    unsigned int vbo;		// vertex buffer object
    
public:
    Star()
    {
        glBindVertexArray(vao);
        
        glGenBuffers(1, &vbo);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        static float vertexCoords[36];
        
        for(int t = 0; t < 6; t+=1) {
            float r = 1.0;
            float theta = t*2.0*M_PI/5;
            vertexCoords[6*t] = 0;
            vertexCoords[6*t+1] = 0;
            vertexCoords[6*t+2] = (r*sin(theta));
            vertexCoords[6*t+3] = (r*cos(theta));
            vertexCoords[6*t+4] = (r/3*sin(theta+M_PI/5));
            vertexCoords[6*t+5] = (r/3*cos(theta+M_PI/5));
        }
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }
    
    void Draw()
    {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 18);
    }

};

class Pent: public Geometry {
    unsigned int vbo;		// vertex buffer object
    
public:
    Pent()
    {
        glBindVertexArray(vao);
        
        glGenBuffers(1, &vbo);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        static float vertexCoords[30];
        
        for(int t = 0; t < 5; t+=1) {
            float r = 1.0;
            float theta = t*2.0*M_PI/5;
            vertexCoords[6*t] = 0;
            vertexCoords[6*t+1] = 0;
            vertexCoords[6*t+2] = (r*sin(theta));
            vertexCoords[6*t+3] = (r*cos(theta));
            vertexCoords[6*t+4] = (r*sin(theta+2*M_PI/5));
            vertexCoords[6*t+5] = (r*cos(theta+2*M_PI/5));
        }
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }
    
    void Draw()
    {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 18);
    }
    
};


class Heart: public Geometry {
    unsigned int vbo;		// vertex buffer object
    static const int num_of_steps = 64;
    
public:
    Heart()
    {
        glBindVertexArray(vao);
        
        glGenBuffers(1, &vbo);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        
        static const int array_length = num_of_steps*6;
        static float vertexCoords[array_length];
        
        for(int t = 0; t < num_of_steps; t+=1) {
            vertexCoords[6*t] = 0;
            vertexCoords[6*t+1] = 0;
            float theta = t*2.0*M_PI/num_of_steps;
            vertexCoords[6*t+2] = 16*pow(sin(theta), 3)/18.0;
            vertexCoords[6*t+3] = (13*cos(theta)-5*cos(2*theta)-2*cos(3*theta)-cos(4*theta))/18.0;
            theta = (t+1)*2.0*M_PI/num_of_steps;
            vertexCoords[6*t+4] = 16*pow(sin(theta), 3)/18.0;
            vertexCoords[6*t+5] = (13*cos(theta)-5*cos(2*theta)-2*cos(3*theta)-cos(4*theta))/18.0;
        }
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }
    
    void Draw()
    {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, num_of_steps*3);
    }

};

class Circle: public Geometry {
    unsigned int vbo;		// vertex buffer object
    static const int num_of_steps = 64;
    
public:
    Circle()
    {
        glBindVertexArray(vao);
        
        glGenBuffers(1, &vbo);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        
        static const int array_length = num_of_steps*6;
        static float vertexCoords[array_length];
        
        for(int t = 0; t < num_of_steps; t+=1) {
            vertexCoords[6*t] = 0;
            vertexCoords[6*t+1] = 0;
            float theta = t*2.0*M_PI/num_of_steps;
            vertexCoords[6*t+2] = 0.8*cos(theta);
            vertexCoords[6*t+3] = 0.8*sin(theta);
            theta = (t+1)*2.0*M_PI/num_of_steps;
            vertexCoords[6*t+4] = 0.8*cos(theta);
            vertexCoords[6*t+5] = 0.8*sin(theta);
        }
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }
    
    void Draw()
    {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, num_of_steps*3);
    }
    
};

class Triangle : public Geometry
{
    unsigned int vbo;		// vertex buffer object
    
public:
    Triangle() {
        
        glBindVertexArray(vao);		// make it active
        
        glGenBuffers(1, &vbo);		// generate a vertex buffer object
        
        // vertex coordinates: vbo -> Attrib Array 0 -> vertexPosition of the vertex shader
        glBindBuffer(GL_ARRAY_BUFFER, vbo); // make it active, it is an array
        static float vertexCoords[] = { -0.8, -0.8, 0.8, -0.8, 0, 0.8 };	// vertex data on the CPU
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }
    
    void Draw()
    {
        glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
        glDrawArrays(GL_TRIANGLES, 0, 3); // draw a single triangle with vertices defined in vao
    }
    
};

class Quad : public Geometry
{
    unsigned int vbo;		// vertex buffer object
    
public:
    Quad()
    {
        glBindVertexArray(vao);
        
        glGenBuffers(1, &vbo);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        static float vertexCoords[] = { -0.8, -0.8, 0.8, -0.8, -0.8, 0.8, 0.8, 0.8};
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }
    
    void Draw()
    {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    
};

class TexturedQuad : public Quad
{
    unsigned int vboTex;
    
public:
    TexturedQuad()
    {
        glBindVertexArray(vao);
        glGenBuffers(1, &vboTex);
        
        // define the texture coordinates here
        // assign the array of texture coordinates to
        // vertex attribute array 1
        glBindBuffer(GL_ARRAY_BUFFER, vboTex);
        static float vertexTextureCoords[] = { 0, 0, 1, 0, 0, 1, 1, 1};
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexTextureCoords), vertexTextureCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }
    
    void Draw()
    {
        glEnable(GL_BLEND); // necessary for transparent pixels
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(vao); 
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); 
        glDisable(GL_BLEND);
    }
};


class Diamond : public Geometry
{
    unsigned int vbo;		// vertex buffer object
    
public:
    Diamond()
    {
        glBindVertexArray(vao);
        
        glGenBuffers(1, &vbo);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        static float vertexCoords[] = { -0.6, 0, 0, -0.8, 0, 0.8, 0.6, 0};
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }
    
    void Draw()
    {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    
};

extern "C" unsigned char* stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);


class Texture {
    unsigned int textureId;
public:
    Texture(const std::string& inputFileName){
        unsigned char* data;
        int width; int height; int nComponents = 4;
        
        data = stbi_load(inputFileName.c_str(), &width, &height, &nComponents, 0);
        
        if(data == NULL) { return; }
        
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        
        delete data; 
    }
    
    void Bind()
    {
        glBindTexture(GL_TEXTURE_2D, textureId);
    }
};


class Material {
    Texture* texture;
    vec4 color1;
    vec4 color2;
    float rate;
public:
    Shader* shader;
    
public:
    Material(Shader* _shader, vec4 _color1, Texture* _texture = nullptr) {
        shader = _shader;
        color1 = _color1;
        rate = 0;
        texture = _texture;
    }
    
    Material(Shader* _shader, vec4 _color1, vec4 _color2, float _rate) {
        shader = _shader;
        color1 = _color1;
        color2 = _color2;
        rate = _rate;
        texture = nullptr;
    }
    
    
    
    void UploadAttributes() {
        if(texture) {
            shader->UploadSamplerID();
            texture->Bind();
        }
        else if(rate == 0) {
            shader->UploadColor(color1);
        }
        else {
            float theta = time_glob*rate;
            vec4 color_combo = vec4(cos(theta)*color1.v[0] +(1-cos(theta))*color2.v[0], cos(theta)*color1.v[1] +(1-cos(theta))*color2.v[1], cos(theta)*color1.v[2] +(1-cos(theta))*color2.v[2]);
            shader->UploadColor(color_combo);
        }
        
    }
};


class Mesh {
    Geometry* geometry;
public:
    Material* material;
    
public:
    Mesh(Geometry* _geometry, Material* _material) {
        geometry = _geometry;
        material = _material;
    }
    
    void Draw() {
        material->UploadAttributes();
        geometry->Draw();
    }
    
};


class Object {
    Shader* shader;
    Mesh* mesh;
    vec2 position, scaling, offset;
    bool draw_last;
    float orientation;
    float rotation_rate;
public:
    Object(Mesh* _mesh, vec2 _position, bool _draw_last, vec2 _scaling, float _orientation, float _rotation_rate) {
        mesh = _mesh;
        shader = mesh->material->shader;
        position = _position;
        draw_last = _draw_last;
        scaling = _scaling;
        orientation = _orientation;
        rotation_rate = _rotation_rate;
    }
    
    bool shouldDrawLast() {
        return draw_last;
    }
    
    void UploadAtributes() {
        // calculate T, S, R from position, scaling, and orientation
        mat4 T = mat4(
                      1.0, 0.0, 0.0, 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      position.x, position.y, 0.0, 1.0);
        
        mat4 S = mat4(
                      scaling.x, 0.0, 0.0, 0.0,
                      0.0, scaling.y, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      0.0, 0.0, 0.0, 1.0);
        
        float alpha = (orientation + time_glob * rotation_rate) / 180.0 * M_PI;
        
        mat4 R = mat4(
                      cos(alpha), sin(alpha), 0.0, 0.0,
                      -sin(alpha), cos(alpha), 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      0.0, 0.0, 0.0, 1.0);
        
        mat4 V = camera.GetViewTransformationMatrix();
        
        mat4 M = S * R * T * V;
        shader->UploadM(M);
    }
    
    void Draw() {
        shader->Run();
        UploadAtributes();
        mesh->Draw();
    }
    
};


class Movement {
public:
    vec2 cell;
    vec2 start_loc;
    vec2 end_loc;
    double start_t;
    double end_t;
    
    Movement(vec2 _cell, vec2 _start_loc, vec2 _end_loc, double _start_t, double _end_t) {
        cell = _cell;
        start_loc = _start_loc;
        end_loc = _end_loc;
        start_t = _start_t;
        end_t = _end_t;
    }
};

class Removal {
public:
    GameObject* gameObject;
    double start_t;
    double end_t;
    
    Removal(GameObject* _gameObject, double _start_t, double _end_t) {
        gameObject = _gameObject;
        start_t = _start_t;
        end_t = _end_t;
    }
};



class Scene {
    std::vector<Shader*> shaders;
    std::vector<Material*> materials;
    std::vector<Geometry*> geometries;
    std::vector<Mesh*> meshes;
    std::vector<Object*> objects;
    std::vector<GameObject> gameObjects;

public:
    std::vector<std::vector<GameObject*>> grid;
    int gem_types;
    int num_of_rows = 10;
    int num_of_cols = 10;
    
    std::vector<Movement*> movements;
    std::vector<Removal*> removals;
    
    Scene() {
        Initialize();
    }
    
    void Initialize() {
        
        
        shaders.push_back(new StandardShader());
        shaders.push_back(new TexturedShader());
        
        materials.push_back(new Material(shaders[0], vec4(1, 0, 0), vec4(1, 0.078,0.576), 8));
        materials.push_back(new Material(shaders[0], vec4(1, 1, 0)));
        materials.push_back(new Material(shaders[0], vec4(0.5, 1, 0), vec4(0,1,0.5), 4));
        materials.push_back(new Material(shaders[0], vec4(0.25, 0, 1), vec4(0,0.25,1), 5));
        materials.push_back(new Material(shaders[0], vec4(0.5, 0.25, 1), vec4(0.5, 0, 1), 6));
        materials.push_back(new Material(shaders[0], vec4(1, 0.5, 0)));
        materials.push_back(new Material(shaders[1], vec4(), new Texture("asteroidtexturepack/asteroid3.png")));
        materials.push_back(new Material(shaders[1], vec4(), new Texture("asteroidtexturepack/asteroid2.png")));

        
        geometries.push_back(new Heart());
        geometries.push_back(new Star());
        geometries.push_back(new Triangle());
        geometries.push_back(new Quad());
        geometries.push_back(new Pent());
        geometries.push_back(new Diamond());
        geometries.push_back(new TexturedQuad());
        geometries.push_back(new TexturedQuad());
        
        
        for (int i = 0; i < materials.size() && i < geometries.size(); i++) {
            meshes.push_back(new Mesh(geometries[i], materials[i]));
        }
        gem_types = meshes.size();
        
        InitializeGrid();
        
        UpdateGrid();
        
    }
    ~Scene() {
        for(int i = 0; i < materials.size(); i++) delete materials[i];
        for(int i = 0; i < geometries.size(); i++) delete geometries[i];
        for(int i = 0; i < meshes.size(); i++) delete meshes[i];
        for(int i = 0; i < objects.size(); i++) delete objects[i];
        for(int i = 0; i < shaders.size(); i++) delete shaders[i];
        
    }
    
    void InitializeGrid() {
        for(int i = 0; i < num_of_cols*num_of_rows; i++) {
            addRandomGameObject();
        }
        for(int i = 0; i < num_of_rows; i++) {
            std::vector<GameObject*> row;
            for(int j = 0; j < num_of_cols; j++) {
                row.push_back(&gameObjects[i+10*j]);
                row.at(j)->set_in_grid(true);
                vec2 cell = vec2(i,j);
                movements.push_back(new Movement(cell,grid_to_coords(vec2(i,j+10)), grid_to_coords(cell), time_glob, time_glob + 3*move_time));
                gameObjects[i+10*j].setPosition(grid_to_coords(vec2(i,j+10)));
            }
            grid.push_back(row);
        }
        //skyfall();
    }
    
    void addRandomGameObject() {
        int gem_type = rand()%gem_types;
        int rotation_rate = 0;
        if(gem_type == 1) {rotation_rate = 10;}
        if(gem_type == 6) {rotation_rate = 40;}
        if(gem_type == 7) {rotation_rate = -20;}
        gameObjects.push_back(GameObject(gem_type, vec2(0,0), 0, rotation_rate));
    }
    
    void UpdateGrid() {
        objects.clear();
        for(int i = 0; i < grid.size(); i++) {
            for(int j = 0; j < grid[0].size(); j++) {
                GameObject* gameObject = grid.at(i).at(j);
                if(gameObject != nullptr) {
                    int gem_type = gameObject->getType();
                    vec2 position = grid_to_coords(vec2(i, j));
                    bool draw_last = false;
                    if(gameObject->isInMotion()) {
                        position = gameObject->getPosition();
                        draw_last = true;
                    }
                    objects.push_back(new Object(meshes[gem_type], position, draw_last, vec2(gameObject->scaling/num_of_rows, gameObject->scaling/num_of_cols), gameObject->getOrientation(), gameObject->getRotationRate()));
                }
            }
        }
        for (int i = 0; i < removals.size(); i++) {
            GameObject* gameObject = removals[i]->gameObject;
            if(gameObject != nullptr) {
                int gem_type = gameObject->getType();
                vec2 position = gameObject->getPosition();
                objects.push_back(new Object(meshes[gem_type], position, true, vec2(gameObject->scaling/num_of_rows, gameObject->scaling/num_of_cols), gameObject->getOrientation(), gameObject->getRotationRate()));
            }
        }
    }
    
    
    vec2 coords_to_grid(vec2 loc) {
        int i = (int)((loc.x + 1)*5);
        int j = (int)((loc.y + 1)*5);
        return vec2(i,j);
    }
    
    vec2 grid_to_coords(vec2 cell) {
        double x = cell.x/5.0-0.9;
        double y = cell.y/5.0-0.9;
        return vec2(x,y);
    }
    
    
    void Draw()
    {
        // draw objects with overriden position
        std::vector<Object*> objects_to_draw_last;
        for(int i = 0; i < objects.size(); i++) {
            if(objects[i]->shouldDrawLast()) {
                objects_to_draw_last.push_back(objects[i]);
            }
            else {
                objects[i]->Draw();
            }
        }
        for(int i = 0; i < objects_to_draw_last.size(); i++) {
            objects_to_draw_last[i]->Draw();
        }
    }
    
    
    void swap(vec2 cell1, vec2 cell2) {
        GameObject* temp = grid[cell1.x][cell1.y];
        grid[cell1.x][cell1.y] = grid[cell2.x][cell2.y];
        grid[cell2.x][cell2.y] = temp;
    }
    
    bool isLegalMove(vec2 cell1, vec2 cell2) {
        swap(cell1, cell2);
        for(int i = 0; i < num_of_cols; i++) {
            for(int j = 0; j < num_of_rows; j++) {
                if(grid[i][j] != nullptr) {
                    int x = i + 1;
                    while(x < num_of_cols) {
                        if(grid[x][j] == nullptr || grid[i][j]->getType() != grid[x][j]->getType()) {
                            break;
                        }
                        x++;
                    }
                    if(x-i >= 3) {
                        swap(cell1, cell2);
                        return true;
                    }
                    int y = j + 1;
                    while(y < num_of_cols) {
                        if(grid[i][y] == nullptr || grid[i][j]->getType() != grid[i][y]->getType()) {
                            break;
                        }
                        y++;
                    }
                    if(y-j >= 3) {
                        swap(cell1, cell2);
                        return true;
                    }
                }
            }
        }
        swap(cell1, cell2);
        return false;
    }
    
    
    bool removeLines() {
        bool lines_found = false;
        for(int i = 0; i < num_of_cols; i++) {
            for(int j = 0; j < num_of_rows; j++) {
                if(grid[i][j] != nullptr) {
                    int x = i + 1;
                    while(x < num_of_cols) {
                        if(grid[x][j] == nullptr || grid[i][j]->getType() != grid[x][j]->getType()) {
                            break;
                        }
                        x++;
                    }
                    if(x-i >= 3) {
                        lines_found = true;
                        for (int a = i; a < x; a++) {
                            remove_cell(vec2(a,j));
                        }
                    }
                    int y = j + 1;
                    while(y < num_of_cols) {
                        if(grid[i][y] == nullptr || grid[i][j]->getType() != grid[i][y]->getType()) {
                            break;
                        }
                        y++;
                    }
                    if(y-j >= 3) {
                        lines_found = true;
                        for (int b = j; b < y; b++) {
                            remove_cell(vec2(i,b));
                        }
                    }
                }
            }
        }
        if(lines_found) {
            for(int i = 0; i < num_of_cols; i++) {
                for(int j = 0; j < num_of_rows; j++) {
                    if(grid[i][j] != nullptr && !grid[i][j]->is_in_grid()) {
                        grid[i][j] = nullptr;
                    }
                }
            }
        }
        return lines_found;
    }
    
    void remove_cell(vec2 cell) {
        grid[cell.x][cell.y]->setPosition(grid_to_coords(vec2(cell.x,cell.y)));
        removals.push_back(new Removal(grid[cell.x][cell.y], time_glob, time_glob+remove_time));
        grid[cell.x][cell.y]->set_in_grid(false);

    }
    
    // return -1 if no cells to destroy, 1 if last cell destroyed, and >1 otherwise
    int processRemovals() {
        int object_destroyed = 0;
        int object_shrunk = 0;
        if(removals.size() != 0) {
            for(int i = 0; i < removals.size(); i++) {
                Removal* r = removals[i];
                GameObject* gameObject = r->gameObject;
                if(gameObject != nullptr) {
                    if (time_glob >= r->start_t && time_glob <= r->end_t) {
                        double scaling = (r->end_t - time_glob)/(r->end_t - r->start_t);
                        gameObject->scaling = scaling;
                        object_shrunk = 2;
                    } else if( time_glob > r->end_t ){
                        removals.erase(removals.begin() + i);
                        i--;
                        object_destroyed = 1;
                    }
                }
            }
            return removals.size();
        }
        return -1;
    }
    
    int processMovements() {
        if(movements.size() != 0) {
            for(int i = 0; i < movements.size(); i++) {
                Movement* movement = movements[i];
                if(movement->start_t == -1) {
                    movement->start_t = time_glob;
                    movement->end_t = time_glob + move_time;
                }
                if(time_glob >= movement->start_t && time_glob <= movement->end_t) {
                    move_object(movement, time_glob);
                }
                else if(time_glob > movement->end_t) {
                    if(grid[movement->cell.x][movement->cell.y] != nullptr) {
                        stop_motion(movement->cell);
                    }
                    movements.erase(movements.begin() + i);
                    delete movement;
                }
            }
            if(movements.size() == 0) {
                removeLines();
            }
            return movements.size();
        }
        return -1;
    }
    
    void processQuake() {
        for(int i = 0; i < grid.size(); i++) {
            for(int j = 0; j < grid[0].size(); j++) {
                if(grid.at(i).at(j) != nullptr && rand()%1000 == 0 ) {
                    remove_cell(vec2(i,j));
                    grid[i][j] = nullptr;
                }
            }
        }
    }
    
    void skyfall() {
        for(int i = 0; i < num_of_cols; i++) {
            for(int j = 0; j < num_of_rows-1; j++) {
                if(grid[i][j] == nullptr) {
                    int y = j+1;
                    while (y < num_of_rows && grid[i][y] == nullptr) {
                        y++;
                    }
                    if(y < num_of_rows) {
                        swap(vec2(i,j), vec2(i,y));
                        movements.push_back(new Movement(vec2(i,j), grid_to_coords(vec2(i,y)), grid_to_coords(vec2(i,j)), -1, -1));
                        set_cell_position(vec2(i,j), grid_to_coords(vec2(i,y)));
                    }
                }
            }
        }
        fillgrid();
    }
    
    bool fillgrid() {
        bool empty_cells = false;
        for(int i = 0; i < num_of_cols; i++) {
            int null_in_row = 0;
            for(int j = 0; j < num_of_rows; j++) {
                if(grid[i][j] == nullptr) {
                    empty_cells = true;
                    addRandomGameObject();
                    grid.at(i).at(j) = &gameObjects.back();
                    grid.at(i).at(j)->set_in_grid(true);
                    movements.push_back(new Movement(vec2(i,j), grid_to_coords(vec2(i,10+null_in_row)), grid_to_coords(vec2(i,j)), -1, -1));
                    set_cell_position(vec2(i,j), grid_to_coords(vec2(i,10+null_in_row)));
                    null_in_row++;
                }
            }
        }
        return empty_cells;
    }
    
    void stop_motion(vec2 cell) {
        grid[cell.x][cell.y]->stopMotion();
    }
    
    void set_cell_position(vec2 cell, vec2 position) {
        grid[cell.x][cell.y]->setPosition(position);
    }
    
    
    bool move_object(Movement* m, double t) {
        if(grid[m->cell.x][m->cell.y] != nullptr) {
            double ratio = (t-m->start_t)/(m->end_t-m->start_t);
            set_cell_position(m->cell, vec2(m->start_loc.x + (m->end_loc.x-m->start_loc.x)*ratio, m->start_loc.y + (m->end_loc.y-m->start_loc.y)*ratio));
            return true;
        }
        return false;
    }
};



Scene* gScene;




// initialization, create an OpenGL context
void onInitialization()
{
    glViewport(0, 0, windowWidth, windowHeight);
    gScene = new Scene();
    
}

void onExit()
{
    delete gScene;
    printf("exit");
}

// window has become invalid: redraw
void onDisplay()
{
    glClearColor(0, 0, 0, 0); // background color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen
    
    gScene->Draw();
    
    glutSwapBuffers(); // exchange the two buffers
}

vec2 selected_grid_cell;
bool b_pressed, q_pressed;



void onMouse(int button, int state, int x, int y) {
    if(!q_pressed && gScene->removals.size() == 0 && gScene->movements.size() == 0) {
        double x_norm = (x/(double)windowWidth - 0.5)*2;
        double y_norm = (y/(double)windowWidth - 0.5)*-2;
        vec2 mouse_click = vec2(x_norm, y_norm);
        if(state == GLUT_DOWN) {
            selected_grid_cell = gScene->coords_to_grid(mouse_click);
            if(b_pressed) {
                gScene->remove_cell(selected_grid_cell);
                gScene->grid[selected_grid_cell.x][selected_grid_cell.y] = nullptr;
            }
        }
        else if(gScene->grid[selected_grid_cell.x][selected_grid_cell.y] != nullptr) {
            gScene->grid[selected_grid_cell.x][selected_grid_cell.y]->setPosition(vec2(-10,-10));
            vec2 to_swap_grid_cell = gScene->coords_to_grid(mouse_click);
            if(fabs(selected_grid_cell.x - to_swap_grid_cell.x)+fabs(selected_grid_cell.y - to_swap_grid_cell.y) == 1) {
                if(gScene->isLegalMove(selected_grid_cell, to_swap_grid_cell)) {
                    gScene->swap(selected_grid_cell, to_swap_grid_cell);
                    gScene->movements.push_back(new Movement(to_swap_grid_cell, mouse_click, gScene->grid_to_coords(to_swap_grid_cell), time_glob, time_glob+move_time));
                    gScene->movements.push_back(new Movement(selected_grid_cell, gScene->grid_to_coords(to_swap_grid_cell), gScene->grid_to_coords(selected_grid_cell), time_glob, time_glob+move_time));
                }
                else {
                    gScene->movements.push_back(new Movement(selected_grid_cell, mouse_click, gScene->grid_to_coords(to_swap_grid_cell), time_glob, time_glob+move_time));
                    gScene->movements.push_back(new Movement(to_swap_grid_cell, gScene->grid_to_coords(to_swap_grid_cell), gScene->grid_to_coords(selected_grid_cell), time_glob, time_glob+move_time));
                    gScene->movements.push_back(new Movement(selected_grid_cell, gScene->grid_to_coords(to_swap_grid_cell), gScene->grid_to_coords(selected_grid_cell), time_glob+move_time, time_glob+2*move_time));
                    gScene->movements.push_back(new Movement(to_swap_grid_cell, gScene->grid_to_coords(selected_grid_cell), gScene->grid_to_coords(to_swap_grid_cell), time_glob+move_time, time_glob+2*move_time));
                }
            }
            else {
                gScene->movements.push_back(new Movement(selected_grid_cell, mouse_click, gScene->grid_to_coords(selected_grid_cell), time_glob, time_glob+move_time));
            }
            gScene->UpdateGrid();
        }
        
        glutPostRedisplay();
    }
}

void onMotion(int x, int y) {
    if(!q_pressed && gScene->removals.size() == 0 && gScene->movements.size() == 0) {
        double x_norm = (x/(double)windowWidth - 0.5)*2;
        double y_norm = (y/(double)windowWidth - 0.5)*-2;
        if(gScene->grid[selected_grid_cell.x][selected_grid_cell.y] != nullptr) {
            gScene->set_cell_position(selected_grid_cell,vec2(x_norm, y_norm));
            gScene->UpdateGrid();
        }
        glutPostRedisplay();
    }
}


void onIdle() {
    
    time_glob = glutGet(GLUT_ELAPSED_TIME) * 0.001;
    
    int removal_ouptput = gScene->processRemovals();
    int movement_ouput = 0;
    if(removal_ouptput == 0) {
        gScene->skyfall();
    }
    else if(removal_ouptput == -1) {
        gScene->processMovements();
    }
    if(movement_ouput != -1 || removal_ouptput != -1) {
        gScene->UpdateGrid();
    }
    // show result
    glutPostRedisplay();
    
}


void onKeyboard(unsigned char key, int x, int y) {
    if(key == 'b') {
        b_pressed = true;
    }
    if(key == 'q') {
        q_pressed = true;
        camera.SetOrientation(5*sin(time_glob*20));
        gScene->processQuake();

    }
}

void onKeyboardUp(unsigned char key, int x, int y) {
    if(key == 'b') {
        b_pressed = false;
    }
    if(key == 'q') {
        q_pressed = false;
        camera.SetOrientation(0);
    }
}

void reshape(int width, int height) {  // GLsizei for non-negative integer
    windowWidth = width;
    windowHeight = height;
    
    // Set the viewport to cover the new window
    glViewport(0, 0, width, height);
    
    glMatrixMode(GL_PROJECTION);  // To operate on the Projection matrix
    glLoadIdentity();             // Reset
    gluOrtho2D(0.0, (GLdouble) width, 0.0, (GLdouble) height);
    
    gScene->UpdateGrid();
    glutPostRedisplay();
}


int main(int argc, char * argv[])
{
    srand(time(NULL));

    glutInit(&argc, argv);
#if !defined(__APPLE__)
    glutInitContextVersion(majorVersion, minorVersion);
#endif
    glutInitWindowSize(windowWidth, windowHeight); 	// application window is initially of resolution 512x512
    glutInitWindowPosition(50, 50);			// relative location of the application window
#if defined(__APPLE__)
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_3_2_CORE_PROFILE);  // 8 bit R,G,B,A + double buffer + depth buffer
#else
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
    glutCreateWindow("Triangle Rendering");
    
#if !defined(__APPLE__)
    glewExperimental = true;
    glewInit();
#endif
    printf("GL Vendor    : %s\n", glGetString(GL_VENDOR));
    printf("GL Renderer  : %s\n", glGetString(GL_RENDERER));
    printf("GL Version (string)  : %s\n", glGetString(GL_VERSION));
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
    printf("GL Version (integer) : %d.%d\n", majorVersion, minorVersion);
    printf("GLSL Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    
    onInitialization();
    
    glutDisplayFunc(onDisplay); // register event handlers
    glutKeyboardFunc(onKeyboard);
    glutKeyboardUpFunc(onKeyboardUp);
    glutMouseFunc(onMouse);
    glutMotionFunc(onMotion);
    glutReshapeFunc(reshape);
    glutIdleFunc(onIdle);

    
    glutMainLoop();
    onExit();
    return 1;
}

