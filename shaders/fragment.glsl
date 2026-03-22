#version 120
varying vec4  v_color;
varying vec2  v_texcoord;
varying float v_eye_dist;

uniform sampler2D tex;
uniform int       tev_mode;
uniform vec4      fog_color;
uniform int       fog_enabled;
uniform float     fog_start;
uniform float     fog_end;

void main() {
    vec4 color;
    if (tev_mode == 0) {
        color = v_color;
    } else {
        color = texture2D(tex, v_texcoord) * v_color;
    }

    if (fog_enabled != 0 && fog_end > fog_start) {
        float fog = clamp((fog_end - v_eye_dist) / (fog_end - fog_start), 0.0, 1.0);
        color.rgb  = mix(fog_color.rgb, color.rgb, fog);
    }

    gl_FragColor = color;
}
