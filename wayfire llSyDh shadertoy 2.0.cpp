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


)";

static const char *advect2_source =
    R"(

)";


//layout(location = 8) uniform float current_time;

static const char *render_source =
    R"(
#version 320 es

precision highp float;
precision highp int;
precision highp image2D;

//layout (location = 0) out vec4 fragColor;
layout (binding = 0, rgba32f) writeonly uniform image2D fragColor;


layout(location = 8) uniform float iTime;
uniform vec2 iResolution;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;


// Interlaced variation - Interesting, but patched together in a hurry.
//#define INTERLACING

// A quick hack to get rid of the winding overlay - in order to show the maze only.
//#define MAZE_ONLY



// Helper vector. If you're doing anything that involves regular triangles or hexagons, the
// 30-60-90 triangle will be involved in some way, which has sides of 1, sqrt(3) and 2.
const vec2 s = vec2(1, 1.7320508);

// Standard vec2 to float hash - Based on IQ's original.
float hash21(vec2 p){ return fract(sin(dot(p, vec2(141.173, 289.927)))*43758.5453); }


// Standard 2D rotation formula.
mat2 r2(in float a){ float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }


// The 2D hexagonal isosuface function: If you were to render a horizontal line and one that
// slopes at 60 degrees, mirror, then combine them, you'd arrive at the following.
float hex(in vec2 p){
    
    p = abs(p);
    
    // Below is equivalent to:
    //return max(p.x*.5 + p.y*.866025, p.x); 

    return max(dot(p, s*.5), p.x); // Hexagon.
    
}

// This function returns the hexagonal grid coordinate for the grid cell, and the corresponding 
// hexagon cell ID - in the form of the central hexagonal point. That's basically all you need to 
// produce a hexagonal grid.
//
// When working with 2D, I guess it's not that important to streamline this particular function.
// However, if you need to raymarch a hexagonal grid, the number of operations tend to matter.
// This one has minimal setup, one "floor" call, a couple of "dot" calls, a ternary operator, etc.
// To use it to raymarch, you'd have to double up on everything - in order to deal with 
// overlapping fields from neighboring cells, so the fewer operations the better.
vec4 getHex(vec2 p){
    
    // The hexagon centers: Two sets of repeat hexagons are required to fill in the space, and
    // the two sets are stored in a "vec4" in order to group some calculations together. The hexagon
    // center we'll eventually use will depend upon which is closest to the current point. Since 
    // the central hexagon point is unique, it doubles as the unique hexagon ID.
    vec4 hC = floor(vec4(p, p - vec2(.5, 1))/s.xyxy) + .5;
    
    // Centering the coordinates with the hexagon centers above.
    vec4 h = vec4(p - hC.xy*s, p - (hC.zw + .5)*s);
    
    // Nearest hexagon center (with respect to p) to the current point. In other words, when
    // "h.xy" is zero, we're at the center. We're also returning the corresponding hexagon ID -
    // in the form of the hexagonal central point. Note that a random constant has been added to 
    // "hC.zw" to further distinguish it from "hC.xy."
    //
    // On a side note, I sometimes compare hex distances, but I noticed that Iomateron compared
    // the Euclidian version, which seems neater, so I've adopted that.
    return dot(h.xy, h.xy)<dot(h.zw, h.zw) ? vec4(h.xy, hC.xy) : vec4(h.zw, hC.zw + vec2(.5, 1));
    
}

// Dot pattern.
float dots(in vec2 p){
    
    p = abs(fract(p) - .5);
    
    return length(p); // Circles.
    
    //return (p.x + p.y)/1.5 + .035; // Diamonds.
    
    //return max(p.x, p.y) + .03; // Squares.
    
    //return max(p.x*.866025 + p.y*.5, p.y) + .01; // Hexagons.
    
    //return min((p.x + p.y)*.7071, max(p.x, p.y)) + .08; // Stars.
    
    
}


// Distance field for the arcs. I think it's called poloidal rotation, or something like that.
float dfPol(vec2 p){
     
    return length(p); // Circular arc.
    
    // There's no rule that says the arcs have to be rounded. Here's a hexagonal one.
    //return hex(p);
    
    // Dodecahedron.
    //return max(hex(p), hex(r2(3.14159/6.)*p));
    
    // Triangle.
    //return max(abs(p.x)*.866025 - p.y, p.y);
    
}


// Truchet pattern distance field.
float df(vec2 p, float dir){
     
    // Weird UV coordinates. The first entry is the Truchet distance field itself,
    // and the second is the polar angle of the arc pixel. The extra ".1" is just a bit
    // of mutational scaling, or something... I can't actually remember why it's there. :)
    vec2 uv = vec2(p.x + .1, p.y);//*vec2(1, 1); // Scaling.
    
    // A checkered dot pattern. At present the pattern needs to have flip symmetry about
    // the center, but I'm pretty sure regular textures could be applied with a few
    // minor changes. Due to the triangular nature of the Truchet pattern, factors of "3"
    // were necessary, but factors of "1.5" seemed to work too. Hence the "4.5."
    return min(dots(uv*4.5), dots(uv*4.5 + .5)) - .3;
    
}


// Polar coordinate of the arc pixel.
float getPolarCoord(vec2 q, float dir){
    
    // The actual animation. You perform that before polar conversion.
    q = r2((iTime*0.05)*dir)*q;
    
    // Polar angle.
    const float aNum = 1.;
    float a = atan(q.y, q.x);
   
    // Wrapping the polar angle.
    return mod(a/3.14159, 2./aNum) - 1./aNum;
   
    
}



void main() {
	vec2 iResolution = vec2(1280.0, 720.0);
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    vec2 fragCoord = vec2(id);
    float res = clamp(iResolution.y, 300.0, 600.0);
    vec2 u = (fragCoord - iResolution.xy*.5)/res;
    vec2 sc = u*4. + s.yx*(iTime*0.05)/12.;
    vec4 h = getHex(sc);
    vec4 h2 = getHex(sc - 1.0 / s);
    vec4 h3 = getHex(sc + 1.0 / s);
    vec2 p = h.xy;
    float eDist = hex(p);
    float cDist = dot(p, p);
    float rnd = hash21(h.zw);
 //float aRnd = sin(rnd*6.283 + (iTime*0.05)*1.5)*.5 + .5; // Animating the random number.
    
    #ifdef INTERLACING
    // Random vec3 - used for some overlapping.
    //vec3 lRnd = vec3(rnd*14.4 + .81, fract(rnd*21.3 + .97), fract(rnd*7.2 + .63));
    vec3 lRnd = vec3(hash21(h.zw + .23), hash21(h.zw + .96), hash21(h.zw + .47));
    #endif
    
    // It's possible to control the randomness to form some kind of repeat pattern.
    //rnd = mod(h.z + h.w, 2.);
    

    // Redundant here, but I might need it later.
    float dir = 1.;
    

    
    // Storage vector.
    vec2 q;
    
    
    // If the grid cell's random ID is above a threshold, flip the Y-coordinates.
    if(rnd>.5) p.y = -p.y;
        
    

    // Determining the closest of the three arcs to the current point, the keeping a copy
    // of the vector used to produce it. That way, you'll know just to render that particular
    // decorated arc, lines, etc - instead of all three. 
    const float r = 1.;
    const float th = .2; // Arc thickness.
    
    // Arc one.
    q = p - vec2(0, r)/s;
    vec3 da = vec3(q, dfPol(q));
    
    // Arc two. "r2" could be hardcoded, but this is a relatively cheap 2D example.
    q = r2(3.14159*2./3.)*p - vec2(0, r)/s;
    vec3 db = vec3(q, dfPol(q));

     // Arc three. 
    q = r2(3.14159*4./3.)*p - vec2(0, r)/s;
    vec3 dc = vec3(q, dfPol(q));
    
    // Compare distance fields, and return the vector used to produce the closest one.
    vec3 q3 = da.z<db.z && da.z<dc.z? da : db.z<dc.z ? db : dc;
    
    
    // TRUCHET PATTERN
    //
    // Set the poloidal arc radius: You can change the poloidal distance field in 
    // the "dfPol" function to a different curve shape, but you'll need to change
    // the radius to one of the figures below.
    //
    q3.z -= .57735/2. + th/2.;  // Circular and dodecahedral arc/curves.
    //q3.z -= .5/2. + th/2.;  // Hexagon curve.
    //q3.z -= .7071/2. + th/2.;  // Triangle curve.
    
    q3.z = max(q3.z, -th - q3.z); // Chop out the smaller radius. The result is an arc.
    
    // Store the result in "d" - only to save writing "q3.z" everywhere.
    float d = q3.z;
    
    // If you'd like to see the maze by itself.
    #ifdef MAZE_ONLY
    d += 1e5;
    #endif
    
    // Truchet border.
    float dBord = max(d - .015, -d);
    

    
 
    
    // MAZE BORDERS
    // Producing the stright-line arc borders. Basically, we're rendering some hexagonal borders around
    // the arcs. The result is the hexagonal maze surrounding the Truchet pattern.
    q = q3.xy;
    const float lnTh = .05;
    q = abs(q);
    
    float arcBord = hex(q);
    //float arcBord = length(q); // Change arc length to ".57735."
    //float arcBord = max(hex(q), hex(r2(3.14159/6.)*q)); // Change arc length to ".57735."
    
    // Making the hexagonal arc.
    float lnOuter = max(arcBord - .5, -(arcBord - .5 + lnTh)); //.57735
    
    
    #ifdef INTERLACING
    float ln = min(lnOuter, (q.y*.866025 + q.x*.5, q.x) - lnTh);
    #else
    float ln = min(lnOuter, arcBord - lnTh);
    #endif
    float lnBord = ln - .03; // Border lines to the maze border, if that makes any sense. :)
     
       
    ///////
    // The moving Truchet pattern. The polar coordinates consist of a wrapped angular coordinate,
    // and the distance field itself.
    float a = getPolarCoord(q3.xy, dir);
    float d2 = df(vec2(q3.z, a), dir);
    float dMask = smoothstep(0., .015, d);
    vec3 bg =  mix(vec3(0, .0, .6), vec3(0, .9, .0), dot(sin(u*6. - cos(u*3.)), vec2(.4/2.)) + .4); 
    
    
    
    bg = mix(bg, bg.xzy, dot(sin(u*6. - cos(u*3.)), vec2(.4/2.)) + .4);
    bg = mix(bg, bg.zxy, dot(sin(u*3. + cos(u*3.)), vec2(.1/2.)) + .1);
   
    #ifdef INTERLACING
    // Putting in background cube lines for the interlaced version.
    float hLines = smoothstep(0., .02, eDist - .5 + .02);
    bg = mix(bg, vec3(0), smoothstep(0., .02, ln)*dMask*hLines);
    #endif
    
    // Lines over the maze lines. Applying difference logic, depending on whether the 
    // pattern is interlaced or not.
    const float tr = 1.;

    float eDist2 = hex(h2.xy);
    float hLines2 = smoothstep(0., .02, eDist2 - .5 + .02);
    #ifdef INTERLACING
    if(rnd>.5 && lRnd.x<.5) hLines2 *= smoothstep(0., .02, ln);
    if(lRnd.x>.5) hLines2 *= dMask;
    #else
    if(rnd>.5) hLines2 *= smoothstep(0., .02, ln);
    hLines2 *= dMask;
    #endif
    bg = mix(bg, vec3(0), hLines2*tr);
    
    float eDist3 = hex(h3.xy);
    float hLines3 = smoothstep(0., .02, eDist3 - .5 + .02);
    #ifdef INTERLACING
    if(rnd<=.5 && lRnd.x>.5) hLines3 *= smoothstep(0., .02, ln);
    if(lRnd.x>.5) hLines3 *= dMask;
    #else
    if(rnd<=.5) hLines3 *= smoothstep(0., .02, ln);
    hLines3 *= dMask;
    #endif
    bg = mix(bg, vec3(0), hLines3*tr);


    // Using the two off-centered hex coordinates to give the background a bit of highlighting.
    float shade = max(1.25 - dot(h2.xy, h2.xy)*2., 0.);
    shade = min(shade, max(dot(h3.xy, h3.xy)*3. + .25, 0.));
    bg = mix(bg, vec3(0), (1.-shade)*.5); 
    
    // I wanted to change the colors of everything at the last minute. It's pretty hacky, so
    // when I'm feeling less lazy, I'll tidy it up. :)
    vec3 dotCol = bg.zyx*vec3(1.5, .4, .4);
    vec3 bCol = mix(bg.zyx, bg.yyy, .25);
    bg = mix(bg.yyy, bg.zyx, .25);
    
    

    // Under the random threshold, and we draw the lines under the Truchet pattern.
    #ifdef INTERLACING
    if(lRnd.x>.5){
       bg = mix(bg, vec3(0), (1. - smoothstep(0., .015, lnBord)));
       bg = mix(bg, bCol, (1. - smoothstep(0., .015, ln))); 
       // Center lines.
       bg = mix(bg, vec3(0), smoothstep(0., .02, eDist3 - .5 + .02)*tr);
    }
    #else
    bg = mix(bg, vec3(0), (1. - smoothstep(0., .015, lnBord)));
    bg = mix(bg, bCol, (1. - smoothstep(0., .015, ln)));
    #endif


   
     // Apply the Truchet shadow to the background.
    bg = mix(bg, vec3(0), (1. - smoothstep(0., .07, d))*.5);
    
    
    // Place the Truchet field to the background, with some additional shading to give it a 
    // slightly rounded, raised feel.
    //vec3 col = mix(bg, vec3(1)*max(-d*3. + .7, 0.), (1. - dMask)*.65);
    // Huttarl suggest slightly more shading on the snake-like pattern edges, so I added just a touch.
    vec3 col = mix(bg, vec3(1)*max(-d*9. + .4, 0.), (1. - dMask)*.65);



    
    // Apply the moving dot pattern to the Truchet.
    //dotCol = mix(dotCol, dotCol.xzy, dot(sin(u*3.14159*2. - cos(u.yx*3.14159*2.)*3.14159), vec2(.25)) + .5);
    col = mix(col, vec3(0), (1. - dMask)*(1. - smoothstep(0., .02, d2)));
    col = mix(col, dotCol, (1. - dMask)*(1. - smoothstep(0., .02, d2 + .125)));
    
    // Truchet border.
    col = mix(col, vec3(0), 1. - smoothstep(0., .015, dBord));
    
    #ifdef INTERLACING
    // Over the random threshold, and we draw the lines over the Truchet.
    if(lRnd.x<=.5){
        col = mix(col, vec3(0), (1. - smoothstep(0., .015, lnBord)));
        col = mix(col, bCol, (1. - smoothstep(0., .015, ln)));  
        // Center lines.
        col = mix(col, vec3(0), smoothstep(0., .02, eDist2 - .5 + .02)*tr);
    }
    #endif


        
    
    // Subtle vignette.
    u = fragCoord/iResolution.xy;
    col *= pow(16.*u.x*u.y*(1. - u.x)*(1. - u.y) , .125) + .25;
    // Colored variation.
    //col = mix(pow(min(vec3(1.5, 1, 1)*col, 1.), vec3(1, 3, 16)), col, 
            //pow(16.*u.x*u.y*(1. - u.x)*(1. - u.y) , .25)*.75 + .25);    
    
    

 

 
    
    // Using the edge distance to produce some repeat contour lines. Standard stuff.
    //if (rnd>.5) h.xy = -h.yx;
    //float cont = clamp(cos(hex(h.xy)*6.283*12.)*1.5 + 1.25, 0., 1.);
    //col = mix(col, vec3(0), (1. - smoothstep(0., .015, ln))*(smoothstep(0., .015, d))*(1.-cont)*.5);
    
    
 
    // Very basic hatch line effect.
    float gr = dot(col, vec3(.299, .587, .114));
    float hatch = (gr<.45)? clamp(sin((sc.x - sc.y)*3.14159*40.)*2. + 1.5, 0., 1.) : 1.;
    float hatch2 = (gr<.25)? clamp(sin((sc.x + sc.y)*3.14159*40.)*2. + 1.5, 0., 1.) : 1.;

    col *= min(hatch, hatch2)*.5 + .5;    
    col *= clamp(sin((sc.x - sc.y)*3.14159*80.)*1.5 + .75, 0., 1.)*.25 + 1.;  
    
 
 
    vec2 u2 = fragCoord / iResolution.xy;
    col *= pow(16.0 * u2.x * u2.y * (1.0 - u2.x) * (1.0 - u2.y), 0.125) + 0.25;

    imageStore(fragColor, id, vec4(sqrt(max(col, 0.0)), 1.0));
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

