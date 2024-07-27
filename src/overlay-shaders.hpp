static const char *overlay_no_overlay = R"(
vec4 overlay_function(vec2 position)
{
    return effect_color(position);
})";

static const char *overlay_rounded_corners = R"(
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
