#version 120
varying vec4  v_color;
varying vec2  v_texcoord;
varying float v_eye_dist;

void main() {
    vec4 eye_pos   = gl_ModelViewMatrix * gl_Vertex;
    gl_Position    = gl_ProjectionMatrix * eye_pos;
    v_color        = gl_Color;
    v_texcoord     = vec2(gl_MultiTexCoord0);
    v_eye_dist     = -eye_pos.z;  // z is negative in eye space, negate for positive dist
}
