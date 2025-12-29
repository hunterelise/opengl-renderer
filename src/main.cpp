#include <iostream>
#include <vector>
#include <cmath>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Shader.h"
#include "OrbitCamera.h"

static void glfw_error_callback(int error, const char* description)
{
    // Errors can happen inside GLFW at any time, so print them.
    std::cerr << "GLFW Error (" << error << "): " << description << "\n";
}

static void framebuffer_size_callback(GLFWwindow*, int width, int height)
{
    // The size of the actual framebuffer can change when resizing or on high DPI displays, so update the viewport to match.
    glViewport(0, 0, width, height);
}

static void createVAO_VBO(unsigned int& vao, unsigned int& vbo, const std::vector<float>& verts)
{
    // A VAO remembers how vertex data is laid out, so later we can draw this shape by just binding the VAO.
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    // Upload the vertex positions once because the grid and axes do not change every frame.
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_STATIC_DRAW);

    // Our simple debug shaders read position from attribute location 0.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Unbind to avoid accidentally editing this VAO when setting up other objects.
    glBindVertexArray(0);
}

static glm::mat4 makePlaneModel(const glm::vec3& n_in, float d, float halfSize)
{
    // The plane math assumes a unit normal, so normalize it to make d behave like a real distance.
    glm::vec3 n = glm::normalize(n_in);

    // The plane is defined by dot(n, p) + d = 0, so p0 = -d * n is a convenient point that lies on the plane.
    glm::vec3 p0 = -d * n;

    // We need two directions that lie on the plane; start with any helper axis that is not pointing the same way as n.
    glm::vec3 a = (std::abs(n.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);

    // Cross products build two perpendicular directions u and v that span the plane surface.
    glm::vec3 u = glm::normalize(glm::cross(a, n));
    glm::vec3 v = glm::normalize(glm::cross(n, u));

    // This matrix stretches a unit quad along u and v, then moves it to p0 so it visually sits on the clipping plane.
    glm::mat4 M(1.0f);
    M[0] = glm::vec4(u * halfSize, 0.0f);
    M[1] = glm::vec4(v * halfSize, 0.0f);
    M[2] = glm::vec4(n, 0.0f);
    M[3] = glm::vec4(p0, 1.0f);
    return M;
}

struct InputState
{
    OrbitCamera cam;

    // These booleans track what the user is trying to do so the mouse can mean orbiting or panning depending on the button.
    bool orbiting = false;
    bool panning = false;
    double lastX = 0.0;
    double lastY = 0.0;
};

static void scroll_callback(GLFWwindow* window, double /*xoffset*/, double yoffset)
{
    // Scrolling changes camera distance, which feels like moving closer to or further from the target.
    auto* input = static_cast<InputState*>(glfwGetWindowUserPointer(window));
    input->cam.zoom((float)yoffset);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/)
{
    // Button state is stored so the cursor callback can just use deltas without needing to ask GLFW which buttons are down.
    auto* input = static_cast<InputState*>(glfwGetWindowUserPointer(window));

    if (button == GLFW_MOUSE_BUTTON_LEFT)
        input->orbiting = (action == GLFW_PRESS);

    if (button == GLFW_MOUSE_BUTTON_RIGHT)
        input->panning = (action == GLFW_PRESS);

    // When a drag starts, capture the current cursor position so the first delta is small and predictable.
    if (action == GLFW_PRESS)
        glfwGetCursorPos(window, &input->lastX, &input->lastY);
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    // We work in mouse movement deltas so the camera control is based on how far you dragged, not where the cursor is on screen.
    auto* input = static_cast<InputState*>(glfwGetWindowUserPointer(window));

    double dx = xpos - input->lastX;
    double dy = ypos - input->lastY;
    input->lastX = xpos;
    input->lastY = ypos;

    // Only apply one kind of movement at a time so orbit and pan do not fight each other.
    if (input->orbiting)
        input->cam.orbit((float)dx, (float)dy);
    else if (input->panning)
        input->cam.pan((float)dx, (float)dy);
}

int main()
{
    // Register early so any setup failures still produce useful information.
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
    {
        std::cerr << "Failed to init GLFW\n";
        return 1;
    }

    // Ask for an OpenGL 3.3 core context so modern vertex array and shader based rendering is available.
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

    // GLAD needs a current context to look up OpenGL function pointers.
    glfwMakeContextCurrent(window);

    // Keep the viewport correct when the window or framebuffer size changes.
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // V sync makes animation smoother and prevents the loop from running as fast as possible.
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

    InputState input;

    // Store the input state on the window so GLFW callbacks can reach the camera without using global variables.
    glfwSetWindowUserPointer(window, &input);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);

    // Start with a view that shows the whole scene so you can immediately see grid, axes, and the cube.
    input.cam.target = glm::vec3(0.0f, 0.0f, 0.0f);
    input.cam.distance = 6.0f;
    input.cam.yaw = glm::radians(45.0f);
    input.cam.pitch = glm::radians(-25.0f);

    // Printing these helps confirm your driver, GPU, and OpenGL version are what you expect.
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "OpenGL:   " << glGetString(GL_VERSION) << "\n";

    // Depth testing is needed for correct 3D overlap so closer surfaces hide farther ones.
    glEnable(GL_DEPTH_TEST);

    // Use a basic shader for simple colored lines and a separate shader for lighting and clipping on the cube.
    Shader basic("shaders/basic.vert", "shaders/basic.frag");
    Shader lit("shaders/lit.vert", "shaders/lit.frag");

    // Lighting needs correct face normals, so each cube face gets its own vertices instead of sharing corners.
    const float cubeVerts[] = {
        -0.5f,-0.5f, 0.5f,  0,0,1,
         0.5f,-0.5f, 0.5f,  0,0,1,
         0.5f, 0.5f, 0.5f,  0,0,1,
        -0.5f, 0.5f, 0.5f,  0,0,1,

         0.5f,-0.5f,-0.5f,  0,0,-1,
        -0.5f,-0.5f,-0.5f,  0,0,-1,
        -0.5f, 0.5f,-0.5f,  0,0,-1,
         0.5f, 0.5f,-0.5f,  0,0,-1,

         -0.5f,-0.5f,-0.5f, -1,0,0,
         -0.5f,-0.5f, 0.5f, -1,0,0,
         -0.5f, 0.5f, 0.5f, -1,0,0,
         -0.5f, 0.5f,-0.5f, -1,0,0,

          0.5f,-0.5f, 0.5f,  1,0,0,
          0.5f,-0.5f,-0.5f,  1,0,0,
          0.5f, 0.5f,-0.5f,  1,0,0,
          0.5f, 0.5f, 0.5f,  1,0,0,

          -0.5f, 0.5f, 0.5f,  0,1,0,
           0.5f, 0.5f, 0.5f,  0,1,0,
           0.5f, 0.5f,-0.5f,  0,1,0,
          -0.5f, 0.5f,-0.5f,  0,1,0,

          -0.5f,-0.5f,-0.5f,  0,-1,0,
           0.5f,-0.5f,-0.5f,  0,-1,0,
           0.5f,-0.5f, 0.5f,  0,-1,0,
          -0.5f,-0.5f, 0.5f,  0,-1,0,
    };

    // Indices avoid repeating the same vertex within a face while still keeping faces separate for correct normals.
    const unsigned int cubeIndices[] = {
        0,1,2,  2,3,0,
        4,5,6,  6,7,4,
        8,9,10, 10,11,8,
        12,13,14, 14,15,12,
        16,17,18, 18,19,16,
        20,21,22, 22,23,20
    };

    unsigned int cubeVAO = 0, cubeVBO = 0, cubeEBO = 0;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);

    // Upload cube data once because the cube geometry stays the same even though its model matrix changes.
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);

    // The element buffer is part of VAO state, so binding it here makes the VAO fully describe how to draw the cube.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    // Tell OpenGL where to find position and normal data inside each vertex for the lit shader.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Unbinding here prevents later setup code from changing the cube layout by mistake.
    glBindVertexArray(0);

    // The grid acts like graph paper in 3D, making scale and orientation easier to understand.
    const int gridHalfSize = 10;
    const float gridStep = 1.0f;

    std::vector<float> gridVerts;
    // Reserving helps performance during setup and keeps the code behavior predictable.
    gridVerts.reserve((gridHalfSize * 4 + 4) * 3);

    for (int i = -gridHalfSize; i <= gridHalfSize; ++i)
    {
        float x = i * gridStep;
        float z = i * gridStep;

        // A line in each direction at each step creates a square grid on the XZ plane.
        gridVerts.insert(gridVerts.end(), { x, 0.0f, -gridHalfSize * gridStep });
        gridVerts.insert(gridVerts.end(), { x, 0.0f,  gridHalfSize * gridStep });

        gridVerts.insert(gridVerts.end(), { -gridHalfSize * gridStep, 0.0f, z });
        gridVerts.insert(gridVerts.end(), { gridHalfSize * gridStep, 0.0f, z });
    }

    unsigned int gridVAO = 0, gridVBO = 0;
    // Reuse the same VAO setup helper so grid and axes are configured the same way.
    createVAO_VBO(gridVAO, gridVBO, gridVerts);
    const int gridVertexCount = (int)(gridVerts.size() / 3);

    // The axes show which direction is X, Y, and Z, which is helpful when learning transforms and camera movement.
    const float axisLen = 2.5f;
    std::vector<float> axesVerts = {
        0.0f, 0.0f, 0.0f,   axisLen, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,   0.0f, axisLen, 0.0f,
        0.0f, 0.0f, 0.0f,   0.0f, 0.0f, axisLen
    };

    unsigned int axesVAO = 0, axesVBO = 0;
    createVAO_VBO(axesVAO, axesVBO, axesVerts);

    // This quad is used only to visualize the clipping plane, not to do the clipping itself.
    const float planeQuadVerts[] = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,

        -1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f
    };

    unsigned int planeVAO = 0, planeVBO = 0;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);

    glBindVertexArray(planeVAO);

    // The quad geometry is constant, and we move it onto the plane using a model matrix built from the plane equation.
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeQuadVerts), planeQuadVerts, GL_STATIC_DRAW);

    // The plane shader only needs position because color and transparency are set through uniforms.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Unbind to keep later VAO setup from accidentally using this buffer or layout.
    glBindVertexArray(0);

    // Keep the plane settings here so input, debug drawing, and shader clipping all reference the same values.
    bool clipEnabled = true;
    glm::vec3 clipNormal(0.0f, 1.0f, 0.0f);
    float clipD = 0.0f;
    bool cWasDown = false;

    // Main loop runs until the window closes. Each pass draws one frame.
    while (!glfwWindowShouldClose(window))
    {
        // Let the user quit quickly while testing.
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Toggle on key press, not while held, so it switches once per press.
        bool cDown = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
        if (cDown && !cWasDown) clipEnabled = !clipEnabled;
        cWasDown = cDown;

        // Moving d slides the plane along its normal, making it easy to see the cube being cut in real time.
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)   clipD += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) clipD -= 0.02f;

        // Picking simple axis normals is a good first test because you can predict the result and verify your clipping math.
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) clipNormal = glm::vec3(1, 0, 0);
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) clipNormal = glm::vec3(0, 1, 0);
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) clipNormal = glm::vec3(0, 0, 1);
        clipNormal = glm::normalize(clipNormal);

        // Always use the real framebuffer size so aspect ratio stays correct on resize and on high DPI screens.
        int w = 1280, h = 720;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);

        // Clear both buffers so the new frame starts clean and depth from the last frame does not interfere.
        glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Projection describes how 3D points map to the screen. Perspective is what makes far objects look smaller.
        const float aspect = (h == 0) ? 1.0f : (float)w / (float)h;
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);

        // View comes from the orbit camera so you can explore the scene while testing clipping and lighting.
        glm::mat4 view = input.cam.viewMatrix();
        glm::vec3 camPos = input.cam.position();

        {
            // The grid is drawn first as background reference so it is always visible behind the cube.
            basic.use();
            glm::mat4 model(1.0f);
            glm::mat4 mvp = proj * view * model;

            basic.setMat4("uMVP", mvp);
            basic.setVec3("uColor", glm::vec3(0.25f, 0.28f, 0.33f));
            basic.setFloat("uAlpha", 1.0f);

            glBindVertexArray(gridVAO);
            glDrawArrays(GL_LINES, 0, gridVertexCount);
            glBindVertexArray(0);
        }

        {
            // Axes are drawn in bright colors so you can quickly tell which way the camera is facing.
            basic.use();
            glm::mat4 model(1.0f);
            glm::mat4 mvp = proj * view * model;

            basic.setMat4("uMVP", mvp);
            basic.setFloat("uAlpha", 1.0f);

            glBindVertexArray(axesVAO);

            basic.setVec3("uColor", glm::vec3(0.90f, 0.20f, 0.20f));
            glDrawArrays(GL_LINES, 0, 2);

            basic.setVec3("uColor", glm::vec3(0.20f, 0.90f, 0.20f));
            glDrawArrays(GL_LINES, 2, 2);

            basic.setVec3("uColor", glm::vec3(0.20f, 0.40f, 0.95f));
            glDrawArrays(GL_LINES, 4, 2);

            glBindVertexArray(0);
        }

        if (clipEnabled)
        {
            // The plane is drawn translucent so you can see both the plane and the scene behind it.
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Don't want the plane to block the cube, so do not write it into the depth buffer.
            glDepthMask(GL_FALSE);

            basic.use();
            glm::mat4 planeModel = makePlaneModel(clipNormal, clipD, 20.0f);
            glm::mat4 planeMVP = proj * view * planeModel;

            basic.setMat4("uMVP", planeMVP);
            basic.setVec3("uColor", glm::vec3(1.0f, 0.9f, 0.2f));
            basic.setFloat("uAlpha", 0.18f);

            glBindVertexArray(planeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            // Put GL state back to normal so the cube draw is not accidentally blended or depth masked.
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        {
            // Using time makes the animation smooth and independent from frame rate.
            float t = (float)glfwGetTime();

            glm::mat4 model(1.0f);

            // Move the cube slightly up so it is not sitting exactly on the grid.
            model = glm::translate(model, glm::vec3(0.0f, 0.6f, 0.0f));

            // Rotation makes lighting and clipping changes easier to see because different faces pass through the plane.
            model = glm::rotate(model, t, glm::vec3(0.2f, 1.0f, 0.0f));

            // The lit shader uses separate matrices so it can compute lighting in the correct space.
            lit.use();
            lit.setMat4("uModel", model);
            lit.setMat4("uView", view);
            lit.setMat4("uProj", proj);

            // Keep material simple so you can focus on understanding lighting and clipping.
            lit.setVec3("uColor", glm::vec3(0.9f, 0.5f, 0.7f));
            lit.setVec3("uCamPos", camPos);

            // A fixed directional light is easy to reason about while learning basic shading.
            lit.setVec3("uLightDir", glm::normalize(glm::vec3(0.6f, 1.0f, 0.4f)));
            lit.setVec3("uAmbient", glm::vec3(0.25f, 0.25f, 0.25f));
            lit.setFloat("uShininess", 64.0f);

            // Sending the plane to the shader lets it discard fragments on one side, which is how the visible clipping happens.
            lit.setInt("uClipEnabled", clipEnabled ? 1 : 0);
            lit.setVec3("uClipNormal", clipNormal);
            lit.setFloat("uClipD", clipD);

            glBindVertexArray(cubeVAO);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)0);
            glBindVertexArray(0);
        }

        // Swap shows the finished image, and polling events keeps input and window messages updating.
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Delete GL objects explicitly so it is clear what resources were created and owned by this file.
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);

    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

    glDeleteVertexArrays(1, &axesVAO);
    glDeleteBuffers(1, &axesVBO);

    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
