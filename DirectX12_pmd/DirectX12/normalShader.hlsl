cbuffer cbTansMatrix : register(b0) {
	float4x4 WVP;
};
cbuffer cbTansMatrixWorld : register(b1) {
	float4x4 World;
};
cbuffer cbTansMatrixVIEWWORLD : register(b2) {
	float4x4 Camera;
};
// �e�N�X�`��
Texture2D<float4> tex0 : register(t0);
SamplerState sam0 : register(s0);
// ���p
Texture2D<float4> shadow_map : register(t1);
SamplerState samp1 : register(s1);

// �}�e���A���J���[
cbuffer cbMatColor : register(b3) {
	float3 Color;
}
cbuffer cbLight : register(b4) {
	float4x4	LightVP;		// View �~ Projection
	float4		LightColor;		// LightColor
	float3		LightDir;		// LightDirection
};

cbuffer cbuffboneMat : register(b5)
{
	matrix boneMats[256];
}

//MRT�p�\����

struct OutPutColor
{
    float4 rt0 : SV_TARGET0;
    float4 rt1 : SV_TARGET1;
};

struct VS_INPUT {
	float4 Position : POSITION;
	float4 Normal	: NORMAL;
	float2 UV	: TEXCODE;
	uint boneid : BONENO;
	uint weight : WEIGHT;
};

struct VS_INPUT_BUMP{
    float4 Position : POSITION;
    float4 Normal : NORMAL;
    float2 UV : TEXCODE;
    float4 Tanget : TANGENT;
    float4 Binormal : BINORMAL;
};

struct PS_INPUT
{ //(VS_OUTPUT)
    float4 Position : SV_POSITION;
    float4 Normal : NORMAL;
    float4 PosSM : POSITION_SM;
    float2 UV : TEXCODE;
};

struct PS_INPUT_BUMP
{ //(VS_OUTPUT)
    float4 Position : SV_POSITION;
    float4 Normal : NORMAL;
    float4 PosSM : POSITION_SM;
    float2 UV : TEXCODE;
    float4 lightTangentDirect : TEXCOORD3;
};

// �ڋ�ԍs��̋t�s����Z�o
float4x4 InvTangentMatrix(
	float3 tangent,
	float3 binormal,
	float3 normal)
{
    float4x4 mat = { float4(tangent, 0.0f), float4(binormal, 0.0f), float4(normal, 0.0f), { 0.0f, 0.0f, 0.0f, 1.0f } };
    return transpose(mat); // �w�肳�ꂽ���͍s���]�u���܂�
}

//���p���_�V�F�[�_
PS_INPUT_BUMP VSFloorShader(VS_INPUT_BUMP input)
{
    PS_INPUT_BUMP output;

	float4 Pos = input.Position;
	float4 Pos2 = input.Position;
	float4 Nrm = input.Normal;

    float3 localNormal = input.Normal.xyz;
    float3 normalizeTangent = input.Tanget.xyz;
    float3 normalizeBinormal = input.Binormal.xyz;

	matrix wvp = mul(World, Camera); 
    // ���W 
	output.Position = mul(wvp, Pos);
    // �@��
	output.Normal = mul(Nrm, World);
    // UV�l
	output.UV = input.UV;
    // �ڋ�Ԃւ����Ă������߂̋t�s����Z�o
    float4x4 inverseTangentMat = InvTangentMatrix(normalizeTangent, normalizeBinormal, localNormal);
    // ��ŋ��߂��t�s���p���ă��C�g��ڋ�Ԃւ����Ă���
    float4 light = float4(-3.0f, 1.0f, 1.0f, 0.0f);
    output.lightTangentDirect = mul(light, inverseTangentMat);

	// �e�̏���
	matrix wlp = (World, LightVP);
	Pos2 = mul(wlp, Pos2);
	Pos2.z = Pos2.z / Pos2.w;
	output.PosSM.x = (1.0f + Pos2.x / Pos2.ww) / 2.0f;
    output.PosSM.y = (1.0f - Pos2.y / Pos2.ww) / 2.0f;
	output.PosSM.z = Pos2.z;

	return output;
}

// ���f���p���_�V�F�[�_
PS_INPUT VSMain(VS_INPUT input) {
	PS_INPUT output;

	float wgt1 = float(input.weight) / 100.0f;
	float wgt2 = 1.0 - wgt1;

	float4 Pos = input.Position;
	float4 Pos2 = input.Position;
	float4 Nrm = input.Normal;

	matrix wvp = mul(World, Camera); // world�ƃJ��������Z
	matrix boneMatrix = boneMats[input.boneid & 0xffff] * wgt1 + boneMats[(input.boneid & 0xffff0000) >> 16] * wgt2;
	wvp = mul(wvp, boneMatrix);

	output.Position = mul(wvp, Pos);
	output.Normal = mul(Nrm, World);
	output.Normal = mul(Nrm, boneMatrix);
	output.Normal.w = 0;
	output.UV = input.UV;

	// �e�̏���

	matrix wlp = (World, LightVP);
	wlp = mul(wlp, boneMatrix);
	Pos2 = mul(wlp, Pos2);
    Pos2.z = Pos2.z / Pos2.w;
    output.PosSM.x = (1.0f + Pos2.x /Pos2.ww) / 2.0f;
    output.PosSM.y = (1.0f - Pos2.y / Pos2.ww) / 2.0f;
	output.PosSM.z = Pos2.z;

	return output;
}

// ���_2
// ���f���p���_�V�F�[�_
PS_INPUT VSMain2(VS_INPUT input)
{
    PS_INPUT output;

    float wgt1 = float(input.weight) / 100.0f;
    float wgt2 = 1.0 - wgt1;

    float4 Pos = input.Position;
    float4 Pos2 = input.Position;
    float4 Nrm = input.Normal;

    matrix wvp = mul(World, LightVP); // world�ƃJ��������Z
    matrix boneMatrix = boneMats[input.boneid & 0xffff] * wgt1 + boneMats[(input.boneid & 0xffff0000) >> 16] * wgt2;
    wvp = mul(wvp, boneMatrix);

    output.Position = mul(wvp, Pos);
    output.Normal = mul(Nrm, World);
    output.Normal = mul(Nrm, boneMatrix);
    output.Normal.w = 0;
    output.UV = input.UV;

	// �e�̏���

    matrix wlp = (World, LightVP);
    wlp = mul(wlp, boneMatrix);
    Pos2 = mul(wlp, Pos2);
    Pos2.z = Pos2.z / Pos2.w;
    output.PosSM.x = (1.0f + Pos2.x / Pos2.ww) / 2.0f;
    output.PosSM.y = (1.0f - Pos2.y / Pos2.ww) / 2.0f;
    output.PosSM.z = Pos2.z;

    return output;
}


// ���f���p�s�N�Z���V�F�[�_
float4 PSMain(PS_INPUT input) : SV_TARGET{
	// �����œ��όv�Z = ���邳
	// return ���邳*�J���[*�e�N�X�`��
	float light = float3(-1.0f, -1.0f, -1.0f);
	float3 lightInit = normalize(light);
	float3 normalInit = normalize(input.Normal);
	float bright = dot(lightInit, normalInit);

	// �e�̏���
    float sm = shadow_map.Sample(sam0, input.PosSM.xy);
	float sma = (input.PosSM.z - 0.003f < sm) ? 1.0f : 0.5f;

	// ������UV�l��F�Ƃ��ďo���Č���
	//return float4(0.5, input.UV, 1);

    return bright * float4(Color, input.Position.a) * tex0.Sample(sam0, input.UV) * sma;
}

// ���p�s�N�Z���V�F�[�_
float4 PSNormalMain(PS_INPUT_BUMP input) : SV_TARGET
{
    // �m�[�}���}�b�v����l(�F)�𓾂�
    // �m�[�}���}�b�v�̐F��0�`1.0f�ł�
    float3 normalColor = tex0.Sample(sam0, input.UV);
    float3 normalVec = 2.0f * normalColor - 1.0f;
    normalVec = normalize(normalVec);

    float3 lightTangentDirect = input.lightTangentDirect.xyz;
    // �ڋ�ԓ��m�̌v�Z
    // �Ƃ����߂����א��K����Y��Ȃ�
    float3 bright = dot(normalize(lightTangentDirect), normalVec);
   
    bright = max(0.0f, bright);

	// �e�̏���
    float sm = shadow_map.Sample(samp1, input.PosSM.xy);
    float sma = (input.PosSM.z - 0.0005f < sm) ? 1.0f : 0.5f;

	// �e�N�X�`���\��t��
    float4 color;
    // albed
    float4 albed = float4(1.0f, 1.0f, 0.0f, 1.0f);

    return color = float4(bright, 1.0f) * sma * albed;
}

//�V���h�[�}�b�v�v�Z�p���_�V�F�[�_
PS_INPUT VSShadowMap(VS_INPUT input) {
	PS_INPUT output;
	float wgt1 = float(input.weight) / 100.0f;
	float wgt2 = 1.0 - wgt1;

	float4 Pos = input.Position;

	matrix wvp = mul(World, LightVP);
	matrix boneMatrix = boneMats[input.boneid & 0xffff] * wgt1 + boneMats[(input.boneid & 0xffff0000) >> 16] * wgt2;
	wvp = mul(wvp, boneMatrix);

	output.Position = mul(wvp, Pos);
	output.Normal = 0;
	output.PosSM = 0;
	output.UV = 0;

	return output;
}