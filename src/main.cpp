#include <iostream>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Shader.h"

static void glfw_error_callback(int error, const char* description)
{
    // GLFW reports errors asynchronously, so route them to stderr to make failures visible even when we cannot recover.
    std::cerr << "GLFW Error (" << error << "): " << description << "\n";
}

static void framebuffer_size_callback(GLFWwindow*, int width, int height)
{
    // The framebuffer size can change independently of window size, so keep the viewport in sync to avoid stretching and clipping.
    glViewport(0, 0, width, height);
}

static void createVAO_VBO(unsigned int& vao, unsigned int& vbo, const std::vector<float>& verts)
{
    // OpenGL stores vertex input state inside a VAO, so we package the layout and buffer binding together for simple reuse while drawing.
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    // Uploading vertices once as static data avoids per frame CPU to GPU traffic for geometry that does not change.
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_STATIC_DRAW);

    // The shader expects position in location 0, so describe how to read 3 floats per vertex from the buffer.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Unbinding here prevents accidental state leakage when other geometry is set up later.
    glBindVertexArray(0);
}

int main()
{
    // Set this before initialization so GLFW can report why init or window creation fails.
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
    {
        std::cerr << "Failed to init GLFW\n";
        return 1;
    }

    // Request a modern core context so the pipeline matches the tutorial style shader based rendering we are using.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "OpenGL-Renderer", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return 1;
    }

    // GL function pointers are context specific, so a context must be current before loading via GLAD.
    glfwMakeContextCurrent(window);

    // Register the resize callback so the viewport stays correct when the user resizes the window.
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Enable v sync to avoid tearing and to keep the loop from running unbounded on fast GPUs.
    glfwSwapInterval(1);

#if defined(GLAD_GL_VERSION_3_3) || defined(GLAD_GL_VERSION_3_0)
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
#else
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
#endif
    {
        std::cerr << "Failed to init GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Printing these helps validate the driver and context actually match what we requested.
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "OpenGL:   " << glGetString(GL_VERSION) << "\n";

    // Without depth testing, triangles draw in submission order and the cube will look incorrect as it rotates.
    glEnable(GL_DEPTH_TEST);

    // Centralize shader compilation and uniform helpers so render code stays focused on scene intent.
    Shader shader("shaders/basic.vert", "shaders/basic.frag");

    // Cube geometry uses indices so shared corners are stored once and reused across faces, reducing vertex data and improving cache use.
    const float cubeVerts[] = {
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };

    // Each face is two triangles; indexing keeps the topology explicit and avoids duplicating vertices per triangle.
    const unsigned int cubeIndices[] = {
        0, 1, 2,  2, 3, 0,
        4, 5, 6,  6, 7, 4,
        4, 7, 3,  3, 0, 4,
        1, 5, 6,  6, 2, 1,
        4, 5, 1,  1, 0, 4,
        3, 2, 6,  6, 7, 3
    };

    unsigned int cubeVAO = 0, cubeVBO = 0, cubeEBO = 0;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);

    // The cube vertices are immutable, so upload once and keep them resident on the GPU.
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);

    // The element buffer binding is stored in the VAO, which is why it is bound while the cube VAO is active.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    // Match attribute layout with the vertex shader so positions feed the correct input.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Unbind to reduce the chance of later setup code accidentally mutating the cube VAO state.
    glBindVertexArray(0);

    // The grid provides a stable visual reference for orientation and scale in the scene.
    const int gridHalfSize = 10;
    const float gridStep = 1.0f;

    std::vector<float> gridVerts;
    // Reserve to avoid repeated reallocations while building a predictable sized vertex list.
    gridVerts.reserve((gridHalfSize * 4 + 4) * 3);

    for (int i = -gridHalfSize; i <= gridHalfSize; ++i)
    {
        float x = i * gridStep;
        float z = i * gridStep;

        // Drawing lines in both directions makes the origin and axes easier to perceive in 3D.
        gridVerts.insert(gridVerts.end(), { x, 0.0f, -gridHalfSize * gridStep });
        gridVerts.insert(gridVerts.end(), { x, 0.0f,  gridHalfSize * gridStep });

        gridVerts.insert(gridVerts.end(), { -gridHalfSize * gridStep, 0.0f, z });
        gridVerts.insert(gridVerts.end(), { gridHalfSize * gridStep, 0.0f, z });
    }

    unsigned int gridVAO = 0, gridVBO = 0;
    // The helper keeps the simple line geometry setup consistent with other VAO and VBO pairs.
    createVAO_VBO(gridVAO, gridVBO, gridVerts);
    const int gridVertexCount = (int)(gridVerts.size() / 3);

    // The axes lines communicate the world basis so camera orientation is obvious during development.
    const float axisLen = 2.5f;
    std::vector<float> axesVerts = {
        0.0f, 0.0f, 0.0f,   axisLen, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,   0.0f, axisLen, 0.0f,
        0.0f, 0.0f, 0.0f,   0.0f, 0.0f, axisLen
    };

    unsigned int axesVAO = 0, axesVBO = 0;
    createVAO_VBO(axesVAO, axesVBO, axesVerts);

    // Main loop runs until the user closes the window, continuously presenting new frames.
    while (!glfwWindowShouldClose(window))
    {
        // Provide an immediate exit path during development without needing window controls.
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Querying the framebuffer size each frame keeps aspect ratio correct on resize and on high DPI displays.
        int w = 1280, h = 720;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);

        // Clearing both color and depth establishes a clean starting point so previous frame data does not interfere.
        glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Perspective projection makes distant objects appear smaller, matching expected 3D viewing.
        const float aspect = (h == 0) ? 1.0f : (float)w / (float)h;
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);

        // A fixed camera is enough for a simple scene and keeps focus on the render pipeline rather than controls.
        glm::vec3 camPos(3.0f, 2.0f, 3.0f);
        glm::vec3 camTarget(0.0f, 0.0f, 0.0f);
        glm::mat4 view = glm::lookAt(camPos, camTarget, glm::vec3(0, 1, 0));

        // Binding the shader once per frame reduces redundant state changes between draws.
        shader.use();

        {
            // The grid stays in world space at the origin, acting as a ground plane reference.
            glm::mat4 model(1.0f);
            glm::mat4 mvp = proj * view * model;

            shader.setMat4("uMVP", mvp);
            // A subdued color keeps the grid visible without dominating the scene.
            shader.setVec3("uColor", glm::vec3(0.25f, 0.28f, 0.33f));

            glBindVertexArray(gridVAO);
            glDrawArrays(GL_LINES, 0, gridVertexCount);
            glBindVertexArray(0);
        }

        {
            // Axes use the same transform as the grid so they represent the same world space basis.
            glm::mat4 model(1.0f);
            glm::mat4 mvp = proj * view * model;
            shader.setMat4("uMVP", mvp);

            glBindVertexArray(axesVAO);

            // Changing a uniform between draws is cheaper than creating separate axis meshes.
            shader.setVec3("uColor", glm::vec3(0.90f, 0.20f, 0.20f));
            glDrawArrays(GL_LINES, 0, 2);

            shader.setVec3("uColor", glm::vec3(0.20f, 0.90f, 0.20f));
            glDrawArrays(GL_LINES, 2, 2);

            shader.setVec3("uColor", glm::vec3(0.20f, 0.40f, 0.95f));
            glDrawArrays(GL_LINES, 4, 2);

            glBindVertexArray(0);
        }

        {
            // Time based rotation ensures consistent motion independent of frame rate.
            float t = (float)glfwGetTime();
            glm::mat4 model(1.0f);

            // Lift the cube so it sits above the grid, preventing z fighting at y equals 0.
            model = glm::translate(model, glm::vec3(0.0f, 0.5f, 0.0f));

            // Rotating around a slightly tilted axis makes the motion more informative than a single axis spin.
            model = glm::rotate(model, t, glm::vec3(0.2f, 1.0f, 0.0f));

            glm::mat4 mvp = proj * view * model;
            shader.setMat4("uMVP", mvp);
            shader.setVec3("uColor", glm::vec3(0.20f, 0.80f, 0.70f));

            glBindVertexArray(cubeVAO);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)0);
            glBindVertexArray(0);
        }

        // Swap presents the rendered image, poll processes input and window events to keep the app responsive.
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Explicit deletes keep lifetime clear and help catch leaks when the project grows.
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);

    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

    glDeleteVertexArrays(1, &axesVAO);
    glDeleteBuffers(1, &axesVBO);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
