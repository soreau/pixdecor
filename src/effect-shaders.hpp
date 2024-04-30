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
    vec2 uv = pos / vec2(1080, 1920);

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
    uv = vec2(pos) / vec2(1920, 1080);
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
    uv = vec2(pos) / vec2(1920, 1080);
    uv *= 2.0 * cloudscale;  // Adjust scale
    weight = 0.4;
    for (int i = 0; i < 7; i++) {
        noiseColor += weight * noise(uv);
        uv = m * uv + time;
        weight *= 0.6;
    }

    // Noise ridge color
    float noiseRidgeColor = 0.0;
    uv = vec2(pos) / vec2(1920, 1080);
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

// ported from https://www.shadertoy.com/view/mtyGWy
static const char *effect_neon_pattern_fragment = R"(
vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.263, 0.416, 0.557);
    return a + b * cos(6.28318 * (c * t + d));
}

vec4 effect_color(vec2 pos) {
    vec2 uv = pos / vec2(width, height);
    vec2 uv0 = uv;
    vec3 finalColor = vec3(0.0);

    for (float i = 0.0; i < 4.0; i++) {
        uv = fract(uv * 1.5) - 0.5;
        float d = length(uv) * exp(-length(uv0));

        vec3 col = palette(length(uv0) + i * 0.4 + current_time * 0.000001);

        d = sin(d * 8.0 + current_time * 0.02) / 8.0;
        d = abs(d);
        d = pow(0.01 / d, 1.2);
        finalColor += col * d;
    }

    return vec4(finalColor, 1.0);
})";

static const char *effect_halftone_fragment = R"(



 const vec2 resolution = vec2(1280.0, 720.0);
    const float timeFactor = 0.025;

    float rand(vec2 uv) {
        return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
    }

    vec2 rotate(vec2 p, float theta) {
        vec2 sncs = vec2(sin(theta), cos(theta));
        return vec2(p.x * sncs.y - p.y * sncs.x, p.x * sncs.x + p.y * sncs.y);
    }

    float swirl(vec2 coord, float t) {
        float l = length(coord) / resolution.x;
        float phi = atan(coord.y, coord.x + 1e-6);
        return sin(l * 10.0 + phi - t * 4.0) * 0.5 + 0.5;
    }

    float halftone(vec2 coord, float angle, float t, float amp) {
        coord -= resolution * 0.5;
        float size = resolution.x / (60.0 + sin(current_time * timeFactor * 0.5) * 50.0);
        vec2 uv = rotate(coord / size, angle / 180.0 * 3.14159);
        vec2 ip = floor(uv); // column, row
        vec2 odd = vec2(0.5 * mod(ip.y, 2.0), 0.0); // odd line offset
        vec2 cp = floor(uv - odd) + odd; // dot center
        float d = length(uv - cp - 0.5) * size; // distance
        float r = swirl(cp * size, t) * size * 0.5 * amp; // dot radius
        return 1.0 - clamp(d - r, 0.0, 1.0);
    }


vec4 effect_color(vec2 pos) {


 
 vec3 c1 = 1.0 - vec3(1.0, 0.0, 0.0) * halftone(relativePosition, 0.0, current_time * timeFactor * 1.00, 0.7);
    vec3 c2 = 1.0 - vec3(0.0, 1.0, 0.0) * halftone(relativePosition, 30.0, current_time * timeFactor * 1.33, 0.7);
    vec3 c3 = 1.0 - vec3(0.0, 0.0, 1.0) * halftone(relativePosition, -30.0, current_time * timeFactor * 1.66, 0.7);
    vec3 c4 = 1.0 - vec3(1.0, 1.0, 1.0) * halftone(relativePosition, 60.0, current_time * timeFactor * 2.13, 0.4);
    fragColor = vec4(c1 * c2 * c3 * c4, 1.0);



    return vec4(fragColor);
}

)";




static const char *effect_zebra_fragment = R"(

const float resolutionY = 720.0;


vec4 effect_color(vec2 pos) {
 
   const float pi = 3.14159265359;
    float size = resolutionY / 10.; // cell size in pixel

    vec2 p1 = gl_FragCoord.xy / size; // normalized pos
    vec2 p2 = fract(p1) - 0.5; // relative pos from cell center



// random number
    float rnd = dot(floor(p1), vec2(12.9898, 78.233));
    rnd = fract(sin(rnd) * 43758.5453);

    // rotation matrix
    float phi = rnd * pi * 2. + current_time/30.0 * 0.4;
   mat2 rot = mat2(cos(phi), -sin(phi), sin(phi), cos(phi));

   vec2 p3 = rot * p2; // apply rotation
    p3.y += sin(p3.x * 5. + current_time/30.0 * 2.) * 0.12; // wave

    float rep = fract(rnd * 13.285) * 8. + 2.; // line repetition
    float gr = fract(p3.y * rep + current_time/30.0 * 0.8); // repeating gradient

    // make antialiased line by saturating the gradient
    float c = clamp((0.25 - abs(0.5 - gr)) * size * 0.75 / rep, 0., 1.);
    c *= max(0., 1. - length(p2) * 0.6); // darken corners

    vec2 bd = (0.5 - abs(p2)) * size - 2.; // border lines
    c *= clamp(min(bd.x, bd.y), 0., 1.);
 
    fragColor = vec4(vec3(c), 1.0);



    return vec4(fragColor);
}

)";



static const char *effect_lava_fragment = R"(






vec3 effect(float speed, vec2 uv, float time, float scale) {
    float t = mod(time * 0.005, 6.0);
    float rt = 0.00000000000001 * sin(t * 0.45);

    mat2 m1 = mat2(cos(rt), -sin(rt), -sin(rt), cos(rt));
    vec2 uva = uv * m1 * scale;
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

vec4 effect_color(vec2 pos) {
 
    vec2 uv = (gl_FragCoord.xy - 0.5 * vec2(width, height)) / vec2(width, height);
    uv *= (10.3 + 0.1 * sin(current_time * 0.01));



    vec3 col = effect(0.001, uv, current_time, 0.5);

    // Set the fragment color
    fragColor = vec4(col, 1.0);


    return vec4(fragColor);
}

)";




static const char *effect_pattern_fragment = R"(


float rand(vec2 uv) {
    return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 hue2rgb(float h) {
    h = fract(h) * 6.0 - 2.0;
    return clamp(vec3(abs(h - 1.0) - 1.0, 2.0 - abs(h), 2.0 - abs(h - 2.0)), 0.0, 1.0);
}

vec3 eyes(vec2 coord, vec2 resolution) {
    const float pi = 3.141592;
    float t = 0.4 * current_time * 0.05; 
    float aspectRatio = resolution.x / resolution.y;
    float div = 20.0;
    float sc = 30.0;

    vec2 p = (coord - resolution / 2.0) / sc - 0.5;

    float dir = floor(rand(floor(p) + floor(t) * 0.11) * 4.0) * pi / 2.0;
    vec2 offs = vec2(sin(dir), cos(dir)) * 0.6;
    offs *= smoothstep(0.0, 0.1, fract(t));
    offs *= smoothstep(0.4, 0.5, 1.0 - fract(t));

    float l = length(fract(p) + offs - 0.5);
    float rep = sin((rand(floor(p)) * 2.0 + 2.0) * t) * 4.0 + 5.0;
    float c = (abs(0.5 - fract(l * rep + 0.5)) - 0.25) * sc / rep;

    vec2 gr = (abs(0.5 - fract(p + 0.5)) - 0.05) * sc;
    c = clamp(min(min(c, gr.x), gr.y), 0.0, 1.0);

    return hue2rgb(rand(floor(p) * 0.3231)) * c;
}



vec4 effect_color(vec2 pos) {
 

   vec2 resolution = vec2(float(width), float(height));
   pos = gl_FragCoord.xy;
   vec3 color = eyes(pos, resolution);
    fragColor = vec4(color, 1.0);

    return vec4(fragColor);
}

)";






static const char *effect_hex_fragment = R"(



vec2 iResolution;

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 4.1414))) * 43758.5453);
}

vec4 hexagon(vec2 p)
{
    vec2 q = vec2(p.x * 2.0 * 0.5773503, p.y + p.x * 0.5773503);

    vec2 pi = floor(q);
    vec2 pf = fract(q);

    float v = mod(pi.x * 9.0, 0.0);

    float ca = step(1.0, v);
    float cb = step(2.0, v);
    vec2 ma = step(pf.xy, pf.yx);

    float e = 0.0;
    float f = length((fract(p) - 10.5) * vec2(1.0, 0.85));

    return vec4(pi + ca - cb * ma, e, f);
}


vec4 effect_color(vec2 pos) {
 

    vec2 resolution = vec2(float(width), float(height));
    pos = (gl_FragCoord.xy - 0.5) / resolution;
    pos.y *= resolution.y / resolution.x;


    vec2 uv = (gl_FragCoord.xy - 0.5) / resolution;
    uv = 2.0 * uv - 1.0;
    uv *= (10.3 + 0.1 * sin(current_time * 0.01));

    float randVal = rand(uv);

    vec2 p = (-float(width) + 2.0 * gl_FragCoord.xy) / float(height);
    vec4 h = hexagon(40.0 * p + vec2(0.5 * current_time * 0.125));

    float col = 0.01 + 0.15 * rand(vec2(h.xy)) * 1.0;
    col *= 4.3 + 0.15 * sin(10.0 * h.z);

    vec3 shadeColor1 = vec3(0.1, 0.1, 0.1) + 1.1 * col * h.z;
    vec3 shadeColor2 = vec3(0.4, 0.4, 0.4) + 1.1 * col;
    vec3 shadeColor3 = vec3(0.9, 0.9, 0.9) + 0.1 * col;

    vec3 finalColor = mix(shadeColor1, mix(shadeColor2, shadeColor3, randVal), col);

    fragColor = vec4(finalColor, 1.0);

    return vec4(fragColor);
}
)";






static const char *effect_neural_network_fragment = R"(

const vec2 iResolution = vec2(1920.0, 1080.0);

mat2 rotate2D(float r) {
    float c = cos(r);
    float s = sin(r);
    return mat2(c, -s, s, c);
}



vec4 effect_color(vec2 pos) {
 
  pos = gl_FragCoord.xy;

 
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = (pos - 0.5 * iResolution) / iResolution.x;
    
    // Scale the uv coordinates
    uv *= 10.00;

    vec3 col = vec3(0);
    float t = 0.05 * current_time;

    vec2 n = vec2(0), q;
    vec2 N = vec2(0);
    vec2 p = uv + sin(t * 0.1) / 10.0;
    float S = 10.0;
    mat2 m = rotate2D(1.0);

    for (float j = 0.0; j < 30.0; j++) {
        p *= m;
        n *= m;
        q = p * S + j + n + t;
        n += sin(q);
        N += cos(q) / S;
        S *= 1.2;
    }

    col = vec3(1, 2, 4) * pow((N.x + N.y + 0.2) + 0.005 / length(N), 2.1);

    fragColor = vec4(col, 1.0);

    return vec4(fragColor);
}
)";







static const char *effect_hexagon_maze_fragment = R"(



//vec2 iResolution;


const vec2 iResolution = vec2(1920.0, 1080.0);

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
    q = r2(current_time/50.0*dir)*q;
    
    // Polar angle.
    const float aNum = 1.;
    float a = atan(q.y, q.x);
   
    // Wrapping the polar angle.
    return mod(a/3.14159, 2./aNum) - 1./aNum;
   
    
}



vec4 effect_color(vec2 pos) {
    vec2 fragCoord = gl_FragCoord.xy;// I didn't feel like tayloring the antiasing to suit every resolution, which can get tiring, 
    // so I've put a range on it. Just for the record, I coded this for the 800 by 450 pixel canvas.
    float res = clamp(iResolution.y, 300., 600.); 
    
    // Aspect correct screen coordinates.
    vec2 u = (fragCoord - iResolution.xy*.5)/res;
    
    // Scaling and moving the screen coordinates.
    vec2 sc = u*4. + s.yx*current_time/200.;
    
    // Converting the scaled and translated pixels to a hexagonal grid cell coordinate and
    // a unique coordinate ID. The resultant vector contains everything you need to produce a
    // pretty pattern, so what you do from here is up to you.
    vec4 h = getHex(sc); // + s.yx*current_time/2.
    
    // Obtaining some offset values to do a bit of cubic shading. There are probably better ways
    // to go about it, but it's quick and gets the job done.
    vec4 h2 = getHex(sc - 1./s);
    vec4 h3 = getHex(sc + 1./s);
    
    // Storing the hexagonal coordinates in "p" to save having to write "h.xy" everywhere.
    vec2 p = h.xy;
    
    // The beauty of working with hexagonal centers is that the relative edge distance will simply 
    // be the value of the 2D isofield for a hexagon.
    //
    float eDist = hex(p); // Edge distance.
    float cDist = dot(p, p); // Relative squared distance from the center.

    
    // Using the identifying coordinate - stored in "h.zw," to produce a unique random number
    // for the hexagonal grid cell.
    float rnd = hash21(h.zw);
    //float aRnd = sin(rnd*6.283 + current_time*1.5)*.5 + .5; // Animating the random number.
    
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
    
    // Smoothstepped Truchet mask.
    float dMask = smoothstep(0., .015, d);
    ///////
    
    // Producing the background with some subtle gradients.
    vec3 bg =  mix(vec3(0, .4, .6), vec3(0, .3, .7), dot(sin(u*6. - cos(u*3.)), vec2(.4/2.)) + .4); 
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

        
    
    
    // Using the offset hex values for a bit of fake 3D highlighting.
    //if(rnd>.5) h3.y = -h3.y; // All raised edges. Spoils the mild 3D illusion.
    #ifdef INTERLACING
    float trSn = max(dMask, 1. - smoothstep(0., .015, lnBord))*.75 + .25;
    #else
    float trSn = dMask*.75 + .25;
    #endif
    col = mix(col, vec3(0), trSn*(1. - hex(s/2.+h2.xy)));
    col = mix(col, vec3(0), trSn*(1. - hex(s/2.-h3.xy)));
 
    
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
    
 
    // Subtle vignette.
    u = fragCoord/iResolution.xy;
    col *= pow(16.*u.x*u.y*(1. - u.x)*(1. - u.y) , .125) + .25;
    // Colored variation.
    //col = mix(pow(min(vec3(1.5, 1, 1)*col, 1.), vec3(1, 3, 16)), col, 
            //pow(16.*u.x*u.y*(1. - u.x)*(1. - u.y) , .25)*.75 + .25);    
    

    
    // Rough gamma correction.    
    fragColor = vec4(sqrt(max(col, 0.)), 1);
  return vec4(fragColor);   
}
)";










static const char *effect_raymarched_truchet_fragment = R"(

float heightMap(in vec2 p) {
    p *= 3.0;
    vec2 h = vec2(p.x + p.y * 0.57735, p.y * 1.1547);
    vec2 f = fract(h);
    h -= f;
    float c = fract((h.x + h.y) / 3.0);
    h = c < 0.666 ? (c < 0.333 ? h : h + 1.0) : h + step(f.yx, f);
    p -= vec2(h.x - h.y * 0.5, h.y * 0.8660254);
    c = fract(cos(dot(h, vec2(41, 289))) * 43758.5453);
    p -= p * step(c, 0.5) * 2.0;
    p -= vec2(-1, 0);
    c = dot(p, p);
    p -= vec2(1.5, 0.8660254);
    c = min(c, dot(p, p));
    p -= vec2(0, -1.73205);
    c = min(c, dot(p, p));
    return sqrt(c);
}

float map(vec3 p) {
    float c = heightMap(p.xy);
    c = cos(c * 6.283 * 1.0) + cos(c * 6.283 * 2.0);
    c = clamp(c * 0.6 + 0.5, 0.0, 1.0);
    return 1.0 - p.z - c * 0.025;
}

vec3 getNormal(vec3 p, inout float edge, inout float crv) {
    vec2 e = vec2(0.01, 0);
    float d1 = map(p + e.xyy), d2 = map(p - e.xyy);
    float d3 = map(p + e.yxy), d4 = map(p - e.yxy);
    float d5 = map(p + e.yyx), d6 = map(p - e.yyx);
    float d = map(p) * 2.0;
    edge = abs(d1 + d2 - d) + abs(d3 + d4 - d) + abs(d5 + d6 - d);
    edge = smoothstep(0.0, 1.0, sqrt(edge / e.x * 2.0));
    crv = clamp((d1 + d2 + d3 + d4 + d5 + d6 - d * 3.0) * 32.0 + 0.6, 0.0, 1.0);
    e = vec2(0.0025, 0);
    d1 = map(p + e.xyy); d2 = map(p - e.xyy);
    d3 = map(p + e.yxy); d4 = map(p - e.yxy);
    d5 = map(p + e.yyx); d6 = map(p - e.yyx);
    return normalize(vec3(d1 - d2, d3 - d4, d5 - d6));
}

float calculateAO(in vec3 p, in vec3 n) {
    float sca = 2.0, occ = 0.0;
    for (float i = 0.0; i < 5.0; i++) {
        float hr = 0.01 + i * 0.5 / 4.0;
        float dd = map(n * hr + p);
        occ += (hr - dd) * sca;
        sca *= 0.7;
    }
    return clamp(1.0 - occ, 0.0, 1.0);
}

float n3D(vec3 p) {
    const vec3 s = vec3(7, 157, 113);
    vec3 ip = floor(p); p -= ip;
    vec4 h = vec4(0.0, s.yz, s.y + s.z) + dot(ip, s);
    p = p * p * (3.0 - 2.0 * p);
    h = mix(fract(sin(h) * 43758.5453), fract(sin(h + s.x) * 43758.5453), p.x);
    h.xy = mix(h.xz, h.yw, p.y);
    return mix(h.x, h.y, p.z);
}

vec3 envMap(vec3 rd, vec3 sn) {
    vec3 sRd = rd;
    rd.xy -= current_time/100.0 * 0.25;
    rd *= 3.0;
    float c = n3D(rd) * 0.57 + n3D(rd * 2.0) * 0.28 + n3D(rd * 4.0) * 0.15;
    c = smoothstep(0.4, 1.0, c);
    vec3 col = vec3(c, c * c, c * c * c * c);
    return mix(col, col.yzx, sRd * 0.25 + 0.25);
}



vec2 hash22(vec2 p) {
    float n = sin(dot(p, vec2(41.0, 289.0))); // Use floating point literals for constants
    return fract(vec2(262144.0, 32768.0) * n) * 0.75 + 0.25; // Make sure all numbers are floats
}

float Voronoi(in vec2 p) {
    vec2 g = floor(p), o; p -= g;
    vec3 d = vec3(1);
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            o = vec2(x, y);
            o += hash22(g + o) - p;
            d.z = dot(o, o);
            d.y = max(d.x, min(d.y, d.z));
            d.x = min(d.x, d.z);
        }
    }
    return max(d.y / 1.2 - d.x * 1.0, 0.0) / 1.2;
}

const vec2 iResolution = vec2(1920.0, 1080.0);


vec4 effect_color(vec2 pos) {
  


  vec2 fragCoord = gl_FragCoord.xy;
    vec3 rd = normalize(vec3(2.0 * fragCoord - iResolution, iResolution.y));
    float tm = current_time / 100.0;
    vec2 a = sin(vec2(1.570796, 0.0) + sin(tm / 4.0) * 0.3);
   rd.xy = mat2(a, -a.y, a.x) * rd.xy;
    vec3 ro = vec3(tm, cos(tm / 4.0), 0.0);
     vec3 lp = ro + vec3(cos(tm / 2.0) * 0.5, sin(tm / 2.0) * 0.5, -0.5);

 float d = 0.0;
 float t = 0.0;


      for (int j = 0; j < 32; j++) {
        d = map(ro + rd * t);
        t += d * 0.7;
        if (d < 0.001) break;
    }
    float edge, crv;
    vec3 sp = ro + rd * t;
    vec3 sn = getNormal(sp, edge, crv);
    vec3 ld = lp - sp;
    float c = heightMap(sp.xy);
     vec3 fold = cos(vec3(1, 2, 4) * c * 6.283);
    float c2 = heightMap((sp.xy + sp.z * 0.025) * 6.);
    c2 = cos(c2 * 6.283 * 3.);
    c2 = (clamp(c2 + 0.5, 0.0, 1.0));
    vec3 oC = vec3(1);
    if (fold.x > 0.) oC = vec3(1, 0.05, 0.1) * c2;
    if (fold.x < 0.05 && fold.y < 0.) oC = vec3(1, 0.7, 0.45) * (c2 * 0.25 + 0.75);
    else if (fold.x < 0.) oC = vec3(1, 0.8, 0.4) * c2;
    float lDist = max(length(ld), 0.001);
    float atten = 1.0 / (1.0 + lDist * 0.125);
    ld /= lDist;
    float diff = max(dot(ld, sn), 0.0);
    float spec = pow(max(dot(reflect(-ld, sn), -rd), 0.0), 16.0);
    float fre = pow(clamp(dot(sn, rd) + 1.0, 0.0, 1.0), 3.0);
    crv = crv * 0.9 + 0.1;
    float ao = calculateAO(sp, sn);
    vec3 col = oC * (diff + 0.5) + vec3(1.0, 0.7, 0.4) * spec * 2.0 + vec3(0.4, 0.7, 1.0) * fre;
    col += (oC * 0.5 + 0.5) * envMap(reflect(rd, sn), sn) * 6.0;
    col *= 1.0 - edge * 0.85;
    col *= atten * crv * ao;
    fragColor = vec4(sqrt(clamp(col, 0.0, 1.0)), 1.0);


    return vec4(fragColor);
}


)";








static const char *effect_neon_rings_fragment = R"(

#define PI 3.14159265358979323846
#define TWO_PI 6.28318530717958647692

const vec2 iResolution = vec2(1920.0, 1080.0);

mat2 rotate2D(float r) {
    float c = cos(r);
    float s = sin(r);
    return mat2(c, -s, s, c);
}

vec4 effect_color(vec2 pos) {


     // Use gl_FragCoord as the pixel position
//   pos =  ivec2(gl_FragCoord.xy);

  //   pos = ivec2(gl_FragCoord.xy);
 //   int x = pos.x;
 //   int y = pos.y;

    // Number of circles
    int numCircles = 5;

    // Initialize final color
    vec3 finalColor = vec3(0.0);

    for (int i = 0; i < numCircles; ++i) {
        // Calculate UV coordinates for each circle
//        vec2 uv = vec2(pos) / vec2(width / 3, height / 3);
        vec2 uv = vec2(pos) / vec2(width*0.3333333333333, height*0.3333333333333);

        // Calculate polar coordinates
        float a = atan(uv.y, uv.x);
        float r = length(uv) * (0.2 + 0.1 * float(i));

        // Map polar coordinates to UV
        uv = vec2(a / TWO_PI, r);

        // Apply horizontal movement based on time for each circle
        float xOffset = sin(current_time * 0.02 + float(i)) * 0.2;
        uv.x += xOffset;

        // Time-dependent color
        float xCol = (uv.x - (current_time / 300000.0)) * 3.0;
        xCol = mod(xCol, 3.0);
        vec3 horColour = vec3(0.25, 0.25, 0.25);

        if (xCol < 1.0) {
            horColour.r += 1.0 - xCol;
            horColour.g += xCol;
        } else if (xCol < 2.0) {
            xCol -= 1.0;
            horColour.g += 1.0 - xCol;
            horColour.b += xCol;
        } else {
            xCol -= 2.0;
            horColour.b += 1.0 - xCol;
            horColour.r += xCol;
        }

        // Draw color beam
        uv = (2.0 * uv) - 1.0;
        float beamWidth = (0.7 + 0.5 * cos(uv.x * 1.0 * TWO_PI * 0.15 * clamp(floor(5.0 + 1.0 * cos(current_time)), 0.0, 1.0))) * abs(1.0 / (30.0 * uv.y));
        vec3 horBeam = vec3(beamWidth);
        
        // Add the color for the current circle to the final color
         finalColor += ((horBeam) * horColour);
    }

    // Output the final color
    fragColor = vec4(finalColor, 1.0);


    return vec4(fragColor);
}
)";




static const char *effect_deco_fragment = R"(

vec3 effect(float speed, vec2 uv, float time, float scale) {
    float t = mod(time * 0.005, 6.0);
    float rt = 0.00000000000001 * sin(t * 0.45);

    mat2 m1 = mat2(cos(rt), -sin(rt), -sin(rt), cos(rt));
    vec2 uva = uv * m1 * scale;
    float irt = 0.005 * cos(t * 0.05);
    mat2 m2 = mat2(sin(irt), cos(irt), -cos(irt), sin(irt));

    for (int i = 1; i < 400; i += 1) {
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

vec4 effect_color(vec2 pos) {
 
    vec2 uv = (gl_FragCoord.xy - 0.5 * vec2(width, height)) / vec2(width, height);
    uv *= (10.3 + 0.1 * sin(current_time * 0.01));



    vec3 col = effect(0.001, uv, current_time, 0.5);

    // Set the fragment color
    fragColor = vec4(col, 1.0);


    return vec4(fragColor);
}

)";









// ported from https://www.shadertoy.com/view/3djfzy
static const char *effect_ice_fragment =
    R"(

vec3 effect(float speed, vec2 uv, float time, float scale) {
    float t = mod(time * 0.005, 6.0);
    float rt = 0.00000000000001 * sin(t * 0.45);

    mat2 m1 = mat2(cos(rt), -sin(rt), -sin(rt), cos(rt));
    vec2 uva = uv * m1 * scale;
    float irt = 0.005 * cos(t * 0.05);
    mat2 m2 = mat2(sin(irt), cos(irt), -cos(irt), sin(irt));

    for (int i = 1; i < 400; i += 1) {
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








float rand(in vec2 _st) {
    return fract(sin(dot(_st.xy, vec2(-0.820, -0.840))) * 4757.153);
}

float noise(in vec2 _st) {
    const vec2 d = vec2(0.0, 1.0);
    vec2 b = floor(_st), f = smoothstep(vec2(0.0), vec2(0.1, 0.3), fract(_st));
    return mix(mix(rand(b), rand(b + d.yx), f.x), mix(rand(b + d.xy), rand(b + d.yy), f.x), f.y);
}

float fbm(in vec2 _st) {
    float v = sin(current_time * 0.005) * 0.2;
    float a = 0.3;
    vec2 shift = vec2(100.0);
    mat2 rot = mat2(cos(0.5), sin(1.0), -sin(0.5), acos(0.5));
    for (int i = 0; i < 3; ++i) {
        v += a * noise(_st);
        _st = rot * _st * 2.0 + shift;
        a *= 1.5;
    }
    return v;
}

vec4 effect_color(vec2 pos) {





 vec2 st = (gl_FragCoord.xy * 2.0 - vec2(width, height)) / min(float(width), float(height)) * 0.5;

    vec2 coord = st * 0.2;
    float len;
    for (int i = 0; i < 3; i++) {
        len = length(coord);
        coord.x += sin(coord.y + current_time * 0.001) * 2.1;
        coord.y += cos(coord.x + current_time * 0.001 + cos(len * 1.0)) * 1.0;
    }
    len -= 3.0;

    vec3 color = vec3(0.0);

    vec2 q = vec2(0.0);
    q.x = fbm(st);
    q.y = fbm(st + vec2(-0.450, 0.650));

    vec2 r = vec2(0.0);
    r.x = fbm(st + 1.0 * q + vec2(0.570, 0.520) + 0.1 * current_time * 0.01);
    r.y = fbm(st + 1.0 * q + vec2(0.340, -0.570) + 0.05 * current_time * 0.01);
    float f = fbm(st + r);

    color = mix(color, cos(len + vec3(0.5, 0.0, -0.1)), 1.0);
    color = mix(vec3(0.478, 0.738, 0.760), vec3(0.563, 0.580, 0.667), color);

    fragColor = vec4((f * f * f + .6 * f * f + .5 * f) * color, 1.0);


    return vec4(fragColor);
}
)";





static const char *effect_fire_fragment = R"(

vec3 effect(float speed, vec2 uv, float time, float scale) {




    float t = mod(time * 0.005, 6.0);
    float rt = 0.00000000000001 * sin(t * 0.45);

    mat2 m1 = mat2(cos(rt), -sin(rt), -sin(rt), cos(rt));
    vec2 uva = uv * m1 * scale;
    float irt = 0.005 * cos(t * 0.05);
    mat2 m2 = mat2(sin(irt), cos(irt), -cos(irt), sin(irt));

    for (int i = 1; i < 400; i += 1) {
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


float rand(vec2 n) {
    return fract(cos(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float noise(vec2 n) {
    const vec2 d = vec2(0.0, 1.0);
    vec2 b = floor(n), f = smoothstep(vec2(0.0), vec2(1.0), fract(n));
    return mix(mix(rand(b), rand(b + d.yx), f.x), mix(rand(b + d.xy), rand(b + d.yy), f.x), f.y);
}

float fbm(vec2 n) {
    float total = 0.0, amplitude = 1.0;
    for (int i = 0; i < 4; i++) {
        total += noise(n) * amplitude;
        n += n;
        amplitude *= 0.5;
    }
    return total;
}
vec4 effect_color(vec2 pos) {
 
 vec2 gid = gl_FragCoord.xy;
    vec2 normalizedCoords = gid / vec2(width, height); // Normalize the coordinates

    const vec3 c1 = vec3(0.5, 0.0, 0.1);
    const vec3 c2 = vec3(0.9, 0.0, 0.0);
    const vec3 c3 = vec3(0.2, 0.0, 0.0);
    const vec3 c4 = vec3(1.0, 0.9, 0.0);
    const vec3 c5 = vec3(0.1);
    const vec3 c6 = vec3(0.9);

    vec2 speed = vec2(0.7, 0.4);
    float shift = 1.0;
    float alpha = 1.0;

    float timeEffect = current_time * 0.05; // Slowing down the animation by reducing the current_time factor

    vec2 p = normalizedCoords * 8.0;
    float q = fbm(p - timeEffect * 0.1);
    vec2 r = vec2(fbm(p + q + timeEffect * speed.x - p.x - p.y), fbm(p + q - timeEffect * speed.y));
    vec3 c = mix(c1, c2, fbm(p + r)) + mix(c3, c4, r.x) - mix(c5, c6, r.y);
    fragColor = vec4(c * cos(shift * normalizedCoords.y), alpha);

    
    return vec4(fragColor);
}

)";




static const char *effect_render_fragment = R"(

precision lowp float;
precision lowp sampler2D;

uniform sampler2D in_b0d;
uniform bool ink;
uniform vec4 smoke_color;
uniform vec4 decor_color;
//uniform int regionInfo[20]; // This will likely go unused in this shader.

//out vec4 fragColor;

vec4 effect_color(vec2 pos) {

  vec2 texCoord = gl_FragCoord.xy; // Assumes a fullscreen quad and matching texture size.
    float c, a;
    vec3 color;

    vec4 s = texture(in_b0d, texCoord);
    c = s.x * 800.0;
    if (c > 255.0)
        c = 255.0;
    a = c / 255.0; // Normalize alpha to the range 0.0 to 1.0
  if (ink)
    {
        if (c > 2.0)
            color = mix(decor_color.rgb, smoke_color.rgb, clamp(a, 0.0, 1.0));
        else
            color = mix(decor_color.rgb, vec3(0.0, 0.0, 0.0), clamp(a, 0.0, 1.0));
        
        if (c > 1.5)
            fragColor = vec4(color, 1.0);
        else
            fragColor = decor_color;
    return vec4(fragColor);
    }

    else
    {
        color = mix(decor_color.rgb, smoke_color.rgb, clamp(a, 0.0, 1.0));
   

        fragColor = vec4(color, decor_color.a);
return vec4(fragColor);
   


    }

 fragColor = vec4(color, decor_color.a);
return vec4(fragColor);
    
}



)";






