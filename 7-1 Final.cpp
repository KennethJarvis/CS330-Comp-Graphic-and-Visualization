#include <GLEW/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>
#include <glm/glm/gtc/type_ptr.hpp>
#include <SOIL2/SOIL2.h>
#include <glm/glm/gtc/constants.hpp>

using namespace std;

int width, height;
const double PI = 3.14159;
const float toRadians = PI / 180.0f;
// Variables for controlling camera speed
GLfloat cameraSpeed = 1.0f;
GLfloat maxCameraSpeed = 5.0f;
bool orthographicMode = false;

// Input function prototypes
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

// Declare view matrix
glm::mat4 viewMatrix;
// Initialize shader
//GLuint shaderProgram;

// Initialie FOV
GLfloat fov = 45.f;

// Define camera attributes
glm::vec3 cameraPosition = glm::vec3(0.f, 0.f, 3.f);
glm::vec3 target = glm::vec3(0.f, 0.f, 0.f);
glm::vec3 cameraDirection = glm::normalize(cameraPosition - target);
glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
glm::vec3 cameraRight = glm::normalize(glm::cross(worldUp, cameraDirection));
glm::vec3 cameraUp = glm::normalize(glm::cross(cameraDirection, cameraRight));
glm::vec3 cameraFront = glm::normalize(glm::vec3(0.f, 0.f, -1.f));

// Declare target prototype
glm::vec3 getTarget();

// Camera transformation prototype
void transformCamera();

// Boolean for keys and mouse buttons
bool keys[1024], mouseButtons[3];

// Boolean to check camera transformation
bool isPanning = false, isOrbiting = false;

// Radius, pitch yaw
GLfloat radius = 3.f, rawYaw = 0.f, rawPitch = 0.f, degYaw, degPitch;

GLfloat deltaTime = 0.f, lastFrame = 0.f;
GLfloat lastX = 320, lastY = 240, xChange, yChange;

bool firstMouseMove = true; // detect initial mouse movement

void initCamera();

const char* vertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec3 vPosition;
    layout(location = 1) in vec3 aColor;
    layout(location = 2) in vec2 texCoord;
    out vec3 FragPos; // Pass the vertex position to the fragment shader
    out vec3 Normal;  // Pass the normal to the fragment shader
    out vec3 oColor;
    out vec2 oTexCoord;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    void main()
    {
        gl_Position = projection * view * model * vec4(vPosition, 1.0);
        FragPos = vec3(model * vec4(vPosition, 1.0));
        Normal = mat3(transpose(inverse(model))) * aColor; // Transform normal to world space
        oColor = aColor;
        oTexCoord = texCoord;
    }
)";

const char* fragmentShaderSource = R"(
        in vec3 FragPos;
        in vec3 Normal;
        in vec3 oColor;
        in vec2 oTexCoord;

        out vec4 fragColor;

        uniform sampler2D diffuseTexture;
        uniform vec3 lightPos;
        uniform vec3 viewPos;
        uniform vec3 lightColor;

        void main()
        {
            // Ambient lighting
            float ambientStrength = 0.5;
            vec3 ambient = ambientStrength * lightColor;

            // Diffuse lighting
            vec3 norm = normalize(Normal);
            vec3 lightDir = normalize(lightPos - FragPos);
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = diff * lightColor;

            // Specular lighting
            float specularStrength = 6.5;
            vec3 viewDir = normalize(viewPos - FragPos);
            vec3 reflectDir = reflect(-lightDir, norm);
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128);
            vec3 specular = specularStrength * spec * lightColor;

            // Combine ambient, diffuse, and specular
            vec3 result = (ambient + diffuse + specular);

            // Use the texture color without multiplying by oColor
            fragColor = texture(diffuseTexture, oTexCoord) * vec4(result, 1.0);
        }
    
)";

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Vertices and indices for the torus
const int torusSegments = 20;
const int torusRings = 10;
const float torusRadius = 0.25f;
const float tubeRadius = 0.1f;

GLfloat torusVertices[(torusSegments + 1) * (torusRings + 1) * 8]; // 8 attributes
GLuint torusIndices[torusSegments * torusRings * 6]; // 6 indices

void generateTorusVerticesAndIndices() {
    int index = 0;
    for (int i = 0; i <= torusSegments; ++i) {
        for (int j = 0; j <= torusRings; ++j) {
            float u = static_cast<float>(i) / torusSegments;
            float v = static_cast<float>(j) / torusRings;
            float theta = 2.0f * PI * u;
            float phi = 2.0f * PI * v;

            float x = (torusRadius + tubeRadius * cos(phi) ) * cos(theta);
            float y = tubeRadius * sin(phi) + 0.7f;
            float z = (torusRadius + tubeRadius * cos(phi)) * sin(theta);

            float nx = cos(phi) * cos(theta);
            float ny = sin(phi);
            float nz = cos(phi) * sin(theta);

            float s = 1.0f - u;
            float t = 1.0f - v;

            torusVertices[index++] = x;
            torusVertices[index++] = y;
            torusVertices[index++] = z;
            torusVertices[index++] = nx;
            torusVertices[index++] = ny;
            torusVertices[index++] = nz;
            torusVertices[index++] = s;
            torusVertices[index++] = t;
        }
    }

    index = 0;
    for (int i = 0; i < torusSegments; ++i) {
        for (int j = 0; j < torusRings; ++j) {
            int p0 = i * (torusRings + 1) + j;
            int p1 = (i + 1) * (torusRings + 1) + j;
            int p2 = (i + 1) * (torusRings + 1) + (j + 1);
            int p3 = i * (torusRings + 1) + (j + 1);

            torusIndices[index++] = p0;
            torusIndices[index++] = p1;
            torusIndices[index++] = p2;
            torusIndices[index++] = p2;
            torusIndices[index++] = p3;
            torusIndices[index++] = p0;
        }
    }
}

// Vertices and indices for the cylinder
const int cylinderSegments = 20;
const float cylinderRadius = 0.2f;
const float cylinderHeight = 0.5f;

GLfloat cylinderVertices[(cylinderSegments + 1) * 2 * 8]; // 8 attributes per vertex
GLuint cylinderIndices[cylinderSegments * 6]; // 6 indices per quad

void generateCylinderVerticesAndIndices() {
    int index = 0;
    for (int i = 0; i <= cylinderSegments; ++i) {
        float u = static_cast<float>(i) / cylinderSegments;
        float theta = 2.0f * PI * u;

        float x = cylinderRadius * cos(theta);
        float y = cylinderHeight / 2.0f;
        float z = cylinderRadius * sin(theta);

        float nx = cos(theta);
        float ny = 0.0f;
        float nz = sin(theta);

        float s = 1.0f - u;
        float t = 0.5f; // You can adjust this for texture mapping

        cylinderVertices[index++] = x;
        cylinderVertices[index++] = y;
        cylinderVertices[index++] = z;
        cylinderVertices[index++] = nx;
        cylinderVertices[index++] = ny;
        cylinderVertices[index++] = nz;
        cylinderVertices[index++] = s;
        cylinderVertices[index++] = t;

        cylinderVertices[index++] = x;
        cylinderVertices[index++] = -y;
        cylinderVertices[index++] = z;
        cylinderVertices[index++] = nx;
        cylinderVertices[index++] = ny;
        cylinderVertices[index++] = nz;
        cylinderVertices[index++] = s;
        cylinderVertices[index++] = 1.0f - t;
    }

    index = 0;
    for (int i = 0; i < cylinderSegments; ++i) {
        int p0 = i * 2;
        int p1 = (i + 1) * 2;
        int p2 = (i + 1) * 2 + 1;
        int p3 = i * 2 + 1;

        cylinderIndices[index++] = p0;
        cylinderIndices[index++] = p1;
        cylinderIndices[index++] = p2;
        cylinderIndices[index++] = p2;
        cylinderIndices[index++] = p3;
        cylinderIndices[index++] = p0;
    }
}

// Vertices and indices for the sphere
const int sphereSegments = 20;
const int sphereRings = 20;
const float sphereRadius = 0.3f;

GLfloat sphereVertices[(sphereSegments + 1) * (sphereRings + 1) * 8];
GLuint sphereIndices[sphereSegments * sphereRings * 6];

void generateSphereVerticesAndIndices() {
    int index = 0;
    for (int i = 0; i <= sphereSegments; ++i) {
        for (int j = 0; j <= sphereRings; ++j) {
            float u = static_cast<float>(i) / sphereSegments;
            float v = static_cast<float>(j) / sphereRings;
            float theta = 2.0f * glm::pi<float>() * u;
            float phi = glm::pi<float>() * v;

            float x = sphereRadius * sin(phi) * cos(theta);
            float y = sphereRadius * cos(phi);
            float z = sphereRadius * sin(phi) * sin(theta);

            float nx = sin(phi) * cos(theta);
            float ny = cos(phi);
            float nz = sin(phi) * sin(theta);

            float s = 1.0f - u;
            float t = 1.0f - v;

            sphereVertices[index++] = x;
            sphereVertices[index++] = y;
            sphereVertices[index++] = z;
            sphereVertices[index++] = nx;
            sphereVertices[index++] = ny;
            sphereVertices[index++] = nz;
            sphereVertices[index++] = s;
            sphereVertices[index++] = t;
        }
    }

    index = 0;
    for (int i = 0; i < sphereSegments; ++i) {
        for (int j = 0; j < sphereRings; ++j) {
            int p0 = i * (sphereRings + 1) + j;
            int p1 = (i + 1) * (sphereRings + 1) + j;
            int p2 = (i + 1) * (sphereRings + 1) + (j + 1);
            int p3 = i * (sphereRings + 1) + (j + 1);

            sphereIndices[index++] = p0;
            sphereIndices[index++] = p1;
            sphereIndices[index++] = p2;
            sphereIndices[index++] = p2;
            sphereIndices[index++] = p3;
            sphereIndices[index++] = p0;
        }
    }
}

void generateSphereVerticesAndIndices();

int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Ken's Final", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Set input callback functions
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    glViewport(0, 0, 800, 600);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glEnable(GL_DEPTH_TEST);

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, sizeof(infoLog), NULL, infoLog);
        std::cerr << "Vertex shader compilation failed: " << infoLog << std::endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, sizeof(infoLog), NULL, infoLog);
        std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
    }

    // Wireframe mode
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);


    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, sizeof(infoLog), NULL, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLfloat vertices[] = {
        // Front face
        -2.0f,  0.6f, 0.3f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // Vertex 0
        -1.8f,  0.6f, 0.3f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,  // Vertex 1
        -1.8f,  1.2f, 0.3f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // Vertex 2
        -2.0f,  1.2f, 0.3f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,  // Vertex 3

        // Back face
        -2.0f,  0.6f, -0.3f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // Vertex 4
        -1.8f,  0.6f, -0.3f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,  // Vertex 5
        -1.8f,  1.2f, -0.3f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // Vertex 6
        -2.0f,  1.2f, -0.3f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,  // Vertex 7

        // Bottom plane face
        -5.5f, 0.3f, 1.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  // Vertex 0
        5.5f, 0.3f, 1.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,  // Vertex 1
        5.5f, 0.3f, -1.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,  // Vertex 2
        -5.5f, 0.3f, -1.5f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f   // Vertex 3
    };

    GLuint indices[] = {
        // Front face
        0, 1, 2,
        2, 3, 0,

        // Back face
        4, 5, 6,
        6, 7, 4,

        // Left face
        0, 3, 7,
        7, 4, 0,

        // Right face
        1, 2, 6,
        6, 5, 1,

        // Top face
        3, 2, 6,
        6, 7, 3,

        // Bottom face
        0, 1, 5,
        5, 4, 0,

        // Bottom plane face
        0, 1, 2,
        2, 3, 0
    };

    GLfloat planeVertices[] = {
        // Positions           // Colors            // Texture Coords
        -2.0f, 0.6f, -2.0f,  0.0f, 0.0f, 1.0f,    0.0f, 0.0f,
         2.0f, 0.6f, -2.0f,  0.0f, 1.0f, 0.0f,    1.0f, 0.0f,
         2.0f, 0.6f,  2.0f,  1.0f, 0.0f, 0.0f,    1.0f, 1.0f,
        -2.0f, 0.6f,  2.0f,  0.5f, 0.5f, 0.5f,    0.0f, 1.0f
    };

    GLuint planeIndices[] = {
        0, 1, 2,
        2, 3, 0
    };

    // Generate cylinder vertices and indices
    generateCylinderVerticesAndIndices();

    // Generate torus vertices and indices
    generateTorusVerticesAndIndices();

    // Generate sphere vertices and indices
    generateSphereVerticesAndIndices();

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));  // Texture coordinates
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint VAO_plane, VBO_plane, EBO_plane;
    glGenVertexArrays(1, &VAO_plane);
    glGenBuffers(1, &VBO_plane);
    glGenBuffers(1, &EBO_plane);

    glBindVertexArray(VAO_plane);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_plane);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_plane);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(planeIndices), planeIndices, GL_STATIC_DRAW);

    // Specify attribute pointers for the plane
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));  // Texture coordinates
    glEnableVertexAttribArray(2);

    // Generate buffers for the cylinder
    GLuint VAO_cylinder, VBO_cylinder, EBO_cylinder;
    glGenVertexArrays(1, &VAO_cylinder);
    glGenBuffers(1, &VBO_cylinder);
    glGenBuffers(1, &EBO_cylinder);

    glBindVertexArray(VAO_cylinder);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_cylinder);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cylinderVertices), cylinderVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_cylinder);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cylinderIndices), cylinderIndices, GL_STATIC_DRAW);

    // Specify attribute pointers for the cylinder
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    // Unbind the VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Generate buffers for the torus
    GLuint VAO_torus, VBO_torus, EBO_torus;
    glGenVertexArrays(1, &VAO_torus);
    glGenBuffers(1, &VBO_torus);
    glGenBuffers(1, &EBO_torus);

    // Bind and fill buffers for the torus
    glBindVertexArray(VAO_torus);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_torus);
    glBufferData(GL_ARRAY_BUFFER, sizeof(torusVertices), torusVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_torus);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(torusIndices), torusIndices, GL_STATIC_DRAW);

    // Specify attribute pointers for the torus
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    // Create VAO, VBO, and EBO for the sphere
    GLuint VAO_sphere, VBO_sphere, EBO_sphere;
    glGenVertexArrays(1, &VAO_sphere);
    glGenBuffers(1, &VBO_sphere);
    glGenBuffers(1, &EBO_sphere);

    glBindVertexArray(VAO_sphere);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_sphere);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sphereVertices), sphereVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_sphere);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(sphereIndices), sphereIndices, GL_STATIC_DRAW);

    // Specify attribute pointers for the sphere
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // Unbind the VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    int planeTexWidth, planeTexHeight, boxTexWidth, boxTexHeight, sphereTexWidth, sphereTexHeight;
    unsigned char* planeImage = SOIL_load_image("brick_texture.jpg", &planeTexWidth, &planeTexHeight, 0, SOIL_LOAD_RGB);
    unsigned char* boxImage = SOIL_load_image("blue.jpg", &boxTexWidth, &boxTexHeight, 0, SOIL_LOAD_RGB);
    unsigned char* sphereImage = SOIL_load_image("green2.png", &sphereTexWidth, &sphereTexHeight, 0, SOIL_LOAD_RGB);

    GLuint planeTexture;
    glGenTextures(1, &planeTexture);
    glBindTexture(GL_TEXTURE_2D, planeTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, planeTexWidth, planeTexHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, planeImage);
    glGenerateMipmap(GL_TEXTURE_2D);
    SOIL_free_image_data(planeImage);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint boxTexture;
    glGenTextures(1, &boxTexture);
    glBindTexture(GL_TEXTURE_2D, boxTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, boxTexWidth, boxTexHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, boxImage);
    glGenerateMipmap(GL_TEXTURE_2D);
    SOIL_free_image_data(boxImage);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint sphereTexture;
    glGenTextures(1, &sphereTexture);
    glBindTexture(GL_TEXTURE_2D, sphereTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, sphereTexWidth, sphereTexHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, sphereImage);
    glGenerateMipmap(GL_TEXTURE_2D);
    SOIL_free_image_data(sphereImage);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Set up projection matrix (Perspective projection)
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

    // Set up view matrix
    glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Set up model matrix
    glm::mat4 model = glm::mat4(1.0f);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));


    initCamera();
    while (!glfwWindowShouldClose(window))
    {
        // Set up lighting parameters
        glm::vec3 lightPos(1.0f, 2.0f, 2.0f);
        glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
        glm::vec3 viewPos = cameraPosition;

        // Set delta time
        GLfloat currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Poll camera transformations
        transformCamera();

        // Update the view matrix
        view = glm::lookAt(cameraPosition, getTarget(), cameraUp);
        //glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(viewPos));
        if (orthographicMode) {
            // Orthographic projection
            glm::mat4 orthoProjection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 100.0f);
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(orthoProjection));
        }
        else {
            // Perspective projection
            glm::mat4 perspectiveProjection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(perspectiveProjection));
        }
        //glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        // Set up model matrices for torus and cylinder
        glm::mat4 modelTorus = glm::mat4(1.0f);
        glm::mat4 modelCylinder = glm::mat4(1.0f);
        glm::mat4 modelSphere = glm::mat4(1.0f);

        // Translate the torus
        modelTorus = glm::translate(modelTorus, glm::vec3(1.0f, 0.0f, 0.0f));

        // Translate the cylinder
        modelCylinder = glm::translate(modelCylinder, glm::vec3(-0.65f, 0.9f, 0.00f));

        // Translate the sphere
        modelSphere = glm::translate(modelSphere, glm::vec3(1.0f, 1.05f, 0.0f));

        // Draw the box
        glBindTexture(GL_TEXTURE_2D, boxTexture);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Draw the plane
        glBindTexture(GL_TEXTURE_2D, planeTexture);
        glBindVertexArray(VAO_plane);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Set up model matrix for cylinder
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelCylinder));

        // VBO cylinder vertices
        glBindBuffer(GL_ARRAY_BUFFER, VBO_cylinder);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cylinderVertices), cylinderVertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Draw the cylinder
        glBindTexture(GL_TEXTURE_2D, boxTexture);  // Box texture
        glBindVertexArray(VAO_cylinder);
        glDrawElements(GL_TRIANGLES, sizeof(cylinderIndices) / sizeof(GLuint), GL_UNSIGNED_INT, 0);

        // Set up model matrix for torus
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelTorus));

        // Draw the torus
        glBindTexture(GL_TEXTURE_2D, boxTexture);  // Box texture
        glBindVertexArray(VAO_torus);
        glDrawElements(GL_TRIANGLES, sizeof(torusIndices) / sizeof(GLuint), GL_UNSIGNED_INT, 0);

        // Set up model matrix for sphere
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelSphere));

        // VBO sphere vertices
        glBindBuffer(GL_ARRAY_BUFFER, VBO_sphere);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(sphereVertices), sphereVertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Draw the sphere
        glBindTexture(GL_TEXTURE_2D, sphereTexture); // Sphere texture
        glBindVertexArray(VAO_sphere);
        glDrawElements(GL_TRIANGLES, sizeof(sphereIndices) / sizeof(GLuint), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glBindTexture(GL_TEXTURE_2D, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
        // Find Camera position
        //cout << "Camera Position: (" << cameraPosition.x << ", " << cameraPosition.y << ", " << cameraPosition.z << ")" << endl;
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &VAO_torus);
    glDeleteBuffers(1, &VBO_torus);
    glDeleteBuffers(1, &EBO_torus);

    glDeleteVertexArrays(1, &VAO_sphere);
    glDeleteBuffers(1, &VBO_sphere);
    glDeleteBuffers(1, &EBO_sphere);

    glfwTerminate();

    return 0;
}

// Define input callback functions
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{

    if (action == GLFW_PRESS)
    {
        keys[key] = true;

        // Toggle between orthographic and perspective views when the 'P' key is pressed
        if (key == GLFW_KEY_P)
        {
            orthographicMode = !orthographicMode;

        }
    }
    else if (action == GLFW_RELEASE)
    {
        keys[key] = false;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{

    cout << "Camera Speed: " << cameraSpeed << endl;

    // Clamp FOV
    if (fov >= 1.f && fov <= 45.f)
        fov -= yoffset * 0.01f;

    // Default FOV
    if (fov < 1.f)
        fov = 1.f;
    if (fov > 45.f)
        fov = 45.f;

    // Adjust camera speed based on scroll offset
    cameraSpeed += yoffset * 0.5f;

    // Clamp camera speed to the specified range
    if (cameraSpeed < 0.1f)
        cameraSpeed = 0.1f;
    if (cameraSpeed > maxCameraSpeed)
        cameraSpeed = maxCameraSpeed;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (action == GLFW_PRESS)
        mouseButtons[button] = true;
    else if (action == GLFW_RELEASE)
        mouseButtons[button] = false;
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    // Display cursor x and y positions
    //cout << "Mouse X: " << xpos << endl;
    //cout << "Mouse Y: " << ypos << endl;

    if (firstMouseMove)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouseMove = false;
    }

    // Calculate cursor offset
    xChange = xpos - lastX;
    yChange = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    // Pan camera
    if (isPanning)
    {
        if (cameraPosition.z < 0.f)
            cameraFront.z = 1.f;
        else
            cameraFront.z = -1.f;


        GLfloat cameraSpeed = xChange * deltaTime;
        cameraPosition += cameraSpeed * cameraRight;

        cameraSpeed = yChange * deltaTime;
        cameraPosition += cameraSpeed * cameraUp;
    }

    // Orbit camera
    if (isOrbiting)
    {
        rawYaw += xChange;
        rawPitch += yChange;

        // Convert yaw and pitch to degrees
        degYaw = glm::radians(rawYaw);
        //degPitch = glm::radians(rawPitch);
        degPitch = glm::clamp(glm::radians(rawPitch), -glm::pi<float>() / 2.f + .1f, glm::pi<float>() / 2.f - .1f);

        // Azimuth altitude formula
        cameraPosition.x = target.x + radius * cosf(degPitch) * sinf(degYaw);
        cameraPosition.y = target.y + radius * sinf(degPitch);
        cameraPosition.z = target.z + radius * cosf(degPitch) * cosf(degYaw);
    }


}

// Define getTarget function
glm::vec3 getTarget()
{
    if (isPanning)
        target = cameraPosition + cameraFront;

    return target;
}

// Define transformCamera function
void transformCamera()
{
    // Pan camera
    if (keys[GLFW_KEY_LEFT_ALT] && mouseButtons[GLFW_MOUSE_BUTTON_MIDDLE])
        isPanning = true;
    else
        isPanning = false;

    // Orbit camera
    if (keys[GLFW_KEY_LEFT_ALT] && mouseButtons[GLFW_MOUSE_BUTTON_LEFT])
        isOrbiting = true;
    else
        isOrbiting = false;

    // Reset camera
    if (keys[GLFW_KEY_F])
        initCamera();

    // WASD & QE key commands

    if (keys[GLFW_KEY_W])
        cameraPosition += cameraSpeed * cameraFront * deltaTime;

    if (keys[GLFW_KEY_S])
        cameraPosition -= cameraSpeed * cameraFront * deltaTime;

    if (keys[GLFW_KEY_A])
        cameraPosition -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed * deltaTime;

    if (keys[GLFW_KEY_D])
        cameraPosition += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed * deltaTime;

    if (keys[GLFW_KEY_Q])
        cameraPosition += cameraUp * cameraSpeed * deltaTime;

    if (keys[GLFW_KEY_E])
        cameraPosition -= cameraUp * cameraSpeed * deltaTime;
}

void initCamera()
{
    cameraPosition = glm::vec3(0.0f, 0.0f, 5.0f); // Initial position
    target = glm::vec3(0.0f, 0.0f, 0.0f);
    cameraDirection = glm::normalize(cameraPosition - target);
    worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    cameraRight = glm::normalize(glm::cross(worldUp, cameraDirection));
    cameraUp = glm::normalize(glm::cross(cameraDirection, cameraRight));
    cameraFront = glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f));
    cameraPosition.y = 1.5f; // Camera height
}