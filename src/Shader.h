#pragma once

#include <string>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class Shader
{
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;

    void setMat4(const char* name, const glm::mat4& value) const;
    void setVec3(const char* name, const glm::vec3& value) const;

    unsigned int id() const { return m_program; }

private:
    unsigned int m_program = 0;

    static std::string readFile(const std::string& path);
    static unsigned int compile(unsigned int type, const std::string& source, const std::string& debugName);
    static unsigned int link(unsigned int vs, unsigned int fs);
};
