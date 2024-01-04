#include "deco-theme.hpp"
#include <wayfire/core.hpp>
#include <wayfire/opengl.hpp>
#include <config.h>
#include <map>
#include <GLES3/gl32.h>

namespace wf
{
namespace decor
{



static const char *diffuse2_source =
    R"(/*
#version 320 es
precision highp float;
precision highp image2D;

layout(binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 8) uniform float current_time;

vec3 effect1(vec2 uv, float time) {
    float t = mod(time * 0.005, 6.0);
    float rt = 0.00000000000001 * sin(t * 0.45);

    mat2 m1 = mat2(cos(rt), -sin(rt), -sin(rt), cos(rt));
    vec2 uva = uv * m1 * 0.5;
    float irt = 0.005 * cos(t * 0.05);
    mat2 m2 = mat2(sin(irt), cos(irt), -cos(irt), sin(irt));

    for (int i = 1; i < 40; i += 1) {
        float it = float(i);
        uva *= m2;
        uva.y += -1.0 + (0.6 / it) * cos(t + it * uva.x + 0.5 * it) * float(mod(it, 0.5) == 0.0);
        uva.x += 1.0 + (0.5 / it) * cos(t + it * uva.y * 0.1 / 5.0 + 0.5 * (it + 15.0));
    }

    float n = 0.5;
    float r = n + n * sin(4.0 * uva.x + t);
    float gb = n + n * sin(3.0 * uva.y);
    return vec3(r, gb * 0.8 * r, gb * r);
}

vec3 swirlEffect(vec2 uv, float time) {
    // Add swirling motion with a slower rate
    float swirlAmount = 1.5;
    float angle = 0.00001 * current_time;  // Adjust the multiplier to control the speed
    float s = sin(angle);
    float c = cos(angle);
    mat2 rotationMatrix = mat2(c, -s, s, c);
    vec2 rotatedUV = rotationMatrix * uv;
    vec2 swirledUV = uv + swirlAmount * vec2(sin(rotatedUV.y), cos(rotatedUV.x));
    
    // Apply the effect to the swirled UV
    return effect1(swirledUV, time);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    vec2 uv = vec2(pos) / vec2(1000, 2000);
    uv = 2.0 * uv - 1.0;
    uv *= (10.3 + 0.1 * sin(current_time * 0.01));

    // Apply the swirling effect
    vec3 col = swirlEffect(uv, current_time);

    // Output the effect result to the intermediate texture (out_tex)
    imageStore(out_tex, pos, vec4(col, 1.0));
}
*/

)";

static const char *advect2_source =
    R"(

)";




static const char *render_source =
    R"(
#version 320 es
precision highp float;
precision highp image2D;

layout(binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(location = 8) uniform float current_time;
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

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = vec2(pos) / vec2(gl_NumWorkGroups.x * gl_WorkGroupSize.x, gl_NumWorkGroups.y * gl_WorkGroupSize.y);

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
    uv = vec2(pos) / vec2(gl_NumWorkGroups.x * gl_WorkGroupSize.x, gl_NumWorkGroups.y * gl_WorkGroupSize.y);
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
    uv = vec2(pos) / vec2(gl_NumWorkGroups.x * gl_WorkGroupSize.x, gl_NumWorkGroups.y * gl_WorkGroupSize.y);
    uv *= 2.0 * cloudscale;  // Adjust scale
    weight = 0.4;
    for (int i = 0; i < 7; i++) {
        noiseColor += weight * noise(uv);
        uv = m * uv + time;
        weight *= 0.6;
    }

    // Noise ridge color
    float noiseRidgeColor = 0.0;
    uv = vec2(pos) / vec2(gl_NumWorkGroups.x * gl_WorkGroupSize.x, gl_NumWorkGroups.y * gl_WorkGroupSize.y);
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

    imageStore(out_tex, pos, vec4(finalColor, 1.0));
}


)";

GLuint texture1, texture2; 

void setup_shader(GLuint *program, std::string source)
{
    auto compute_shader = OpenGL::compile_shader(source.c_str(), GL_COMPUTE_SHADER);
    auto compute_program  = GL_CALL(glCreateProgram());
    GL_CALL(glAttachShader(compute_program, compute_shader));
    GL_CALL(glLinkProgram(compute_program));

    int s = GL_FALSE;
#define LENGTH 1024 * 128
    char log[LENGTH];
    GL_CALL(glGetProgramiv(compute_program, GL_LINK_STATUS, &s));
    GL_CALL(glGetProgramInfoLog(compute_program, LENGTH, NULL, log));

    if (s == GL_FALSE)
    {
        LOGE("Failed to link shader:\n", source,
            "\nLinker output:\n", log);
    }

    GL_CALL(glDeleteShader(compute_shader));
    *program = compute_program;
}

/** Create a new theme with the default parameters */
decoration_theme_t::decoration_theme_t()
{
    OpenGL::render_begin();


 setup_shader(&diffuse2_program, diffuse2_source);
  setup_shader(&advect2_program, advect2_source);
    setup_shader(&render_program, render_source);
    OpenGL::render_end();
}

decoration_theme_t::~decoration_theme_t()
{
	GL_CALL(glDeleteProgram(diffuse2_program));
	GL_CALL(glDeleteProgram(advect2_program));
    GL_CALL(glDeleteProgram(render_program));
    destroy_textures();
}

void decoration_theme_t::create_textures()
{
	
GL_CALL(glGenTextures(1, &texture1)) 	;
GL_CALL(glGenTextures(1, &texture2));


    GL_CALL(glGenTextures(1, &texture));
    GL_CALL(glGenTextures(1, &b0u));
    GL_CALL(glGenTextures(1, &b0v));
    GL_CALL(glGenTextures(1, &b0d));
    GL_CALL(glGenTextures(1, &b1u));
    GL_CALL(glGenTextures(1, &b1v));
    GL_CALL(glGenTextures(1, &b1d));
}

void decoration_theme_t::destroy_textures()
{
    GL_CALL(glDeleteTextures(1, &texture));
    GL_CALL(glDeleteTextures(1, &b0u));
    GL_CALL(glDeleteTextures(1, &b0v));
    GL_CALL(glDeleteTextures(1, &b0d));
    GL_CALL(glDeleteTextures(1, &b1u));
    GL_CALL(glDeleteTextures(1, &b1v));
    GL_CALL(glDeleteTextures(1, &b1d));
}

/** @return The available height for displaying the title */
int decoration_theme_t::get_title_height() const
{
    return title_height;
}

/** @return The available border for resizing */
int decoration_theme_t::get_border_size() const
{
    return border_size;
}

 



void decoration_theme_t::run_shader(GLuint program, uint width, uint height)
{
  // 	for (int i = 1; i < 40; ++i)
    {
    GL_CALL(glUseProgram(program));
 //   GL_CALL(glUniform1i(9, i));

	GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 1));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0u));
    GL_CALL(glBindImageTexture(1, b0u, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 2));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0v));
    GL_CALL(glBindImageTexture(2, b0v, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 3));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0d));
    GL_CALL(glBindImageTexture(3, b0d, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 4));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b1u));
    GL_CALL(glBindImageTexture(4, b1u, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 5));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b1v));
    GL_CALL(glBindImageTexture(5, b1v, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 6));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b1d));
    GL_CALL(glBindImageTexture(6, b1d, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glUniform1i(5, width));
    GL_CALL(glUniform1i(6, height));
    GL_CALL(glDispatchCompute(width / 8, height / 8, 1));
    GL_CALL(glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT));
    
	}
}


/**
 * Fill the given rectangle with the background color(s).
 *
 * @param fb The target framebuffer, must have been bound already
 * @param rectangle The rectangle to redraw.
 * @param scissor The GL scissor rectangle to use.
 * @param active Whether to use active or inactive colors
 */
void decoration_theme_t::render_background(const wf::render_target_t& fb,
    wf::geometry_t rectangle, const wf::geometry_t& scissor, bool active, wf::pointf_t p)
{
    if (rectangle.width <= 0 || rectangle.height <= 0)
    {
        return;
    }
    
rectangle.width =rectangle.width ;
rectangle.height = rectangle.height ;
    rectangle.width += 2 - (rectangle.width % 2);
    rectangle.height += 2 - (rectangle.height % 2);

    OpenGL::render_begin(fb);
    if (rectangle.width != saved_width || rectangle.height != saved_height)
    {
        LOGI("recreating smoke textures: ", rectangle.width, " != ", saved_width, " || ", rectangle.height, " != ", saved_height);
        saved_width = rectangle.width;
        saved_height = rectangle.height;

        destroy_textures();
        create_textures();

        std::vector<GLfloat> clear_data(rectangle.width * rectangle.height * 4, 0);

        GL_CALL(glActiveTexture(GL_TEXTURE0 + 0));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RGBA, GL_FLOAT, &clear_data[0]);
        GL_CALL(glActiveTexture(GL_TEXTURE0 + 1));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, b0u));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RED, GL_FLOAT, &clear_data[0]);
        GL_CALL(glActiveTexture(GL_TEXTURE0 + 2));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, b0v));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RED, GL_FLOAT, &clear_data[0]);
        GL_CALL(glActiveTexture(GL_TEXTURE0 + 3));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, b0d));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RED, GL_FLOAT, &clear_data[0]);
        GL_CALL(glActiveTexture(GL_TEXTURE0 + 4));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, b1u));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RED, GL_FLOAT, &clear_data[0]);
        GL_CALL(glActiveTexture(GL_TEXTURE0 + 5));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, b1v));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RED, GL_FLOAT, &clear_data[0]);
        GL_CALL(glActiveTexture(GL_TEXTURE0 + 6));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, b1d));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RED, GL_FLOAT, &clear_data[0]);
    }
    GL_CALL(glUseProgram(motion_program));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 1));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0u));
    GL_CALL(glBindImageTexture(1, b0u, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 2));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0v));
    GL_CALL(glBindImageTexture(2, b0v, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 3));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0d));
    GL_CALL(glBindImageTexture(3, b0d, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 4));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b1u));
    GL_CALL(glBindImageTexture(4, b1u, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 5));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b1v));
    GL_CALL(glBindImageTexture(5, b1v, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 6));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b1d));
    GL_CALL(glBindImageTexture(6, b1d, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    wf::point_t point{int(p.x), int(p.y)};
    // upload stuff
    GL_CALL(glUniform1i(3, point.x));
    GL_CALL(glUniform1i(4, point.y));
    GL_CALL(glUniform1i(5, rectangle.width));
    GL_CALL(glUniform1i(6, rectangle.height));
    GL_CALL(glUniform1i(7, wf::get_current_time()));
    GL_CALL(glDispatchCompute(rectangle.width / 8, rectangle.height / 8, 1));
    GL_CALL(glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT));
  //  for (int k = 0; k < 2; k++)
  //  run_shader(diffuse1_program, rectangle.width, rectangle.height);
   // run_shader(project1_program, rectangle.width, rectangle.height);
  //  for (int k = 0; k < 2; k++)
  //  run_shader(project2_program, rectangle.width, rectangle.height);
  //  run_shader(project3_program, rectangle.width, rectangle.height);
  //  run_shader(advect1_program, rectangle.width, rectangle.height);
  //  run_shader(project4_program, rectangle.width, rectangle.height);
  //  for (int k = 0; k < 2; k++)
  //  run_shader(project5_program, rectangle.width, rectangle.height);
   // run_shader(project6_program, rectangle.width, rectangle.height);
   // for (int k = 0; k < 2; k++)
    run_shader(diffuse2_program, rectangle.width, rectangle.height);
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT); // Add this line

    run_shader(advect2_program, rectangle.width, rectangle.height);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT); // Add this line

      GL_CALL(glUniform1f(8, wf::get_current_time() / 30.0));
    GL_CALL(glUseProgram(render_program));
      GL_CALL(glUniform1f(8, wf::get_current_time() / 30.0));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));
    GL_CALL(glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 1));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0u));
    GL_CALL(glBindImageTexture(1, b0u, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 2));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0v));
    GL_CALL(glBindImageTexture(2, b0v, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 3));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0d));
    GL_CALL(glBindImageTexture(3, b0d, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 4));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b1u));
    GL_CALL(glBindImageTexture(4, b1u, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 5));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b1v));
    GL_CALL(glBindImageTexture(5, b1v, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 6));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b1d));
    GL_CALL(glBindImageTexture(6, b1d, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GLfloat color[4] = {0.1, 1.0, 0.1, 0.5};
    GL_CALL(glUniform4fv(7, 1, color));
    GL_CALL(glDispatchCompute(rectangle.width / 8, rectangle.height / 8, 1));
    GL_CALL(glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT));
    fb.logic_scissor(scissor);
    OpenGL::render_transformed_texture(wf::texture_t{texture}, rectangle, fb.get_orthographic_projection(), glm::vec4{1}, OpenGL::TEXTURE_TRANSFORM_INVERT_Y);
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 1));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 2));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 3));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 4));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 5));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 6));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CALL(glUseProgram(0));
    OpenGL::render_end();
}

/**
 * Render the given text on a cairo_surface_t with the given size.
 * The caller is responsible for freeing the memory afterwards.
 */
cairo_surface_t*decoration_theme_t::render_text(std::string text,
    int width, int height) const
{
    const auto format = CAIRO_FORMAT_ARGB32;
    auto surface = cairo_image_surface_create(format, width, height);

    if (height == 0)
    {
        return surface;
    }

    auto cr = cairo_create(surface);

    const float font_scale = 0.8;
    const float font_size  = height * font_scale;

    PangoFontDescription *font_desc;
    PangoLayout *layout;

    // render text
    font_desc = pango_font_description_from_string(((std::string)font).c_str());
    pango_font_description_set_absolute_size(font_desc, font_size * PANGO_SCALE);

    layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, font_desc);
    pango_layout_set_text(layout, text.c_str(), text.size());
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(font_desc);
    g_object_unref(layout);
    cairo_destroy(cr);

    return surface;
}

cairo_surface_t*decoration_theme_t::get_button_surface(button_type_t button,
    const button_state_t& state) const
{
    cairo_surface_t *button_surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, state.width, state.height);

    auto cr = cairo_create(button_surface);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    /* Clear the button background */
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, state.width, state.height);
    cairo_fill(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    /** A gray that looks good on light and dark themes */
    color_t base = {0.60, 0.60, 0.63, 0.36};

    /**
     * We just need the alpha component.
     * r == g == b == 0.0 will be directly set
     */
    double line  = 0.27;
    double hover = 0.27;

    /** Coloured base on hover/press. Don't compare float to 0 */
    if (fabs(state.hover_progress) > 1e-3)
    {
        switch (button)
        {
          case BUTTON_CLOSE:
            base = {242.0 / 255.0, 80.0 / 255.0, 86.0 / 255.0, 0.63};
            break;

          case BUTTON_TOGGLE_MAXIMIZE:
            base = {57.0 / 255.0, 234.0 / 255.0, 73.0 / 255.0, 0.63};
            break;

          case BUTTON_MINIMIZE:
            base = {250.0 / 255.0, 198.0 / 255.0, 54.0 / 255.0, 0.63};
            break;

          default:
            assert(false);
        }

        line *= 2.0;
    }

    /** Draw the base */
    cairo_set_source_rgba(cr,
        base.r + 0.0 * state.hover_progress,
        base.g + 0.0 * state.hover_progress,
        base.b + 0.0 * state.hover_progress,
        base.a + hover * state.hover_progress);
    cairo_arc(cr, state.width / 2, state.height / 2,
        state.width / 2, 0, 2 * M_PI);
    cairo_fill(cr);

    /** Draw the border */
    cairo_set_line_width(cr, state.border);
    cairo_set_source_rgba(cr, 0.00, 0.00, 0.00, line);
    // This renders great on my screen (110 dpi 1376x768 lcd screen)
    // How this would appear on a Hi-DPI screen is questionable
    double r = state.width / 2 - 0.5 * state.border;
    cairo_arc(cr, state.width / 2, state.height / 2, r, 0, 2 * M_PI);
    cairo_stroke(cr);

    /** Draw the icon  */
    cairo_set_source_rgba(cr, 0.00, 0.00, 0.00, line / 2);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    switch (button)
    {
      case BUTTON_CLOSE:
        cairo_set_line_width(cr, 1.5 * state.border);
        cairo_move_to(cr, 1.0 * state.width / 4.0,
            1.0 * state.height / 4.0);
        cairo_line_to(cr, 3.0 * state.width / 4.0,
            3.0 * state.height / 4.0); // '\' part of x
        cairo_move_to(cr, 3.0 * state.width / 4.0,
            1.0 * state.height / 4.0);
        cairo_line_to(cr, 1.0 * state.width / 4.0,
            3.0 * state.height / 4.0); // '/' part of x
        cairo_stroke(cr);
        break;

      case BUTTON_TOGGLE_MAXIMIZE:
        cairo_set_line_width(cr, 1.5 * state.border);
        cairo_rectangle(
            cr, // Context
            state.width / 4.0, state.height / 4.0, // (x, y)
            state.width / 2.0, state.height / 2.0 // w x h
        );
        cairo_stroke(cr);
        break;

      case BUTTON_MINIMIZE:
        cairo_set_line_width(cr, 1.75 * state.border);
        cairo_move_to(cr, 1.0 * state.width / 4.0,
            state.height / 2.0);
        cairo_line_to(cr, 3.0 * state.width / 4.0,
            state.height / 2.0);
        cairo_stroke(cr);
        break;

      default:
        assert(false);
    }

    cairo_fill(cr);
    cairo_destroy(cr);

    return button_surface;
}
}
}

