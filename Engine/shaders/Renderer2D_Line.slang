// Thick lines and poly lines. Each segment is a quad around both ends, widened in screen space by the
// vertex shader. Where two segments meet, the pixels both quads cover are split along the bisector of the
// corner, so each pixel is drawn once and the outside of the corner is rounded. Ends with no neighbour get
// round caps. The texture runs along the line and its height fits the width.
import Camera;
import TextureSlots;

// Must match LineFlags in Renderer2D.cpp
static const int WIDTH_IN_PIXELS = 1; // otherwise in world units
static const int HAS_PREVIOUS = 2;
static const int HAS_NEXT = 4;

struct VertexInput
{
	[[vk::location(0)]] float3 start;
	[[vk::location(1)]] float3 end;
	[[vk::location(2)]] float3 previous; // start of the segment before, with HAS_PREVIOUS
	[[vk::location(3)]] float3 next; // end of the segment after, with HAS_NEXT
	[[vk::location(4)]] float2 corner; // x: -1 at the start, +1 at the end. y: -1 or +1 across the line
	[[vk::location(5)]] float4 colour;
	[[vk::location(6)]] float width;
	[[vk::location(7)]] int flags;
	[[vk::location(8)]] float texIndex;
	[[vk::location(9)]] float textureStart; // texture u at the start
	[[vk::location(10)]] float texturePerWorld; // texture u per world unit along the line
	[[vk::location(11)]] int entityId;
};

// A segment in pixels from the centre of the viewport
struct Segment
{
	float2 start;
	float2 along;
	float length;
	float halfWidth;
};

struct VertexOutput
{
	float4 position : SV_Position;
	noperspective float2 pixel;
	float4 colour;
	// Segments packed as (start, along) and (length, halfWidth)
	nointerpolation float4 segment;
	nointerpolation float2 segmentSize;
	nointerpolation float4 previous;
	nointerpolation float2 previousSize;
	nointerpolation float4 next;
	nointerpolation float2 nextSize;
	nointerpolation float3 texture; // slot, u at the start, u per pixel
	nointerpolation int flags;
	nointerpolation int entityId;
};

struct FragmentOutput
{
	float4 colour : SV_Target0;
	int entityId : SV_Target1;
};

float2 ToPixels(float3 world, float2 halfViewport)
{
	float4 clip = mul(u_Camera.viewProjection, float4(world, 1.0));
	return clip.xy / clip.w * halfViewport;
}

Segment MakeSegment(float3 start, float3 end, float width, bool widthInPixels, float2 halfViewport)
{
	Segment segment;
	segment.start = ToPixels(start, halfViewport);
	float2 delta = ToPixels(end, halfViewport) - segment.start;
	segment.length = length(delta);
	segment.along = segment.length > 0.0001 ? delta / segment.length : float2(1.0, 0.0);
	segment.halfWidth = width * 0.5;
	if (!widthInPixels)
	{
		float2 worldAlong = end.xy - start.xy;
		float3 worldAcross = dot(worldAlong, worldAlong) > 0.0 ? float3(normalize(float2(-worldAlong.y, worldAlong.x)), 0.0) : float3(0.0, 1.0, 0.0);
		segment.halfWidth = length(ToPixels(start + worldAcross * segment.halfWidth, halfViewport) - segment.start);
	}
	return segment;
}

Segment Unpack(float4 packed, float2 size)
{
	Segment segment;
	segment.start = packed.xy;
	segment.along = packed.zw;
	segment.length = size.x;
	segment.halfWidth = size.y;
	return segment;
}

// Pixels along and across the segment from its start
float2 Local(Segment segment, float2 pixel)
{
	float2 offset = pixel - segment.start;
	return float2(dot(offset, segment.along), dot(offset, float2(-segment.along.y, segment.along.x)));
}

// Whether the pixel is inside the segment's quad
bool Covers(Segment segment, float2 pixel)
{
	float2 local = Local(segment, pixel);
	float extent = segment.halfWidth + 1.0;
	return local.x >= -extent && local.x <= segment.length + extent && abs(local.y) <= extent;
}

// Positive on the second segment's side of the corner between them, zero or negative on the first's
float JoinSide(Segment first, Segment second, float2 pixel)
{
	float2 normal = first.along + second.along;
	if (dot(normal, normal) < 0.000001)
		normal = float2(-first.along.y, first.along.x);
	return dot(pixel - second.start, normal);
}

[shader("vertex")]
VertexOutput vs_main(VertexInput input)
{
	float2 halfViewport = max(u_Camera.viewportSize, float2(1.0)) * 0.5;
	bool widthInPixels = (input.flags & WIDTH_IN_PIXELS) != 0;

	Segment segment = MakeSegment(input.start, input.end, input.width, widthInPixels, halfViewport);
	Segment previous = segment;
	Segment next = segment;
	if ((input.flags & HAS_PREVIOUS) != 0)
		previous = MakeSegment(input.previous, input.start, input.width, widthInPixels, halfViewport);
	if ((input.flags & HAS_NEXT) != 0)
		next = MakeSegment(input.end, input.next, input.width, widthInPixels, halfViewport);

	// One extra pixel around the line for the antialiased edge
	float extent = segment.halfWidth + 1.0;
	bool atEnd = input.corner.x > 0.0;
	float2 across = float2(-segment.along.y, segment.along.x);
	float2 pixel = segment.start + segment.along * (atEnd ? segment.length : 0.0)
		+ segment.along * input.corner.x * extent + across * input.corner.y * extent;
	float4 clip = mul(u_Camera.viewProjection, float4(atEnd ? input.end : input.start, 1.0));

	float worldLength = length(input.end - input.start);

	VertexOutput output;
	output.position = float4(pixel / halfViewport * clip.w, clip.z, clip.w);
	output.pixel = pixel;
	output.colour = input.colour;
	output.segment = float4(segment.start, segment.along);
	output.segmentSize = float2(segment.length, segment.halfWidth);
	output.previous = float4(previous.start, previous.along);
	output.previousSize = float2(previous.length, previous.halfWidth);
	output.next = float4(next.start, next.along);
	output.nextSize = float2(next.length, next.halfWidth);
	output.texture = float3(input.texIndex, input.textureStart, segment.length > 0.0001 ? input.texturePerWorld * worldLength / segment.length : 0.0);
	output.flags = input.flags;
	output.entityId = input.entityId;
	return output;
}

[shader("fragment")]
FragmentOutput fs_main(VertexOutput input)
{
	Segment segment = Unpack(input.segment, input.segmentSize);
	float2 local = Local(segment, input.pixel);
	float distance = length(local - float2(clamp(local.x, 0.0, segment.length), 0.0));
	float alpha = clamp(segment.halfWidth + 0.5 - distance, 0.0, 1.0);

	// Pixels both quads cover at a corner are drawn by the segment on their side of it
	if ((input.flags & HAS_PREVIOUS) != 0)
	{
		Segment previous = Unpack(input.previous, input.previousSize);
		if (JoinSide(previous, segment, input.pixel) <= 0.0 && Covers(previous, input.pixel))
			alpha = 0.0;
	}
	if ((input.flags & HAS_NEXT) != 0)
	{
		Segment next = Unpack(input.next, input.nextSize);
		if (JoinSide(segment, next, input.pixel) > 0.0 && Covers(next, input.pixel))
			alpha = 0.0;
	}

	// Half a texel in from the top and bottom, as the sampler repeats
	int slot = int(input.texture.x);
	float halfTexel = 0.5 / max(SlotSize(slot).y, 1.0);
	float2 uv = float2(input.texture.y + local.x * input.texture.z,
		clamp(0.5 - local.y / (2.0 * max(segment.halfWidth, 0.0001)), halfTexel, 1.0 - halfTexel));
	float4 colour = SampleSlot(slot, uv) * input.colour;

	if (colour.a * alpha <= 0.0001)
		discard;

	FragmentOutput output;
	output.colour = float4(colour.rgb, colour.a * alpha);
	output.entityId = input.entityId;
	return output;
}
