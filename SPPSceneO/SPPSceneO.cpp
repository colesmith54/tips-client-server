// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPSceneO.h"
#include "SPPSTLUtils.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	uint32_t GetSceneVersion()
	{
		return 1;
	}

	void OElement::AddChild(OElement* InChild)
	{
		InChild->RemoveFromParent();
		_children.push_back(InChild);
		InChild->_parent = this;
	}

	void OElement::RemoveChild(OElement* InChild)
	{
		SE_ASSERT(InChild->_parent == this);
		bool bFoundEle = false;
		for (int32_t Iter = 0; Iter < _children.size(); Iter++)
		{
			if (_children[Iter] == InChild)
			{
				EraseWithBackSwap(Iter, _children);
				bFoundEle = true;
				break;
			}
		}
		SE_ASSERT(bFoundEle);
		InChild->_parent = nullptr;

		if (InChild->_scene)
		{
			InChild->RemovedFromScene();
		}
	}

	void OElement::AddedToScene(class OScene* InScene)
	{
		_scene = InScene;
		for (auto& curChild : _children)
		{
			curChild->AddedToScene(InScene);
		}
	}

	void OElement::RemovedFromScene()
	{
		_scene = nullptr;
		for (auto& curChild : _children)
		{
			curChild->RemovedFromScene();
		}
	}

	void OElement::UpdateTransform()
	{
		OElement* lastFromTop = this;
		OElement* top = _parent;
		while (top)
		{
			if (top->_parent)
			{
				lastFromTop = top;
				top = top->_parent;
			}
			else
			{
				break;
			}
		}

		SE_ASSERT(top);

		auto SceneType = top->get_type();
		if (SceneType.is_derived_from(rttr::type::get<OScene>()))
		{
			OScene* topScene = (OScene*)top;
			topScene->RemoveChild(lastFromTop);
			topScene->AddChild (lastFromTop);
		}
	}

	OElement* OElement::GetTop() 
	{
		if (_parent)
		{
			return _parent->GetTop();
		}
		else
		{
			return this;
		}
	}

	bool OElement::Finalize()
	{
		if (SPPObject::Finalize())
		{
			if (_parent)
			{
				_parent->RemoveChild(this);

				while(_children.size())
				{
					RemoveChild(_children.front());
				}
			}

			return true;
		}

		return false;
	}

	OElement* OElement::GetTopBeforeScene()
	{
		if (_parent == _scene || _parent == nullptr)
		{
			return this;
		}
		else 
		{
			return _parent->GetTopBeforeScene();
		}
	}

	Matrix4x4 OElement::GenerateLocalToWorld(bool bSkipTopTranslation) const
	{
		Eigen::Quaternion<float> q = EulerAnglesToQuaternion(_rotation);

		Matrix3x3 scaleMatrix = Matrix3x3::Identity();
		scaleMatrix(0, 0) = _scale[0];
		scaleMatrix(1, 1) = _scale[1];
		scaleMatrix(2, 2) = _scale[2];
		Matrix3x3 rotationMatrix = q.matrix();

		Matrix4x4 transform = Matrix4x4::Identity();
		transform.block<3, 3>(0, 0) = scaleMatrix * rotationMatrix;
		transform.block<1, 3>(3, 0) = (bSkipTopTranslation && _parent == _scene) ?
			Vector3(0, 0, 0) :
			Vector3((float)_translation[0], (float)_translation[1], (float)_translation[2]);

		if (_parent)
		{
			transform = transform * _parent->GenerateLocalToWorld(bSkipTopTranslation);
		}

		return transform;
	}

	void OElement::RemoveFromParent()
	{
		if (_parent)
		{
			_parent->RemoveChild(this);
		}
	}

	bool OElement::Intersect_Ray(const Ray& InRay, IntersectionInfo& oInfo) const
	{
		if (_bounds)
		{
			return Intersection::Intersect_RaySphere(InRay, _bounds, oInfo.location);
		}
		return false;
	}

	OScene::OScene(const std::string& InName, SPPDirectory* InParent) : OElement(InName, InParent)
	{ 
		_octree = std::make_unique<LooseOctree>();
		_octree->Initialize(Vector3d(0, 0, 0), 50000, 3);
	}

	void OScene::AddChild(OElement* InChild)
	{
		OElement::AddChild(InChild);
		InChild->AddedToScene(this);
		if (InChild->Bounds())
		{
			_octree->AddElement(InChild);
		}
	}
	void OScene::RemoveChild(OElement* InChild)
	{
		OElement::RemoveChild(InChild);
		if (InChild->IsInOctree())
		{
			_octree->RemoveElement(InChild);
		}
		InChild->RemovedFromScene();
	}
}

using namespace SPP;

RTTR_REGISTRATION
{	
	rttr::registration::class_<Vector3>("Vector3")
		.property("x", &Vector3::GetX, &Vector3::SetX)
		.property("y", &Vector3::GetY, &Vector3::SetY)
		.property("z", &Vector3::GetZ, &Vector3::SetZ)
		;

	rttr::registration::class_<Vector3d>("Vector3d")
		.property("x", &Vector3d::GetX, &Vector3d::SetX)
		.property("y", &Vector3d::GetY, &Vector3d::SetY)
		.property("z", &Vector3d::GetZ, &Vector3d::SetZ)
		;

	rttr::registration::class_<Vector4d>("Vector4d")
		.property("x", &Vector4d::GetX, &Vector4d::SetX)
		.property("y", &Vector4d::GetY, &Vector4d::SetY)
		.property("z", &Vector4d::GetZ, &Vector4d::SetZ)
		.property("w", &Vector4d::GetW, &Vector4d::SetW)
		;

	rttr::registration::class_<OElement>("OElement")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_parent", &OElement::_parent)(rttr::policy::prop::as_reference_wrapper)
		.property("_children", &OElement::_children)(rttr::policy::prop::as_reference_wrapper)
		.property("_translation", &OElement::_translation)(rttr::policy::prop::as_reference_wrapper)
		.property("_rotation", &OElement::_rotation)(rttr::policy::prop::as_reference_wrapper)
		.property("_scale", &OElement::_scale)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<OScene>("OScene")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}