#version 120

varying vec2 texture_coord;
uniform sampler2D depth_tex;
uniform sampler2D shaded_tex;

uniform float zmin_local;
uniform float dz_local;
uniform float zmin_global;
uniform float dz_global;
uniform int is_2d;

uniform vec3 color;

vec4 shade(vec4 norm)
{
    vec3 light = vec3(0.99 * color.r, 0.96 * color.g, 0.89 * color.b);
    vec3 dark = vec3(0.20 * color.r, 0.25 * color.g, 0.3 * color.b);

    float a = dot(2 * (norm.xyz - vec3(0.5)), vec3(0.0, 0.0, 1.0));
    float b = dot(2 * (norm.xyz - vec3(0.5)), vec3(0.57, -0.57, 0.57));

    if (is_2d == 1)
    {
        a = a*0.5 + 0.5;
        return vec4((a*light + (1-a)*dark), 1);
    }
    else
    {
        return vec4((a*light + (1-a)*dark)*0.35 +
                    (b*light + (1-b)*dark)*0.65, 1);
    }
}

void main() {
    vec4 depth = texture2D(depth_tex, texture_coord);

    float coverage = depth.a;
    if (coverage <= 0.005)
    {
        discard;
    }

    vec4 out_color = shade(texture2D(shaded_tex, texture_coord));
    out_color.a = smoothstep(0.0, 1.0, coverage);
    gl_FragColor = out_color;

    // Recover true unattenuated depth on edge pixels for gl_FragDepth
    float d = coverage > 0.01 ? (depth.r / coverage) : depth.r;
    float fd_local = d * dz_local + zmin_local;
    float fd_global = (fd_local - zmin_global) / dz_global;
    gl_FragDepth = 0.9 - fd_global*0.3;
}
