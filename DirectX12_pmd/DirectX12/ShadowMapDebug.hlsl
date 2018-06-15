cbuffer cbTansMatrix : register(b0){
	float4x4 WVP;
};

Texture2D<float4> tex0 : register(t0);
SamplerState samp0 : register(s0);
SamplerState samp1 : register(s1);

struct VS_INPUT{
	float3 Position : POSITION;
	float3 Normal	: NORMAL;
	float2 UV		: TEXCOORD;
};


struct PS_INPUT{//(VS_OUTPUT)
	float4 Position : SV_POSITION;
	float2 UV		: TEXCOORD;
};


PS_INPUT VSMain(VS_INPUT input){
	PS_INPUT output;

	float4 Pos = float4(input.Position, 1.0f);
	output.Position = mul(Pos, WVP);
	output.UV.x = input.UV.x;
	output.UV.y = input.UV.y;

	return output;
}


float4 PSMain(PS_INPUT input) : SV_TARGET{
    float4 color = tex0.Sample(samp0, input.UV);
    return color;
}



