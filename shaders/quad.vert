#version 430 core

// Full-screen triangle trick: no VBO needed, driven entirely by gl_VertexID.
// The oversized triangle (covering [-1,3]x[-1,3]) clips to the full NDC quad.
void main() {
    const vec2 verts[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);
}
