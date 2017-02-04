#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <chrono>
#include <thread>
#include <string>

//
// Begin Utility functions
//

inline void CheckOpenGLError(const char* stmt, const char* fname, int line)
{
    GLenum err = glGetError();
    //  const GLubyte* sError = gluErrorString(err);

    if (err != GL_NO_ERROR){
        printf("OpenGL error %08x, at %s:%i - for %s.\n", err, fname, line, stmt);
        exit(1);
    }
}

// GL Check Macro. Will terminate the program if a GL error is detected. 
#define GL_C(stmt) do {					\
	stmt;						\
	CheckOpenGLError(#stmt, __FILE__, __LINE__);	\
    } while (0)

inline char* GetShaderLogInfo(GLuint shader) {
    GLint len;
    GLsizei actualLen;

    GL_C(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len));
    char* infoLog = new char[len];

    GL_C(glGetShaderInfoLog(shader, len, &actualLen, infoLog));

    return  infoLog;
}

inline GLuint CreateShaderFromString(const std::string& shaderSource, const GLenum shaderType) {
    std::string src = shaderSource;

    GLuint shader;

    GL_C(shader = glCreateShader(shaderType));
    const char *c_str = src.c_str();
    GL_C(glShaderSource(shader, 1, &c_str, NULL));
    GL_C(glCompileShader(shader));

    GLint compileStatus;
    GL_C(glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus));

    if (compileStatus != GL_TRUE) {
        printf("Could not compile shader\n\n%s \n\n%s\n", src.c_str(),
            GetShaderLogInfo(shader));
        exit(1);
    }

    return shader;
}

/*
Load shader with only vertex and fragment shader.
*/
inline GLuint LoadNormalShader(const std::string& vsSource, const std::string& fsShader){
    // Create the shaders
    GLuint vs = CreateShaderFromString(vsSource, GL_VERTEX_SHADER);
    GLuint fs = CreateShaderFromString(fsShader, GL_FRAGMENT_SHADER);

    // Link the program
    GLuint shader = glCreateProgram();
    glAttachShader(shader, vs);
    glAttachShader(shader, fs);
    glLinkProgram(shader);


    GLint Result;
    glGetProgramiv(shader, GL_LINK_STATUS, &Result);
    if (Result == GL_FALSE) {
        printf("Could not link shader \n\n%s\n", GetShaderLogInfo(shader));
        exit(1);
    }

    glDetachShader(shader, vs);
    glDetachShader(shader, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return shader;
}

//
// End Utility functions
//
// Now the actual demo starts. 


const int WINDOW_WIDTH = 1497;
const int WINDOW_HEIGHT = 1014;

GLFWwindow* window;
GLuint displayShader;
GLuint fractalShader;
GLuint blurShader;
GLuint fractalTexture; // we will be writing and loading from this texture with image/load feature. 
int fbWidth, fbHeight; // frame buffer dimensions. 
GLuint vao;
float totalTime = 0.0f; // keep track of time for shader.
int FRAME_RATE = 60;

void InitGlfw() {
    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    // we need opengl 4.2 for image load store. 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Image Load Store Demo", NULL, NULL);
    if (!window) {
        glfwTerminate();
    }
    glfwMakeContextCurrent(window);

    // load GLAD.
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // Bind and create VAO, otherwise, we can't do anything in OpenGL.
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

    //
    // create for image load/store usage.
    //
    GL_C(glGenTextures(1, &fractalTexture));
    GL_C(glBindTexture(GL_TEXTURE_2D, fractalTexture));
    // We must appearently use glTexStorage2D to set texture format, when using image load/store.
    // The traditional 'glTexImage2D' absolutely won't work for some reason.
    // We specify GL_RGBA8UI, so we get RGBA, with every channel an unsigned byte. 
    // so every color fits in an unsigned byte. 
    GL_C(glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8UI, fbWidth, fbHeight));
    GL_C(glBindTexture(GL_TEXTURE_2D, 0));
}

void Render() {
    // we will only be writing to 'fractalTexture' for the next two shaders, and not to the screen framebuffer,
    // so turn of color write and depth write for good measure. 
    GL_C(glDepthMask(false));
    GL_C(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));

    // bind our texture to binding point 3. This means we can access it in our shaders using
    // "layout(binding=3)"
    // and we can both read and write from it. 
    GL_C(glBindImageTexture(3, fractalTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI));

    //
    // Pass 1: Render a fractal to the texture.
    // We are using attribute-less rendering to do this.
    // This means that we call
    // glDrawArrays(GL_POINTS, 0, N)
    // without actually sending any vertices.
    // The effect of this is that the vertex shader is launched N times.
    // So basically, we launch N threads on the GPU by doing this.
    // An alternative would be to use a compute shader, but that means we have to use 
    // OpenGL 4.3. With attribute-less rendering we can get away with using only 4.2!
    //
    // And note that the fragment shader is just kept empty, and all the computations
    // are done in the vertex shader.
    GLuint shader = fractalShader;
    GL_C(glUseProgram(shader));
    GL_C((glUniform1f(glGetUniformLocation(shader, "uTime"), totalTime)));
    GL_C((glUniform1i(glGetUniformLocation(shader, "uWidth"), fbWidth)));
    GL_C((glUniform1i(glGetUniformLocation(shader, "uHeight"), fbHeight)));

    GL_C(glDrawArrays(GL_POINTS, 0, fbWidth*fbHeight)); // launch one thread for each pixel. 
    // make sure all computations are done, before we do the next pass, with a barrier. 
    GL_C(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));

    //
    // Pass 2: Do box filter blur on the texture. 
    // Again, we use attribute-less rendering for this. 
    //
    shader = blurShader;
    GL_C(glUseProgram(shader));
    GL_C((glUniform1i(glGetUniformLocation(shader, "uWidth"), fbWidth)));
    GL_C((glUniform1i(glGetUniformLocation(shader, "uHeight"), fbHeight)));

    GL_C(glDrawArrays(GL_POINTS, 0, fbWidth*fbHeight));
    GL_C(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));

    //
    // Pass 3: Finally, we display the blurred fractal texture. 
    // So we do a fullscreen pass where we sample from the texture for every fragment.
    //

    // setup rendering to screen. re-enable color write and depth write. 
    GL_C(glViewport(0, 0, fbWidth, fbHeight));
    GL_C(glClearColor(0.0f, 0.0f, 0.3f, 1.0f));
    GL_C(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    GL_C(glDepthMask(true));
    GL_C(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));

    shader = displayShader;
    GL_C(glUseProgram(shader));
    GL_C((glUniform1i(glGetUniformLocation(shader, "uWidth"), fbWidth)));
    GL_C((glUniform1i(glGetUniformLocation(shader, "uHeight"), fbHeight)));
    // we draw one big triangle that covers the screen. And the vertices are stored
    // in the vertex shader, so we don't send any vertices. so no VBO.
    GL_C(glDrawArrays(GL_TRIANGLES, 0, 3)); 
}

int main(int argc, char** argv)
{
    InitGlfw();


    //
    // This shader renders the Mandelbrot set to the texture. 
    //
    fractalShader = LoadNormalShader(
        "#version 420\n"

        "uniform int uWidth;"
        "uniform int uHeight;"
        "uniform float uTime;"
        "uniform layout(binding=3, rgba8ui) writeonly uimage2D uFractalTexture;"

        "void main() {"
        // first vertex will have id 0, the second 1, and so on. And the final one has id N-1,
        // if the shader was launched with 
        // glDrawArrays(GL_POINTS, 0, N);

        // And we convert this vertex id to 2D:
        "  ivec2 i = ivec2(gl_VertexID % uWidth, gl_VertexID / uWidth);"
        "  vec2 uv = vec2(i) * vec2(1.0 / float(uWidth), 1.0 / float(uHeight));"

        // BEGIN FRACTAL RENDERING CODE
        "  float n = 0.0;"
        "  vec2 c = vec2(-.745, .186) +  (uv - 0.5)*(2.0+ 1.7*cos(1.8*uTime)  ), "
        "  z = vec2(0.0);"
        "  const int M =128;\n"
        "  for (int i = 0; i<M; i++)"
        "  {"
        "    z = vec2(z.x*z.x - z.y*z.y, 2.*z.x*z.y) + c;"
        "    if (dot(z, z) > 2) break;"

        "    n++;"
        "  }"
        "  vec3 bla = vec3(0,0,0.0);"
        "  vec3 blu = vec3(0,0,0.8);"
        "  vec4 color;"
        "  if( n >= 0 && n <= M/2-1 ) { color = vec4( mix( vec3(0.2, 0.1, 0.4), blu, n / float(M/2-1) ), 1.0) ;  }"
        "  if( n >= M/2 && n <= M ) { color = vec4( mix( blu, bla, float(n - M/2 ) / float(M/2) ), 1.0) ;  }"
        // END FRACTAL RENDERING CODE

        // Now we write the computed color to the texture.
        // Note that we must use integer texture coordinates for image load/store. 
        // Also, texture format is RGBA8UI, so we convert the color channels to  
        // unsigned byte. So convert from range [0,1] to range [0,255].
        "  imageStore(uFractalTexture, i , uvec4(color * 255.0f));"
        "}",

        "#version 420\n"
        "void main() {}" // empty fragment shader.
        );

    //
    // This shader does a box-filter blur on the texture. 
    //
    blurShader = LoadNormalShader(
        "#version 420\n"

        "uniform int uWidth;"
        "uniform int uHeight;"
        "uniform layout(binding=3, rgba8ui) uimage2D uFractalTexture;"

        // sample with clamping from the texture. 
        "vec4 csample(ivec2 i) {"
        "  i = ivec2(clamp(i.x, 0, uWidth-1), clamp(i.y, 0, uHeight-1));"
        "  return imageLoad(uFractalTexture, i);"
        "}\n"

        "#define R 8\n" // filter radius
        "#define W (1.0 / ((1.0+2.0*float(R)) * (1.0+2.0*float(R))))\n" // this macro computes the filter weights. 
        "void main() {"
        "  ivec2 i = ivec2(gl_VertexID % uWidth, gl_VertexID / uWidth);"

        "  vec4 sum = vec4(0.0);"
        // first compute the blurred color. 
        "  for(int x = -R; x <= +R; x++ )"
        "    for(int y = -R; y <= +R; y++ )"
        "      sum += W * csample(i + ivec2(x,y));"

        // now store the blurred color.
        "  imageStore(uFractalTexture,  i, uvec4(sum) );"

        "}",

        "#version 420\n"
        "void main() {}" // empty fragment shader.
        );


    //
    // This shader displays the texture to the screen.
    //
    displayShader = LoadNormalShader(
        "#version 420\n"

        "out vec2 uv;"

        // From the vertex shader, we output vertices that form a big triangle that covers the screen. 
        "const vec2 verts[3] = vec2[](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));"
        "const vec2 uvs[3] = vec2[](vec2(0, 0), vec2(2, 0), vec2(0, 2));"

        "void main() {"
        "  uv = uvs[gl_VertexID];"
        "  gl_Position =  vec4( verts[gl_VertexID] , 0.0, 1.0);"
        "}",

        "#version 420\n"

        "out vec4 color;"
        "in vec2 uv;"

        "uniform layout(binding=3, rgba8ui) readonly uimage2D uFractalTexture;"
        "uniform int uWidth;"
        "uniform int uHeight;"

        "void main() {"
        "  vec4 s = imageLoad(uFractalTexture, ivec2(float(uWidth) * uv.x, float(uHeight) * uv.y)) ;"

        // RGBA8UI is in range [0,255], so scale down. 
        "  color = (1.0 / 255.0) * s;"
        "}"
        );

    while (!glfwWindowShouldClose(window)) {
        float frameStartTime = (float)glfwGetTime();

        glfwPollEvents();

        // handle input
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        Render();

        glfwSwapBuffers(window);

        //
        // frame rate regulation code:
        //
        float frameEndTime = (float)glfwGetTime();
        float frameDuration = frameEndTime - frameStartTime;
        float sleepDuration = 1.0f / (float)FRAME_RATE - frameDuration;
        if (sleepDuration > 0.0) {
            std::this_thread::sleep_for(std::chrono::milliseconds((int)sleepDuration));
        }
        totalTime += 1.0f / (float)FRAME_RATE;
    }

    glfwTerminate();
    exit(EXIT_SUCCESS);
}
