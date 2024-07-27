static const char *overlay_no_overlay =
    R"(
vec4 overlay_function(vec2 position)
{
    return effect_color(position);
})";

static const char *overlay_rounded_corners =
    R"(
uniform int corner_radius;
uniform int shadow_radius;
uniform vec4 shadow_color;

vec4 overlay_function(vec2 pos)
{
    vec4 c = shadow_color;
    vec4 m = vec4(0.0);
    float diffuse = 1.0 / max(float(shadow_radius / 2), 1.0);

    float distanceToEdgeX = ceil(min(pos.x, width - pos.x));
    float distanceToEdgeY = ceil(min(pos.y, height - pos.y));

    float radii_sum = float(shadow_radius) + float(corner_radius);

    // We have a corner
    if ((distanceToEdgeX <= radii_sum) && (distanceToEdgeY <= radii_sum))
    {
        float d = distance(vec2(distanceToEdgeX, distanceToEdgeY), vec2(radii_sum)) - float(corner_radius);
        vec4 s = mix(c, m, 1.0 - exp(-pow(d * diffuse, 2.0)));

        vec4 eff_color = effect_color(pos);
        return mix(eff_color, s, clamp(d, 0.0, 1.0));
    }

    bool closeToX = ceil(distanceToEdgeX) < float(shadow_radius);
    bool closeToY = ceil(distanceToEdgeY) < float(shadow_radius);

    if (!closeToX && !closeToY)
    {
        return effect_color(pos);
    }

    // Edges
    float d = float(shadow_radius) - (closeToX ? distanceToEdgeX : distanceToEdgeY);
    return mix(c, m, 1.0 - exp(-pow(d * diffuse, 2.0)));
})";




static const char *overlay_beveled_glass =
    R"(


#define MARKER_RADIUS 12.5
#define THICCNESS 2.0

uniform int beveled_radius;
uniform int title_height;
uniform int border_size;



bool isInsideTriangle(vec2 p, vec2 a, vec2 b, vec2 c) {
    vec2 v0 = b - a;
    vec2 v1 = c - a;
    vec2 v2 = p - a;

    float dot00 = dot(v0, v0);
    float dot01 = dot(v0, v1);
    float dot02 = dot(v0, v2);
    float dot11 = dot(v1, v1);
    float dot12 = dot(v1, v2);

    float invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    return (u >= 0.0) && (v >= 0.0) && (u + v <= 1.0);
}


vec4 toBezier(float delta, int i, vec4 P0, vec4 P1, vec4 P2, vec4 P3)
{
    float t = delta * float(i);
    float t2 = t * t;
    float one_minus_t = 1.0 - t;
    float one_minus_t2 = one_minus_t * one_minus_t;
    return (P0 * one_minus_t2 * one_minus_t + P1 * 3.0 * t * one_minus_t2 + P2 * 3.0 * t2 * one_minus_t + P3 * t2 * t);
}

float sin01(float x) {
    return (sin(x) + 1.0) / 2.0;
}





vec4 drawCurveBenzier(vec2 p1, vec2 p2, int setCount) {
    vec4 col = vec4(0.0);
    vec2 fragCoord = gl_FragCoord.xy;
    float curveRadius = float(beveled_radius/2);

    for (int i = 0; i <= setCount; ++i) {
        vec4 bezierPoint = toBezier(1.0 / float(setCount), i, vec4(p1, 0.0, 1.0), vec4(p1 + (p2 - p1) * 0.5, 0.0, 1.0), vec4(p1 + (p2 - p1) * 0.5, 0.0, 1.0), vec4(p2, 0.0, 1.0));
        vec2 curvePoint = bezierPoint.xy / bezierPoint.w;
        float distance = length(fragCoord - curvePoint);
        float curveIntensity = smoothstep(0.0, THICCNESS, curveRadius - distance);
        col.rgb += vec3(0.1, 0.1, 0.1) * curveIntensity;
        col.a = max(col.a, curveIntensity);
    }

    return col;
}



vec4 drawCurve(vec2 p1, vec2 p2, int setCount) {
    vec4 col = vec4(0.0);
    ivec2 storePos = ivec2(gl_FragCoord.xy);

    float curveRadius = float(beveled_radius + 1);
    vec2 fragCoord = vec2(storePos);

      float t = clamp(dot(fragCoord - p1, p2 - p1) / dot(p2 - p1, p2 - p1), 0.0, 1.0);
    vec2 curvePoint = mix(p1, p2, t);

    float distance = length(fragCoord - curvePoint);
    float curveIntensity = smoothstep(0.0, curveRadius, curveRadius - distance);

    // Calculate gradient along the line with grey to white to grey
    vec3 gradientColor;

    if (t < 0.5) {
        gradientColor = mix(vec3(0.7), vec3(1.0), t * 2.0);
    } else {
        gradientColor = mix(vec3(1.0), vec3(0.7), (t - 0.5) * 2.0);
    }

    // Use the curveIntensity to interpolate between gradient colors
    col.rgb = gradientColor * curveIntensity;
    col.a = max(col.a, curveIntensity);

    return col;
}




vec4 overlay_function(vec2 pos)
{
   


vec2 fragCoord = gl_FragCoord.xy;

    



 vec4 col = vec4(0.0);
    float space = 15.0; 

    vec2 p1 = vec2(0.0 + THICCNESS, float(height) - THICCNESS - space);
    vec2 p2 = vec2(0.0 + THICCNESS, 0.0 + THICCNESS + space);

    vec2 x1 = vec2(float(width) - THICCNESS, float(height) - THICCNESS - space);
    vec2 x2 = vec2(float(width) - THICCNESS, 0.0 + THICCNESS + space);

    vec2 y1 = vec2(0.0 + THICCNESS + space, float(height) - THICCNESS);
    vec2 y2 = vec2(float(width) - THICCNESS - space, float(height) - THICCNESS);

    vec2 z1 = vec2(0.0 + THICCNESS + space, 0.0 + THICCNESS);
    vec2 z2 = vec2(float(width) - THICCNESS - space, 0.0 + THICCNESS);


 float curveRadius = float(beveled_radius);
// Draw the lines connecting the points

int setwidth = int(width);
int setheight = int(height);


if (setwidth<=400 && setheight<=300 ){
    col += drawCurveBenzier(p1, p2, setheight);
    col += drawCurveBenzier(x1, x2, setheight);
    col += drawCurveBenzier(y1, y2, setwidth);
    col += drawCurveBenzier(z1, z2, setwidth);
}
else {
    col += drawCurve(p1, p2, setheight);
    col += drawCurve(x1, x2, setheight);
    col += drawCurve(y1, y2, setwidth);
    col += drawCurve(z1, z2, setwidth);
    }


float INNERspace = 0.0; 




//left
    vec2 INNERp1 = vec2(0.0 - THICCNESS+ float(border_size), float(height)   + THICCNESS -float(title_height) );
    vec2 INNERp2 = vec2(0.0 - THICCNESS+ float(border_size),0.0 - THICCNESS + INNERspace+float(border_size));
//right     
    vec2 INNERx1 = vec2(float(width) + THICCNESS-float(border_size), float(height)   + THICCNESS -float(title_height));
    vec2 INNERx2 = vec2(float(width) + THICCNESS-float(border_size),0.0 - THICCNESS + INNERspace+float(border_size));


//top
    vec2 INNERy1 = vec2(0.0 + THICCNESS + INNERspace+float(border_size), float(height)   + THICCNESS -float(title_height));
    vec2 INNERy2 = vec2(float(width) - THICCNESS - INNERspace-float(border_size), float(height) + THICCNESS-float(title_height));

//bottom
   vec2 INNERz1 = vec2(0.0 +INNERspace+float(border_size), 0.0 - THICCNESS + INNERspace+float(border_size));
    vec2 INNERz2 = vec2(float(width) - THICCNESS - INNERspace-float(border_size) ,0.0 - THICCNESS + INNERspace+float(border_size) );





if (setwidth<=400 && setheight<=300 ){
    col += drawCurveBenzier(INNERp1, INNERp2, setheight);
    col += drawCurveBenzier(INNERx1, INNERx2, setheight);
    col += drawCurveBenzier(INNERy1, INNERy2, setwidth);
    col += drawCurveBenzier(INNERz1, INNERz2, setwidth);
}
else{
    col += drawCurve(INNERp1, INNERp2, setheight);
    col += drawCurve(INNERx1, INNERx2, setheight);
    col += drawCurve(INNERy1, INNERy2, setwidth);
    col += drawCurve(INNERz1, INNERz2, setwidth);
    }


    vec2 p3 = fragCoord;
    vec2 p12 = p2 - p1;
    vec2 p13 = p3 - p1;

    vec2 x3 = fragCoord;
    vec2 x12 = x2 - x1;
    vec2 x13 = x3 - x1;

    vec2 y3 = fragCoord;
    vec2 y12 = y2 - y1;
    vec2 y13 = y3 - y1;

    vec2 z3 = fragCoord;
    vec2 z12 = z2 - z1;
    vec2 z13 = z3 - z1;

    float d = dot(p12, p13) / length(p12); // = length(p13) * cos(angle)
    float dx = dot(x12, x13) / length(x12);
    float dy = dot(y12, y13) / length(y12);
    float dz = dot(z12, z13) / length(z12);

    vec2 p4 = p1 + normalize(p12) * d;
    vec2 x4 = x1 + normalize(x12) * dx;
    vec2 y4 = y1 + normalize(y12) * dy;
    vec2 z4 = z1 + normalize(z12) * dz;


    if (((length(p4 - p3) < THICCNESS && length(p4 - p1) <= length(p12) && length(p4 - p2) <= length(p12)) ||
         (length(x4 - x3) < THICCNESS && length(x4 - x1) <= length(x12) && length(x4 - x2) <= length(x12))) ||
         (length(y4 - y3) < THICCNESS && length(y4 - y1) <= length(y12) && length(y4 - y2) <= length(y12)) ||
         (length(z4 - z3) < THICCNESS && length(z4 - z1) <= length(z12) && length(z4 - z2) <= length(z12))) { 
        col = vec4(0.1, 0.1, 0.1, 1.0);
    }
    

   
    vec2 INNERp3 = fragCoord;
    vec2 INNERp12 = INNERp2 - INNERp1;
    vec2 INNERp13 = INNERp3 - INNERp1;

    vec2 INNERx3 = fragCoord;
    vec2 INNERx12 = INNERx2 - INNERx1;
    vec2 INNERx13 = INNERx3 - INNERx1;

    vec2 INNERy3 = fragCoord;
    vec2 INNERy12 = INNERy2 - INNERy1;
    vec2 INNERy13 = INNERy3 - INNERy1;

    vec2 INNERz3 = fragCoord;
    vec2 INNERz12 = INNERz2 - INNERz1;
    vec2 INNERz13 = INNERz3 - INNERz1;

    float INNERd = dot(INNERp12, INNERp13) / length(INNERp12); // = length(p13) * cos(angle)
    float INNERdx = dot(INNERx12, INNERx13) / length(INNERx12);
    float INNERdy = dot(INNERy12, INNERy13) / length(INNERy12);
    float INNERdz = dot(INNERz12, INNERz13) / length(INNERz12);

    vec2 INNERp4 = INNERp1 + normalize(INNERp12) * INNERd;
    vec2 INNERx4 = INNERx1 + normalize(INNERx12) * INNERdx;
    vec2 INNERy4 = INNERy1 + normalize(INNERy12) * INNERdy;
    vec2 INNERz4 = INNERz1 + normalize(INNERz12) * INNERdz;
    
       if (((length(INNERp4 - INNERp3) < THICCNESS && length(INNERp4 - INNERp1) <= length(INNERp12) && length(INNERp4 - INNERp2) <= length(INNERp12)) ||
         (length(INNERx4 - INNERx3) < THICCNESS && length(INNERx4 - INNERx1) <= length(INNERx12) && length(INNERx4 - INNERx2) <= length(INNERx12))) ||
         (length(INNERy4 - INNERy3) < THICCNESS && length(INNERy4 - INNERy1) <= length(INNERy12) && length(INNERy4 - INNERy2) <= length(INNERy12)) ||
         (length(INNERz4 - INNERz3) < THICCNESS && length(INNERz4 - INNERz1) <= length(INNERz12) && length(INNERz4 - INNERz2) <= length(INNERz12))) { 
        col = vec4(0.1, 0.1, 0.1, 1.0);
    }       


    // Draw the lines connecting the points
       // Calculate the line segment connecting p1 and y1
    vec2 dir = normalize(y1 - p1);
    vec2 relPoint = fragCoord - p1;
    float alongLine = dot(relPoint, dir);
    float distance = length(relPoint - alongLine * dir);

    // Draw the line connecting p1 and y1
    if (distance < THICCNESS && alongLine > 0.0 && alongLine < length(y1 - p1)) {
        col = vec4(0.1, 0.1, 0.1, 1.0);
    }


    
    dir = normalize(y2 - x1);
    relPoint = fragCoord - x1;
     alongLine = dot(relPoint, dir);
     distance = length(relPoint - alongLine * dir);

    if (distance < THICCNESS && alongLine > 0.0 && alongLine < length(x1 - y2)) {
        col = vec4(0.1, 0.1, 0.1, 1.0);
    }


 dir = normalize(p2 - z1);
    relPoint = fragCoord - z1;
     alongLine = dot(relPoint, dir);
     distance = length(relPoint - alongLine * dir);

    if (distance < THICCNESS && alongLine > 0.0 && alongLine < length(z1 - p2)) {
        col = vec4(0.1, 0.1, 0.1, 1.0);
    }



 dir = normalize(x2 - z2);
    relPoint = fragCoord - z2;
     alongLine = dot(relPoint, dir);
     distance = length(relPoint - alongLine * dir);

    if (distance < THICCNESS && alongLine > 0.0 && alongLine < length(x2 - z2)) {
        col = vec4(0.1, 0.1, 0.1, 1.0);
    }
    // Draw other lines...

    vec2 t1 = vec2(0.0, float(height));
    vec2 t2 = vec2(float(width), float(height));
    vec2 t3 = vec2(-0.5, -0.5);
    vec2 t4 = vec2(float(width), -0.5);


   
vec4 combine_col= col+effect_color(pos);
  




int shadow_radius=-0;
int corner_radius=15;


vec4 c = vec4(1.0);
vec4 m = vec4(0.0, 0.0, 0.0, 0.0); // Transparent for the cut corners
float diffuse = 1.0 / max(float(shadow_radius / 2), 1.0);

float distanceToEdgeX = ceil(min(pos.x, width - pos.x));
float distanceToEdgeY = ceil(min(pos.y, height - pos.y));

float radii_sum = float(shadow_radius) + float(corner_radius);

// Check for 45-degree cut corners
if ((distanceToEdgeX <= radii_sum) && (distanceToEdgeY <= radii_sum))
{
    // Calculate if within the 45-degree triangle area
    float diagonalDistance = distanceToEdgeX + distanceToEdgeY;
    if (diagonalDistance <= radii_sum) {
        // Inside the cut corner area, make it fully transparent
        return m; // Return transparent color
    }
}

/*
bool closeToX = distanceToEdgeX < float(shadow_radius);
bool closeToY = distanceToEdgeY < float(shadow_radius);

if (!closeToX && !closeToY)
{
  //  return effect_color(pos);
}

// Handle shadow along edges
float shadow_d = float(shadow_radius) - (closeToX ? distanceToEdgeX : distanceToEdgeY);
vec4 edgeShadow = mix(c, m, 1.0 - exp(-pow(shadow_d  * diffuse, 2.0)));
vec4 shadow = vec4(edgeShadow.rgb, edgeShadow.a * clamp(1.0 - shadow_d  / float(shadow_radius), 0.0, 1.0)); // Adjust alpha to fade shadow
*/

 return combine_col;
  
})";
