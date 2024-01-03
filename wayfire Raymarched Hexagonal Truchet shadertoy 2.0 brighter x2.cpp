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




static const char *motion_source =
    R"(









)";

static const char *advect2_source =
    R"(

)";

//vec2 resolution = vec2(1280.0, 720.0);

static const char *render_source = R"(
#version 320 es

precision highp float;
precision highp int;
precision highp image2D;

layout(local_size_x = 30, local_size_y = 30, local_size_z = 1) in;
layout(binding = 0, rgba8) writeonly uniform image2D imageOut;


layout(location = 8) uniform float  iTime;
uniform vec2 iResolution;

float heightMap(in vec2 p) { 
    
    p *= 5.;
    
    // Hexagonal coordinates.
    vec2 h = vec2(p.x + p.y*.57735, p.y*1.1547);
    
    // Closest hexagon center.
    vec2 f = fract(h); h -= f;
    float c = fract((h.x + h.y)/3.);
    h =  c<.666 ?   c<.333 ?  h  :  h + 1.  :  h  + step(f.yx, f); 

    p -= vec2(h.x - h.y*.5, h.y*.8660254);
    
    // Rotate (flip, in this case) random hexagons. Otherwise, you'd have a bunch of circles only.
    // Note that "h" is unique to each hexagon, so we can use it as the random ID.
    c = fract(cos(dot(h, vec2(41, 289)))*43758.5453); // Reusing "c."
    p -= p*step(c, .5)*2.; // Equivalent to: if (c<.5) p *= -1.;
    
    // Minimum squared distance to neighbors. Taking the square root after comparing, for speed.
    // Three partitions need to be checked due to the flipping process.
    p -= vec2(-1, 0);
    c = dot(p, p); // Reusing "c" again.
    p -= vec2(1.5, .8660254);
    c = min(c, dot(p, p));
    p -= vec2(0, -1.73205);
    c = min(c, dot(p, p));
    
    return sqrt(c);
    
    // Wrapping the values - or folding the values over (abs(c-.5)*2., cos(c*6.283*1.), etc) - to produce 
    // the nicely lined-up, wavy patterns. I"m perfoming this step in the "map" function. It has to do 
    // with coloring and so forth.
    //c = sqrt(c);
    //c = cos(c*6.283*1.) + cos(c*6.283*2.);
    //return (clamp(c*.6+.5, 0., 1.));

}

// Raymarching an XY-plane - raised a little by the hexagonal Truchet heightmap. Pretty standard.
float map(vec3 p){
    
    
    float c = heightMap(p.xy); // Height map.
    // Wrapping, or folding the height map values over, to produce the nicely lined-up, wavy patterns.
    c = cos(c*6.283*1.) + cos(c*6.283*2.);
    c = (clamp(c*.6+.5, 0., 1.));

    
    // Back plane, placed at vec3(0., 0., 1.), with plane normal vec3(0., 0., -1).
    // Adding some height to the plane from the heightmap. Not much else to it.
    return 1. - p.z - c*.025;

    
}

// The normal function with some edge detection and curvature rolled into it. Sometimes, it's possible to 
// get away with six taps, but we need a bit of epsilon value variance here, so there's an extra six.
vec3 getNormal(vec3 p, inout float edge, inout float crv) { 
    
    vec2 e = vec2(.01, 0); // Larger epsilon for greater sample spread, thus thicker edges.

    // Take some distance function measurements from either side of the hit point on all three axes.
    float d1 = map(p + e.xyy), d2 = map(p - e.xyy);
    float d3 = map(p + e.yxy), d4 = map(p - e.yxy);
    float d5 = map(p + e.yyx), d6 = map(p - e.yyx);
    float d = map(p)*2.;    // The hit point itself - Doubled to cut down on calculations. See below.
     
    // Edges - Take a geometry measurement from either side of the hit point. Average them, then see how
    // much the value differs from the hit point itself. Do this for X, Y and Z directions. Here, the sum
    // is used for the overall difference, but there are other ways. Note that it's mainly sharp surface 
    // curves that register a discernible difference.
    edge = abs(d1 + d2 - d) + abs(d3 + d4 - d) + abs(d5 + d6 - d);
    //edge = max(max(abs(d1 + d2 - d), abs(d3 + d4 - d)), abs(d5 + d6 - d)); // Etc.
    
    // Once you have an edge value, it needs to normalized, and smoothed if possible. How you 
    // do that is up to you. This is what I came up with for now, but I might tweak it later.
    edge = smoothstep(0., 1., sqrt(edge/e.x*2.));
    
    // We may as well use the six measurements to obtain a rough curvature value while we're at it.
    crv = clamp((d1 + d2 + d3 + d4 + d5 + d6 - d*3.)*32. + .6, 0., 1.);
    
    // Redoing the calculations for the normal with a more precise epsilon value.
    e = vec2(.0025, 0);
    d1 = map(p + e.xyy), d2 = map(p - e.xyy);
    d3 = map(p + e.yxy), d4 = map(p - e.yxy);
    d5 = map(p + e.yyx), d6 = map(p - e.yyx); 
    
    
    // Return the normal.
    // Standard, normalized gradient mearsurement.
    return normalize(vec3(d1 - d2, d3 - d4, d5 - d6));
}



// I keep a collection of occlusion routines... OK, that sounded really nerdy. :)
// Anyway, I like this one. I'm assuming it's based on IQ's original.
float calculateAO(in vec3 p, in vec3 n)
{
    float sca = 2., occ = 0.;
    for(float i=0.; i<5.; i++){
    
        float hr = .01 + i*.5/4.;        
        float dd = map(n * hr + p);
        occ += (hr - dd)*sca;
        sca *= 0.7;
    }
    return clamp(1.0 - occ, 0., 1.);    
}


/*
// Surface bump function. Cheap, but with decent visual impact.
float bumpSurf3D( in vec3 p){
    
    float c = heightMap((p.xy + p.z*.025)*6.);
    c = cos(c*6.283*3.);
    //c = sqrt(clamp(c+.5, 0., 1.));
    c = (c*.5 + .5);
    
    return c;

}

// Standard function-based bump mapping function.
vec3 dbF(in vec3 p, in vec3 nor, float bumpfactor){
    
    const vec2 e = vec2(0.001, 0);
    float ref = bumpSurf3D(p);                 
    vec3 grad = (vec3(bumpSurf3D(p - e.xyy),
                      bumpSurf3D(p - e.yxy),
                      bumpSurf3D(p - e.yyx) )-ref)/e.x;                     
          
    grad -= nor*dot(nor, grad);          
                      
    return normalize( nor + grad*bumpfactor );
    
}
*/

// Compact, self-contained version of IQ's 3D value noise function.
float n3D(vec3 p){
    
    const vec3 s = vec3(7, 157, 113);
    vec3 ip = floor(p); p -= ip; 
    vec4 h = vec4(0., s.yz, s.y + s.z) + dot(ip, s);
    p = p*p*(3. - 2.*p); //p *= p*p*(p*(p * 6. - 15.) + 10.);
    h = mix(fract(sin(h)*43758.5453), fract(sin(h + s.x)*43758.5453), p.x);
    h.xy = mix(h.xz, h.yw, p.y);
    return mix(h.x, h.y, p.z); // Range: [0, 1].
}

// Simple environment mapping. Pass the reflected vector in and create some
// colored noise with it. The normal is redundant here, but it can be used
// to pass into a 3D texture mapping function to produce some interesting
// environmental reflections.
vec3 envMap(vec3 rd, vec3 sn){
    
    vec3 sRd = rd; // Save rd, just for some mixing at the end.
    
    // Add a time component, scale, then pass into the noise function.
    rd.xy -= iTime*.25 *0.0125;
    rd *= 3.;
    
    float c = n3D(rd)*.57 + n3D(rd*2.)*.28 + n3D(rd*4.)*.15; // Noise value.
    c = smoothstep(0.4, 1., c); // Darken and add contast for more of a spotlight look.
    
    vec3 col = vec3(c, c*c, c*c*c*c); // Simple, warm coloring.
    //vec3 col = vec3(min(c*1.5, 1.), pow(c, 2.5), pow(c, 12.)); // More color.
    
    // Mix in some more red to tone it down and return.
    return mix(col, col.yzx, sRd*.25+.25); 
    
}



// vec2 to vec2 hash.
vec2 hash22(vec2 p) { 

    // Faster, but doesn't disperse things quite as nicely as other combinations. :)
    float n = sin(dot(p, vec2(41, 289)));
    return fract(vec2(262144, 32768)*n)*.75 + .25; 
    
    // Animated.
    //p = fract(vec2(262144, 32768)*n); 
    //return sin( p*6.2831853 + iTime *0.0125)*.35 + .65; 
    
}

// 2D 2nd-order Voronoi: Obviously, this is just a rehash of IQ's original. I've tidied
// up those if-statements. Since there's less writing, it should go faster. That's how 
// it works, right? :)
//
float Voronoi(in vec2 p){
    
    vec2 g = floor(p), o; p -= g;
    
    vec3 d = vec3(1); // 1.4, etc. "d.z" holds the distance comparison value.
    
    for(int y = -1; y <= 1; y++){
        for(int x = -1; x <= 1; x++){
            
            o = vec2(x, y);
            o += hash22(g + o) - p;
            
            d.z = dot(o, o); 
            // More distance metrics.
            //o = abs(o);
            //d.z = max(o.x*.8666 + o.y*.5, o.y);// 
            //d.z = max(o.x, o.y);
            //d.z = (o.x*.7 + o.y*.7);
            
            d.y = max(d.x, min(d.y, d.z));
            d.x = min(d.x, d.z); 
                       
        }
    }
    
    return max(d.y/1.2 - d.x*1., 0.)/1.2;
    //return d.y - d.x; // return 1.-d.x; // etc.
    
}



void main() {

	vec2 iResolution = vec2(1920.0, 1080.0);
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);



    // Unit directional ray - Coyote's observation.
    vec3 rd = normalize(vec3(2. * vec2(id) - iResolution.xy, iResolution.y));


    float tm = (iTime / 2.) * 0.0125;

    // Rotate the XY-plane back and forth. Note that sine and cosine are kind of rolled into one.
    vec2 a = sin(vec2(1.570796, 0) + sin(tm / 4.) * .3);

    // Fabrice's observation.
    rd.xy = mat2(a, -a.y, a.x) * rd.xy;

    // Ray origin. Moving in the X-direction to the right.
  //  vec3 ro = vec3(tm, cos(tm / 4.), 0.);
vec3 ro = vec3(0.0,0.0, 0.);
    // Light position, hovering around behind the camera.
    vec3 lp = ro + vec3(cos(tm / 2.) * .5, sin(tm / 2.) * .5, -.5);

    // Standard raymarching segment. Because of the straightforward setup, not many iterations are necessary.
    float d, t = 0.;
    for (int j = 0; j < 32; j++) {
        d = map(ro + rd * t); // distance to the function.
        t += d * .7;          // Total distance from the camera to the surface.

        // The plane "is" the far plane, so no far=plane break is needed.
        if (d < 0.001)
            break;
    }

    // Edge and curve value. Passed into, and set, during the normal calculation.
    float edge, crv;

    // Surface position, surface normal and light direction.
    vec3 sp = ro + rd * t;
    vec3 sn = getNormal(sp, edge, crv);
    vec3 ld = lp - sp;

    
    // Coloring and texturing the surface.
    //
    // Height map.
    float c = heightMap(sp.xy); 
    
    // Folding, or wrapping, the values above to produce the snake-like pattern that lines up with the randomly
    // flipped hex cells produced by the height map.
    vec3 fold = cos(vec3(1, 2, 4)*c*6.283);
    
    // Using the height map value, then wrapping it, to produce a finer grain Truchet pattern for the overlay.
    float c2 = heightMap((sp.xy + sp.z*.025)*6.);
    c2 = cos(c2*6.283*3.);
    c2 = (clamp(c2+.5, 0., 1.)); 

    
    // Function based bump mapping. I prefer none in this example, but it's there if you want it.   
    //if(temp.x>0. || temp.y>0.) sn = dbF(sp, sn, .001);
    
    // Surface color value.
    vec3 oC = vec3(1);

    if(fold.x>0.) oC = vec3(1, .05, .1)*c2; // Reddish pink with finer grained Truchet overlay.
    
    if(fold.x<0.05 && (fold.y)<0.) oC = vec3(1, .7, .45)*(c2*.25 + .75); // Lighter lined borders.
    else if(fold.x<0.) oC = vec3(1, .8, .4)*c2; // Gold, with overlay.
        
    //oC *= n3D(sp*128.)*.35 + .65; // Extra fine grained noisy texturing.

     
    // Sending some greenish particle pulses through the snake-like patterns. With all the shininess going 
    // on, this effect is a little on the subtle side.
    float p1 = 1.0 - smoothstep(0., .1, fold.x*.5+.5); // Restrict to the snake-like path.
    // Other path.
    //float p2 = 1.0 - smoothstep(0., .1, cos(heightMap(sp.xy + 1. + (iTime*0.0125)/4.)*6.283)*.5+.5);
    float p2 = 1.0 - smoothstep(0., .1, Voronoi(sp.xy*4. + vec2(tm, cos(tm/4.))));
    p1 = (p2 + .25)*p1; // Overlap the paths.
    oC += oC.yxz*p1*p1; // Gives a kind of electron effect. Works better with just Voronoi, but it'll do.
    
   
    
    
    float lDist = max(length(ld), 0.001); // Light distance.
    float atten = 1./(1. + lDist*.125); // Light attenuation.
    
    ld /= lDist; // Normalizing the light direction vector.
    
   
    float diff = max(dot(ld, sn), 0.); // Diffuse.
    float spec = pow(max( dot( reflect(-ld, sn), -rd ), 0.0 ), 16.); // Specular.
    float fre = pow(clamp(dot(sn, rd) + 1., .0, 1.), 3.); // Fresnel, for some mild glow.
    
    // Shading. Note, there are no actual shadows. The camera is front on, so the following
    // two functions are enough to give a shadowy appearance.
    crv = crv*.9 + .1; // Curvature value, to darken the crevices.
    float ao = calculateAO(sp, sn); // Ambient occlusion, for self shadowing.

 
    
    // Combining the terms above to light the texel.
 vec3 col = oC*(diff + .5) + vec3(1., .7, .4)*spec*2. + vec3(.4, .7, 1)*fre;

    
   col += (oC*.5+.5)*envMap(reflect(rd, sn), sn)*6.; // Fake environment mapping.
 
    
    // Edges.
    col *= 1. - edge*.85; // Darker edges.   
    
    // Applying the shades.
    col *= (atten*crv*ao);

col *= 2.0;
 // Rough gamma correction, then write to the output buffer.
    vec3 finalColor = sqrt(clamp(col, 0., 1.));
    ivec2 fragCoord = ivec2(id);  // Convert to ivec2
    imageStore(imageOut, fragCoord, vec4(finalColor, 1.0));  // Use vec4 instead of uvec4

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

  setup_shader(&motion_program, motion_source);
  setup_shader(&advect2_program, advect2_source);
    setup_shader(&render_program, render_source);
    OpenGL::render_end();
}

decoration_theme_t::~decoration_theme_t()
{
	 GL_CALL(glDeleteProgram(motion_program));
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
    GL_CALL(glDispatchCompute(width / 30, height / 30, 1));
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
    GL_CALL(glDispatchCompute(rectangle.width / 30, rectangle.height / 30, 1));
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
    GL_CALL(glDispatchCompute(rectangle.width / 30, rectangle.height / 30, 1));
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

