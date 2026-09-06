#version 410
// Draw three vertices with a bound empty VAO; no vertex buffers required.
out vec2 clip_position;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    clip_position = p * 2.0 - 1.0;
    gl_Position = vec4(clip_position, 1.0, 1.0);
}
