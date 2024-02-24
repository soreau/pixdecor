static const char *generic_effect_vertex_shader = R"(
#version 320 es

in highp vec2 position;
out highp vec2 relativePosition;

uniform mat4 MVP;
uniform vec2 topLeftCorner;

void main() {
    relativePosition = position.xy - topLeftCorner;
    gl_Position = MVP * vec4(position.xy, 0.0, 1.0);
})";

static const char *generic_effect_fragment_header = R"(
#version 320 es
precision highp float;
precision highp image2D;

in highp vec2 relativePosition;
uniform float current_time;
uniform float width;
uniform float height;

out vec4 fragColor;
)";

static const char *generic_effect_fragment_main = R"(
void main()
{
    fragColor = overlay_function(relativePosition);
})";

// ported from https://www.shadertoy.com/view/WdXBW4
static const char *effect_clouds_fragment = R"(
float cloudscale=2.1;  // Added cloudscale parameter
const mat2 m = mat2(1.6, 1.2, -1.2, 1.6);

vec2 hash(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float noise(vec2 p) {
    const float K1 = 0.366025404; // (sqrt(3)-1)/2;
    const float K2 = 0.211324865; // (3-sqrt(3))/6;
    vec2 i = floor(p + (p.x + p.y) * K1);
    vec2 a = p - i + (i.x + i.y) * K2;
    vec2 o = (a.x > a.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec2 b = a - o + K2;
    vec2 c = a - 1.0 + 2.0 * K2;
    vec3 h = max(0.5 - vec3(dot(a, a), dot(b, b), dot(c, c)), 0.0);
    vec3 n = h * h * h * h * vec3(dot(a, hash(i + 0.0)), dot(b, hash(i + o)), dot(c, hash(i + 1.0)));
    return dot(n, vec3(70.0));
}

float fbm(vec2 n) {
    float total = 0.0, amplitude = 0.1;
    for (int i = 0; i < 7; i++) {
        total += noise(n) * amplitude;
        n = m * n;
        amplitude *= 0.4;
    }
    return total;
}

vec4 effect_color(vec2 pos)
{
    vec2 uv = pos / vec2(width, height);

    float time = 0.003 * current_time;  // Time variable for animation

    float cloudPattern1 = fbm(uv * 10.0 * cloudscale + time);
    float cloudPattern2 = fbm(uv * 5.0 * cloudscale + time);
    float cloudPattern3 = fbm(uv * 3.0 * cloudscale + time);

    // Combine different cloud patterns with different weights
    float cloudPattern = 0.5 * cloudPattern1 + 0.3 * cloudPattern2 + 0.2 * cloudPattern3;

    // Ridge noise shape
    float ridgeNoiseShape = 0.0;
    uv *= cloudscale * 1.1;  // Adjust scale
    float weight = 0.8;
    for (int i = 0; i < 8; i++) {
        ridgeNoiseShape += abs(weight * noise(uv));
        uv = m * uv + time;
        weight *= 0.7;
    }

    // Noise shape
    float noiseShape = 0.0;
    uv = vec2(pos) / vec2(width, height);
    uv *= cloudscale * 1.1;  // Adjust scale
    weight = 0.7;
    for (int i = 0; i < 8; i++) {
        noiseShape += weight * noise(uv);
        uv = m * uv + time;
        weight *= 0.6;
    }

    noiseShape *= ridgeNoiseShape + noiseShape;

    // Noise color
    float noiseColor = 0.0;
    uv = vec2(pos) / vec2(width, height);
    uv *= 2.0 * cloudscale;  // Adjust scale
    weight = 0.4;
    for (int i = 0; i < 7; i++) {
        noiseColor += weight * noise(uv);
        uv = m * uv + time;
        weight *= 0.6;
    }

    // Noise ridge color
    float noiseRidgeColor = 0.0;
    uv = vec2(pos) / vec2(width, height);
    uv *= 3.0 * cloudscale;  // Adjust scale
    weight = 0.4;
    for (int i = 0; i < 7; i++) {
        noiseRidgeColor += abs(weight * noise(uv));
        uv = m * uv + time;
        weight *= 0.6;
    }

    noiseColor += noiseRidgeColor;

    // Sky tint
    float skytint = 0.5;
    vec3 skyColour1 = vec3(0.1, 0.2, 0.3);
    vec3 skyColour2 = vec3(0.4, 0.7, 1.0);
    vec3 skycolour = mix(skyColour1, skyColour2, smoothstep(0.4, 0.6, uv.y));

    // Cloud darkness
    float clouddark = 0.5;

    // Cloud Cover, Cloud Alpha
    float cloudCover = 0.01;
    float cloudAlpha = 2.0;

    // Movement effect
    uv = uv + time;

    // Use a bright color for clouds
    vec3 cloudColor = vec3(1.0, 1.0, 1.0); ;  // Bright white color for clouds

    // Mix the cloud color with the background, considering darkness, cover, and alpha
    vec3 finalColor = mix(skycolour, cloudColor * clouddark, cloudPattern + noiseShape + noiseColor) * (1.0 - cloudCover) + cloudColor * cloudCover;
    finalColor = mix(skycolour, finalColor, cloudAlpha);

    return vec4(finalColor, 1.0);
})";
