#include "VMDLoader.h"
#include "VMDData.h"


VMDLoader::VMDLoader()
{
}


VMDLoader::~VMDLoader()
{
}

VMDData* VMDLoader::LoadMotion(char* str)
{
	FILE* fp = fopen(str, "rb");
	// �w�b�_�ǂݍ���
	fread(&_header.vmdheader, sizeof(_header.vmdheader), 1, fp);
	fread(&_header.vmdModelName, sizeof(_header.vmdModelName), 1, fp);
	// ���[�V�����f�[�^���ǂݍ���
	fread(&_motionCount, sizeof(unsigned int), 1, fp);
	_motion.resize(_motionCount);
	_data = new VMDData();
	_data->GetMotionData().resize(_motionCount);
	// ���[�V�����f�[�^�ǂݍ���
	int index = 0;
	for (auto& m : _motion)
	{
		fread(&m.boneName, sizeof(m.boneName), 1, fp);
		fread(&m.frameNo, sizeof(m.frameNo), 1, fp);
		fread(&m.location, sizeof(m.location), 1, fp);
		fread(&m.rotation, sizeof(m.rotation), 1, fp);
		fread(&m.interporation, sizeof(m.interporation), 1, fp);
		MotionData motion = { m.frameNo, DirectX::XMLoadFloat4(&m.rotation) };
		_data->GetMotionKeyflameData()[m.boneName].push_back(motion);
	}
	fclose(fp);

	return _data;
}