// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCamera.h"
#include "SPPPrimitiveShapes.h"

namespace SPP
{
	//5 inches
	static const float NearClippingZ = 0.127f;
	//3 miles
	static const float FarClippingZ = 5000.0f;

	/*
		Calculate frustum split depths and matrices for the shadow map cascades
		Based on https://johanmedestrom.wordpress.com/2016/03/18/opengl-cascaded-shadow-maps/
	*/
	//void CreateCascadeMatrices(float Near, float DesiredFar, uint8_t CascadeCount)
	//{
	//	float cascadeSplitLambda = 0.95f;

	//	std::vector<float> cascadeSplits;
	//	cascadeSplits.resize(CascadeCount);

	//	float nearClip = Near;
	//	float farClip = DesiredFar;
	//	float clipRange = farClip - nearClip;

	//	float minZ = nearClip;
	//	float maxZ = nearClip + clipRange;

	//	float range = maxZ - minZ;
	//	float ratio = maxZ / minZ;

	//	// Calculate split depths based on view camera frustum
	//	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	//	for (uint32_t i = 0; i < CascadeCount; i++) {
	//		float p = (i + 1) / static_cast<float>(CascadeCount);
	//		float log = minZ * std::pow(ratio, p);
	//		float uniform = minZ + range * p;
	//		float d = cascadeSplitLambda * (log - uniform) + uniform;
	//		cascadeSplits[i] = (d - nearClip) / clipRange;
	//	}

	//	// Calculate orthographic projection matrix for each cascade
	//	float lastSplitDist = 0.0;
	//	for (uint32_t i = 0; i < CascadeCount; i++) 
	//	{
	//		float splitDist = cascadeSplits[i];

	//		glm::vec3 frustumCorners[8] = {
	//			glm::vec3(-1.0f,  1.0f, -1.0f),
	//			glm::vec3(1.0f,  1.0f, -1.0f),
	//			glm::vec3(1.0f, -1.0f, -1.0f),
	//			glm::vec3(-1.0f, -1.0f, -1.0f),
	//			glm::vec3(-1.0f,  1.0f,  1.0f),
	//			glm::vec3(1.0f,  1.0f,  1.0f),
	//			glm::vec3(1.0f, -1.0f,  1.0f),
	//			glm::vec3(-1.0f, -1.0f,  1.0f),
	//		};

	//		// Project frustum corners into world space
	//		glm::mat4 invCam = glm::inverse(camera.matrices.perspective * camera.matrices.view);
	//		for (uint32_t i = 0; i < 8; i++) {
	//			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
	//			frustumCorners[i] = invCorner / invCorner.w;
	//		}

	//		for (uint32_t i = 0; i < 4; i++) {
	//			glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
	//			frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
	//			frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
	//		}

	//		// Get frustum center
	//		glm::vec3 frustumCenter = glm::vec3(0.0f);
	//		for (uint32_t i = 0; i < 8; i++) {
	//			frustumCenter += frustumCorners[i];
	//		}
	//		frustumCenter /= 8.0f;

	//		float radius = 0.0f;
	//		for (uint32_t i = 0; i < 8; i++) {
	//			float distance = glm::length(frustumCorners[i] - frustumCenter);
	//			radius = glm::max(radius, distance);
	//		}
	//		radius = std::ceil(radius * 16.0f) / 16.0f;

	//		glm::vec3 maxExtents = glm::vec3(radius);
	//		glm::vec3 minExtents = -maxExtents;

	//		glm::vec3 lightDir = normalize(-lightPos);
	//		glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
	//		glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

	//		// Store split distance and matrix in cascade
	//		cascades[i].splitDepth = (camera.getNearClip() + splitDist * clipRange) * -1.0f;
	//		cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;

	//		lastSplitDist = cascadeSplits[i];
	//	}
	//}

	SPP_MATH_API void getBoundsForAxis(bool xAxis,
		const Vector3& center,
		float radius,
		float nearZ,
		const Matrix4x4& projMatrix,
		Vector4& U,
		Vector4& L)
	{
		bool trivialAccept = (center[2] + radius) < nearZ; // Entirely in back of nearPlane (Trivial Accept)
		const Vector3& a = xAxis ? Vector3(1, 0, 0) : Vector3(0, 1, 0);

		// given in coordinates (a,z), where a is in the direction of the vector a, and z is in the standard z direction
		Vector2 projectedCenter(a.dot(center), center[2]);
		Vector2 bounds_az[2];
		float tSquared = projectedCenter.dot(projectedCenter) - std::powf(radius,2);
		float t = 0, cLength = 0, costheta = 0, sintheta = 0;

		if (tSquared > 0) { // Camera is outside sphere
			// Distance to the tangent points of the sphere (points where a vector from the camera are tangent to the sphere) (calculated a-z space)
			t = std::sqrt(tSquared);
			cLength = projectedCenter.norm();

			// Theta is the angle between the vector from the camera to the center of the sphere and the vectors from the camera to the tangent points
			costheta = t / cLength;
			sintheta = radius / cLength;
		}
		float sqrtPart = 0.0f;
		if (!trivialAccept) 
			sqrtPart = std::sqrt(std::powf(radius,2) - std::powf(nearZ - projectedCenter[1], 2));

		for (int i = 0; i < 2; ++i) {
			if (tSquared > 0) {
				Matrix2x2 rotateTheta{
					{ costheta, -sintheta },
					{ sintheta, costheta } };

				bounds_az[i] = costheta * (projectedCenter * rotateTheta);
			}

			if (!trivialAccept && (tSquared <= 0 || bounds_az[i][1] > nearZ)) {
				bounds_az[i][0] = projectedCenter[0] + sqrtPart;
				bounds_az[i][1] = nearZ;
			}
			sintheta *= -1; // negate theta for B
			sqrtPart *= -1; // negate sqrtPart for B
		}
		U.head<3>() = bounds_az[0][0] * a;
		U[2] = bounds_az[0][1];
		U[3] = 1.0f;
		L.head<3>() = bounds_az[1][0] * a;
		L[2] = bounds_az[1][1];
		L[3] = 1.0f;
	}

	/** Center is in camera space */
	Vector4 getBoundingBox(const Vector3& center, float radius, float nearZ, const Matrix4x4& projMatrix) 
	{
		Vector4 maxXHomogenous, minXHomogenous, maxYHomogenous, minYHomogenous;
		getBoundsForAxis(true, center, radius, nearZ, projMatrix, maxXHomogenous, minXHomogenous);
		getBoundsForAxis(false, center, radius, nearZ, projMatrix, maxYHomogenous, minYHomogenous);
		// We only need one coordinate for each point, so we save computation by only calculating x(or y) and w
		float maxX = maxXHomogenous.dot(projMatrix.row(0)) / maxXHomogenous.dot(projMatrix.row(3));
		float minX = minXHomogenous.dot(projMatrix.row(0)) / minXHomogenous.dot(projMatrix.row(3));
		float maxY = maxYHomogenous.dot(projMatrix.row(1)) / maxYHomogenous.dot(projMatrix.row(3));
		float minY = minYHomogenous.dot(projMatrix.row(1)) / minYHomogenous.dot(projMatrix.row(3));
		return Vector4(minX, minY, maxX, maxY);
	}


	/** Center is in camera space */
	void tileClassification(int tileNumX,
		int tileNumY,
		int tileWidth,
		int tileHeight,
		const Vector3& center,
		float radius,
		float nearZ,
		const Matrix4x4& projMatrix)
	{
		Vector4 projectedBounds = getBoundingBox(center, radius, nearZ, projMatrix);

		int32_t minTileX = std::max< int32_t>(0, (int)(projectedBounds[0] / tileWidth));
		int32_t maxTileX = std::min< int32_t>(tileNumX - 1, (int)(projectedBounds[2] / tileWidth));

		int32_t minTileY = std::max< int32_t>(0, (int)(projectedBounds[1] / tileHeight));
		int32_t maxTileY = std::min< int32_t>(tileNumY - 1, (int)(projectedBounds[3] / tileHeight));

		for (int i = minTileX; i <= maxTileX; ++i) {
			for (int j = minTileY; j <= maxTileY; ++j) {
				// This tile is touched by the bounding box of the sphere
				// Put application specific tile-classification code here.
			}
		}
	}
		
	void Camera::Initialize(const Vector3d& InPosition, const Vector3& InEuler, float FoV, float AspectRatio)
	{
		_cameraPosition = InPosition;
		_eulerAngles = InEuler;
		_FoV = FoV;

		SetupStandardCorrection();
#if 1
		GenerateLHInverseZPerspectiveMatrix(FoV, AspectRatio);
#else
		GenerateLeftHandFoVPerspectiveMatrix(FoV, AspectRatio);	
#endif
		BuildCameraMatrices();		
	}

	void Camera::Initialize(const Vector3d& InPosition, const Vector3& InEuler, const Vector2& Extents, const Vector2& NearFar)
	{
		_cameraPosition = InPosition;
		_eulerAngles = InEuler;
		_FoV = -1.0f;

		SetupStandardCorrection();
		GenerateOrthogonalMatrix(Extents, NearFar);
		BuildCameraMatrices();
	}

	void Camera::BuildCameraMatrices()
	{
		Eigen::Quaternion<float> q = EulerAnglesToQuaternion(_eulerAngles);

		Matrix3x3 rotationMatrix = q.matrix();

		_cameraMatrix = Matrix4x4::Identity();
		_cameraMatrix.block<3, 3>(0, 0) = rotationMatrix;
		_invCameraMatrix = _cameraMatrix.inverse();
		//_cameraMatrix.block<1, 3>(3, 0) = Vector3(InPosition[0], InPosition[1], InPosition[2]);

		_viewProjMatrix = _invCameraMatrix * _correctionMatrix * _projectionMatrix;
		_invViewProjMatrix = _viewProjMatrix.inverse();
	}

	void Camera::CreateRays(uint32_t Width, uint32_t Height, const std::function<void(const Ray&, uint32_t, uint32_t)>& InRayTest)
	{		
		for (uint32_t yIter = 0; yIter < Height; yIter++)
		{
			for (uint32_t xIter = 0; xIter < Width; xIter++)
			{
				Vector4 posNear = Vector4(((xIter / (float)Width) * 2.0f - 1.0f), -((yIter / (float)Height) * 2.0f - 1.0f), 1, 1.0f);
				//Vector4 posFar = Vector4(((xIter / (float)Width) * 2.0f - 1.0f), -((yIter / (float)Height) * 2.0f - 1.0f), 1 - 0.001f, 1.0f);

				Vector4 worldNear = posNear * GetInvViewProjMatrix();
				worldNear /= worldNear[3];
				//Vector4 worldFar = posFar * GetInvViewProjMatrix();
				//worldFar /= worldFar[3];

				Vector3 rayStart = worldNear.head<3>();
				//Vector3 rayEnd = worldFar.head<3>();
				Vector3 rayDir = worldNear.head<3>().normalized();

				Ray curRay(rayStart.cast<double>() + _cameraPosition, rayDir);
				InRayTest(curRay, xIter, yIter);
			}
		}
	}

	void Camera::GenerateLeftHandFoVPerspectiveMatrix(float FoV, float AspectRatio)
	{
		_projectionMatrix = Matrix4x4::Identity();

		float xscale = 1.0f / (float)std::tan(DegToRad(FoV) * 0.5f);
		float yscale = xscale * AspectRatio;

		float fnDelta = FarClippingZ - NearClippingZ;
		_projectionMatrix(0, 0) = xscale; // scale the x coordinates of the projected point 
		_projectionMatrix(1, 1) = yscale; // scale the y coordinates of the projected point 
		_projectionMatrix(2, 2) = FarClippingZ / fnDelta; // used to remap z to [0,1] 
		_projectionMatrix(3, 2) = (-NearClippingZ * FarClippingZ) / fnDelta; // used to remap z [0,1] 
		_projectionMatrix(2, 3) = 1; // set w = z 
		_projectionMatrix(3, 3) = 0;		

		_invProjectionMatrix = _projectionMatrix.inverse();

		bIsInvertedZ = false;
	}

	void Camera::GenerateLHInverseZPerspectiveMatrix(float FoV, float AspectRatio)
	{
		float xscale = 1.0f / (float)std::tan(DegToRad(FoV) * 0.5f);
		float yscale = xscale * AspectRatio;

		_projectionMatrix = Matrix4x4{
			{ xscale, 0, 0, 0 },
			{ 0, yscale, 0, 0 },
			{ 0, 0, 0, 1.0f },
			{ 0, 0, NearClippingZ, 0 }
		};

		_invProjectionMatrix = _projectionMatrix.inverse();

		bIsInvertedZ = true;
		bIsOrthogonal = false;
	}


	void Camera::GenerateOrthogonalMatrix(const Vector2& InSize, const Vector2& InDepthRange)
	{
		_projectionMatrix = Matrix4x4::Identity();

		float fnDelta = InDepthRange[1] - InDepthRange[0];

		_projectionMatrix = Matrix4x4{
			{ 2.0f / InSize[0], 0,					0,								0 },
			{ 0,				2.0f / InSize[1],	0,								0 },
			{ 0,				0,					1 / fnDelta,					0 },
			{ 0,				0,					InDepthRange[0] / -fnDelta,		1.0f }
		};

		_invProjectionMatrix = _projectionMatrix.inverse();

		bIsInvertedZ = false;
		bIsOrthogonal = true;
	}

	float Camera::GetRecipTanHalfFovy() const
	{
		return _projectionMatrix(1, 1);
	}

	
	void Camera::SetupStandardCorrection()
	{		
		_correctionMatrix = Matrix4x4::Identity();
		//EX: to flip coordinates
		_correctionMatrix.block<1, 3>(0, 0) = Vector3(1, 0, 0);
		_correctionMatrix.block<1, 3>(1, 0) = Vector3(0, 1, 0);
		_correctionMatrix.block<1, 3>(2, 0) = Vector3(0, 0, 1);		
	}

	Vector3 Camera::CameraDirection()
	{
		return _cameraMatrix.block<1, 3>(0, 0);
	}

	void Camera::TurnCamera(const Vector2& CameraTurn)
	{
		_eulerAngles[1] += CameraTurn[0] * _turnSpeedModifier;
		_eulerAngles[0] += CameraTurn[1] * _turnSpeedModifier;

		if (CameraTurn[0] || CameraTurn[1])
		{
			BuildCameraMatrices();
		}
	}

	void Camera::SetCameraPosition(const Vector3d& InPosition)
	{
		_cameraPosition = InPosition;
	}

	Vector3 Camera::GetCameraMoveDelta(float DeltaTime, ERelativeDirection Direction)
	{
		Vector3 movementDir(0, 0, 0);
		switch (Direction)
		{
		case ERelativeDirection::Forward:
			movementDir = _cameraMatrix.block<1, 3>(2, 0);
			break;
		case ERelativeDirection::Back:
			movementDir = -_cameraMatrix.block<1, 3>(2, 0);
			break;
		case ERelativeDirection::Left:
			movementDir = -_cameraMatrix.block<1, 3>(0, 0);
			break;
		case ERelativeDirection::Right:
			movementDir = _cameraMatrix.block<1, 3>(0, 0);
			break;
		case ERelativeDirection::Up:
			movementDir = _cameraMatrix.block<1, 3>(1, 0);
			break;
		case ERelativeDirection::Down:
			movementDir = -_cameraMatrix.block<1, 3>(1, 0);
			break;
		}
		movementDir *= DeltaTime * _speed;
		return movementDir;
	}

	void Camera::MoveCamera(float DeltaTime, ERelativeDirection Direction)
	{
		auto moveDelta = GetCameraMoveDelta(DeltaTime, Direction);		
		_cameraPosition += Vector3d(moveDelta[0], moveDelta[1], moveDelta[2]);
	}

	CameraCullInfo Camera::GetCullingData()
	{
		float znear = 0.5f;
		Matrix4x4 tProj = _projectionMatrix.transpose();

		Plane planeX;
		Plane planeY;

		planeX.coeffs() = tProj.block<1, 4>(3, 0) + tProj.block<1, 4>(0, 0); // x + w < 0
		planeY.coeffs() = tProj.block<1, 4>(3, 0) + tProj.block<1, 4>(1, 0); // y + w < 0

		planeX.normalize();
		planeY.normalize();

		CameraCullInfo cullData = {};
		cullData.P00 = _projectionMatrix(0, 0);
		cullData.P11 = _projectionMatrix(1, 1);
		cullData.znear = NearClippingZ;
		cullData.zfar = FarClippingZ;
		cullData.frustum[0] = planeX.coeffs()[0];
		cullData.frustum[1] = planeX.coeffs()[2];
		cullData.frustum[2] = planeY.coeffs()[1];
		cullData.frustum[3] = planeY.coeffs()[2];
		return cullData;
	}

	void Camera::SphereProjectionTest(const Sphere& InSphere)
	{
		auto cullingData = GetCullingData();

		Vector4 sphereCamPosition = ToVector4((InSphere.GetCenter() - _cameraPosition).cast<float>());
		sphereCamPosition = sphereCamPosition * _invCameraMatrix;

#if 0
		bool visible = true;
		// the left/top/right/bottom plane culling utilizes frustum symmetry to cull against two planes at the same time
		visible = visible && center.z * cullData.frustum[1] - abs(center.x) * cullData.frustum[0] > -radius;
		visible = visible && center.z * cullData.frustum[3] - abs(center.y) * cullData.frustum[2] > -radius;
		// the near/far plane culling uses camera space Z directly
		visible = visible && center.z + radius > cullData.znear && center.z - radius < cullData.zfar;

		visible = visible || cullData.cullingEnabled == 0;

		if (visible && cullData.occlusionEnabled == 1)
		{
			vec4 aabb;
			if (projectSphere(center, radius, cullData.znear, cullData.P00, cullData.P11, aabb))
			{
				aabb = aabb * ViewScalar.xyxy;

				float width = (aabb.z - aabb.x) * cullData.pyramidWidth;
				float height = (aabb.w - aabb.y) * cullData.pyramidHeight;

				float level = floor(log2(max(width, height)));

				// Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
				float depth = textureLod(depthPyramid, (aabb.xy + aabb.zw) * 0.5, level).x;
				float depthSphere = cullData.znear / (center.z - radius);

				visible = visible && depthSphere > depth;
			}
		}
#endif
	}

	void Camera::GetFrustumSpheresForRanges(const std::vector<float>& DepthRanges, 
		std::vector<Sphere>& OutFrustumSpheres)
	{
		float ZToUse = bIsInvertedZ ? 1.0f : 0.0f;

		std::array< Vector4, 4 > nearPoints = {
			//near
			Vector4(-1, -1, ZToUse, 1),
			Vector4(-1, +1, ZToUse, 1),
			Vector4(+1, +1, ZToUse, 1),
			Vector4(+1, -1, ZToUse, 1)
		};
		
		std::array< Vector3, 4 > frustumDirections;
		for (int32_t Iter = 0; Iter < nearPoints.size(); Iter++)
		{
			// this does not have world location
			Vector4 currentCorner = (nearPoints[Iter] * _invProjectionMatrix);
			currentCorner /= currentCorner[3];
			frustumDirections[Iter] = ToVector3(currentCorner).normalized();
		}

		OutFrustumSpheres.resize(DepthRanges.size());
		std::array<Vector3d, 8> points;
		float LastDepth = NearClippingZ;
		for (int32_t DepthIter = 0; DepthIter < DepthRanges.size(); DepthIter++)
		{
			auto curDepth = DepthRanges[DepthIter];

			
			for (int32_t Iter = 0; Iter < frustumDirections.size(); Iter++)
			{
				points[Iter] = (frustumDirections[Iter] * LastDepth).cast<double>();
				points[Iter + 4] = (frustumDirections[Iter] * curDepth).cast<double>();
			}
			
			OutFrustumSpheres[DepthIter] = MinimumBoundingSphere< Vector3d, Vector3d >(points.data(), 8);
		}
	}

	void Camera::GetFrustumCorners(Vector3 OutFrustumCorners[8])
	{
		Vector4 viewSpacefrustumCorners[8] = {
			//near
			Vector4(-1, -1, 0, 1),
			Vector4(-1, +1, 0, 1),
			Vector4(+1, +1, 0, 1),
			Vector4(+1, -1, 0, 1),
			//far 
			Vector4(-1, -1, 1, 1),
			Vector4(-1, +1, 1, 1),
			Vector4(+1, +1, 1, 1),
			Vector4(+1, -1, 1, 1)
		};

		_viewProjMatrix = _cameraMatrix.inverse() * _correctionMatrix * _projectionMatrix;
		_invViewProjMatrix = _viewProjMatrix.inverse();

		for (int32_t Iter = 0; Iter < 8; Iter++)
		{
			Vector4 currentCorner = (viewSpacefrustumCorners[Iter] * _invViewProjMatrix);
			currentCorner /= currentCorner[3];
			
			OutFrustumCorners[Iter] = currentCorner.block<1, 3>(0, 0);
		}
	}

	void Camera::GetFrustumPlanes(std::vector<Planed> & planes) const
	{	
		size_t TotalPlanes = 4;
		
		if (!bIsOrthogonal)
		{
			TotalPlanes = bIsInvertedZ ? 5 : 6;
		}

		planes.resize(TotalPlanes);

		// left
		Eigen::Vector4d* coeff0 = &planes[0].coeffs();
		Eigen::Vector4d* coeff1 = &planes[1].coeffs();
		Eigen::Vector4d* coeff2 = &planes[2].coeffs();
		Eigen::Vector4d* coeff3 = &planes[3].coeffs();

		Eigen::Vector4d* coeff4 = nullptr;
		Eigen::Vector4d* coeff5 = nullptr;

		if (bIsOrthogonal == false)
		{
			coeff4 = &planes[4].coeffs();
			if (!bIsInvertedZ)
			{
				coeff5 = &planes[5].coeffs();
			}
		}		

		Matrix4x4d cameraMatrixWithTranslation = _cameraMatrix.cast<double>();
		cameraMatrixWithTranslation.block<1, 3>(3, 0) = Vector3d(_cameraPosition[0], _cameraPosition[1], _cameraPosition[2]);

		Matrix4x4d viewProjMatrixWithTranslation = cameraMatrixWithTranslation.inverse() * _correctionMatrix.cast<double>() * _projectionMatrix.cast<double>();

		viewProjMatrixWithTranslation.transposeInPlace();

		// left
		*coeff0 = viewProjMatrixWithTranslation.block<1, 4>(3, 0) + viewProjMatrixWithTranslation.block<1, 4>(0, 0);
		// right
		*coeff1 = viewProjMatrixWithTranslation.block<1, 4>(3, 0) - viewProjMatrixWithTranslation.block<1, 4>(0, 0);
		// bottom
		*coeff2 = viewProjMatrixWithTranslation.block<1, 4>(3, 0) + viewProjMatrixWithTranslation.block<1, 4>(1, 0);
		// top
		*coeff3 = viewProjMatrixWithTranslation.block<1, 4>(3, 0) - viewProjMatrixWithTranslation.block<1, 4>(1, 0);
		// near
		if (coeff4)
		{
			*coeff4 = viewProjMatrixWithTranslation.block<1, 4>(3, 0) + viewProjMatrixWithTranslation.block<1, 4>(2, 0);
		}
		// far
		if (coeff5)
		{
			*coeff5 = viewProjMatrixWithTranslation.block<1, 4>(3, 0) - viewProjMatrixWithTranslation.block<1, 4>(2, 0);
		}

		// normalize all planes
		for(auto &curPlane : planes)
		{
			curPlane.normalize();
		}		
	}

}