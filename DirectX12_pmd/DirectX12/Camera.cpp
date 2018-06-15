#include "Camera.h"



Camera::Camera(ID3D12Resource* inResource)
{
	_viewCameraMatrixBuffer = inResource;
}


Camera::~Camera()
{

}
