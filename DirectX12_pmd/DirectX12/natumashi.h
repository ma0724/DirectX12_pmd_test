#pragma once
#include <vector>
#include <DirectXMath.h>
#include <wrl/client.h>

#ifndef ASDX_ALIGN
#if _MSC_VER
#define ASDX_ALIGN( alignment )    __declspec( align(alignment) )
#else
#define ASDX_ALIGN( alignment )    __attribute__( aligned(alignment) )
#endif
#endif//ASDX_ALIGN

// �{�[���\���p�\����
struct Bones {
	DirectX::XMFLOAT3 headPos;
	DirectX::XMFLOAT3 tailPos;
};

ASDX_ALIGN(256)
struct pmdVertex
{
	DirectX::XMFLOAT3 pos;		 // ���_
	DirectX::XMFLOAT3 normal;	 // �@��
	DirectX::XMFLOAT2 uv;		 // UV���
	WORD bone_num[2];	 // �{�[���ԍ�
	BYTE bone_weight;	 // �d��
	BYTE edge_flag;
};

ASDX_ALIGN(256)
struct usePMDMaterial {
	DirectX::XMFLOAT3 diffuse;			//�F���
	float alpha;						//�A���t�@���
	float specularity;
	DirectX::XMFLOAT3 specular;			//�X�y�L�������
	DirectX::XMFLOAT3 ambient;			//�A���r�G���g���(mirror_color)
	unsigned int offset;
	unsigned int count;
	Microsoft::WRL::ComPtr<ID3D12Resource> texture;
};


struct pmdMaterial {
	DirectX::XMFLOAT3 diffuse_color;		// dr, dg, db // �����F
	float alpha;							// �����F�̕s�����x
	float specularity;
	DirectX::XMFLOAT3 specular_color;		// sr, sg, sb // ����F
	DirectX::XMFLOAT3 mirror_color;			// mr, mg, mb // ���F(ambient)
	BYTE toon_index;						// toon??.bmp // 0.bmp:0xFF, 1(01).bmp:0x00 10.bmp:0x09
	BYTE edge_flag;							// �֊s�A�e
	DWORD face_vert_count;					// �ʒ��_��
	char texture_file_name[20];				// �e�N�X�`���t�@�C��or�X�t�B�A�t�@�C����
};

ASDX_ALIGN(256)
// �{�[�����
struct BoneInformation {
	char bone_name[20];			// �{�[���̖��O
	WORD parent_bone_index;		// �e�{�[���ԍ�(�Ȃ��ꍇ��0xFFFF)
	WORD tail_pos_bone_index;	// tail�ʒu�̃{�[���ԍ�
	BYTE bone_type;				// �{�[���̎��
	WORD ik_parent_bone_index;	// IK�{�[���ԍ�(�Ȃ��ꍇ��0)
	DirectX::XMFLOAT3 bone_head_pos;// �{�[���̃w�b�h�̈ʒu
};

// �{�[���c���[�\����
struct BoneNode {
	std::vector<int> childIdx;//�C���f�b�N�X
							  //std::vector<BoneNode> child;
};

// ���[�V�������
struct MotionData {
	//std::string boneName;
	unsigned int flameNo;
	DirectX::XMVECTOR quaternion;
	//DirectX::XMFLOAT3 location;
};

// IK���X�g
struct IKList {
	unsigned short boneIdx;
	unsigned short tboneIdx;
	unsigned char chainLen;
	unsigned short iterationNum;
	float restriction;
	std::vector<unsigned short> boneIndices;
};

// �|���S���p���_
struct Vertex3D {
	DirectX::XMFLOAT3 Position;	//�ʒu
	DirectX::XMFLOAT3 Normal;	//�@��
	DirectX::XMFLOAT2 UV;		//UV���W
	DirectX::XMFLOAT3 Tangent;	// �ڐ�
	DirectX::XMFLOAT3 Binormal; // �]�@��
};