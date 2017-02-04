
/*
GLFW
*/
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <string>

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

    // before the shader source code, we append the contents of the file "shader_common"
    // this contains important functions that are common between some shaders.
    std::string src = shaderSource;//LoadFile("shader_common")

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
// Above is utility functions. 
// From here on the actual demo starts.
//

const int WINDOW_WIDTH = 1497;
const int WINDOW_HEIGHT = 1014;
const int GUI_WIDTH = 250;

GLFWwindow* window;
GLuint fractalShader;
GLuint vao;
float time = 0.0f; // keep track of time for shader.

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
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);

	// load GLAD.
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	// Bind and create VAO, otherwise, we can't do anything in OpenGL.
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
}

void Render() {
	int fbWidth, fbHeight;
	glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    GL_C(glViewport(0, 0, fbWidth, fbHeight));
    GL_C(glClearColor(0.0f, 0.0f, 0.3f, 1.0f)); 
    GL_C(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
        
	GLuint shader = fractalShader;
	GL_C(glUseProgram(shader));
    GL_C((glUniform1f(glGetUniformLocation(shader, "uTime"), time)));

    GL_C(glDrawArrays(GL_TRIANGLES, 0, 3));
}	

int main(int argc, char** argv)
{
	InitGlfw();

    fractalShader = LoadNormalShader(
        "#version 430\n"
        "out vec2 uv;"

        "const vec2 verts[3] = vec2[](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));"
        "const vec2 uvs[3] = vec2[](vec2(0, 0), vec2(2, 0), vec2(0, 2));"

        "void main() {"
        "  uv = uvs[gl_VertexID];"
        "  gl_Position =  vec4( verts[gl_VertexID] , 0.0, 1.0 );"

        "}",

        "#version 430\n"

        "out vec4 color;"
        "in vec2 uv;"
        "uniform float uTime;"

        "void main() {"

        "float n = 0.0;"
        "vec2 c = vec2(-.745, .186) +  (uv - 0.5)*(2.0+ 0.3*cos(0.5*uTime)  ), "
        "z = vec2(0.0);"

        "const int M =128;\n"

        "for (int i = 0; i<M; i++)"
        "{"
        "  z = vec2(z.x*z.x - z.y*z.y, 2.*z.x*z.y) + c;"
        "  if (dot(z, z) > 2) break;"

        "  n++;"
        "}"

        "vec3 bla = vec3(0,0,0.0);"
        "vec3 blu = vec3(0,0,0.8);"
        " if( n >= 0 && n <= M/2-1 ) { color = vec4( mix( vec3(0.2, 0.1, 0.4), blu, n / float(M/2-1) ), 1.0) ;  }"
        " if( n >= M/2 && n <= M ) { color = vec4( mix( blu, bla, float(n - M/2 ) / float(M/2) ), 1.0) ;  }"
        "}"
        );

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

        // handle input
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        time += 0.1f;
	
		Render();

		glfwSwapBuffers(window);
	}

	glfwTerminate();
	exit(EXIT_SUCCESS);
}
