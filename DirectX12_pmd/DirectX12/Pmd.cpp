#include "Pmd.h"
#include <fstream>
#include "D3D12Manager.h"
#include "WICTextureLoader12.h"
#include <tchar.h>
#include "d3dx12.h"
#include <algorithm>
#include <DirectXMath.h>
#include "pmdMesh.h"

#define DISABLE_C4996   __pragma(warning(push)) __pragma(warning(disable:4996))
#define ENABLE_C4996    __pragma(warning(pop))

constexpr float PI = 3.1495;

using namespace DirectX;
using namespace Microsoft::WRL;

Pmd::Pmd()
	: _vertexBuffer{},
	_indexBuffer{},
	_constantBuffer{},
	_constantBufferWorld{},
	_constantBufferViewProjection{},
	_constantBufferMaterial{},
	_texture{},
	_nullTexture{},
	_dh_texture{},
	_dh_materials{}
{
	_pmdMesh = new pmdMesh();
}

// 書き直しましょうね
DirectX::XMFLOAT3 operator-(DirectX::XMFLOAT3 invalL, DirectX::XMFLOAT3 invalR)
{
	invalL.x -= invalR.x;
	invalL.y -= invalR.y;
	invalL.z -= invalR.z;

	return invalL;
}

DirectX::XMFLOAT3 operator+(DirectX::XMFLOAT3 invalL, DirectX::XMFLOAT3 invalR)
{
	invalL.x += invalR.x;
	invalL.y += invalR.y;
	invalL.z += invalR.z;

	return invalL;
}

bool operator==(DirectX::XMFLOAT3 invalL, DirectX::XMFLOAT3 invalR)
{
	if ((invalL.x == invalR.x) && (invalL.y == invalR.y) && (invalL.z == invalR.z))
	{
		return true;
	}

	return false;
}
// 外積
XMFLOAT3 operator^(XMFLOAT3& invalL, XMFLOAT3& invalR)
{
	XMFLOAT3 result = XMFLOAT3(invalL.y*invalR.z - invalL.z * invalR.y, invalL.z*invalR.x - invalL.x*invalR.z, invalL.x*invalR.y - invalL.y*invalR.x);
	return result;
}
// 行列計算
XMFLOAT3 operator*=(XMFLOAT3& invalL, XMMATRIX& invalR)
{
	XMFLOAT4X4 matrix;
	XMStoreFloat4x4(&matrix, invalR);
	float x = matrix._11*invalL.x + matrix._12*invalL.y + matrix._13 * invalL.z;
	float y = matrix._21*invalL.x + matrix._22*invalL.y + matrix._23 * invalL.z;
	float z = matrix._31*invalL.x + matrix._32*invalL.y + matrix._33 * invalL.z;
	return XMFLOAT3(x, y, z);
}
// 斜辺計算
float Pmd::Length(XMFLOAT3 inValue)
{
	float result = inValue.x * inValue.x + inValue.y * inValue.y + inValue.z * inValue.z;
	result = sqrt(result);
	return result;
}

DirectX::XMFLOAT3 Pmd::Normalize(DirectX::XMFLOAT3 invalL)
{
	auto distance = this->Length(invalL);
	invalL.x /= distance;
	invalL.y /= distance;
	invalL.z /= distance;

	return invalL;
}

Pmd::~Pmd()
{
}


HRESULT Pmd::Initialize(ID3D12Device *inDevice, ID3D12GraphicsCommandList* inCmdList, ID3D12CommandQueue* inCmdQueue, ID3D12CommandAllocator* inCmdAlloc, ID3D12PipelineState* inPiplineState, UINT64& inFlames, HANDLE& inFenceEvent, ID3D12Fence* inFnece, ID3D12Resource *shadowResource)
{
	HRESULT result;

	_vertexCount = 0;
	_indexCount = 0;
	_materialCount = 0;
	_boneCount = 0;
	_idx = 0;
	_ikBoneCount = 0;

	// pmd読み込み
	DISABLE_C4996
		FILE* fp = fopen("ModelData/miku.pmd", "rb");
	fseek(fp, 283, SEEK_SET);

	//頂点数読み込み
	fread(&_vertexCount, sizeof(unsigned int), 1, fp);
	_vertcies.resize(_vertexCount);
	for (auto& at : _vertcies)
	{
		fread(&at.pos, sizeof(at.pos), 1, fp);
		fread(&at.normal, sizeof(at.normal), 1, fp);
		fread(&at.uv, sizeof(at.uv), 1, fp);
		fread(&at.bone_num, sizeof(at.bone_num), 1, fp);
		fread(&at.bone_weight, sizeof(at.bone_weight), 1, fp);
		fread(&at.edge_flag, sizeof(at.edge_flag), 1, fp);
	}

	// インデックス数読み込み
	fread(&_indexCount, sizeof(_indexCount), 1, fp);
	_indcies.resize(_indexCount);
	// インデックス読み込み?
	fread(&_indcies[0], _indcies.size() * sizeof(unsigned short), 1, fp);

	// マテリアル数の読み込み
	fread(&_materialCount, sizeof(_materialCount), 1, fp);
	// マテリアルデータの読み込み
	_materials.resize(_materialCount);
	_useMaterials.resize(_materialCount);
	for (auto& m : _materials) {
		fread(&m.diffuse_color, sizeof(float), 3, fp);
		fread(&m.alpha, sizeof(float), 1, fp);
		fread(&m.specularity, sizeof(float), 1, fp);
		fread(&m.specular_color, sizeof(float), 3, fp);
		fread(&m.mirror_color, sizeof(float), 3, fp);
		fread(&m.toon_index, sizeof(BYTE), 1, fp);
		fread(&m.edge_flag, sizeof(BYTE), 1, fp);
		fread(&m.face_vert_count, sizeof(BYTE), 24, fp);
	}
	//===========================================================
	/*std::vector<pmdMaterial>::iterator it = _materials.begin();
	DWORD cnt = 0;
	for (int i = 0; i < 17; i++)
	{
	cnt += it->face_vert_count;
	it++;
	}*/
	// 上の方法でインデックス数と一緒を確認
	//===========================================================
	// ボーン数読み込み
	fread(&_boneCount, sizeof(unsigned short), 1, fp);

	// 領域を得る
	_pmdMesh->BoneInformation().resize(_boneCount);
	_pmdMesh->Bone().resize(_boneCount);
	_pmdMesh->BoneMatrix().resize(_boneCount);
	_pmdMesh->BoneNode().resize(_boneCount);
	std::fill(_pmdMesh->BoneMatrix().begin(), _pmdMesh->BoneMatrix().end(), XMMatrixIdentity());
	// ボーン情報の読み込み
	for (auto& b : _pmdMesh->BoneInformation())
	{
		fread(&b.bone_name, sizeof(b.bone_name), 1, fp);
		fread(&b.parent_bone_index, sizeof(b.parent_bone_index), 1, fp);
		fread(&b.tail_pos_bone_index, sizeof(b.tail_pos_bone_index), 1, fp);
		fread(&b.bone_type, sizeof(b.bone_type), 1, fp);
		fread(&b.ik_parent_bone_index, sizeof(b.ik_parent_bone_index), 1, fp);
		fread(&b.bone_head_pos, sizeof(b.bone_head_pos), 1, fp);
		_pmdMesh->BoneMap()[b.bone_name] = _idx++;
	}
	// 頂点とお尻を得る
	for (int i = 0; i < _boneCount; i++)
	{
		if (_pmdMesh->BoneInformation()[i].tail_pos_bone_index != 0)
		{
			_pmdMesh->Bone()[i].headPos = _pmdMesh->BoneInformation()[i].bone_head_pos;
			_pmdMesh->Bone()[i].tailPos = _pmdMesh->BoneInformation()[_pmdMesh->BoneInformation()[i].tail_pos_bone_index].bone_head_pos - _pmdMesh->BoneInformation()[i].bone_head_pos;
		}
		if (_pmdMesh->BoneInformation()[i].parent_bone_index != 0xFFFF)
		{
			_pmdMesh->BoneNode()[_pmdMesh->BoneInformation()[i].parent_bone_index].childIdx.push_back(i);
		}
	}
	// IKボーンの数
	fread(&_ikBoneCount, sizeof(unsigned short), 1, fp);
	// IKボーン情報を読みとる
	_pmdMesh->IkList().resize(_ikBoneCount);
	for (auto& ikb : _pmdMesh->IkList())
	{
		fread(&ikb.boneIdx, sizeof(ikb.boneIdx), 1, fp);
		fread(&ikb.tboneIdx, sizeof(ikb.tboneIdx), 1, fp);
		fread(&ikb.chainLen, sizeof(ikb.chainLen), 1, fp);
		fread(&ikb.iterationNum, sizeof(ikb.iterationNum), 1, fp);
		fread(&ikb.restriction, sizeof(ikb.restriction), 1, fp);
		ikb.boneIndices.resize(ikb.chainLen);
		for (auto& indices : ikb.boneIndices)
		{
			fread(&indices, sizeof(indices), 1, fp);
		}
	}
	// mapを作る
	int ikListIndex = 0;
	for (auto& b : _pmdMesh->BoneInformation())
	{
		if (b.parent_bone_index == _pmdMesh->IkList()[ikListIndex].boneIdx)
		{
			_pmdMesh->IkMap()[b.bone_name] = _pmdMesh->IkList()[ikListIndex];
			if (++ikListIndex >= _pmdMesh->IkList().size())
			{
				break;
			}
		}
	}
	fclose(fp);
	ENABLE_C4996
	// いったん使うものを使用
	for (unsigned int i = 0; i < _materialCount; ++i)
	{
		_useMaterials[i].diffuse = _materials[i].diffuse_color;
		_useMaterials[i].ambient = _materials[i].mirror_color;
		_useMaterials[i].count = _materials[i].face_vert_count;
		_useMaterials[i].offset = _materials[i].face_vert_count;
	}




	D3D12_HEAP_PROPERTIES heapProperties{};
	D3D12_RESOURCE_DESC   resourceDesc{};

	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = 256;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;// アンチェリ関連のはず

										// ①定数バッファの作成(合成行列用)
	{
		result = inDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_constantBuffer));
		if (FAILED(result))
		{
			return result;
		}
	}
	// ②定数バッファの作成(World用)
	{
		result = inDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_constantBufferWorld));
		if (FAILED(result))
		{
			return result;
		}
	}
	// ③定数バッファの作成(View * Projection用)
	{
		result = inDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_constantBufferViewProjection));
		if (FAILED(result))
		{
			return result;
		}
	}
	/// 先にすべてを取得することに意味がある
	resourceDesc.Width = sizeof(usePMDMaterial) *_materialCount;
	// ④定数バッファの作成(マテリアル用)
	{
		result = inDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_constantBufferMaterial));
		if (FAILED(result))
		{
			return result;
		}
	}
	// 頂点バッファの作成
	{
		resourceDesc.Width = _vertcies.size() * sizeof(pmdVertex) /*+ (256 - _vertcies.size() * sizeof(pmdVertex) % 256) % 256*/; // 一つ当たりの頂点のサイズ * 弧を作るためのちょてんの数 * 弧の数
		result = inDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_vertexBuffer));
		if (FAILED(result))
		{
			return result;
		}
	}
	// インデックスバッファ
	{
		resourceDesc.Width = _indcies.size() * sizeof(unsigned short) + (256 - _indcies.size() * sizeof(unsigned short) % 256) % 256;
		result = inDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_indexBuffer));
		if (FAILED(result))
		{
			return result;
		}
	}

	// テクスチャ用のデスクリプターヒープを作成する
	_dh_texture.resize(_materialCount);
	D3D12_DESCRIPTOR_HEAP_DESC dh_desc;
	dh_desc.NumDescriptors = 2;	// デスクリプタの数 影も入れる
	dh_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//今回はシェーダリソースで使うよ
	dh_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//可視化
	dh_desc.NodeMask = 0;
	for (auto& dhtex : _dh_texture)
	{
		result = inDevice->CreateDescriptorHeap(&dh_desc, IID_PPV_ARGS(&dhtex));
		if (FAILED(result))
		{
			return result;
		}
	}

	// 中間ソースの生成等
	for (unsigned int i = 0; i < _materialCount; ++i)
	{
		wchar_t a[20] = {};
		FILE* checkFile;
		wchar_t* w = NULL;
		if (_materials[i].texture_file_name != NULL)
		{
			const char* str = _materials[i].texture_file_name;
			std::string texturefilename = str;
			size_t extIdx = texturefilename.rfind('.');
			std::string ext = texturefilename.substr(extIdx + 1, texturefilename.length() - extIdx);

			std::string pass = "ModelData/";
			std::string filename = str;
			std::string file = pass + str;
			char* strTex = new char[file.size() + 1];
			std::char_traits<char>::copy(strTex, file.c_str(), file.size() + 1);
			std::string texturefile = strTex;
			size_t Idx = texturefile.rfind('*');
			std::string ext2 = texturefile.substr(0, Idx);
			str = ext2.c_str();
			DISABLE_C4996
				checkFile = fopen(str, "rb");
			ENABLE_C4996
				/*if (ext != "spa")
				{*/
			for (int i = 0; i < 20; i++)
			{
				if (*(str + 1) != NULL)
				{
					a[i] = (wchar_t)*(str + i);
				}
				else
				{
					break;
				}
			}
			w = (wchar_t*)a;
			//}
		}
		if (checkFile == NULL)
		{
			ComPtr<ID3D12Resource> textureUploadHeap;

			result = DirectX::LoadWICTextureFromFile(inDevice, _T("ModelData/null.jpg"), _useMaterials[i].texture.ReleaseAndGetAddressOf(), _decodedData, _subresource);
			if (FAILED(result))
			{
				return result;
			}

			D3D12_RESOURCE_DESC uploadDesc = {
				D3D12_RESOURCE_DIMENSION_BUFFER,
				0,
				GetRequiredIntermediateSize(_useMaterials[i].texture.Get(), 0, 1),
				1,
				1,
				1,
				DXGI_FORMAT_UNKNOWN,
				{ 1, 0 },
				D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
				D3D12_RESOURCE_FLAG_NONE
			};

			D3D12_HEAP_PROPERTIES props = {
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
				D3D12_MEMORY_POOL_UNKNOWN,
				1,
				1
			};

			result = inDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(textureUploadHeap.GetAddressOf()));
			if (FAILED(result))
			{
				return result;
			}

			UpdateSubresources(inCmdList, _useMaterials[i].texture.Get(), textureUploadHeap.Get(), static_cast<UINT>(0), static_cast<UINT>(0), static_cast<UINT>(1), &_subresource);

			inCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_useMaterials[i].texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			D3D12_CPU_DESCRIPTOR_HANDLE handleSRV{};
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Format = _useMaterials[i].texture.Get()->GetDesc().Format;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;
			srv_desc.Texture2D.MostDetailedMip = 0;
			srv_desc.Texture2D.PlaneSlice = 0;
			srv_desc.Texture2D.ResourceMinLODClamp = 0.0F;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			handleSRV = _dh_texture[i]->GetCPUDescriptorHandleForHeapStart();
			// テクスチャとデスクリプターヒープとを紐付ける
			inDevice->CreateShaderResourceView(_useMaterials[i].texture.Get(), &srv_desc, handleSRV);
			srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
			handleSRV.ptr += inDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			inDevice->CreateShaderResourceView(shadowResource, &srv_desc, handleSRV);
			inCmdList->Close();
			ID3D12CommandList* command_lists[] = { inCmdList };
			// closeしてから呼ぼうね
			inCmdQueue->ExecuteCommandLists(_countof(command_lists), command_lists); // 実行するコマンドリストの配列をコマンドキューへ送信します
																					 // Managerと違って1をイベント値として扱っていきましょう！
			WaitForPreviousFrame(inFlames, inCmdQueue, inFenceEvent, inFnece);

			inCmdAlloc->Reset();
			inCmdList->Reset(inCmdAlloc, inPiplineState);

		}																				// 設定されていないところではnull
		else
		{
			ComPtr<ID3D12Resource> textureUploadHeap;

			result = DirectX::LoadWICTextureFromFile(inDevice, w, _useMaterials[i].texture.ReleaseAndGetAddressOf(), _decodedData, _subresource);
			if (FAILED(result))
			{
				return result;
			}
			D3D12_RESOURCE_DESC uploadDesc = {
				D3D12_RESOURCE_DIMENSION_BUFFER,
				0,
				GetRequiredIntermediateSize(_useMaterials[i].texture.Get(), 0, 1),
				1,
				1,
				1,
				DXGI_FORMAT_UNKNOWN,
				{ 1, 0 },
				D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
				D3D12_RESOURCE_FLAG_NONE
			};

			D3D12_HEAP_PROPERTIES props = {
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
				D3D12_MEMORY_POOL_UNKNOWN,
				1,
				1
			};

			result = inDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(textureUploadHeap.GetAddressOf()));
			if (FAILED(result))
			{
				return result;
			}

			UpdateSubresources(inCmdList, _useMaterials[i].texture.Get(), textureUploadHeap.Get(), static_cast<UINT>(0), static_cast<UINT>(0), static_cast<UINT>(1), &_subresource);
			if (FAILED(result))
			{
				return result;
			}
			inCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_useMaterials[i].texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			D3D12_CPU_DESCRIPTOR_HANDLE handleSRV{};
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Format = _useMaterials[i].texture.Get()->GetDesc().Format;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;
			srv_desc.Texture2D.MostDetailedMip = 0;
			srv_desc.Texture2D.PlaneSlice = 0;
			srv_desc.Texture2D.ResourceMinLODClamp = 0.0F;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			handleSRV = _dh_texture[i]->GetCPUDescriptorHandleForHeapStart();
			// テクスチャとデスクリプターヒープとを紐付ける
			inDevice->CreateShaderResourceView(_useMaterials[i].texture.Get(), &srv_desc, handleSRV);
			srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
			handleSRV.ptr += inDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			inDevice->CreateShaderResourceView(shadowResource, &srv_desc, handleSRV);
			inCmdList->Close();
			ID3D12CommandList* command_lists[] = { inCmdList };
			// closeしてから呼ぼうね
			inCmdQueue->ExecuteCommandLists(_countof(command_lists), command_lists); // 実行するコマンドリストの配列をコマンドキューへ送信します
																					 // コマンド転送自体はすぐ終わるけどGPUの処理が終わらないかもしれないから待とうね
			WaitForPreviousFrame(inFlames, inCmdQueue, inFenceEvent, inFnece);

			inCmdAlloc->Reset();
			inCmdList->Reset(inCmdAlloc, inPiplineState);
			// 開けたら閉めようね
			fclose(checkFile);
		}
	}

	// データの書き込み
	// 書き込み可能にする
	// 頂点データの書き込み
	pmdVertex *vbuffer{};
	_vertexBuffer->Map(0, nullptr, (void**)&vbuffer);
	memcpy(&vbuffer[0], &_vertcies[0], _vertcies.size() * sizeof(pmdVertex));
	vbuffer = nullptr;
	_vertexBuffer->Unmap(0, nullptr);

	//インデックスデータの書き込み
	unsigned *ibuffer{};
	_indexBuffer->Map(0, nullptr, (void**)&ibuffer);
	memcpy(&ibuffer[0], &_indcies[0], _indcies.size() * sizeof(unsigned short));
	ibuffer = nullptr;
	// 閉じる
	_indexBuffer->Unmap(0, nullptr);

	// マテリアルバッファ書き込み
	unsigned int U32Size = static_cast<unsigned int>(sizeof(usePMDMaterial));
	int offset = 0;
	for (unsigned int i = 0; i < _materialCount; ++i)
	{
		usePMDMaterial *matBuffer{};
		_constantBufferMaterial.Get()->Map(0, nullptr, (void**)&matBuffer);
		memcpy(&matBuffer[i], &_useMaterials[i], U32Size);
		_constantBufferMaterial.Get()->Unmap(0, nullptr);
		matBuffer = nullptr;
	}

	// ディフューズカラー用のでスクリプターヒープを作成
	_dh_materials.resize(_materialCount);
	D3D12_DESCRIPTOR_HEAP_DESC dh_mt_desc;
	dh_mt_desc.NumDescriptors = 1;	// デスクリプタの数
	dh_mt_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//今回はシェーダリソースで使うよ
	dh_mt_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//可視化
	dh_mt_desc.NodeMask = 0;

	for (auto& mtDh : _dh_materials)
	{
		result = inDevice->CreateDescriptorHeap(&dh_mt_desc, IID_PPV_ARGS(&mtDh));
	}
	// コンスタントバッファビューを作成
	D3D12_CPU_DESCRIPTOR_HANDLE handleMatCBV{};
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
	cbv_desc.BufferLocation = _constantBufferMaterial.Get()->GetGPUVirtualAddress();
	cbv_desc.SizeInBytes = U32Size; //一区切り;
	unsigned int dhSize = inDevice->GetDescriptorHandleIncrementSize(dh_mt_desc.Type);//デスクリプタのサイズ
																					  //handleMatCBV = &_dh_materials.Get()->GetCPUDescriptorHandleForHeapStart();
	for (unsigned int i = 0; i < _materialCount; ++i)
	{

		// ようは配列のように１ブロックに固まってると仮想してずらそうとしてたんだろうが、そんなことはまったくなくて。
		// こうすればいいんじゃね？
		handleMatCBV = _dh_materials[i].Get()->GetCPUDescriptorHandleForHeapStart();

		inDevice->CreateConstantBufferView(&cbv_desc, handleMatCBV);
		//　マテリアルバッファのアドレスずらし
		cbv_desc.BufferLocation += U32Size;
		result = inDevice->GetDeviceRemovedReason();
		if (FAILED(result))
		{
			return result;
		}
	}

	return S_OK;
}

HRESULT Pmd::Draw(ID3D12GraphicsCommandList *inCommandList)
{
	HRESULT hr;
	static int Cnt{};
	//++Cnt;

	//カメラの設定
	XMMATRIX view = XMMatrixLookAtLH({ 0.0f, 20.0f, -41.0f }, { 0.0f, 5.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
	XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(50.0f), 640.0f / 480.0f, 1.0f, 300.0f);

	//オブジェクトの回転の設定
	XMMATRIX world = XMMatrixRotationY(XMConvertToRadians(static_cast<float>(Cnt % 1800)) / 5.0f);


	// 合成行列用コンスタントバッファを書き込む
	// 行列に変換(Store)
	XMMATRIX *buffer{};
	hr = _constantBuffer->Map(0, nullptr, (void**)&buffer);
	if (FAILED(hr)) {

		return hr;
	}
	*buffer = world * view * projection;
	_constantBuffer->Unmap(0, nullptr);

	// worldだけ投げる
	/*XMFLOAT4X4 MatWorld;
	XMStoreFloat4x4(&MatWorld, world);*/
	XMMATRIX *bufferWorld{};
	hr = _constantBufferWorld->Map(0, nullptr, (void**)&bufferWorld);
	if (FAILED(hr)) {
		return hr;
	}
	*bufferWorld = world;
	_constantBufferWorld->Unmap(0, nullptr);
	bufferWorld = nullptr;

	// viewとprojectioの合成行列を投げる
	//XMFLOAT4X4 MatVP;
	//XMStoreFloat4x4(&MatVP, XMMatrixTranspose(view * projection));
	XMMATRIX *bufferViewProjection{};
	hr = _constantBufferViewProjection->Map(0, nullptr, (void**)&bufferViewProjection);
	if (FAILED(hr)) {
		return hr;
	}
	*bufferViewProjection = view*projection;
	_constantBufferViewProjection->Unmap(0, nullptr);
	bufferViewProjection = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vertex_view{};
	vertex_view.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
	vertex_view.StrideInBytes = sizeof(pmdVertex);
	vertex_view.SizeInBytes = static_cast<UINT>(_vertcies.size()) * sizeof(pmdVertex) /*+ (256 - _vertcies.size() * sizeof(pmdVertex) % 256) % 256*/;

	D3D12_INDEX_BUFFER_VIEW index_view{};
	index_view.BufferLocation = _indexBuffer->GetGPUVirtualAddress();
	index_view.SizeInBytes = static_cast<UINT>(_indcies.size()) * sizeof(unsigned short) /*+ (256 - _indcies.size() * sizeof(unsigned short) % 256) % 256*/;
	index_view.Format = DXGI_FORMAT_R16_UINT;

	//定数バッファをシェーダのレジスタに
	inCommandList->SetGraphicsRootConstantBufferView(0, _constantBuffer->GetGPUVirtualAddress());
	inCommandList->SetGraphicsRootConstantBufferView(1, _constantBufferWorld->GetGPUVirtualAddress());
	inCommandList->SetGraphicsRootConstantBufferView(2, _constantBufferViewProjection->GetGPUVirtualAddress());

	//インデックスを使用し、トライアングルリストを描画
	inCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	inCommandList->IASetVertexBuffers(0, 1, &vertex_view);
	inCommandList->IASetIndexBuffer(&index_view);

	DWORD offsetIndex = 0;
	int i = 0;
	//マテリアルごとに描画
	for (auto& m : _useMaterials)
	{

		//テクスチャをシェーダのレジスタにセット
		ID3D12DescriptorHeap* tex_heaps[] = { _dh_texture[i].Get() };
		inCommandList->SetDescriptorHeaps(_countof(tex_heaps), tex_heaps);
		//ルートパラメータに合わせてるのを忘れないでね
		inCommandList->SetGraphicsRootDescriptorTable(4, _dh_texture[i]->GetGPUDescriptorHandleForHeapStart());// コマンドリストに設定

																											   //マテリアルをシェーダのレジスタにセット
		ID3D12DescriptorHeap* mat_heaps[] = { _dh_materials[i].Get() };
		inCommandList->SetDescriptorHeaps(_countof(mat_heaps), mat_heaps);
		//ルートパラメータに合わせてるのを忘れないでね
		inCommandList->SetGraphicsRootDescriptorTable(3, _dh_materials[i].Get()->GetGPUDescriptorHandleForHeapStart());// コマンドリストに設定

		inCommandList->DrawIndexedInstanced(m.count, 1, offsetIndex, 0, 0);
		offsetIndex += m.offset;
		i++;
	}

	return S_OK;
}

HRESULT Pmd::WaitForPreviousFrame(UINT64& inFlames, ID3D12CommandQueue* ommand_queue_, HANDLE& fence_event_, ID3D12Fence* inFnece) {
	HRESULT hr;

	const UINT64 fence = inFlames;
	hr = ommand_queue_->Signal(inFnece, fence);
	if (FAILED(hr)) {
		return -1;
	}
	++inFlames;

	if (inFnece->GetCompletedValue() < fence) {
		hr = inFnece->SetEventOnCompletion(fence, fence_event_);
		if (FAILED(hr)) {
			return -1;
		}

		WaitForSingleObject(fence_event_, INFINITE);
	}
	return S_OK;
}

// CCD-IKの実装
void Pmd::CcdikSolve(std::map <std::string, IKList>& ikmap, const char* ikname, XMFLOAT3& location)
{
	int ikIdx = ikmap[ikname].boneIdx;											// IKボーンマトリクス番号を取得する
	std::vector<unsigned short>& indices = ikmap[ikname].boneIndices;			// IKに対応するボーンのインデックス
	std::vector<unsigned short>::iterator indicesIterator = indices.begin();	// IKに対応するボーンコンテナのイテレータ

	std::vector<XMFLOAT3> tmpLocation(indices.size());							// サイクリックのために保持しておく
	XMFLOAT3 ikPos = _pmdMesh->BoneInformation()[ikIdx].bone_head_pos;						// IKの座標
	XMFLOAT3 targetPos = ikPos + location;										// IKの座標（目的地）

	int nodeCount = indices.size();
	for (int i = 0; i < nodeCount; ++i)
	{
		tmpLocation[i] = _pmdMesh->BoneInformation()[indices[i]].bone_head_pos;
	}

	for (int c = 0; c < 40; ++c) // 40回やっても届かないならダメ
	{
		for (int i = 0; i < nodeCount; ++i)
		{
			// チェック
			auto name = _pmdMesh->BoneInformation()[indices[i]].bone_name;
			// 現在のIKボーンの位置が目的地なら終わり
			if (ikPos == targetPos)
			{
				break;
			}
			// 先っちょのIKとさかのぼりノードでベクトル作成
			// IKに対応するすべてのボーン単位で処理する
			XMFLOAT3 originalVec = ikPos - tmpLocation[i];
			// 目標地点とさかのぼりノードでベクトルを作成
			XMFLOAT3 transVector = targetPos - tmpLocation[i];

			// ベクトル長がちいさすぎる場合は処理を打ち切る
			if (abs(Length(transVector)) < 0.0001 || abs(Length(originalVec)) < 0.0001)
			{
				return;
			}
			// 軸を作成(外積)
			XMFLOAT3 norm = Normalize(originalVec) ^ Normalize(transVector);

			// 右ひざと左ひざの軸はX軸回転固定
			//const char* name = _boneInformation[indices[i]].bone_name;
			if (strcmp(_pmdMesh->BoneInformation()[indices[i]].bone_name, "右ひざ") == 0 ||
				strcmp(_pmdMesh->BoneInformation()[indices[i]].bone_name, "左ひざ") == 0)
			{
				norm.x = 1.0f;
				norm.y = 0.0f;
				norm.z = 0.0f;
			}
			else
			{
				// 異常だよね
				if (Length(norm) == 0)
				{
					return;
				}
			}
			
			// 角度計算
			XMVECTOR rot = XMVector3AngleBetweenNormals(XMLoadFloat3(&Normalize(originalVec)), XMLoadFloat3(&Normalize(transVector)));
			rot *= 0.5f;	// これはそうなっているのです

			// 角度が小さすぎるときは場合は処理を打ち切る
			// SIMD拡張命令がかかわってるならしょうがないよね
			if (abs(rot.m128_f32[0] == 0.00f))
			{
				return;
			}
			float strict = (2.0f / static_cast<float>(nodeCount)*static_cast<float>(i + 1));
			if (rot.m128_f32[0] > strict)
			{
				rot.m128_f32[0] = strict;
			}
			// 軸中心にオリジナルベクトルとトランスベクトルの内積分回転する回転クォータニオンを作成する
			XMVECTOR q = XMQuaternionRotationAxis(XMLoadFloat3(&norm), rot.m128_f32[0]);

			// ボーン変換行列を計算
			XMFLOAT3 offset = _pmdMesh->BoneInformation()[indices[i]].bone_head_pos;
			XMMATRIX rotAt = XMMatrixTranslation(-offset.x, -offset.y, -offset.z)*XMMatrixRotationQuaternion(q)*XMMatrixTranslation(offset.x, offset.y, offset.z);
			offset = tmpLocation[i];
			// 理論上の変換行列を作成
			rotAt = XMMatrixTranslation(-offset.x, -offset.y, -offset.z)*XMMatrixRotationQuaternion(q)*XMMatrixTranslation(offset.x, offset.y, offset.z);
			// 理論値を更新
			ikPos*=rotAt;

			_pmdMesh->BoneMatrix()[indices[i]] = _pmdMesh->BoneMatrix()[indices[i]] * rotAt;
			for (int j = i - 1; j >= 0; --j)
			{
				tmpLocation[j] *= rotAt;
			}
		}
	}
}