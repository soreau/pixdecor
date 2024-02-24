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
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 3) uniform int px;
layout(location = 4) uniform int py;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int rand1;
layout(location = 8) uniform int rand2;
layout(location = 9) uniform int radius;

void motion(int x, int y)
{
	int i, i0, i1, j, j0, j1, d = 2;

	if (x - d < 1)
		i0 = 1;
	else
		i0 = x - d;
	if (i0 + 2 * d > width - 1)
		i1 = width - 1;
	else
		i1 = i0 + 2 * d;

	if (y - d < 1)
		j0 = 1;
	else
		j0 = y - d;
	if (j0 + 2 * d > height - 1)
		j1 = height - 1;
	else
		j1 = j0 + 2 * d;

	for (i = i0; i < i1; i++)
	{
		for (j = j0; j < j1; j++) {
			if (i < radius || j < radius || i > (width - 1) - radius || j > (height - 1) - radius || (i > border_size && i < (width - 1) - border_size && j > (title_height - 1) && j < (height - 1) - border_size))
			{
				continue;
			}
			vec4 b0u = imageLoad(in_b0u, ivec2(i, j));
			vec4 b0v = imageLoad(in_b0v, ivec2(i, j));
			vec4 b0d = imageLoad(in_b0d, ivec2(i, j));
			float u = b0u.x;
			float v = b0v.x;
			float d = b0d.x;
			imageStore(out_b0u, ivec2(i, j), vec4(u + float(256 - (rand1 & 512)), 0.0, 0.0, 0.0));
			imageStore(out_b0v, ivec2(i, j), vec4(v + float(256 - (rand2 & 512)), 0.0, 0.0, 0.0));
			imageStore(out_b0d, ivec2(i, j), vec4(d + 1.0, 0.0, 0.0, 0.0));
		}
	}
}

void main()
{
    motion(px, py);
}
)";

// Generic smoke shader beginning
static const char *smoke_header =
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

layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 10) uniform int regionInfo[20];
)";

// Generic main method for a compute shader which runs for a particular region
static const char *effect_run_for_region_main =
    R"(
void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    int z = int(gl_GlobalInvocationID.z);

    ivec4 myRegion = ivec4(regionInfo[z * 5], regionInfo[z * 5 + 1], regionInfo[z * 5 + 2], regionInfo[z * 5 + 3]);
    if (regionInfo[z * 5 + 4] > 0)
    {
        pos = pos.yx;
    }

    if (all(lessThan(pos, myRegion.zw)))
    {
        pos += myRegion.xy;
        run_pixel(pos.x, pos.y);
    }
})";

static const char *diffuse1_source =
    R"(
void run_pixel(int x, int y)
{
	int k;
	float t, a = 0.0002;

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
)";

static const char *project1_source =
    R"(
void run_pixel(int x, int y) {
	int k, l, s;
	float h;

	h = 1.0 / float(width);
	s = width;

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
)";

static const char *project2_source =
    R"(
void run_pixel(int x, int y)
{
	int k, l, s;
	float h;

	h = 1.0 / float(width);
	s = width;

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
})";

static const char *project3_source =
    R"(
void run_pixel(int x, int y)
{
	int k, l, s;
	float h;

	h = 1.0 / float(width);
	s = width;

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
})";

static const char *advect1_source =
    R"(
void run_pixel(int x, int y) /* b1.u, b1.v, b1.u, b0.u */
{
	int stride;
	int i, j;
	float fx, fy;
	stride = width;

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
})";

static const char *project4_source =
    R"(
void run_pixel(int x, int y)
{
	int k, l, s;
	float h;

	h = 1.0 / float(width);
	s = width;

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
})";

static const char *project5_source =
    R"(
void run_pixel(int x, int y)
{
	int k, l, s;
	float h;

	h = 1.0 / float(width);
	s = width;

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
})";

static const char *project6_source =
    R"(
void run_pixel(int x, int y)
{
	int k, l, s;
	float h;

	h = 1.0 / float(width);
	s = width;

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
})";

static const char *diffuse2_source =
    R"(
void run_pixel(int x, int y)
{
	int k, stride;
	float t, a = 0.0002;
	stride = width;

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
})";

static const char *advect2_source =
    R"(
void run_pixel(int x, int y) /*b0u, b0v, b1d, b0d*/
{
	int stride;
	int i, j;
	float fx, fy;
	stride = width;

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
})";

static const char *render_source =
    R"(
#version 320 es

precision lowp image2D;

layout (binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout (binding = 3, r32f) uniform readonly image2D in_b0d;
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 4) uniform bool ink;
layout(location = 8) uniform vec4 smoke_color;
layout(location = 9) uniform vec4 decor_color;
layout(location = 10) uniform int regionInfo[20];

void run_pixel(int x, int y)
{
	float c, r, g, b, a;

	vec4 s = imageLoad(in_b0d, ivec2(x, y));
	c = s.x * 800.0;
	if (c > 255.0)
		c = 255.0;
	a = c;
	vec3 color;
	if (ink)
	{
		if (c > 2.0)
			color = mix(decor_color.rgb, smoke_color.rgb, clamp(a, 0.0, 1.0));
		else
			color = mix(decor_color.rgb, vec3(0.0, 0.0, 0.0), clamp(a, 0.0, 1.0));
		if (c > 1.5)
			imageStore(out_tex, ivec2(x, y), vec4(color, 1.0));
		else
			imageStore(out_tex, ivec2(x, y), decor_color);
		return;
	} else
	{
		color = mix(decor_color.rgb, smoke_color.rgb, clamp(a, 0.0, 1.0));
	}
	imageStore(out_tex, ivec2(x, y), vec4(color, decor_color.a));
})";
