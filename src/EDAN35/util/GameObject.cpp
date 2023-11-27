#pragma once
#include "Transform.cpp"
#include "core/Bonobo.h"
#include "core/ShaderProgramManager.hpp"
#include "core/TRSTransform.h"
#include "core/helpers.hpp"
#include "core/node.hpp"

#include <algorithm>
#include <list>
#include <map>

struct shader_program {
  std::string name;
  GLuint *program;
  std::function<void(GLuint)> const set_uniforms;
};

struct texture_program {
  std::string name;
  const GLuint *texture;
  bonobo::material_data material;
};

class GameObject {
public:
  std::string name;
  Transform transform;
  static std::map<std::string, shader_program *> shaderLibrary;
  static std::map<std::string, texture_program *> textureLibrary;

  static void
  addShaderToLibrary(ShaderProgramManager *program_manager, std::string name,
                     std::function<void(GLuint)> const &set_uniforms) {

    GLuint *shader = new GLuint(0u);
    program_manager->CreateAndRegisterProgram(
        name.c_str(),
        {{ShaderType::vertex, "EDAN35/" + name + ".vert"},
         {ShaderType::fragment, "EDAN35/" + name + ".frag"}},
        *shader);
    if (shader == 0u)
      LogError(("Failed to load " + name + " shader").c_str());

    auto program = new shader_program{name, shader, set_uniforms};
    shaderLibrary.insert(std::make_pair(name, program));
  }

  static void setTextureLibrary(std::vector<texture_program *> textures) {
    for (auto texture : textures) {
      textureLibrary.insert(std::make_pair(texture->name, texture));
    }
  }

  GameObject(std::string name) { this->name = name; }

  void setMesh(bonobo::mesh_data const &shape) { node.set_geometry(shape); }
  void setShader(GLuint const *const program,
                 std::function<void(GLuint)> const &set_uniforms) {
    node.set_program(program, set_uniforms);
  }

  void setEnabled(bool enabled) { this->enabled = enabled; }
  bool setShader(std::string name) {
    if (shaderLibrary.count(name)) {
      auto shader = shaderLibrary[name];
      auto a = shader->set_uniforms;

      // printf("Setting shader for %s to %s\n", this->name.c_str(),
      // shader->name.c_str());
      setShader(shader->program, shader->set_uniforms);
      return true;
    } else
      return false;
  }

  bool addTexture(std::string name) {
    if (textureLibrary.count(name)) {
      auto texture = textureLibrary[name];
      // printf("Setting texture for %s to %s\n", this->name.c_str(),
      // name.c_str());
      addTexture(texture->name, *texture->texture, GL_TEXTURE_2D);
      node.set_material_constants(texture->material);
      return true;
    }
    return false;
  }
  void addTexture(std::string const &name, GLuint const texture,
                  GLenum const type) {
    node.add_texture(name, texture, type);
  }

  void setTexture(std::string const &name, GLuint const texture,
                  GLenum const type) {
    node.set_texture(name, texture, type);
  }

  void setRandomTexture() {
    auto it = textureLibrary.begin();
    std::advance(it, rand() % textureLibrary.size());
    printf("Setting random texture for %s to %s\n", this->name.c_str(),
           it->first.c_str());
    if (it->first != "Earth" && it->first != "Moon") {
      addTexture(it->first);
    } else
      setRandomTexture();
  }

  void setMaterialConstants(bonobo::material_data const &constants) {
    node.set_material_constants(constants);
  }

  glm::mat4 render(glm::mat4 const &view_projection,
                   glm::mat4 const &parent_transform, bool show_basis,
                   float basis_length, float basis_width) {
    glm::mat4 tf = parent_transform * transform.getMatrix();
    if (!enabled)
      return tf;
    glEnable(GL_BLEND); // temp fix for powerup transparencty
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    node.renderCoool(tf, view_projection);
    glDisable(GL_BLEND);
    if (show_basis) {
      bonobo::renderBasis(basis_width, basis_length, view_projection, tf);
    }
    for (auto child : _children) {
      child->render(view_projection, tf, show_basis, basis_length, basis_width);
    }
    return tf;
  }


  void addChild(GameObject *child) { _children.push_front(child); }
  void removeChild(GameObject *child) {
    auto it = std::find(_children.begin(), _children.end(), child);
    if (*it ==
        child) { // it is last element if not found so we run an extra check
      _children.erase(it);
    }
    // printf("Removed child, count: %ld\n", _children.size());
  }

private:
  std::list<GameObject *> _children; // could be hashmap later
  Node node;
  bool enabled = true;
};

std::map<std::string, shader_program *> GameObject::shaderLibrary = {};
std::map<std::string, texture_program *> GameObject::textureLibrary = {};
