#pragma once
#include <DirectXMath.h>
#include <vector>
class VMDData;

class VMDLoader
{
private:
	struct VMDHeader {
		char vmdheader[30];
		char vmdModelName[20];
	};
	struct VMDMmotion
	{
		char boneName[15];
		unsigned int frameNo;	// �t���[���ԍ�
		DirectX::XMFLOAT3 location;	// �ʒu
		DirectX::XMFLOAT4 rotation;	// [Quaternion] ��]
		unsigned char interporation[64]; // [4][4][4] �⊮(�x�W�F�f�[�^)
	};

	VMDHeader _header;
	std::vector<VMDMmotion> _motion;
	unsigned int _motionCount;
	VMDData* _data;

public:
	VMDLoader();
	~VMDLoader();

	VMDData* LoadMotion(char* str);
};

