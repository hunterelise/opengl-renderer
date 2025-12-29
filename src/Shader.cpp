#include "Shader.h"

#include <glad/glad.h>

#include <fstream>
#include <sstream>
#include <iostream>

static void printShaderLog(unsigned int shader, const std::string& name)
{
    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success) return;

    int len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    std::string log(len, '\0');
    glGetShaderInfoLog(shader, len, nullptr, log.data());

    std::cerr << "[Shader compile error] " << name << "\n" << log << "\n";
}

static void printProgramLog(unsigned int program)
{
    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success) return;

    int len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
    std::string log(len, '\0');
    glGetProgramInfoLog(program, len, nullptr, log.data());

    std::cerr << "[Program link error]\n" << log << "\n";
}

std::string Shader::readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::in);
    if (!file)
        throw std::runtime_error("Failed to open file: " + path);

    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

unsigned int Shader::compile(unsigned int type, const std::string& source, const std::string& debugName)
{
    unsigned int s = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    printShaderLog(s, debugName);
    return s;
}

unsigned int Shader::link(unsigned int vs, unsigned int fs)
{
    unsigned int p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);

    printProgramLog(p);

    glDetachShader(p, vs);
    glDetachShader(p, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    return p;
}

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
    const std::string vsSource = readFile(vertexPath);
    const std::string fsSource = readFile(fragmentPath);

    const unsigned int vs = compile(GL_VERTEX_SHADER, vsSource, vertexPath);
    const unsigned int fs = compile(GL_FRAGMENT_SHADER, fsSource, fragmentPath);
    m_program = link(vs, fs);
}

Shader::~Shader()
{
    if (m_program != 0)
        glDeleteProgram(m_program);
}

void Shader::use() const
{
    glUseProgram(m_program);
}

void Shader::setMat4(const char* name, const glm::mat4& value) const
{
    const int loc = glGetUniformLocation(m_program, name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, &value[0][0]);
}

void Shader::setVec3(const char* name, const glm::vec3& value) const
{
    const int loc = glGetUniformLocation(m_program, name);
    glUniform3f(loc, value.x, value.y, value.z);
}
