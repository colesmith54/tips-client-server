// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// 
// Windows Header Files
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <memory>
#include <thread>
#include <optional>

#include "SPPString.h"
#include "SPPEngine.h"
#include "SPPApplication.h"
#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPTextures.h"
#include "SPPMesh.h"
#include "SPPLogging.h"
#include "SPPSceneRendering.h"
#include "SPPOctree.h"
#include "SPPTerrain.h"
#include "SPPPythonInterface.h"
#include "ThreadPool.h"
#include "SPPFileSystem.h"
#include "SPPPlatformCore.h"

#include "SPPSceneO.h"
#include "SPPGraphicsO.h"
#include "SPPPhysX.h"

#include "SPPSDFO.h"

#include "SPPBlenderFile.h"

#include "SPPJsonEnvironmentImporter.h"
#include "SPPHandledTimers.h"

#include "SPPBitSetArray.h"
#include "SPPLoadMagicaCSG.h"

#include <condition_variable>

#include "SPPGarbageCollection.h"
#include "SPPAnimation.h"

#include "SPPSparseVirtualizedVoxelOctree.h"
#include "SPPTiming.h"

#include "FastNoiseLite/FastNoiseLite.h"

#define MAX_LOADSTRING 100

using namespace SPP;
using namespace std::chrono_literals;


LogEntry LOG_APP("APP");

class SimpleViewer
{
	enum class ESelectionMode
	{
		None,
		Gizmo,
		Turn
	};

private:
	STDElapsedTimer _timer;
	HWND _mainDXWindow = nullptr;
	Vector2 _mouseDelta = Vector2(0, 0);
	uint8_t _keys[255] = { 0 };

	Vector2i _mousePosition = { -1, -1 };
	Vector2i _mouseCaptureSpot = { -1, -1 };
	std::chrono::high_resolution_clock::time_point _lastTime;
	//std::shared_ptr< SPP::MeshMaterial > _gizmoMat;	

	bool _htmlReady = false;

	ESelectionMode _selectionMode = ESelectionMode::None;
	std::unique_ptr<SPP::ApplicationWindow> app;
	RT_RenderScene* renderableSceneShared = nullptr;
	std::future<bool> graphicsResults;

	VgCapsuleElement* _charCapsule = nullptr;
	VgEnvironment* _gameworld = nullptr;
	std::shared_ptr< PhysicsCharacter > _characterCapsule;

	std::vector<OShapeGroup*> _startingGroups;

public:
	
	void Initialize(HINSTANCE hInstance)
	{
		app = SPP::CreateApplication();
		app->Initialize(1280, 720, hInstance);

#if 0

		SparseVirtualizedVoxelOctree testTree(Vector3d(0, 0, 0), Vector3(50, 10, 50), 0.05f, 65536);

		testTree.BeginWrite();

		auto curDimensions = testTree.GetDimensions();

		// Create and configure FastNoise object
		FastNoiseLite noise;
		noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);

		for (int32_t IterZ = 200; IterZ < curDimensions[2] - 200; IterZ++)
		{
			for (int32_t IterX = 200; IterX < curDimensions[0] - 200; IterX++)
			{
				auto noiseValue = noise.GetNoise((float)IterX, (float)IterZ) * 0.5f + 0.5f;

				Vector3i SetPos(IterX, (noiseValue * 40) + (curDimensions[1] / 2 - 40), IterZ);

				testTree.Set(SetPos, 200);
			}
		}

		//testTree.Set(Vector3i{ 512, 512,2 }, 2);
		//testTree.SetBox(Vector3d(0, 0, 0), Vector3(2, 2, 0.1f), 200);
		//testTree.SetSphere(Vector3d(3, 0, 0), 3, 200);

		//testTree.SetSphere(Vector3d(0, 3, 0), 3, 200);
		//testTree.SetDisk(Vector3d(0, 0, 0), 3, 200);

		//testTree.EndWrite([](uint8_t InLevel, uint32_t InPage, const void *InMem) {
		//	SPP_LOG(LOG_APP, LOG_INFO, "update SVVO %u:%u", InLevel, InPage);
		//});

		//for(int32_t Iter = 0; Iter < 1000; Iter++)
		{
			SparseVirtualizedVoxelOctree::VoxelHitInfo hit;
			Ray createRay(Vector3d(1, 10, 1), (Vector3(0, 0, 0) - Vector3(1, 10, 1)).normalized());
			testTree.CastRay(createRay, hit);

			SPP_LOG(LOG_APP, LOG_INFO, "single test: %u", hit.totalChecks);
		}

		Camera testCam;
		testCam.Initialize(Vector3d(0, 9, 0), Vector3(20, 0, 0), 65.0f, 1.77f);

		{
			uint64_t totalTests = 0;
			uint32_t wRay = 1365;
			uint32_t hRay = 768;
			std::vector<Color3> sliceData;
			sliceData.resize(wRay * hRay, Color3(0,0,0) );

			{
				//ScopeTimer rayCastTime;
				testCam.CreateRays(wRay, hRay, [&](const Ray& InRay, uint32_t CurX, uint32_t CurY) {
					SparseVirtualizedVoxelOctree::VoxelHitInfo hit;
					if (testTree.CastRay(InRay, hit))
					{
						sliceData[(CurY * wRay) + CurX] = ((hit.normal * 0.5f + Vector3(1, 1, 1)) * 255.0f).cast< uint8_t >();
					}
					totalTests += hit.totalChecks;
				});
			}

			SPP_LOG(LOG_APP, LOG_INFO, "totalTests: %u avg test per pixel: %f", totalTests, (double)totalTests / ((double)wRay * (double)hRay)  );
			SaveImageToFile("raytest.bmp", wRay, hRay, TextureFormat::RGB_888, (uint8_t*)sliceData.data());
		}
		
		for(int32_t Iter = 0; Iter < testTree.GetLevelCount(); Iter++)
		{
			int32_t imgX, imgY;
			std::vector<Color3> sliceData;
			testTree.GetSlice(Vector3d(0, 0, 0), EAxis::Y, Iter, imgX, imgY, sliceData);
			std::string SaveString = std::string_format("sphereslice_%d.bmp", Iter);
			SaveImageToFile(SaveString.c_str(), imgX, imgY, TextureFormat::RGB_888, (uint8_t*)sliceData.data());
		}

#endif
		_mainDXWindow = (HWND)app->GetOSWindow();

		GGI()->CreateGraphicsDevice();
		auto graphicsDevice = GGI()->GetGraphicsDevice();
		graphicsDevice->Initialize(1280, 720, app->GetOSWindow());

		//auto SDFShader = _graphicsDevice->CreateShader();
		//auto sparseBuf = _graphicsDevice->CreateBuffer(GPUBufferType::Sparse);

		//auto gpuCommand = RunOnRT([&]()
		//	{
		//		sparseBuf->Initialize(testTree.GetLevelMaxSize(0));

		//		//sparseBuf->SetSparsePageMem();
		//		std::vector<BufferPageData> bufferData;
		//		testTree.EndWrite([&](uint8_t InLevel, uint32_t InPage, const void* InMem) {
		//			if (InLevel == 0)
		//			{
		//				bufferData.push_back({ InMem, InPage });
		//				SPP_LOG(LOG_APP, LOG_INFO, "update SVVO %u:%u", InLevel, InPage);
		//			}
		//			});

		//		auto gpuBuf = sparseBuf->GetGPUBuffer();
		//		gpuBuf->SetSparsePageMem(bufferData.data(), bufferData.size());

		//		//SDFShader->CompileShaderFromFile("shaders/SignedDistanceFieldCompute.hlsl", "main_cs");
		//	});
		//gpuCommand.wait();

		/////////////SCENE SETUP

		InitializePhysX();
		InitializeAnimation();
		
		//auto value = LoadSkeleton(*AssetPath("meshes/simplehumanrigged_mocap.skj"));
		//auto value2 = LoadAnimations(*AssetPath("meshes/testhumanoid.anj"), value);

		//auto animatorTest = AllocateObject<OAnimator>("testanim", nullptr);

		//animatorTest->SetSkeleton(value);
		//animatorTest->AddAnimation(value2);

		//animatorTest->PlayAnimation("Action");

		//_gameworld = LoadJsonGameScene(*AssetPath("scenes/visibilityTest/visibilitytest.spj"));
		//_gameworld = LoadJsonGameScene(*AssetPath("scenes/fullcity/fullcity.spj"));
		_gameworld = LoadJsonGameScene(*AssetPath("scenes/voxelTest/voxelTest.spj"));
		AddToRoot(_gameworld);
				
		auto topChildren = _gameworld->GetChildren();
		Sphere totalBounds(Vector3d(0, 0, 0), 300);

		// get a COPY of htem;
		//for (auto& curShild : topChildren)
		//{
		//	totalBounds += curShild->Bounds();
		//}				
#if 0
		auto MeshType = rttr::type::get<VgMeshElement>();

		for (int32_t IterY = -3; IterY <= 3; IterY++)
		{			
			for (int32_t IterX = -3; IterX <= 3; IterX++)
			{
				if (!IterX && !IterY)continue;

				Vector3d PositionOffset(totalBounds.GetRadius() * IterX, 0, totalBounds.GetRadius() * IterY);

				//topChildren is a copy
				for (auto& curShild : topChildren)
				{
					auto curChildType = curShild->get_type();

					if (MeshType == curChildType)
					{
						auto curMesh = rttr::rttr_cast<VgMeshElement*>(curShild);

						auto newName = std::string_format("%s_%d_%d", curMesh->GetName(), IterY, IterX);

						auto meshElement = AllocateObject<VgMeshElement>(newName.c_str(), _gameworld);

						meshElement->GetRotation() = curMesh->GetRotation();
						meshElement->GetPosition() = curMesh->GetPosition() + PositionOffset;
						meshElement->GetScale() = curMesh->GetScale();

						meshElement->SetMesh(curMesh->GetMesh());
						meshElement->SetMaterial(curMesh->GetMaterial());						

						_gameworld->AddChild(meshElement);

					}
				}
			}
		}
#endif

#if 0
		auto loadedElements = LoadMagicaCSGFile(*AssetPath("MagicaCSGFiles/testhumanoid.mcsg"));

		int32_t shapeGCnt = 0;
		int32_t simpleCnt = 0;
		for (int32_t IterY = 0; IterY <= 0; IterY++)
		{
			for (int32_t IterX = 0; IterX <= 0; IterX++)
			{
				auto& topLayer = loadedElements.front();

				std::string shapeGroupName = std::string_format("shapeG_%s_%d", topLayer.Name.c_str(), shapeGCnt++);
				auto startingGroup = AllocateObject<OShapeGroup>(shapeGroupName.c_str(), _gameworld);
				startingGroup->GetPosition()[1] = 1;

				startingGroup->GetPosition()[0] = (float)IterX * 1.2f;
				startingGroup->GetPosition()[2] = (float)IterY * 1.2f;

				startingGroup->GetRotation()[0] = 90;
				startingGroup->GetRotation()[1] = -90;

				startingGroup->GetScale() = Vector3(1.0f / 50.0f, 1.0f / 50.0f, 1.0f / 50.0f);

				for (auto& curShape : topLayer.Shapes)
				{
					OShape* newShape = nullptr;
					std::string shapeName = std::string_format("shape_%s_%d", curShape.Name.c_str(), simpleCnt++);
					newShape = AllocateObject<OShape>(shapeName.c_str(), startingGroup);

					newShape->SetTransformArgs(
						{
							.translation = curShape.Translation.cast<double>(),
							.rotation = ToEulerAngles(curShape.Rotation) * 57.2958f,
							.scale = curShape.Scale
						});

					EShapeType shapeType = EShapeType::Sphere;
					EShapeOp shapeOp = EShapeOp::Add;

					switch (curShape.Type)
					{
					case EMagicaCSG_ShapeType::Cube:
						shapeType = EShapeType::Box;
						break;
					case EMagicaCSG_ShapeType::Cylinder:
						shapeType = EShapeType::Cylinder;
						break;
					}

					switch (curShape.Mode)
					{
					case EMagicaCSG_ShapeOP::Subtract:
						shapeOp = EShapeOp::Subtract;
						break;
					case EMagicaCSG_ShapeOP::Intersect:
						shapeOp = EShapeOp::Intersect;
						break;
					}

					newShape->SetShapeArgs(
						{
							.shapeType = shapeType,
							.shapeOp = shapeOp,
							.shapeBlendFactor = curShape.Blend / 50.0f,
							.shapeColor = curShape.Color
						}
					);

					startingGroup->AddChild(newShape);
				}

				_gameworld->AddChild(startingGroup);
				_startingGroups.push_back(startingGroup);
			}
		}
#endif
		//_gameworld->AddToGraphicsDevice(graphicsDevice);

		auto GameObjectSVVO = AllocateObject<VgSVVO>("svvo", _gameworld);
		GameObjectSVVO->GetScale() = Vector3(100, 20, 100);
		GameObjectSVVO->SetVoxelSize(0.1f);
		//GameObjectSVVO->SetVoxelSize(1);

		//GameObjectSVVO->SetVoxelSize(0.05f);

		// figure out a better model
		_gameworld->AddChild(GameObjectSVVO);

		if (true)
		{
			auto directSVVO = GameObjectSVVO->GetSVVO();
			auto curDimensions = directSVVO->GetDimensions();

			uint32_t imgW, imgH;
			TextureFormat imgF;
			std::vector<uint8_t> imgData;
			if (LoadImageFromFile(*AssetPath("textures/maptest.png"), imgW, imgH, imgF, imgData))
			{
				SimpleRGBA* colorData = (SimpleRGBA*)imgData.data();

				for (int32_t IterZ = 0; IterZ < curDimensions[2]; IterZ++)
				{
					for (int32_t IterX = 0; IterX < curDimensions[0]; IterX++)
					{
						SimpleRGBA* currentcolor = colorData + IterX + IterZ * curDimensions[0];

						int32_t startY = currentcolor->R + 50;
						for (int32_t IterY = startY, yCnt = 0; yCnt < 20; IterY--, yCnt++)
						{
							Vector3i SetPos(IterX, IterY, IterZ);
							directSVVO->Set(SetPos, 200);
						}
					}
				}
			}
		}

		if(false)
		{
			auto directSVVO = GameObjectSVVO->GetSVVO();
			auto curDimensions = directSVVO->GetDimensions();

			// Create and configure FastNoise object
			FastNoiseLite noise;
			noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);

			for (int32_t IterZ = 0; IterZ < curDimensions[2]; IterZ++)
			{
				for (int32_t IterX = 0; IterX < curDimensions[0]; IterX++)
				{
					auto noiseValue = noise.GetNoise((float)IterX, (float)IterZ) * 0.5f + 0.5f;

					int32_t startY = noiseValue * 128 + curDimensions[1] / 2 - 30;
					for (int32_t IterY = startY, yCnt = 0; yCnt < 20; IterY--, yCnt++)
					{
						Vector3i SetPos(IterX, IterY, IterZ);
						directSVVO->Set(SetPos, 200);
					}

				}
			}

#if 0
			Camera testCam;
			testCam.Initialize(Vector3d(0, 9, 0), Vector3(-20, 0, 0), 65.0f, 1.77f);

			{
				uint64_t totalTests = 0;
				uint32_t wRay = 1365;
				uint32_t hRay = 768;
				std::vector<Color3> sliceData;
				sliceData.resize(wRay * hRay, Color3(0, 0, 0));

				{
					//ScopeTimer rayCastTime;
					testCam.CreateRays(wRay, hRay, [&](const Ray& InRay, uint32_t CurX, uint32_t CurY) {
						SparseVirtualizedVoxelOctree::VoxelHitInfo hit;
						if (directSVVO->CastRay(InRay, hit))
						{
							sliceData[(CurY * wRay) + CurX] = ((hit.normal * 0.5f + Vector3(1, 1, 1)) * 255.0f).cast< uint8_t >();
						}
						totalTests += hit.totalChecks;
					});
				}

				SPP_LOG(LOG_APP, LOG_INFO, "totalTests: %u avg test per pixel: %f", totalTests, (double)totalTests / ((double)wRay * (double)hRay));
				SaveImageToFile("raytest.bmp", wRay, hRay, TextureFormat::RGB_888, (uint8_t*)sliceData.data());
			}
#endif
		}

		GameObjectSVVO->FullRTUpdate();

		_gameworld->GetOctree()->Report();

		
		_charCapsule = AllocateObject<VgCapsuleElement>("currentCapsule", nullptr);
		auto& curPos = _charCapsule->GetPosition();
		curPos[1] = 5.0;
		
		_gameworld->AddChild(_charCapsule);

		renderableSceneShared = _gameworld->GetRenderScene();
		
		auto& cam = renderableSceneShared->GetCPUCamera();
		cam.GetCameraPosition()[1] = 9;

		std::vector<Sphere> rangeSpheres;
		cam.GetFrustumSpheresForRanges( { 50, 150, 450 }, rangeSpheres);

		//SPP::MakeResidentAllGPUResources();

		std::mutex tickMutex;
		std::condition_variable cv;

		auto LastTime = std::chrono::high_resolution_clock::now();
		float DeltaTime = 0.016f;

		auto ourAppEvents = ApplicationEvents{
			._msgLoop = [this]()
			{
				this->Update();
			},
			._windowClosed = [this]()
			{
				this->ShutDown();
			}
		};
		app->SetEvents(ourAppEvents);
		auto ourInputEvents = InputEvents{
			.keyDown = [this](uint8_t KeyValue) 
			{
				_keys[KeyValue] = true;
			},
			.keyUp = [this](uint8_t KeyValue) 
			{
				_keys[KeyValue] = false;
			},
			.mouseDown = [this](int32_t mouseX, int32_t mouseY, uint8_t mouseButton)
			{
				this->MouseDown(mouseX, mouseY, mouseButton);
			},
			.mouseUp = [this](int32_t mouseX, int32_t mouseY, uint8_t mouseButton)
			{
				this->MouseUp(mouseX, mouseY, mouseButton);
			},
			.mouseMove = [this](int32_t mouseX, int32_t mouseY, uint8_t mouseButton)
			{
				this->MouseMove(mouseX, mouseY, mouseButton);
			}
		};
		app->SetInputEvents(ourInputEvents);

		_lastTime = std::chrono::high_resolution_clock::now();

		GC_MarkAndSweep();
	}

	void Run()
	{
		app->Run();
	}

	void ShutDown()
	{
		RemoveFromRoot(_gameworld);

		IterateObjects([](SPPObject* InObj) -> bool
			{
				// not visible to root
				SPP_LOG(LOG_APP, LOG_INFO, "exists prior to shutdown %s", InObj->GetName());
				return true;
			});

		GC_MarkAndSweep();

		IterateObjects([](SPPObject* InObj) -> bool
			{
				// not visible to root
				SPP_LOG(LOG_APP, LOG_INFO, "still remains object %s", InObj->GetName());
				return true;
			});

		auto graphicsDevice = GGI()->GetGraphicsDevice();
		graphicsDevice->Flush();
		graphicsDevice->Shutdown();
		GGI()->DestroyraphicsDevice();

		app.reset();
	}

	void Update()
	{
		auto graphicsDevice = GGI()->GetGraphicsDevice();
		if (!graphicsDevice) return;

		auto currentElapsedMS = _timer.getElapsedMilliseconds();

		
		if (currentElapsedMS < 16.666f)
		{
			std::this_thread::sleep_for( std::chrono::duration<double, std::milli>(16.666f - currentElapsedMS) );
			currentElapsedMS += _timer.getElapsedMilliseconds();
		}
	
		auto currentElapsed = currentElapsedMS / 1000.0f;
		_gameworld->Update(currentElapsed);			

		RECT rect;
		GetClientRect(_mainDXWindow, &rect);

		int32_t WindowSizeX = rect.right - rect.left;
		int32_t WindowSizeY = rect.bottom - rect.top;

		graphicsDevice->ResizeBuffers(WindowSizeX, WindowSizeY);

		auto& cam = renderableSceneShared->GetCPUCamera();

		cam.GenerateLHInverseZPerspectiveMatrix(75.0f, (float)WindowSizeX / (float)WindowSizeY);

		auto CurrentTime = std::chrono::high_resolution_clock::now();
		auto secondTime = std::chrono::duration_cast<std::chrono::microseconds>(CurrentTime - _lastTime).count();
		_lastTime = CurrentTime;

		auto DeltaTime = (float)secondTime * 1.0e-6f;

		if (_mainDXWindow)
		{
			RECT rect;
			GetClientRect(_mainDXWindow, &rect);

			auto Width = rect.right - rect.left;
			auto Height = rect.bottom - rect.top;

			// will be ignored if same
			graphicsDevice->ResizeBuffers(Width, Height);
		}
		
		if (_selectionMode == ESelectionMode::Turn)
		{
			Vector3 cameraMoveDelta(0, 0, 0);

			if (_keys[0x57])
				cameraMoveDelta += cam.GetCameraMoveDelta(DeltaTime, SPP::ERelativeDirection::Forward);
			if (_keys[0x53])
				cameraMoveDelta += cam.GetCameraMoveDelta(DeltaTime, SPP::ERelativeDirection::Back);

			if (_keys[0x41])
				cameraMoveDelta += cam.GetCameraMoveDelta(DeltaTime, SPP::ERelativeDirection::Left);
			if (_keys[0x44])
				cameraMoveDelta += cam.GetCameraMoveDelta(DeltaTime, SPP::ERelativeDirection::Right);

			if (!cameraMoveDelta.isApprox(Vector3(0, 0, 0)))
			{
				Vector3d camMove(cameraMoveDelta[0], cameraMoveDelta[1], cameraMoveDelta[2]);
				_charCapsule->Move(camMove, DeltaTime);
				cam.SetCameraPosition(_charCapsule->GetPosition());
			}
		}

		//
		graphicsDevice->RunFrame();
	}

	void KeyDown(uint8_t KeyValue)
	{
		//SPP_QL("kd: 0x%X", KeyValue);
		_keys[KeyValue] = true;
	}

	void KeyUp(uint8_t KeyValue)
	{
		//SPP_QL("ku: 0x%X", KeyValue);
		_keys[KeyValue] = false;

	}

	void MouseClick()
	{
		auto graphicsDevice = GGI()->GetGraphicsDevice();
		if (!graphicsDevice || _startingGroups.empty()) return;

		auto WindowSizeX = graphicsDevice->GetDeviceWidth();
		auto WindowSizeY = graphicsDevice->GetDeviceHeight();

		auto& cam = renderableSceneShared->GetCPUCamera();

		Vector4 MousePosNear = Vector4(
			((_mousePosition[0] / (float)WindowSizeX) * 2.0f - 1.0f), 
			((_mousePosition[1] / (float)WindowSizeY) * 2.0f - 1.0f), 0.0f, 1.0f);
		Vector4 MousePosFar = Vector4(MousePosNear[0], MousePosNear[1], 1.0f, 1.0f);

		Vector4 MouseLocalNear = MousePosNear * cam.GetInvViewProjMatrix();
		MouseLocalNear /= MouseLocalNear[3];
		Vector4 MouseLocalFar = MousePosFar * cam.GetInvViewProjMatrix();
		MouseLocalFar /= MouseLocalFar[3];

		Vector3 MouseStart = Vector3(MouseLocalNear[0], MouseLocalNear[1], MouseLocalNear[2]);
		Vector3 MouseEnd = Vector3(MouseLocalFar[0], MouseLocalFar[1], MouseLocalFar[2]);
		Vector3 MouseRay = (MouseEnd - MouseStart).normalized();

		Ray ray(MouseStart.cast<double>() + cam.GetCameraPosition(), MouseRay);

		for (auto& _startingGroup : _startingGroups)
		{
			IntersectionInfo hitInfo;
			if (_startingGroup->Intersect_Ray(ray, hitInfo))
			{
				// hit something
				SPP_QL("hit something");

				//renderableSceneShared->AddDebugLine(MouseStart.cast<double>() + cam.GetCameraPosition(),
				//	hitInfo.location, Vector3(1,0,0));

				static uint32_t simpleCnt = 44;
				OShape* newShape = nullptr;
				std::string shapeName = std::string_format("shape_%s_%d", "shoot", simpleCnt++);
				newShape = AllocateObject<OShape>(shapeName.c_str(), _startingGroup);

				auto WorldToLocal = _startingGroup->GenerateLocalToWorld(true).inverse();

				Vector3d localTransform = ToVector3(ToVector4((hitInfo.location - _startingGroup->GetPosition()).cast<float>()) * WorldToLocal).cast<double>();

				newShape->SetTransformArgs(
					{
						.translation = localTransform,
						.rotation = Vector3(1,1,1),
						.scale = Vector3(3,3,3)
					});

				EShapeType shapeType = EShapeType::Sphere;
				EShapeOp shapeOp = EShapeOp::Subtract;

				newShape->SetShapeArgs(
					{
						.shapeType = shapeType,
						.shapeOp = shapeOp,
						.shapeBlendFactor = 0,
						.shapeColor = Color3(255,0,0)
					}
				);
				_startingGroup->AddChild(newShape);

				_gameworld->RemoveChild(_startingGroup);
				_gameworld->AddChild(_startingGroup);
			}
			else
			{
				//renderableSceneShared->AddDebugLine(MouseStart.cast<double>() + cam.GetCameraPosition(),
				//	MouseEnd.cast<double>() + cam.GetCameraPosition());
			}
		}
	}

	void MouseDown(int32_t mouseX, int32_t mouseY, uint8_t mouseButton)
	{
		//SPP_QL("md: %d %d %d", mouseX, mouseY, mouseButton);
		if (mouseButton == 0)
		{
			MouseClick();
		}

		if (mouseButton == 2)
		{
			CaptureWindow(_mainDXWindow);
			_selectionMode = ESelectionMode::Turn;
			_mouseCaptureSpot = Vector2i(mouseX, mouseY);
		}
	}

	void MouseUp(int32_t mouseX, int32_t mouseY, uint8_t mouseButton)
	{
		//SPP_QL("mu: %d %d %d", mouseX, mouseY, mouseButton);
		if (_selectionMode != ESelectionMode::None)
		{
			_selectionMode = ESelectionMode::None;
			CaptureWindow(nullptr);
		}
	}

	void MouseMove(int32_t mouseX, int32_t mouseY, uint8_t MouseState)
	{
		Vector2i currentMouse = { mouseX, mouseY };
		//SPP_QL("mm: %d %d", mouseX, mouseY);
		_mousePosition = currentMouse;

		if (_selectionMode == ESelectionMode::Turn)
		{
			Vector2i Delta = (currentMouse - _mouseCaptureSpot);
			_mouseCaptureSpot = _mousePosition;

			auto& cam = renderableSceneShared->GetCPUCamera();
			cam.TurnCamera(Vector2(-Delta[0], -Delta[1]));
		}		
	}

	void OnResize(int32_t InWidth, int32_t InHeight)
	{
		//graphicsDevice->ResizeBuffers(InWidth, InHeight);
	}
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	MainWatch watchMain;
	UNREFERENCED_PARAMETER(hPrevInstance);

#ifdef _DEBUG
	LoadLibraryA("SPPVulkand.dll");
#else
	LoadLibraryA("SPPVulkan.dll");
#endif

/*
	BitSetArray newBitSet(128);

	{
		auto firstBit = newBitSet.GetFirstFree();
		auto secondBit = newBitSet.GetFirstFree();
	}
	
	auto thirdBit = newBitSet.GetFirstFree();
	
	struct TestData
	{
		float values[3];
	};
	std::vector<TestData> data;
	data.resize(128);
	data[0].values[0] = 10.0f;
	LeaseManager bufferArray(data);

	_declspec(align(256u)) struct GPUDrawConstants
	{
		//altered viewposition translated
		Matrix4x4 LocalToWorldScaleRotation;
		Vector3d Translation;
		uint32_t MaterialID;
	};


	auto newResource = std::make_shared< ArrayResource >();
	auto drawData = newResource->InitializeFromType< GPUDrawConstants >(5000);

	LeaseManager bigDrawer(drawData);

	{
		auto newLease = bigDrawer.GetLease();

	}
*/

	//Alloc Console
	//print some stuff to the console
	//make sure to include #include "stdio.h"
	//note, you must use the #include <iostream>/ using namespace std
	//to use the iostream... #incldue "iostream.h" didn't seem to work
	//in my VC 6
	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
	printf("Debugging Window:\n");

	IntializeCore(std::wstring_to_utf8(lpCmdLine).c_str());
	IntializeGraphicsThread();

	// setup global asset path
	SPP::GRootPath = stdfs::absolute(stdfs::current_path() / "../").generic_string();
	SPP::GBinaryPath = SPP::GRootPath + "Binaries/";
	SPP::GAssetPath = SPP::GRootPath + "Assets/";



	//
	{
		TextureAsset test;
		test.LoadFromDisk(*AssetPath("textures/SkyTextureOverCast_Cubemap.ktx2"));
	}

	//SPP::CallPython();
	int ErrorCode = 0;		

	{
		SimpleViewer viewer;
		viewer.Initialize(hInstance);
		viewer.Run();
	}

	return ErrorCode;
}


