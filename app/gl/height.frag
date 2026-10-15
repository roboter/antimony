#version 120

varying vec2 texture_coord;
uniform sampler2D depth_tex;

uniform float zmin_local;
uniform float dz_local;
uniform float zmin_global;
uniform float dz_global;

uniform vec3 color;

void main() {
    vec4 depth = texture2D(depth_tex, texture_coord);

    float coverage = depth.a;
    if (coverage <= 0.005)
    {
        discard;
    }

    float d = coverage > 0.01 ? (depth.r / coverage) : depth.r;
    float fd_local = d * dz_local + zmin_local;
    float fd_global = (fd_local - zmin_global) / dz_global;

    float alpha = smoothstep(0.0, 1.0, coverage);
    gl_FragColor = vec4(color.r * fd_global * alpha,
                        color.g * fd_global * alpha,
                        color.b * fd_global * alpha,
                        alpha);
}
