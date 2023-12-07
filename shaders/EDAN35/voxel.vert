#version 410
layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 tex_coord_in;// not available
layout (location = 3) in vec3 tangent;// not available
layout (location = 4) in vec3 binormal;// not available

uniform mat4 model_to_world;
uniform mat4 normal_model_to_world;
uniform mat4 world_to_model;
uniform mat4 vertex_world_to_clip;

uniform vec3 camera_position;

// world space
flat out float face_dot_v;

// model space
flat out vec3 model_cam_pos;
out vec3 fV;
out vec3 pos;

void main()
{
    pos = vertex;
    vec3 world_pos = (model_to_world * vec4(vertex, 1)).xyz;
    vec3 V = world_pos - camera_position;

    vec3 world_normal = normalize(vec3(transpose(inverse(model_to_world)) * vec4(normal, 0.0)));
    face_dot_v = dot(world_normal, V);

    fV = (world_to_model * vec4(V, 0)).xyz;
    model_cam_pos = (world_to_model * vec4(camera_position, 1)).xyz;

    gl_Position = vertex_world_to_clip*vec4(world_pos, 1);
}
