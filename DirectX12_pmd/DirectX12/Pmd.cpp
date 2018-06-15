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

// ���������܂��傤��
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
// �O��
XMFLOAT3 operator^(XMFLOAT3& invalL, XMFLOAT3& invalR)
{
	XMFLOAT3 result = XMFLOAT3(invalL.y*invalR.z - invalL.z * invalR.y, invalL.z*invalR.x - invalL.x*invalR.z, invalL.x*invalR.y - invalL.y*invalR.x);
	return result;
}
// �s��v�Z
XMFLOAT3 operator*=(XMFLOAT3& invalL, XMMATRIX& invalR)
{
	XMFLOAT4X4 matrix;
	XMStoreFloat4x4(&matrix, invalR);
	float x = matrix._11*invalL.x + matrix._12*invalL.y + matrix._13 * invalL.z;
	float y = matrix._21*invalL.x + matrix._22*invalL.y + matrix._23 * invalL.z;
	float z = matrix._31*invalL.x + matrix._32*invalL.y + matrix._33 * invalL.z;
	return XMFLOAT3(x, y, z);
}
// �Εӌv�Z
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

	// pmd�ǂݍ���
	DISABLE_C4996
		FILE* fp = fopen("ModelData/miku.pmd", "rb");
	fseek(fp, 283, SEEK_SET);

	//���_���ǂݍ���
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

	// �C���f�b�N�X���ǂݍ���
	fread(&_indexCount, sizeof(_indexCount), 1, fp);
	_indcies.resize(_indexCount);
	// �C���f�b�N�X�ǂݍ���?
	fread(&_indcies[0], _indcies.size() * sizeof(unsigned short), 1, fp);

	// �}�e���A�����̓ǂݍ���
	fread(&_materialCount, sizeof(_materialCount), 1, fp);
	// �}�e���A���f�[�^�̓ǂݍ���
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
	// ��̕��@�ŃC���f�b�N�X���ƈꏏ���m�F
	//===========================================================
	// �{�[�����ǂݍ���
	fread(&_boneCount, sizeof(unsigned short), 1, fp);

	// �̈�𓾂�
	_pmdMesh->BoneInformation().resize(_boneCount);
	_pmdMesh->Bone().resize(_boneCount);
	_pmdMesh->BoneMatrix().resize(_boneCount);
	_pmdMesh->BoneNode().resize(_boneCount);
	std::fill(_pmdMesh->BoneMatrix().begin(), _pmdMesh->BoneMatrix().end(), XMMatrixIdentity());
	// �{�[�����̓ǂݍ���
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
	// ���_�Ƃ��K�𓾂�
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
	// IK�{�[���̐�
	fread(&_ikBoneCount, sizeof(unsigned short), 1, fp);
	// IK�{�[������ǂ݂Ƃ�
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
	// map�����
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
	// ��������g�����̂��g�p
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
	resourceDesc.SampleDesc.Quality = 0;// �A���`�F���֘A�̂͂�

										// �@�萔�o�b�t�@�̍쐬(�����s��p)
	{
		result = inDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_constantBuffer));
		if (FAILED(result))
		{
			return result;
		}
	}
	// �A�萔�o�b�t�@�̍쐬(World�p)
	{
		result = inDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_constantBufferWorld));
		if (FAILED(result))
		{
			return result;
		}
	}
	// �B�萔�o�b�t�@�̍쐬(View * Projection�p)
	{
		result = inDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_constantBufferViewProjection));
		if (FAILED(result))
		{
			return result;
		}
	}
	/// ��ɂ��ׂĂ��擾���邱�ƂɈӖ�������
	resourceDesc.Width = sizeof(usePMDMaterial) *_materialCount;
	// �C�萔�o�b�t�@�̍쐬(�}�e���A���p)
	{
		result = inDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_constantBufferMaterial));
		if (FAILED(result))
		{
			return result;
		}
	}
	// ���_�o�b�t�@�̍쐬
	{
		resourceDesc.Width = _vertcies.size() * sizeof(pmdVertex) /*+ (256 - _vertcies.size() * sizeof(pmdVertex) % 256) % 256*/; // �������̒��_�̃T�C�Y * �ʂ���邽�߂̂���Ă�̐� * �ʂ̐�
		result = inDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_vertexBuffer));
		if (FAILED(result))
		{
			return result;
		}
	}
	// �C���f�b�N�X�o�b�t�@
	{
		resourceDesc.Width = _indcies.size() * sizeof(unsigned short) + (256 - _indcies.size() * sizeof(unsigned short) % 256) % 256;
		result = inDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_indexBuffer));
		if (FAILED(result))
		{
			return result;
		}
	}

	// �e�N�X�`���p�̃f�X�N���v�^�[�q�[�v���쐬����
	_dh_texture.resize(_materialCount);
	D3D12_DESCRIPTOR_HEAP_DESC dh_desc;
	dh_desc.NumDescriptors = 2;	// �f�X�N���v�^�̐� �e�������
	dh_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//����̓V�F�[�_���\�[�X�Ŏg����
	dh_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//����
	dh_desc.NodeMask = 0;
	for (auto& dhtex : _dh_texture)
	{
		result = inDevice->CreateDescriptorHeap(&dh_desc, IID_PPV_ARGS(&dhtex));
		if (FAILED(result))
		{
			return result;
		}
	}

	// ���ԃ\�[�X�̐�����
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
			// �e�N�X�`���ƃf�X�N���v�^�[�q�[�v�Ƃ�R�t����
			inDevice->CreateShaderResourceView(_useMaterials[i].texture.Get(), &srv_desc, handleSRV);
			srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
			handleSRV.ptr += inDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			inDevice->CreateShaderResourceView(shadowResource, &srv_desc, handleSRV);
			inCmdList->Close();
			ID3D12CommandList* command_lists[] = { inCmdList };
			// close���Ă���Ăڂ���
			inCmdQueue->ExecuteCommandLists(_countof(command_lists), command_lists); // ���s����R�}���h���X�g�̔z����R�}���h�L���[�֑��M���܂�
																					 // Manager�ƈ����1���C�x���g�l�Ƃ��Ĉ����Ă����܂��傤�I
			WaitForPreviousFrame(inFlames, inCmdQueue, inFenceEvent, inFnece);

			inCmdAlloc->Reset();
			inCmdList->Reset(inCmdAlloc, inPiplineState);

		}																				// �ݒ肳��Ă��Ȃ��Ƃ���ł�null
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
			// �e�N�X�`���ƃf�X�N���v�^�[�q�[�v�Ƃ�R�t����
			inDevice->CreateShaderResourceView(_useMaterials[i].texture.Get(), &srv_desc, handleSRV);
			srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
			handleSRV.ptr += inDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			inDevice->CreateShaderResourceView(shadowResource, &srv_desc, handleSRV);
			inCmdList->Close();
			ID3D12CommandList* command_lists[] = { inCmdList };
			// close���Ă���Ăڂ���
			inCmdQueue->ExecuteCommandLists(_countof(command_lists), command_lists); // ���s����R�}���h���X�g�̔z����R�}���h�L���[�֑��M���܂�
																					 // �R�}���h�]�����̂͂����I��邯��GPU�̏������I���Ȃ���������Ȃ�����҂Ƃ���
			WaitForPreviousFrame(inFlames, inCmdQueue, inFenceEvent, inFnece);

			inCmdAlloc->Reset();
			inCmdList->Reset(inCmdAlloc, inPiplineState);
			// �J������߂悤��
			fclose(checkFile);
		}
	}

	// �f�[�^�̏�������
	// �������݉\�ɂ���
	// ���_�f�[�^�̏�������
	pmdVertex *vbuffer{};
	_vertexBuffer->Map(0, nullptr, (void**)&vbuffer);
	memcpy(&vbuffer[0], &_vertcies[0], _vertcies.size() * sizeof(pmdVertex));
	vbuffer = nullptr;
	_vertexBuffer->Unmap(0, nullptr);

	//�C���f�b�N�X�f�[�^�̏�������
	unsigned *ibuffer{};
	_indexBuffer->Map(0, nullptr, (void**)&ibuffer);
	memcpy(&ibuffer[0], &_indcies[0], _indcies.size() * sizeof(unsigned short));
	ibuffer = nullptr;
	// ����
	_indexBuffer->Unmap(0, nullptr);

	// �}�e���A���o�b�t�@��������
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

	// �f�B�t���[�Y�J���[�p�̂ŃX�N���v�^�[�q�[�v���쐬
	_dh_materials.resize(_materialCount);
	D3D12_DESCRIPTOR_HEAP_DESC dh_mt_desc;
	dh_mt_desc.NumDescriptors = 1;	// �f�X�N���v�^�̐�
	dh_mt_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//����̓V�F�[�_���\�[�X�Ŏg����
	dh_mt_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//����
	dh_mt_desc.NodeMask = 0;

	for (auto& mtDh : _dh_materials)
	{
		result = inDevice->CreateDescriptorHeap(&dh_mt_desc, IID_PPV_ARGS(&mtDh));
	}
	// �R���X�^���g�o�b�t�@�r���[���쐬
	D3D12_CPU_DESCRIPTOR_HANDLE handleMatCBV{};
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
	cbv_desc.BufferLocation = _constantBufferMaterial.Get()->GetGPUVirtualAddress();
	cbv_desc.SizeInBytes = U32Size; //���؂�;
	unsigned int dhSize = inDevice->GetDescriptorHandleIncrementSize(dh_mt_desc.Type);//�f�X�N���v�^�̃T�C�Y
																					  //handleMatCBV = &_dh_materials.Get()->GetCPUDescriptorHandleForHeapStart();
	for (unsigned int i = 0; i < _materialCount; ++i)
	{

		// �悤�͔z��̂悤�ɂP�u���b�N�Ɍł܂��Ă�Ɖ��z���Ă��炻���Ƃ��Ă��񂾂낤���A����Ȃ��Ƃ͂܂������Ȃ��āB
		// ��������΂����񂶂�ˁH
		handleMatCBV = _dh_materials[i].Get()->GetCPUDescriptorHandleForHeapStart();

		inDevice->CreateConstantBufferView(&cbv_desc, handleMatCBV);
		//�@�}�e���A���o�b�t�@�̃A�h���X���炵
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

	//�J�����̐ݒ�
	XMMATRIX view = XMMatrixLookAtLH({ 0.0f, 20.0f, -41.0f }, { 0.0f, 5.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
	XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(50.0f), 640.0f / 480.0f, 1.0f, 300.0f);

	//�I�u�W�F�N�g�̉�]�̐ݒ�
	XMMATRIX world = XMMatrixRotationY(XMConvertToRadians(static_cast<float>(Cnt % 1800)) / 5.0f);


	// �����s��p�R���X�^���g�o�b�t�@����������
	// �s��ɕϊ�(Store)
	XMMATRIX *buffer{};
	hr = _constantBuffer->Map(0, nullptr, (void**)&buffer);
	if (FAILED(hr)) {

		return hr;
	}
	*buffer = world * view * projection;
	_constantBuffer->Unmap(0, nullptr);

	// world����������
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

	// view��projectio�̍����s��𓊂���
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

	//�萔�o�b�t�@���V�F�[�_�̃��W�X�^��
	inCommandList->SetGraphicsRootConstantBufferView(0, _constantBuffer->GetGPUVirtualAddress());
	inCommandList->SetGraphicsRootConstantBufferView(1, _constantBufferWorld->GetGPUVirtualAddress());
	inCommandList->SetGraphicsRootConstantBufferView(2, _constantBufferViewProjection->GetGPUVirtualAddress());

	//�C���f�b�N�X���g�p���A�g���C�A���O�����X�g��`��
	inCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	inCommandList->IASetVertexBuffers(0, 1, &vertex_view);
	inCommandList->IASetIndexBuffer(&index_view);

	DWORD offsetIndex = 0;
	int i = 0;
	//�}�e���A�����Ƃɕ`��
	for (auto& m : _useMaterials)
	{

		//�e�N�X�`�����V�F�[�_�̃��W�X�^�ɃZ�b�g
		ID3D12DescriptorHeap* tex_heaps[] = { _dh_texture[i].Get() };
		inCommandList->SetDescriptorHeaps(_countof(tex_heaps), tex_heaps);
		//���[�g�p�����[�^�ɍ��킹�Ă�̂�Y��Ȃ��ł�
		inCommandList->SetGraphicsRootDescriptorTable(4, _dh_texture[i]->GetGPUDescriptorHandleForHeapStart());// �R�}���h���X�g�ɐݒ�

																											   //�}�e���A�����V�F�[�_�̃��W�X�^�ɃZ�b�g
		ID3D12DescriptorHeap* mat_heaps[] = { _dh_materials[i].Get() };
		inCommandList->SetDescriptorHeaps(_countof(mat_heaps), mat_heaps);
		//���[�g�p�����[�^�ɍ��킹�Ă�̂�Y��Ȃ��ł�
		inCommandList->SetGraphicsRootDescriptorTable(3, _dh_materials[i].Get()->GetGPUDescriptorHandleForHeapStart());// �R�}���h���X�g�ɐݒ�

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

// CCD-IK�̎���
void Pmd::CcdikSolve(std::map <std::string, IKList>& ikmap, const char* ikname, XMFLOAT3& location)
{
	int ikIdx = ikmap[ikname].boneIdx;											// IK�{�[���}�g���N�X�ԍ����擾����
	std::vector<unsigned short>& indices = ikmap[ikname].boneIndices;			// IK�ɑΉ�����{�[���̃C���f�b�N�X
	std::vector<unsigned short>::iterator indicesIterator = indices.begin();	// IK�ɑΉ�����{�[���R���e�i�̃C�e���[�^

	std::vector<XMFLOAT3> tmpLocation(indices.size());							// �T�C�N���b�N�̂��߂ɕێ����Ă���
	XMFLOAT3 ikPos = _pmdMesh->BoneInformation()[ikIdx].bone_head_pos;						// IK�̍��W
	XMFLOAT3 targetPos = ikPos + location;										// IK�̍��W�i�ړI�n�j

	int nodeCount = indices.size();
	for (int i = 0; i < nodeCount; ++i)
	{
		tmpLocation[i] = _pmdMesh->BoneInformation()[indices[i]].bone_head_pos;
	}

	for (int c = 0; c < 40; ++c) // 40�����Ă��͂��Ȃ��Ȃ�_��
	{
		for (int i = 0; i < nodeCount; ++i)
		{
			// �`�F�b�N
			auto name = _pmdMesh->BoneInformation()[indices[i]].bone_name;
			// ���݂�IK�{�[���̈ʒu���ړI�n�Ȃ�I���
			if (ikPos == targetPos)
			{
				break;
			}
			// ��������IK�Ƃ����̂ڂ�m�[�h�Ńx�N�g���쐬
			// IK�ɑΉ����邷�ׂẴ{�[���P�ʂŏ�������
			XMFLOAT3 originalVec = ikPos - tmpLocation[i];
			// �ڕW�n�_�Ƃ����̂ڂ�m�[�h�Ńx�N�g�����쐬
			XMFLOAT3 transVector = targetPos - tmpLocation[i];

			// �x�N�g������������������ꍇ�͏�����ł��؂�
			if (abs(Length(transVector)) < 0.0001 || abs(Length(originalVec)) < 0.0001)
			{
				return;
			}
			// �����쐬(�O��)
			XMFLOAT3 norm = Normalize(originalVec) ^ Normalize(transVector);

			// �E�Ђ��ƍ��Ђ��̎���X����]�Œ�
			//const char* name = _boneInformation[indices[i]].bone_name;
			if (strcmp(_pmdMesh->BoneInformation()[indices[i]].bone_name, "�E�Ђ�") == 0 ||
				strcmp(_pmdMesh->BoneInformation()[indices[i]].bone_name, "���Ђ�") == 0)
			{
				norm.x = 1.0f;
				norm.y = 0.0f;
				norm.z = 0.0f;
			}
			else
			{
				// �ُ킾���
				if (Length(norm) == 0)
				{
					return;
				}
			}
			
			// �p�x�v�Z
			XMVECTOR rot = XMVector3AngleBetweenNormals(XMLoadFloat3(&Normalize(originalVec)), XMLoadFloat3(&Normalize(transVector)));
			rot *= 0.5f;	// ����͂����Ȃ��Ă���̂ł�

			// �p�x������������Ƃ��͏ꍇ�͏�����ł��؂�
			// SIMD�g�����߂���������Ă�Ȃ炵�傤���Ȃ����
			if (abs(rot.m128_f32[0] == 0.00f))
			{
				return;
			}
			float strict = (2.0f / static_cast<float>(nodeCount)*static_cast<float>(i + 1));
			if (rot.m128_f32[0] > strict)
			{
				rot.m128_f32[0] = strict;
			}
			// �����S�ɃI���W�i���x�N�g���ƃg�����X�x�N�g���̓��ϕ���]�����]�N�H�[�^�j�I�����쐬����
			XMVECTOR q = XMQuaternionRotationAxis(XMLoadFloat3(&norm), rot.m128_f32[0]);

			// �{�[���ϊ��s����v�Z
			XMFLOAT3 offset = _pmdMesh->BoneInformation()[indices[i]].bone_head_pos;
			XMMATRIX rotAt = XMMatrixTranslation(-offset.x, -offset.y, -offset.z)*XMMatrixRotationQuaternion(q)*XMMatrixTranslation(offset.x, offset.y, offset.z);
			offset = tmpLocation[i];
			// ���_��̕ϊ��s����쐬
			rotAt = XMMatrixTranslation(-offset.x, -offset.y, -offset.z)*XMMatrixRotationQuaternion(q)*XMMatrixTranslation(offset.x, offset.y, offset.z);
			// ���_�l���X�V
			ikPos*=rotAt;

			_pmdMesh->BoneMatrix()[indices[i]] = _pmdMesh->BoneMatrix()[indices[i]] * rotAt;
			for (int j = i - 1; j >= 0; --j)
			{
				tmpLocation[j] *= rotAt;
			}
		}
	}
}