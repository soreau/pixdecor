#include <wayfire/debug.hpp>

#include "deco-effects.hpp"

namespace wf
{
namespace decor
{
static const char *motion_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 1, r32f) uniform readonly image2D in_b0u;
layout (binding = 1, r32f) uniform writeonly image2D out_b0u;
layout (binding = 2, r32f) uniform readonly image2D in_b0v;
layout (binding = 2, r32f) uniform writeonly image2D out_b0v;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (binding = 3, r32f) uniform writeonly image2D out_b0d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 3) uniform int px;
layout(location = 4) uniform int py;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int time;

float rand(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

void motion(int x, int y)
{
	int i, i0, i1, j, j0, j1, d = 5;

	if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1 ||
	    (x >= border_size && x <= width - border_size && y >= title_height && y <= height - border_size))
	{
		return;
	}

	if (x - d < 1)
		i0 = 1;
	else
		i0 = x - d;
	if (i0 + 2 * d > width - 1)
		i1 = width - 1;
	else
		i1 = i0 + 2 * d;
	if (i1 > border_size && i1 < width - border_size && y > title_height && y < height - border_size)
		i1 = border_size - 1;

	if (y - d < 1)
		j0 = 1;
	else
		j0 = y - d;
	if (j0 + 2 * d > height - 1)
		j1 = height - 1;
	else
		j1 = j0 + 2 * d;
	if (x > border_size && x < width - border_size && j1 > title_height && j1 < height - border_size)
		j1 = title_height - 1;

	for (i = i0; i < i1; i++)
	{
		for (j = j0; j < j1; j++) {
			vec4 b0u = imageLoad(in_b0u, ivec2(i, j));
			vec4 b0v = imageLoad(in_b0v, ivec2(i, j));
			vec4 b0d = imageLoad(in_b0d, ivec2(i, j));
			float u = b0u.x;
			float v = b0v.x;
			float d = b0d.x;
			imageStore(out_b0u, ivec2(i, j), vec4(u + float(256 - int(float(time) * rand(vec2(i, j))) % 512), 0.0, 0.0, 0.0));
			imageStore(out_b0v, ivec2(i, j), vec4(v + float(256 - int(float(time) * rand(vec2(i - 1, j + 1))) % 512), 0.0, 0.0, 0.0));
			imageStore(out_b0d, ivec2(i, j), vec4(d + 1.0, 0.0, 0.0, 0.0));
		}
	}
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x == px && pos.y == py)
    {
        motion(pos.x, pos.y);
    }
}
)";

static const char *diffuse1_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 0, rgba32f) uniform readonly image2D in_tex;
layout (binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout (binding = 1, r32f) uniform readonly image2D in_b0u;
layout (binding = 1, r32f) uniform writeonly image2D out_b0u;
layout (binding = 2, r32f) uniform readonly image2D in_b0v;
layout (binding = 2, r32f) uniform writeonly image2D out_b0v;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (binding = 3, r32f) uniform writeonly image2D out_b0d;
layout (binding = 4, r32f) uniform readonly image2D in_b1u;
layout (binding = 4, r32f) uniform writeonly image2D out_b1u;
layout (binding = 5, r32f) uniform readonly image2D in_b1v;
layout (binding = 5, r32f) uniform writeonly image2D out_b1v;
layout (binding = 6, r32f) uniform readonly image2D in_b1d;
layout (binding = 6, r32f) uniform writeonly image2D out_b1d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;

void diffuse1(int x, int y)
{
	int k, stride;
	float t, a = 0.0002;

	stride = width;

	if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1 ||
	    (x >= border_size && x <= width - border_size && y >= title_height && y <= height - border_size))
	{
		return;
	}

	vec4 s = imageLoad(in_b0u, ivec2(x, y));
	vec4 d1 = imageLoad(in_b1u, ivec2(x - 1, y));
	vec4 d2 = imageLoad(in_b1u, ivec2(x + 1, y));
	vec4 d3 = imageLoad(in_b1u, ivec2(x, y - 1));
	vec4 d4 = imageLoad(in_b1u, ivec2(x, y + 1));
	float sx = s.x;
	float du1 = d1.x;
	float du2 = d2.x;
	float du3 = d3.x;
	float du4 = d4.x;
	float t1 = du1 + du2 + du3 + du4;
	s = imageLoad(in_b0v, ivec2(x, y));
	d1 = imageLoad(in_b1v, ivec2(x - 1, y));
	d2 = imageLoad(in_b1v, ivec2(x + 1, y));
	d3 = imageLoad(in_b1v, ivec2(x, y - 1));
	d4 = imageLoad(in_b1v, ivec2(x, y + 1));
	float sy = s.x;
	du1 = d1.x;
	du2 = d2.x;
	du3 = d3.x;
	du4 = d4.x;
	float t2 = du1 + du2 + du3 + du4;
	imageStore(out_b1u, ivec2(x, y), vec4((sx + a * t1) / (1.0 + 4.0 * a) * 0.995, 0.0, 0.0, 0.0));
	imageStore(out_b1v, ivec2(x, y), vec4((sy + a * t2) / (1.0 + 4.0 * a) * 0.995, 0.0, 0.0, 0.0));
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    diffuse1(pos.x, pos.y);
}
)";

static const char *project1_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 0, rgba32f) uniform readonly image2D in_tex;
layout (binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout (binding = 1, r32f) uniform readonly image2D in_b0u;
layout (binding = 1, r32f) uniform writeonly image2D out_b0u;
layout (binding = 2, r32f) uniform readonly image2D in_b0v;
layout (binding = 2, r32f) uniform writeonly image2D out_b0v;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (binding = 3, r32f) uniform writeonly image2D out_b0d;
layout (binding = 4, r32f) uniform readonly image2D in_b1u;
layout (binding = 4, r32f) uniform writeonly image2D out_b1u;
layout (binding = 5, r32f) uniform readonly image2D in_b1v;
layout (binding = 5, r32f) uniform writeonly image2D out_b1v;
layout (binding = 6, r32f) uniform readonly image2D in_b1d;
layout (binding = 6, r32f) uniform writeonly image2D out_b1d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;

void project1(int x, int y)
{
	int k, l, s;
	float h;

	h = 1.0 / float(width);
	s = width;

	if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1 ||
	    (x >= border_size && x <= width - border_size && y >= title_height && y <= height - border_size))
	{
		return;
	}

	vec4 s1 = imageLoad(in_b1u, ivec2(x - 1, y));
	vec4 s2 = imageLoad(in_b1u, ivec2(x + 1, y));
	vec4 s3 = imageLoad(in_b1v, ivec2(x, y - 1));
	vec4 s4 = imageLoad(in_b1v, ivec2(x, y + 1));
	float u1 = s1.x;
	float u2 = s2.x;
	float v1 = s3.x;
	float v2 = s4.x;
	imageStore(out_b0u, ivec2(x, y), vec4(0.0, 0.0, 0.0, 0.0));
	imageStore(out_b0v, ivec2(x, y), vec4(-0.5 * h * (u2 - u1 + v2 - v1), 0.0, 0.0, 0.0));
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    project1(pos.x, pos.y);
}
)";

static const char *project2_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 0, rgba32f) uniform readonly image2D in_tex;
layout (binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout (binding = 1, r32f) uniform readonly image2D in_b0u;
layout (binding = 1, r32f) uniform writeonly image2D out_b0u;
layout (binding = 2, r32f) uniform readonly image2D in_b0v;
layout (binding = 2, r32f) uniform writeonly image2D out_b0v;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (binding = 3, r32f) uniform writeonly image2D out_b0d;
layout (binding = 4, r32f) uniform readonly image2D in_b1u;
layout (binding = 4, r32f) uniform writeonly image2D out_b1u;
layout (binding = 5, r32f) uniform readonly image2D in_b1v;
layout (binding = 5, r32f) uniform writeonly image2D out_b1v;
layout (binding = 6, r32f) uniform readonly image2D in_b1d;
layout (binding = 6, r32f) uniform writeonly image2D out_b1d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;

void project2(int x, int y)
{
	int k, l, s;
	float h;

	h = 1.0 / float(width);
	s = width;

	if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1 ||
	    (x >= border_size && x <= width - border_size && y >= title_height && y <= height - border_size))
	{
		return;
	}

	vec4 s0 = imageLoad(in_b0v, ivec2(x, y));
	vec4 s1 = imageLoad(in_b0u, ivec2(x - 1, y));
	vec4 s2 = imageLoad(in_b0u, ivec2(x + 1, y));
	vec4 s3 = imageLoad(in_b0u, ivec2(x, y - 1));
	vec4 s4 = imageLoad(in_b0u, ivec2(x, y + 1));
	float u1 = s1.x;
	float u2 = s2.x;
	float u3 = s3.x;
	float u4 = s4.x;
	imageStore(out_b0u, ivec2(x, y), vec4((s0.x + u1 + u2 + u3 + u4) / 4.0, 0.0, 0.0, 0.0));
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    project2(pos.x, pos.y);
}
)";

static const char *project3_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 0, rgba32f) uniform readonly image2D in_tex;
layout (binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout (binding = 1, r32f) uniform readonly image2D in_b0u;
layout (binding = 1, r32f) uniform writeonly image2D out_b0u;
layout (binding = 2, r32f) uniform readonly image2D in_b0v;
layout (binding = 2, r32f) uniform writeonly image2D out_b0v;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (binding = 3, r32f) uniform writeonly image2D out_b0d;
layout (binding = 4, r32f) uniform readonly image2D in_b1u;
layout (binding = 4, r32f) uniform writeonly image2D out_b1u;
layout (binding = 5, r32f) uniform readonly image2D in_b1v;
layout (binding = 5, r32f) uniform writeonly image2D out_b1v;
layout (binding = 6, r32f) uniform readonly image2D in_b1d;
layout (binding = 6, r32f) uniform writeonly image2D out_b1d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;

void project3(int x, int y)
{
	int k, l, s;
	float h;

	h = 1.0 / float(width);
	s = width;

	if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1 ||
	    (x >= border_size && x <= width - border_size && y >= title_height && y <= height - border_size))
	{
		return;
	}

	vec4 s0x = imageLoad(in_b1u, ivec2(x, y));
	vec4 s1 = imageLoad(in_b0u, ivec2(x - 1, y));
	vec4 s2 = imageLoad(in_b0u, ivec2(x + 1, y));
	vec4 s0y = imageLoad(in_b1v, ivec2(x, y));
	vec4 s3 = imageLoad(in_b0u, ivec2(x, y - 1));
	vec4 s4 = imageLoad(in_b0u, ivec2(x, y + 1));
	float su = s0x.x;
	float u1 = s1.x;
	float u2 = s2.x;
	float sv = s0y.x;
	float u3 = s3.x;
	float u4 = s4.x;
	imageStore(out_b1u, ivec2(x, y), vec4(su - 0.5 * (u2 - u1) / h, 0.0, 0.0, 0.0));
	imageStore(out_b1v, ivec2(x, y), vec4(sv - 0.5 * (u4 - u3) / h, 0.0, 0.0, 0.0));
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    project3(pos.x, pos.y);
}
)";

static const char *advect1_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 0, rgba32f) uniform readonly image2D in_tex;
layout (binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout (binding = 1, r32f) uniform readonly image2D in_b0u;
layout (binding = 1, r32f) uniform writeonly image2D out_b0u;
layout (binding = 2, r32f) uniform readonly image2D in_b0v;
layout (binding = 2, r32f) uniform writeonly image2D out_b0v;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (binding = 3, r32f) uniform writeonly image2D out_b0d;
layout (binding = 4, r32f) uniform readonly image2D in_b1u;
layout (binding = 4, r32f) uniform writeonly image2D out_b1u;
layout (binding = 5, r32f) uniform readonly image2D in_b1v;
layout (binding = 5, r32f) uniform writeonly image2D out_b1v;
layout (binding = 6, r32f) uniform readonly image2D in_b1d;
layout (binding = 6, r32f) uniform writeonly image2D out_b1d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;

void advect1(int x, int y) /* b1.u, b1.v, b1.u, b0.u */
{
	int stride;
	int i, j;
	float fx, fy;

	stride = width;

	if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1 ||
	    (x >= border_size && x <= width - border_size && y >= title_height && y <= height - border_size))
	{
		return;
	}

	vec4 sx = imageLoad(in_b1u, ivec2(x, y));
	vec4 sy = imageLoad(in_b1v, ivec2(x, y));
	float ix = float(x) - sx.x;
	float iy = float(y) - sy.x;
	if (ix < 0.5)
		ix = 0.5;
	if (iy < 0.5)
		iy = 0.5;
	if (ix > float(width) - 1.5)
		ix = float(width) - 1.5;
	if (iy > float(height) - 1.5)
		iy = float(height) - 1.5;
	i = int(ix);
	j = int(iy);
	fx = ix - float(i);
	fy = iy - float(j);
	vec4 s0x = imageLoad(in_b1u, ivec2(i,     j));
	vec4 s1x = imageLoad(in_b1u, ivec2(i + 1, j));
	vec4 s2x = imageLoad(in_b1u, ivec2(i,     j + 1));
	vec4 s3x = imageLoad(in_b1u, ivec2(i + 1, j + 1));
	float p1 = (s0x.x * (1.0 - fx) + s1x.x * fx) * (1.0 - fy) + (s2x.x * (1.0 - fx) + s3x.x * fx) * fy;
	imageStore(out_b0u, ivec2(x, y), vec4(p1, 0.0, 0.0, 0.0));
	ix = float(x) - sx.x;
	iy = float(y) - sy.x;
	if (ix < 0.5)
		ix = 0.5;
	if (iy < 0.5)
		iy = 0.5;
	if (ix > float(width) - 1.5)
		ix = float(width) - 1.5;
	if (iy > float(height) - 1.5)
		iy = float(height) - 1.5;
	i = int(ix);
	j = int(iy);
	fx = ix - float(i);
	fy = iy - float(j);
	vec4 s0y = imageLoad(in_b1v, ivec2(i,     j));
	vec4 s1y = imageLoad(in_b1v, ivec2(i + 1, j));
	vec4 s2y = imageLoad(in_b1v, ivec2(i,     j + 1));
	vec4 s3y = imageLoad(in_b1v, ivec2(i + 1, j + 1));
	float p2 = (s0y.x * (1.0 - fx) + s1y.x * fx) * (1.0 - fy) + (s2y.x * (1.0 - fx) + s3y.x * fx) * fy;
	imageStore(out_b0v, ivec2(x, y), vec4(p2, 0.0, 0.0, 0.0));
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    advect1(pos.x, pos.y);
}
)";

static const char *project4_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 0, rgba32f) uniform readonly image2D in_tex;
layout (binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout (binding = 1, r32f) uniform readonly image2D in_b0u;
layout (binding = 1, r32f) uniform writeonly image2D out_b0u;
layout (binding = 2, r32f) uniform readonly image2D in_b0v;
layout (binding = 2, r32f) uniform writeonly image2D out_b0v;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (binding = 3, r32f) uniform writeonly image2D out_b0d;
layout (binding = 4, r32f) uniform readonly image2D in_b1u;
layout (binding = 4, r32f) uniform writeonly image2D out_b1u;
layout (binding = 5, r32f) uniform readonly image2D in_b1v;
layout (binding = 5, r32f) uniform writeonly image2D out_b1v;
layout (binding = 6, r32f) uniform readonly image2D in_b1d;
layout (binding = 6, r32f) uniform writeonly image2D out_b1d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;

void project4(int x, int y)
{
	int k, l, s;
	float h;

	h = 1.0 / float(width);
	s = width;

	if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1 ||
	    (x >= border_size && x <= width - border_size && y >= title_height && y <= height - border_size))
	{
		return;
	}

	vec4 s1 = imageLoad(in_b0u, ivec2(x - 1, y));
	vec4 s2 = imageLoad(in_b0u, ivec2(x + 1, y));
	vec4 s3 = imageLoad(in_b0v, ivec2(x, y - 1));
	vec4 s4 = imageLoad(in_b0v, ivec2(x, y + 1));
	float u1 = s1.x;
	float u2 = s2.x;
	float v1 = s3.x;
	float v2 = s4.x;
	imageStore(out_b1u, ivec2(x, y), vec4(0.0, 0.0, 0.0, 0.0));
	imageStore(out_b1v, ivec2(x, y), vec4(-0.5 * h * (u2 - u1 + v2 - v1), 0.0, 0.0, 0.0));
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    project4(pos.x, pos.y);
}
)";

static const char *project5_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 0, rgba32f) uniform readonly image2D in_tex;
layout (binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout (binding = 1, r32f) uniform readonly image2D in_b0u;
layout (binding = 1, r32f) uniform writeonly image2D out_b0u;
layout (binding = 2, r32f) uniform readonly image2D in_b0v;
layout (binding = 2, r32f) uniform writeonly image2D out_b0v;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (binding = 3, r32f) uniform writeonly image2D out_b0d;
layout (binding = 4, r32f) uniform readonly image2D in_b1u;
layout (binding = 4, r32f) uniform writeonly image2D out_b1u;
layout (binding = 5, r32f) uniform readonly image2D in_b1v;
layout (binding = 5, r32f) uniform writeonly image2D out_b1v;
layout (binding = 6, r32f) uniform readonly image2D in_b1d;
layout (binding = 6, r32f) uniform writeonly image2D out_b1d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;

void project5(int x, int y)
{
	int k, l, s;
	float h;

	h = 1.0 / float(width);
	s = width;

	if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1 ||
	    (x >= border_size && x <= width - border_size && y >= title_height && y <= height - border_size))
	{
		return;
	}

	vec4 s0 = imageLoad(in_b1v, ivec2(x, y));
	vec4 s1 = imageLoad(in_b1u, ivec2(x - 1, y));
	vec4 s2 = imageLoad(in_b1u, ivec2(x + 1, y));
	vec4 s3 = imageLoad(in_b1u, ivec2(x, y - 1));
	vec4 s4 = imageLoad(in_b1u, ivec2(x, y + 1));
	float u1 = s1.x;
	float u2 = s2.x;
	float u3 = s3.x;
	float u4 = s4.x;
	imageStore(out_b1u, ivec2(x, y), vec4((s0.x + u1 + u2 + u3 + u4) / 4.0, 0.0, 0.0, 0.0));
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    project5(pos.x, pos.y);
}
)";

static const char *project6_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 0, rgba32f) uniform readonly image2D in_tex;
layout (binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout (binding = 1, r32f) uniform readonly image2D in_b0u;
layout (binding = 1, r32f) uniform writeonly image2D out_b0u;
layout (binding = 2, r32f) uniform readonly image2D in_b0v;
layout (binding = 2, r32f) uniform writeonly image2D out_b0v;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (binding = 3, r32f) uniform writeonly image2D out_b0d;
layout (binding = 4, r32f) uniform readonly image2D in_b1u;
layout (binding = 4, r32f) uniform writeonly image2D out_b1u;
layout (binding = 5, r32f) uniform readonly image2D in_b1v;
layout (binding = 5, r32f) uniform writeonly image2D out_b1v;
layout (binding = 6, r32f) uniform readonly image2D in_b1d;
layout (binding = 6, r32f) uniform writeonly image2D out_b1d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;

void project6(int x, int y)
{
	int k, l, s;
	float h;

	h = 1.0 / float(width);
	s = width;

	if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1 ||
	    (x >= border_size && x <= width - border_size && y >= title_height && y <= height - border_size))
	{
		return;
	}

	vec4 s0x = imageLoad(in_b0u, ivec2(x, y));
	vec4 s1 = imageLoad(in_b1u, ivec2(x - 1, y));
	vec4 s2 = imageLoad(in_b1u, ivec2(x + 1, y));
	vec4 s0y = imageLoad(in_b0v, ivec2(x, y));
	vec4 s3 = imageLoad(in_b1u, ivec2(x, y - 1));
	vec4 s4 = imageLoad(in_b1u, ivec2(x, y + 1));
	float su = s0x.x;
	float u1 = s1.x;
	float u2 = s2.x;
	float sv = s0y.x;
	float u3 = s3.x;
	float u4 = s4.x;
	imageStore(out_b0u, ivec2(x, y), vec4(su - 0.5 * (u2 - u1) / h, 0.0, 0.0, 0.0));
	imageStore(out_b0v, ivec2(x, y), vec4(sv - 0.5 * (u4 - u3) / h, 0.0, 0.0, 0.0));
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    project6(pos.x, pos.y);
}
)";

static const char *diffuse2_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 0, rgba32f) uniform readonly image2D in_tex;
layout (binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout (binding = 1, r32f) uniform readonly image2D in_b0u;
layout (binding = 1, r32f) uniform writeonly image2D out_b0u;
layout (binding = 2, r32f) uniform readonly image2D in_b0v;
layout (binding = 2, r32f) uniform writeonly image2D out_b0v;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (binding = 3, r32f) uniform writeonly image2D out_b0d;
layout (binding = 4, r32f) uniform readonly image2D in_b1u;
layout (binding = 4, r32f) uniform writeonly image2D out_b1u;
layout (binding = 5, r32f) uniform readonly image2D in_b1v;
layout (binding = 5, r32f) uniform writeonly image2D out_b1v;
layout (binding = 6, r32f) uniform readonly image2D in_b1d;
layout (binding = 6, r32f) uniform writeonly image2D out_b1d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;

void diffuse2(int x, int y)
{
	int k, stride;
	float t, a = 0.0002;

	stride = width;

	if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1 ||
	    (x >= border_size && x <= width - border_size && y >= title_height && y <= height - border_size))
	{
		return;
	}

	vec4 s = imageLoad(in_b0d, ivec2(x, y));
	vec4 d1 = imageLoad(in_b1d, ivec2(x - 1, y));
	vec4 d2 = imageLoad(in_b1d, ivec2(x + 1, y));
	vec4 d3 = imageLoad(in_b1d, ivec2(x, y - 1));
	vec4 d4 = imageLoad(in_b1d, ivec2(x, y + 1));
	float sz = s.x;
	float du1 = d1.x;
	float du2 = d2.x;
	float du3 = d3.x;
	float du4 = d4.x;
	t = du1 + du2 + du3 + du4;
	imageStore(out_b1d, ivec2(x, y), vec4((sz + a * t) / (1.0 + 4.0 * a) * 0.995, 0.0, 0.0, 0.0));
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    diffuse2(pos.x, pos.y);
}
)";

static const char *advect2_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 0, rgba32f) uniform readonly image2D in_tex;
layout (binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout (binding = 1, r32f) uniform readonly image2D in_b0u;
layout (binding = 1, r32f) uniform writeonly image2D out_b0u;
layout (binding = 2, r32f) uniform readonly image2D in_b0v;
layout (binding = 2, r32f) uniform writeonly image2D out_b0v;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (binding = 3, r32f) uniform writeonly image2D out_b0d;
layout (binding = 4, r32f) uniform readonly image2D in_b1u;
layout (binding = 4, r32f) uniform writeonly image2D out_b1u;
layout (binding = 5, r32f) uniform readonly image2D in_b1v;
layout (binding = 5, r32f) uniform writeonly image2D out_b1v;
layout (binding = 6, r32f) uniform readonly image2D in_b1d;
layout (binding = 6, r32f) uniform writeonly image2D out_b1d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;

void advect2(int x, int y) /*b0u, b0v, b1d, b0d*/
{
	int stride;
	int i, j;
	float fx, fy;

	stride = width;

	if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1 ||
	    (x >= border_size && x <= width - border_size && y >= title_height && y <= height - border_size))
	{
		return;
	}

	vec4 sx = imageLoad(in_b0u, ivec2(x, y));
	vec4 sy = imageLoad(in_b0v, ivec2(x, y));
	float ix = float(x) - sx.x;
	float iy = float(y) - sy.x;
	if (ix < 0.5)
		ix = 0.5;
	if (iy < 0.5)
		iy = 0.5;
	if (ix > float(width) - 1.5)
		ix = float(width) - 1.5;
	if (iy > float(height) - 1.5)
		iy = float(height) - 1.5;
	i = int(ix);
	j = int(iy);
	fx = ix - float(i);
	fy = iy - float(j);
	vec4 s0z = imageLoad(in_b1d, ivec2(i,     j));
	vec4 s1z = imageLoad(in_b1d, ivec2(i + 1, j));
	vec4 s2z = imageLoad(in_b1d, ivec2(i,     j + 1));
	vec4 s3z = imageLoad(in_b1d, ivec2(i + 1, j + 1));
	float p1 = (s0z.x * (1.0 - fx) + s1z.x * fx) * (1.0 - fy) + (s2z.x * (1.0 - fx) + s3z.x * fx) * fy;
	imageStore(out_b0d, ivec2(x, y), vec4(p1, 0.0, 0.0, 0.0));
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    advect2(pos.x, pos.y);
}
)";

static const char *render_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 4) uniform bool ink;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform vec4 smoke_color;
layout(location = 8) uniform vec4 decor_color;

void render(int x, int y)
{
	float c, r, g, b, a;

	if (x >= border_size && x <= width - border_size && y >= title_height && y <= height - border_size)
	{
		return;
	}

	vec4 s = imageLoad(in_b0d, ivec2(x, y));
	c = s.x * (800.0 / 256.0);
	if (c > 1.0)
		c = 1.0;
	a = c * smoke_color.a;
	if (ink)
	{
		r = exp(pow(c * smoke_color.r - 0.85, 2.) * -33.0);
		g = exp(pow(c * smoke_color.g - 0.85, 2.) * -33.0);
		b = exp(pow(c * smoke_color.b - 0.85, 2.) * -33.0);
	} else
	{
		r = c * smoke_color.r;
		g = c * smoke_color.g;
		b = c * smoke_color.b;
	}
	if (c < 0.5)
	{
		r = decor_color.r;
		g = decor_color.g;
		b = decor_color.b;
		a = decor_color.a;
	}
	imageStore(out_tex, ivec2(x, y), vec4(r, g, b, a));
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    render(pos.x, pos.y);
}
)";

void setup_shader(GLuint *program, std::string source)
{
    auto compute_shader  = OpenGL::compile_shader(source.c_str(), GL_COMPUTE_SHADER);
    auto compute_program = GL_CALL(glCreateProgram());
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

smoke_t::smoke_t()
{
    OpenGL::render_begin();
    setup_shader(&motion_program, motion_source);
    setup_shader(&diffuse1_program, diffuse1_source);
    setup_shader(&diffuse2_program, diffuse2_source);
    setup_shader(&project1_program, project1_source);
    setup_shader(&project2_program, project2_source);
    setup_shader(&project3_program, project3_source);
    setup_shader(&project4_program, project4_source);
    setup_shader(&project5_program, project5_source);
    setup_shader(&project6_program, project6_source);
    setup_shader(&advect1_program, advect1_source);
    setup_shader(&advect2_program, advect2_source);
    setup_shader(&render_program, render_source);
    OpenGL::render_end();
}

smoke_t::~smoke_t()
{
    GL_CALL(glDeleteProgram(motion_program));
    GL_CALL(glDeleteProgram(diffuse1_program));
    GL_CALL(glDeleteProgram(diffuse2_program));
    GL_CALL(glDeleteProgram(project1_program));
    GL_CALL(glDeleteProgram(project2_program));
    GL_CALL(glDeleteProgram(project3_program));
    GL_CALL(glDeleteProgram(project4_program));
    GL_CALL(glDeleteProgram(project5_program));
    GL_CALL(glDeleteProgram(project6_program));
    GL_CALL(glDeleteProgram(advect1_program));
    GL_CALL(glDeleteProgram(advect2_program));
    GL_CALL(glDeleteProgram(render_program));
    destroy_textures();
}

void smoke_t::create_textures()
{
    GL_CALL(glGenTextures(1, &texture));
    GL_CALL(glGenTextures(1, &b0u));
    GL_CALL(glGenTextures(1, &b0v));
    GL_CALL(glGenTextures(1, &b0d));
    GL_CALL(glGenTextures(1, &b1u));
    GL_CALL(glGenTextures(1, &b1v));
    GL_CALL(glGenTextures(1, &b1d));
}

void smoke_t::destroy_textures()
{
    GL_CALL(glDeleteTextures(1, &texture));
    GL_CALL(glDeleteTextures(1, &b0u));
    GL_CALL(glDeleteTextures(1, &b0v));
    GL_CALL(glDeleteTextures(1, &b0d));
    GL_CALL(glDeleteTextures(1, &b1u));
    GL_CALL(glDeleteTextures(1, &b1v));
    GL_CALL(glDeleteTextures(1, &b1d));
}

void smoke_t::run_shader(GLuint program, int width, int height, int title_height, int border_size)
{
    GL_CALL(glUseProgram(program));
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
    GL_CALL(glUniform1i(1, title_height + border_size));
    GL_CALL(glUniform1i(2, border_size * 2));
    GL_CALL(glUniform1i(5, width));
    GL_CALL(glUniform1i(6, height));
    GL_CALL(glDispatchCompute(width / 15, height / 15, 1));
    GL_CALL(glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT));
}

/**
 * Fill the given rectangle with the background color(s).
 *
 * @param fb The target framebuffer, must have been bound already
 * @param rectangle The rectangle to redraw.
 * @param scissor The GL scissor rectangle to use.
 * @param active Whether to use active or inactive colors
 */
void smoke_t::render_effect(const wf::render_target_t& fb, wf::geometry_t rectangle,
    const wf::geometry_t& scissor, bool ink, wf::pointf_t p,
    wf::color_t decor_color, wf::color_t effect_color,
    int title_height, int border_size)
{
    if ((rectangle.width <= 0) || (rectangle.height <= 0))
    {
        return;
    }

    OpenGL::render_begin(fb);
    if ((rectangle.width != saved_width) || (rectangle.height != saved_height))
    {
        LOGI("recreating smoke textures: ", rectangle.width, " != ", saved_width, " || ", rectangle.height,
            " != ", saved_height);
        saved_width  = rectangle.width;
        saved_height = rectangle.height;

        destroy_textures();
        create_textures();

        std::vector<GLfloat> clear_data(rectangle.width * rectangle.height * 4, 0);

        GL_CALL(glActiveTexture(GL_TEXTURE0 + 0));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RGBA, GL_FLOAT,
            &clear_data[0]);
        GL_CALL(glActiveTexture(GL_TEXTURE0 + 1));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, b0u));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RED, GL_FLOAT,
            &clear_data[0]);
        GL_CALL(glActiveTexture(GL_TEXTURE0 + 2));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, b0v));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RED, GL_FLOAT,
            &clear_data[0]);
        GL_CALL(glActiveTexture(GL_TEXTURE0 + 3));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, b0d));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RED, GL_FLOAT,
            &clear_data[0]);
        GL_CALL(glActiveTexture(GL_TEXTURE0 + 4));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, b1u));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RED, GL_FLOAT,
            &clear_data[0]);
        GL_CALL(glActiveTexture(GL_TEXTURE0 + 5));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, b1v));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RED, GL_FLOAT,
            &clear_data[0]);
        GL_CALL(glActiveTexture(GL_TEXTURE0 + 6));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, b1d));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, rectangle.width, rectangle.height));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rectangle.width, rectangle.height, GL_RED, GL_FLOAT,
            &clear_data[0]);
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
    wf::point_t point{int(p.x), int(p.y)};
    // upload stuff
    GL_CALL(glUniform1i(1, title_height + border_size));
    GL_CALL(glUniform1i(2, border_size));
    GL_CALL(glUniform1i(3, point.x));
    GL_CALL(glUniform1i(4, point.y));
    GL_CALL(glUniform1i(5, rectangle.width));
    GL_CALL(glUniform1i(6, rectangle.height));
    GL_CALL(glUniform1i(7, wf::get_current_time()));
    GL_CALL(glDispatchCompute(rectangle.width / 15, rectangle.height / 15, 1));
    GL_CALL(glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT));
    for (int k = 0; k < 2; k++)
    {
        run_shader(diffuse1_program, rectangle.width, rectangle.height, title_height, border_size);
    }

    run_shader(project1_program, rectangle.width, rectangle.height, title_height, border_size);
    for (int k = 0; k < 2; k++)
    {
        run_shader(project2_program, rectangle.width, rectangle.height, title_height, border_size);
    }

    run_shader(project3_program, rectangle.width, rectangle.height, title_height, border_size);
    run_shader(advect1_program, rectangle.width, rectangle.height, title_height, border_size);
    run_shader(project4_program, rectangle.width, rectangle.height, title_height, border_size);
    for (int k = 0; k < 2; k++)
    {
        run_shader(project5_program, rectangle.width, rectangle.height, title_height, border_size);
    }

    run_shader(project6_program, rectangle.width, rectangle.height, title_height, border_size);
    for (int k = 0; k < 2; k++)
    {
        run_shader(diffuse2_program, rectangle.width, rectangle.height, title_height, border_size);
    }

    run_shader(advect2_program, rectangle.width, rectangle.height, title_height, border_size);
    GL_CALL(glUseProgram(render_program));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));
    GL_CALL(glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 3));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0d));
    GL_CALL(glBindImageTexture(3, b0d, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
    GLfloat effect_color_f[4] =
    {GLfloat(effect_color.r), GLfloat(effect_color.g), GLfloat(effect_color.b),
        GLfloat(effect_color.a)};
    GLfloat decor_color_f[4] =
    {GLfloat(decor_color.r), GLfloat(decor_color.g), GLfloat(decor_color.b), GLfloat(decor_color.a)};
    GL_CALL(glUniform1i(1, title_height + border_size));
    GL_CALL(glUniform1i(2, border_size * 2));
    GL_CALL(glUniform1i(4, ink));
    GL_CALL(glUniform1i(5, rectangle.width));
    GL_CALL(glUniform1i(6, rectangle.height));
    GL_CALL(glUniform4fv(7, 1, effect_color_f));
    GL_CALL(glUniform4fv(8, 1, decor_color_f));
    GL_CALL(glDispatchCompute(rectangle.width / 15, rectangle.height / 15, 1));
    GL_CALL(glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT));
    fb.logic_scissor(scissor);
    OpenGL::render_transformed_texture(wf::texture_t{texture}, rectangle,
        fb.get_orthographic_projection(), glm::vec4{1}, OpenGL::TEXTURE_TRANSFORM_INVERT_Y);
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
}
}
