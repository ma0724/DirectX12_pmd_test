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

// ボーン表示用構造体
struct Bones {
	DirectX::XMFLOAT3 headPos;
	DirectX::XMFLOAT3 tailPos;
};

ASDX_ALIGN(256)
struct pmdVertex
{
	DirectX::XMFLOAT3 pos;		 // 頂点
	DirectX::XMFLOAT3 normal;	 // 法線
	DirectX::XMFLOAT2 uv;		 // UV情報
	WORD bone_num[2];	 // ボーン番号
	BYTE bone_weight;	 // 重み
	BYTE edge_flag;
};

ASDX_ALIGN(256)
struct usePMDMaterial {
	DirectX::XMFLOAT3 diffuse;			//色情報
	float alpha;						//アルファ情報
	float specularity;
	DirectX::XMFLOAT3 specular;			//スペキュラ情報
	DirectX::XMFLOAT3 ambient;			//アンビエント情報(mirror_color)
	unsigned int offset;
	unsigned int count;
	Microsoft::WRL::ComPtr<ID3D12Resource> texture;
};


struct pmdMaterial {
	DirectX::XMFLOAT3 diffuse_color;		// dr, dg, db // 減衰色
	float alpha;							// 減衰色の不透明度
	float specularity;
	DirectX::XMFLOAT3 specular_color;		// sr, sg, sb // 光沢色
	DirectX::XMFLOAT3 mirror_color;			// mr, mg, mb // 環境色(ambient)
	BYTE toon_index;						// toon??.bmp // 0.bmp:0xFF, 1(01).bmp:0x00 10.bmp:0x09
	BYTE edge_flag;							// 輪郭、影
	DWORD face_vert_count;					// 面頂点数
	char texture_file_name[20];				// テクスチャファイルorスフィアファイル名
};

ASDX_ALIGN(256)
// ボーン情報
struct BoneInformation {
	char bone_name[20];			// ボーンの名前
	WORD parent_bone_index;		// 親ボーン番号(ない場合は0xFFFF)
	WORD tail_pos_bone_index;	// tail位置のボーン番号
	BYTE bone_type;				// ボーンの種類
	WORD ik_parent_bone_index;	// IKボーン番号(ない場合は0)
	DirectX::XMFLOAT3 bone_head_pos;// ボーンのヘッドの位置
};

// ボーンツリー構造体
struct BoneNode {
	std::vector<int> childIdx;//インデックス
							  //std::vector<BoneNode> child;
};

// モーション情報
struct MotionData {
	//std::string boneName;
	unsigned int flameNo;
	DirectX::XMVECTOR quaternion;
	//DirectX::XMFLOAT3 location;
};

// IKリスト
struct IKList {
	unsigned short boneIdx;
	unsigned short tboneIdx;
	unsigned char chainLen;
	unsigned short iterationNum;
	float restriction;
	std::vector<unsigned short> boneIndices;
};

// ポリゴン用頂点
struct Vertex3D {
	DirectX::XMFLOAT3 Position;	//位置
	DirectX::XMFLOAT3 Normal;	//法線
	DirectX::XMFLOAT2 UV;		//UV座標
	DirectX::XMFLOAT3 Tangent;	// 接線
	DirectX::XMFLOAT3 Binormal; // 従法線
};