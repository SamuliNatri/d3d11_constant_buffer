cbuffer constants : register(b0) {
	row_major float4x4 model;
};

struct VS_Input {
	float3 position: POSITION;
	float2 uv: UV;
};

struct VS_Output {
	float4 position: SV_POSITION;
	float2 uv: UV;
};

/*
static matrix model = {
	{1,0,0,0},
	{0,1,0,0},
	{0,0,1,0},
	{1,0,0,1}
};
*/

VS_Output vs_main(VS_Input input) {
	VS_Output output;
	output.position = float4(input.position, 1.0f);
	output.position = mul(output.position, model);
	output.uv = input.uv;
	return output;
};

Texture2D my_texture;
SamplerState my_sampler;

float4 ps_main(VS_Output input): SV_TARGET {
	return my_texture.Sample(my_sampler, input.uv);
};