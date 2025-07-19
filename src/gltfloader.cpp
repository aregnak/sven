#include "gltfloader.h"
#include <iostream>

bool LoadModel(tinygltf::Model& model, const std::string& filepath)
{
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret;
    if (filepath.substr(filepath.find_last_of(".") + 1) == "glb")
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
    else
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);

    if (!warn.empty())
        std::cout << "Warn: " << warn << std::endl;
    if (!err.empty())
        std::cerr << "Err: " << err << std::endl;

    return ret;
}

void GltfLoader::ProcessNode(const tinygltf::Model& model)
{
    for (const auto& mesh : model.meshes)
    {
        LoadMesh(model, mesh);
    }
}

void GltfLoader::LoadMesh(const tinygltf::Model& model, const tinygltf::Mesh& gltfMesh)
{
    for (const auto& primitive : gltfMesh.primitives)
    {
        if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
            continue;

        Mesh mesh;

        const auto& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
        const auto& posView = model.bufferViews[posAccessor.bufferView];
        const auto& posBuffer = model.buffers[posView.buffer];
        const float* posData = reinterpret_cast<const float*>(
            &posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

        const float* normalData = nullptr;
        if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
        {
            const auto& normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
            const auto& normView = model.bufferViews[normAccessor.bufferView];
            const auto& normBuffer = model.buffers[normView.buffer];
            normalData = reinterpret_cast<const float*>(
                &normBuffer.data[normView.byteOffset + normAccessor.byteOffset]);
        }

        const float* texCoordData = nullptr;
        if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
        {
            const auto& texAccessor =
                model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
            const auto& texView = model.bufferViews[texAccessor.bufferView];
            const auto& texBuffer = model.buffers[texView.buffer];
            texCoordData = reinterpret_cast<const float*>(
                &texBuffer.data[texView.byteOffset + texAccessor.byteOffset]);
        }

        for (size_t i = 0; i < posAccessor.count; i++)
        {
            Vertex v{};
            v.position = glm::vec3(posData[i * 3 + 0], posData[i * 3 + 1], posData[i * 3 + 2]);
            if (normalData)
                v.normal =
                    glm::vec3(normalData[i * 3 + 0], normalData[i * 3 + 1], normalData[i * 3 + 2]);
            if (texCoordData)
                v.texCoord = glm::vec2(texCoordData[i * 2 + 0], texCoordData[i * 2 + 1]);
            mesh.vertices.push_back(v);
        }

        const auto& idxAccessor = model.accessors[primitive.indices];
        const auto& idxView = model.bufferViews[idxAccessor.bufferView];
        const auto& idxBuffer = model.buffers[idxView.buffer];

        if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
        {
            const uint16_t* idxData = reinterpret_cast<const uint16_t*>(
                &idxBuffer.data[idxView.byteOffset + idxAccessor.byteOffset]);
            for (size_t i = 0; i < idxAccessor.count; i++)
            {
                mesh.indices.push_back(static_cast<unsigned int>(idxData[i]));
            }
        }
        else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        {
            const uint32_t* idxData = reinterpret_cast<const uint32_t*>(
                &idxBuffer.data[idxView.byteOffset + idxAccessor.byteOffset]);
            for (size_t i = 0; i < idxAccessor.count; i++)
            {
                mesh.indices.push_back(static_cast<unsigned int>(idxData[i]));
            }
        }

        meshes.push_back(mesh);
    }
}