#pragma once
#include "parametric_shapes.hpp"
#include "core/Log.h"

#include <glm/glm.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

bonobo::mesh_data parametric_shapes::createCube(float const width,
                                                float const height,
                                                float const depth) {
  auto vertices = std::vector<glm::vec3>(8);
  auto trigs = std::vector<glm::uvec3>(12);

  bonobo::mesh_data data;

  // vertices[0] = glm::vec3(-width * .5f, -height * .5f, depth * .5f);
  // vertices[1] = glm::vec3(width * .5f, -height * .5f, depth * .5f);
  // vertices[2] = glm::vec3(width * .5f, height * .5f, depth * .5f);
  // vertices[3] = glm::vec3(-width * .5f, height * .5f, depth * .5f);
  // vertices[4] = glm::vec3(-width * .5f, -height * .5f, -depth * .5f);
  // vertices[5] = glm::vec3(width * .5f, -height * .5f, -depth * .5f);
  // vertices[6] = glm::vec3(width * .5f, height * .5f, -depth * .5f);
  // vertices[7] = glm::vec3(-width * .5f, height * .5f, -depth * .5f);
  // simplify with a loop:
  for (int i = 0; i < 8; i++) {
    vertices[i] = glm::vec3((i & 1) ? width * .5f : -width * .5f,
                            (i & 2) ? height * .5f : -height * .5f,
                            (i & 4) ? depth * .5f : -depth * .5f);
  }
  // create indicies array with a for loop
  for (int i = 0; i < 8; i++) {
    trigs[i * 3] = glm::uvec3(i, (i + 1) % 4, i + 4);
    trigs[i * 3 + 1] = glm::uvec3(i + 4, (i + 1) % 4, (i + 1) % 4 + 4);
    trigs[i * 3 + 2] = glm::uvec3(i + 4, i, (i + 1) % 4);

  }

  //
  // NOTE:
  //
  // Only the values preceeded by a `\todo` tag should be changed, the
  // other ones are correct!
  //

  // Create a Vertex Array Object: it will remember where we stored the
  // data on the GPU, and  which part corresponds to the vertices, which
  // one for the normals, etc.
  //
  // The following function will create new Vertex Arrays, and pass their
  // name in the given array (second argument). Since we only need one,
  // pass a pointer to `data.vao`.
  glGenVertexArrays(1, &data.vao);

  // To be able to store information, the Vertex Array has to be bound
  // first.
  glBindVertexArray(data.vao);

  // BO
  glGenBuffers(1, &data.bo);
  glBindBuffer(GL_ARRAY_BUFFER, data.bo);
  auto vertBufferSize = vertices.size() * sizeof(glm::vec3);
  // auto texBufferSize = texCoords.size() * sizeof(glm::vec3);
  auto bufferSize = vertBufferSize;// + texBufferSize;
  glBufferData(GL_ARRAY_BUFFER, bufferSize, nullptr, GL_STATIC_DRAW);

  glBufferSubData(GL_ARRAY_BUFFER, 0, vertBufferSize, vertices.data());
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(0x0));

  // glBufferSubData(GL_ARRAY_BUFFER, vertBufferSize, texBufferSize,
  //                 texCoords.data());
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3,
      GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const *>(vertBufferSize));

  // Now, let's allocate a second one for the indices.
  //
  // Have the buffer's name stored into `data.ibo`.
  glGenBuffers(1, /*! \todo fill me */ &data.ibo);

  // We still want a 1D-array, but this time it should be a 1D-array of
  // elements, aka. indices!
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
               /*! \todo bind the previously generated Buffer */ data.ibo);

  glBufferData(
      GL_ELEMENT_ARRAY_BUFFER,
      /*! \todo how many bytes should the buffer contain? */
      sizeof(glm::uvec3) * trigs.size(),
      /* where is the data stored on the CPU? */ trigs.data(),
      /* inform OpenGL that the data is modified once, but used often */
      GL_STATIC_DRAW);

  data.indices_nb = /*! \todo how many indices do we have? */ trigs.size() * 3;

  // All the data has been recorded, we can unbind them.
  glBindVertexArray(0u);
  glBindBuffer(GL_ARRAY_BUFFER, 0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

  return data;
}
bonobo::mesh_data
parametric_shapes::createQuad(float const width, float const height,
                              unsigned int const horizontal_split_count,
                              unsigned int const vertical_split_count) {
  int vert_count_x = horizontal_split_count + 2;
  int vert_count_z = vertical_split_count + 2;
  int vert_count = vert_count_x * vert_count_z;

  auto vertices = std::vector<glm::vec3>(vert_count_x * vert_count_z);
  auto texCoords = std::vector<glm::vec3>(vert_count_x * vert_count_z);

  bonobo::mesh_data data;

  int index = 0;
  for (size_t i = 0; i < vert_count_x; i++) {
    for (size_t j = 0; j < vert_count_z; j++) {
      vertices[index] =
          glm::vec3(i * width / (vert_count_x - 1), // float conversion ok?
                    0, j * height / (vert_count_z - 1));
      texCoords[index++] = glm::vec3(float(i) / float(vert_count_x - 1),
                                     float(j) / float(vert_count_z - 1), 0.0f);
    }
  }

  auto trigs =
      std::vector<glm::uvec3>((vert_count_x - 1) * (vert_count_z - 1) * 2);

  for (int i = 0; i < vert_count - vert_count_z;
       i++) // hoppa över sista z raden, de ska inte binda till något
  {
    if ((i + 1) % vert_count_z == 0)
      continue; // dont bind from last vert in row
    trigs.push_back(glm::uvec3(i, i + vert_count_z, i + 1 + vert_count_z));
    trigs.push_back(glm::uvec3(i, i + vert_count_z + 1, i + 1));
  }

  //
  // NOTE:
  //
  // Only the values preceeded by a `\todo` tag should be changed, the
  // other ones are correct!
  //

  // Create a Vertex Array Object: it will remember where we stored the
  // data on the GPU, and  which part corresponds to the vertices, which
  // one for the normals, etc.
  //
  // The following function will create new Vertex Arrays, and pass their
  // name in the given array (second argument). Since we only need one,
  // pass a pointer to `data.vao`.
  glGenVertexArrays(1, &data.vao);

  // To be able to store information, the Vertex Array has to be bound
  // first.
  glBindVertexArray(data.vao);

  // BO
  glGenBuffers(1, &data.bo);
  glBindBuffer(GL_ARRAY_BUFFER, data.bo);
  auto vertBufferSize = vertices.size() * sizeof(glm::vec3);
  auto texBufferSize = texCoords.size() * sizeof(glm::vec3);
  auto bufferSize = vertBufferSize + texBufferSize;
  glBufferData(GL_ARRAY_BUFFER, bufferSize, nullptr, GL_STATIC_DRAW);

  glBufferSubData(GL_ARRAY_BUFFER, 0, vertBufferSize, vertices.data());
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(0x0));

  glBufferSubData(GL_ARRAY_BUFFER, vertBufferSize, texBufferSize,
                  texCoords.data());
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3,
      GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const *>(vertBufferSize));

  // Now, let's allocate a second one for the indices.
  //
  // Have the buffer's name stored into `data.ibo`.
  glGenBuffers(1, /*! \todo fill me */ &data.ibo);

  // We still want a 1D-array, but this time it should be a 1D-array of
  // elements, aka. indices!
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
               /*! \todo bind the previously generated Buffer */ data.ibo);

  glBufferData(
      GL_ELEMENT_ARRAY_BUFFER,
      /*! \todo how many bytes should the buffer contain? */
      sizeof(glm::uvec3) * trigs.size(),
      /* where is the data stored on the CPU? */ trigs.data(),
      /* inform OpenGL that the data is modified once, but used often */
      GL_STATIC_DRAW);

  data.indices_nb = /*! \todo how many indices do we have? */ trigs.size() * 3;

  // All the data has been recorded, we can unbind them.
  glBindVertexArray(0u);
  glBindBuffer(GL_ARRAY_BUFFER, 0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

  return data;
}

bonobo::mesh_data
parametric_shapes::createSphere(float const radius,
                                unsigned int const longitude_split_count,
                                unsigned int const latitude_split_count) {
  auto calc_vertex = [radius](float st, float ct, float sp,
                              float cp) -> glm::vec3 {
    return radius * glm::vec3(st * sp, -cp, ct * sp);
  };
  auto calc_tangent = [radius](float st, float ct, float sp,
                               float cp) -> glm::vec3 {
    return glm::normalize(glm::vec3(ct, 0, -st));
  };
  auto calc_bitangent = [radius](float st, float ct, float sp,
                                 float cp) -> glm::vec3 {
    return glm::normalize(glm::vec3(st * cp, sp, ct * cp));
  };
  return createShape(longitude_split_count, latitude_split_count, calc_vertex,
                     calc_tangent, calc_bitangent, glm::two_pi<float>(),
                     glm::pi<float>());
}

bonobo::mesh_data parametric_shapes::createTorus(float const major_radius,
                                                 float const minor_radius,
                                                 uint const major_split_count,
                                                 uint const minor_split_count) {
  auto r1 = major_radius;
  auto r2 = minor_radius;
  auto calc_vertex = [r1, r2](float st, float ct, float sp,
                              float cp) -> glm::vec3 {
    return glm::vec3((r1 + r2 * ct) * cp, -r2 * st, (r1 + r2 * ct) * sp);
  };
  auto calc_tangent = [r1](float st, float ct, float sp,
                           float cp) -> glm::vec3 {
    return glm::normalize(glm::vec3(-st * cp, -ct, -st * sp));
  };
  auto calc_bitangent = [r1, r2](float st, float ct, float sp,
                                 float cp) -> glm::vec3 {
    return glm::normalize(
        glm::vec3(-(r1 + r2 * ct) * sp, 0, (r1 + r2 * ct) * cp));
  };
  return createShape(minor_split_count, major_split_count, calc_vertex,
                     calc_tangent, calc_bitangent, glm::two_pi<float>(),
                     glm::two_pi<float>());
}

bonobo::mesh_data parametric_shapes::createShape(
    unsigned int const longitude_split_count,
    unsigned int const latitude_split_count,
    std::function<glm::vec3(float st, float ct, float sp, float cp)>
        calc_vertex,
    std::function<glm::vec3(float st, float ct, float sp, float cp)>
        calc_tangent,
    std::function<glm::vec3(float st, float ct, float sp, float cp)>
        calc_bitangent,
    float max_theta, float max_phi) {
  auto const lat_edge = latitude_split_count + 1u;
  auto const long_edge = longitude_split_count + 1u;
  auto const lat_verts = lat_edge + 1u;
  auto const long_verts = long_edge + 1u;
  uint const vertices_nb = lat_verts * long_verts;

  auto vertices = std::vector<glm::vec3>(vertices_nb);
  auto normals = std::vector<glm::vec3>(vertices_nb);
  auto bitangents = std::vector<glm::vec3>(vertices_nb);
  auto tangents = std::vector<glm::vec3>(vertices_nb);
  auto texCoords = std::vector<glm::vec3>(vertices_nb);

  auto const d_theta = max_theta / long_edge;
  auto const d_phi = max_phi / lat_edge;

  float theta = 0;
  uint idx = 0;
  for (int i = 0; i < long_verts; i++) {
    auto sin_theta = sin(theta);
    auto cos_theta = cos(theta);

    float phi = 0;
    for (int j = 0; j < lat_verts; j++) {
      auto sin_phi = sin(phi);
      auto cos_phi = cos(phi);
      vertices[idx] = calc_vertex(sin_theta, cos_theta, sin_phi, cos_phi);
      tangents[idx] = calc_tangent(sin_theta, cos_theta, sin_phi, cos_phi);
      bitangents[idx] = calc_bitangent(sin_theta, cos_theta, sin_phi, cos_phi);
      normals[idx] = glm::normalize(glm::cross(bitangents[idx], tangents[idx]));
      // texCoords[idx] = glm::vec3(static_cast<float>(j) /
      // (static_cast<float>(lat_verts)),
      //                              static_cast<float>(i) /
      //                              (static_cast<float>(long_verts)), 0.0f);
      texCoords[idx] = glm::vec3(theta / max_theta, phi / max_phi, 0.0f);
      // if(j == 0)
      // 	normals[idx] = glm::vec3(0, -1, 0);
      // else if(j == lat_verts-1)
      // 	normals[idx] = glm::vec3(0, 1, 0);
      idx++;
      phi += d_phi;
    }
    theta += d_theta;
  }

  auto trigs = std::vector<glm::uvec3>(vertices_nb);
  for (int i = 0; i < vertices_nb - lat_verts;
       i++) //-lat_verts, hoppa över sista vertikala raden, de ska inte binda
            // till något
  {
    // 0, 1, 2
    // 0, 2, 3
    if ((i + 1) % lat_verts ==
        0) // hoppa över alla punkter på nordpolen, de ska inte binda till något
      continue;
    trigs.push_back(glm::uvec3(i, i + lat_verts, i + 1));
    trigs.push_back(glm::uvec3(i + 1, i + lat_verts, i + 1 + lat_verts));
    // trigs[i*2] = glm::uvec3(i, i + lat_verts, i + 1);
    // trigs[i*2+1] = glm::uvec3(i + 1, i + lat_verts, i + 1 + lat_verts);
  }
  // printf("vertices:\n");
  // for (int i = 0; i < vertices.size(); i++)
  // {
  //     printf("%f %f %f\n", vertices[i].x, vertices[i].y, vertices[i].z);
  //     /* code */
  // }
  // printf("trigs:\n");
  // for (int i = 0; i < trigs.size(); i++)
  // {
  //     printf("%d %d %d\n", trigs[i].x, trigs[i].y, trigs[i].z);
  // }

  //! \todo Implement this function
  bonobo::mesh_data data;

  // VAO
  glGenVertexArrays(1, &data.vao);
  glBindVertexArray(data.vao);
  // BO
  glGenBuffers(1, &data.bo);
  glBindBuffer(GL_ARRAY_BUFFER, data.bo);
  auto subBufferSize =
      sizeof(glm::vec3) *
      (vertices_nb); // whole buffer is x4 position, normal, binormal, tangent
  glBufferData(GL_ARRAY_BUFFER, subBufferSize * 5, nullptr, GL_STATIC_DRAW);

  glBufferSubData(GL_ARRAY_BUFFER, 0, subBufferSize, vertices.data());
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(0x0));

  glBufferSubData(GL_ARRAY_BUFFER, subBufferSize, subBufferSize,
                  normals.data());
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::normals));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(subBufferSize));

  glBufferSubData(GL_ARRAY_BUFFER, subBufferSize * 2, subBufferSize,
                  bitangents.data());
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::binormals));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3,
      GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<GLvoid const *>(subBufferSize * 2));

  glBufferSubData(GL_ARRAY_BUFFER, subBufferSize * 3, subBufferSize,
                  tangents.data());
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::tangents));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(subBufferSize * 3));

  glBufferSubData(GL_ARRAY_BUFFER, subBufferSize * 4, subBufferSize,
                  texCoords.data());
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3,
      GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<GLvoid const *>(subBufferSize * 4));

  glBindBuffer(GL_ARRAY_BUFFER, 0u);
  // IBO
  glGenBuffers(1, &data.ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(glm::uvec3) * trigs.size(),
               trigs.data(), GL_STATIC_DRAW);

  data.indices_nb = trigs.size() * 3;
  // All the data has been recorded, we can unbind them.
  glBindVertexArray(0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

  return data;
}
bonobo::mesh_data
parametric_shapes::createCircleRing(float const radius,
                                    float const spread_length,
                                    unsigned int const circle_split_count,
                                    unsigned int const spread_split_count) {
  auto const circle_slice_edges_count = circle_split_count + 1u;
  auto const spread_slice_edges_count = spread_split_count + 1u;
  auto const circle_slice_vertices_count = circle_slice_edges_count + 1u;
  auto const spread_slice_vertices_count = spread_slice_edges_count + 1u;
  auto const vertices_nb =
      circle_slice_vertices_count * spread_slice_vertices_count;

  auto vertices = std::vector<glm::vec3>(vertices_nb);
  auto normals = std::vector<glm::vec3>(vertices_nb);
  auto texcoords = std::vector<glm::vec3>(vertices_nb);
  auto tangents = std::vector<glm::vec3>(vertices_nb);
  auto binormals = std::vector<glm::vec3>(vertices_nb);

  float const spread_start = radius - 0.5f * spread_length;
  float const d_theta =
      glm::two_pi<float>() / (static_cast<float>(circle_slice_edges_count));
  float const d_spread =
      spread_length / (static_cast<float>(spread_slice_edges_count));

  // generate vertices iteratively
  size_t index = 0u;
  float theta = 0.0f;
  for (unsigned int i = 0u; i < circle_slice_vertices_count; ++i) {
    float const cos_theta = std::cos(theta);
    float const sin_theta = std::sin(theta);

    float distance_to_centre = spread_start;
    for (unsigned int j = 0u; j < spread_slice_vertices_count; ++j) {
      // vertex
      vertices[index] = glm::vec3(distance_to_centre * cos_theta,
                                  distance_to_centre * sin_theta, 0.0f);

      // texture coordinates
      texcoords[index] =
          glm::vec3(static_cast<float>(j) /
                        (static_cast<float>(spread_slice_vertices_count)),
                    static_cast<float>(i) /
                        (static_cast<float>(circle_slice_vertices_count)),
                    0.0f);

      // tangent
      auto const t = glm::vec3(cos_theta, sin_theta, 0.0f);
      tangents[index] = t;

      // binormal
      auto const b = glm::vec3(-sin_theta, cos_theta, 0.0f);
      binormals[index] = b;

      // normal
      auto const n = glm::cross(t, b);
      normals[index] = n;

      distance_to_centre += d_spread;
      ++index;
    }

    theta += d_theta;
  }

  // create index array
  auto index_sets = std::vector<glm::uvec3>(2u * circle_slice_edges_count *
                                            spread_slice_edges_count);

  // generate indices iteratively
  index = 0u;
  for (unsigned int i = 0u; i < circle_slice_edges_count; ++i) {
    for (unsigned int j = 0u; j < spread_slice_edges_count; ++j) {
      index_sets[index] =
          glm::uvec3(spread_slice_vertices_count * (i + 0u) + (j + 0u),
                     spread_slice_vertices_count * (i + 0u) + (j + 1u),
                     spread_slice_vertices_count * (i + 1u) + (j + 1u));
      ++index;

      index_sets[index] =
          glm::uvec3(spread_slice_vertices_count * (i + 0u) + (j + 0u),
                     spread_slice_vertices_count * (i + 1u) + (j + 1u),
                     spread_slice_vertices_count * (i + 1u) + (j + 0u));
      ++index;
    }
  }

  bonobo::mesh_data data;
  glGenVertexArrays(1, &data.vao);
  assert(data.vao != 0u);
  glBindVertexArray(data.vao);

  auto const vertices_offset = 0u;
  auto const vertices_size =
      static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
  auto const normals_offset = vertices_size;
  auto const normals_size =
      static_cast<GLsizeiptr>(normals.size() * sizeof(glm::vec3));
  auto const texcoords_offset = normals_offset + normals_size;
  auto const texcoords_size =
      static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec3));
  auto const tangents_offset = texcoords_offset + texcoords_size;
  auto const tangents_size =
      static_cast<GLsizeiptr>(tangents.size() * sizeof(glm::vec3));
  auto const binormals_offset = tangents_offset + tangents_size;
  auto const binormals_size =
      static_cast<GLsizeiptr>(binormals.size() * sizeof(glm::vec3));
  auto const bo_size =
      static_cast<GLsizeiptr>(vertices_size + normals_size + texcoords_size +
                              tangents_size + binormals_size);
  glGenBuffers(1, &data.bo);
  assert(data.bo != 0u);
  glBindBuffer(GL_ARRAY_BUFFER, data.bo);
  glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

  glBufferSubData(GL_ARRAY_BUFFER, vertices_offset, vertices_size,
                  static_cast<GLvoid const *>(vertices.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(0x0));

  glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size,
                  static_cast<GLvoid const *>(normals.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::normals));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(normals_offset));

  glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size,
                  static_cast<GLvoid const *>(texcoords.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3,
      GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<GLvoid const *>(texcoords_offset));

  glBufferSubData(GL_ARRAY_BUFFER, tangents_offset, tangents_size,
                  static_cast<GLvoid const *>(tangents.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::tangents));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(tangents_offset));

  glBufferSubData(GL_ARRAY_BUFFER, binormals_offset, binormals_size,
                  static_cast<GLvoid const *>(binormals.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::binormals));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3,
      GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<GLvoid const *>(binormals_offset));

  glBindBuffer(GL_ARRAY_BUFFER, 0u);

  data.indices_nb = static_cast<GLsizei>(index_sets.size() * 3u);
  glGenBuffers(1, &data.ibo);
  assert(data.ibo != 0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(index_sets.size() * sizeof(glm::uvec3)),
               reinterpret_cast<GLvoid const *>(index_sets.data()),
               GL_STATIC_DRAW);

  glBindVertexArray(0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

  return data;
}
