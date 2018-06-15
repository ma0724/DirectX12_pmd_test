#include "ShadowView.h"



ShadowView::ShadowView(ID3D12Resource* inResource)
{
	_viewShadowMatrixBuffer = inResource;
}


ShadowView::~ShadowView()
{

}
