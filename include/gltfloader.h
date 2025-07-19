#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <tiny_gltf.h>

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
};

class GltfLoader
{
public:
    bool LoadModel(const std::string& filepath);
    const std::vector<Mesh>& GetMeshes() const { return meshes; }

private:
    std::vector<Mesh> meshes;
    void ProcessNode(const tinygltf::Model& model);
    void LoadMesh(const tinygltf::Model& model, const tinygltf::Mesh& gltfMesh);
};