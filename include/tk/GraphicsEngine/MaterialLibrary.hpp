//
// Material Library
//
// materials manage
//
// TODO:
// 1. dynamic clear resources avoid taking up too much memory
// 2. currently, only generate static materials at initialization,
//    expand to dynamic load materials
//

#pragma once

#include "MemoryAllocator.hpp"
#include "../type.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <span>
#include <vector>
#include <map>

namespace tk { namespace graphics_engine {

  // only transform object from local to world
  struct PushConstant 
  {
    glm::mat4       model;
    VkDeviceAddress vertices = {};
  };
  
  // for easy, we only have 2D position and color
  // maybe expand to texture in future
  struct Vertex
  {
    alignas(16) glm::vec2 position;
    alignas(16) glm::vec3 color;
  };

  // mesh have vertices and indices
  // indices use uint8_t for minimize byte necessary
  // simply 2D shape not need so many indices
  struct Mesh
  {
    std::vector<Vertex>   vertices;
    std::vector<uint16_t> indices;
  };

  class MaterialLibrary
  {
  public:
    // HACK: should it be use internal single time command?
    static void init(MemoryAllocator& mem_alloc, class Command& cmd, class DestructorStack& destructor);
    static void destroy();

    static auto& get_mesh_infos() noexcept { return _mesh_infos; }
    static auto& get_mesh_buffer() noexcept { return _mesh_buffer; }

  private:
    static auto get_meshs() -> std::vector<Mesh>;
    static void build_mesh_infos(std::span<MeshInfo> mesh_infos);

    static void generate_materials();

  private:
    MaterialLibrary()  = delete;
    ~MaterialLibrary() = delete;

    MaterialLibrary(MaterialLibrary const&)            = delete;
    MaterialLibrary(MaterialLibrary&&)                 = delete;
    MaterialLibrary& operator=(MaterialLibrary const&) = delete;
    MaterialLibrary& operator=(MaterialLibrary&&)      = delete;

  private:
    struct Material
    {
      ShapeType          type;
      std::vector<Color> colors;
    };

    inline static MemoryAllocator* _mem_alloc = nullptr;

    // HACK: only single mesh data and single mesh buffer, how to use on dynamic( and also materials and mesh infos)
    inline static MeshBuffer _mesh_buffer;
    inline static std::vector<Material> _materials;
    inline static std::map<ShapeType, std::map<Color, MeshInfo>> _mesh_infos;
  };

}}
