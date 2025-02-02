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

#include "SPPSDFO.h"
#include "SPPSceneO.h"
#include "SPPGraphicsO.h"

#include "SPPBlenderFile.h"

#include "SPPJsonSceneImporter.h"

#include <condition_variable>

#define MAX_LOADSTRING 100

using namespace SPP;
using namespace std::chrono_literals;


class SimpleViewer
{
	enum class ESelectionMode
	{
		None,
		Gizmo,
		Turn
	};

private:
	std::shared_ptr<GraphicsDevice> _graphicsDevice;

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
	std::shared_ptr<RT_RenderScene> renderableSceneShared;
	std::future<bool> graphicsResults;

public:
	
	void Initialize(HINSTANCE hInstance)
	{
		app = SPP::CreateApplication();
		app->Initialize(1280, 720, hInstance);

		_mainDXWindow = (HWND)app->GetOSWindow();

		_graphicsDevice = GGI()->CreateGraphicsDevice();
		_graphicsDevice->Initialize(1280, 720, app->GetOSWindow());
		
		/////////////SCENE SETUP

#if 1
		auto _renderableScene = LoadJsonScene(*AssetPath("scenes/walkingtest/testwalkable.spj"));
		_renderableScene->AddToGraphicsDevice(_graphicsDevice.get());

		renderableSceneShared = _renderableScene->GetRenderSceneShared();

		//auto& cam = renderableSceneShared->GetCamera();
		//cam.GetCameraPosition()[2] = -100;
#else

		auto meshtest = std::make_shared< Mesh>();
		meshtest->LoadMesh(*AssetPath("meshes/trianglesphere.obj"));

		auto meshvertexShader = _graphicsDevice->CreateShader();
		auto meshpixelShader = _graphicsDevice->CreateShader();

		auto meshMaterial = _graphicsDevice->CreateMaterial();

		meshMaterial->SetMaterialArgs({ .vertexShader = meshvertexShader, .pixelShader = meshpixelShader });

		auto gpuCommand = RunOnRT([meshvertexShader, meshpixelShader]()
			{
				meshvertexShader->Initialize(EShaderType::Vertex);
				meshvertexShader->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_vs");
				meshpixelShader->Initialize(EShaderType::Pixel);
				meshpixelShader->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_ps");
			});
		gpuCommand.wait();

		auto _renderableScene = AllocateObject<ORenderableScene>("rScene", nullptr);

		renderableSceneShared = _renderableScene->GetRenderSceneShared();

		auto loadedMesh = AllocateObject<OMesh>("simpleMesh", nullptr);
		loadedMesh->SetMesh(meshtest);

		auto meshMat = AllocateObject<OMaterial>("simplerMaterial", nullptr);
		meshMat->SetMaterial(meshMaterial);

		auto meshElement = AllocateObject<OMeshElement>("simpleMeshElement", nullptr);
		meshElement->SetMesh(loadedMesh);
		meshElement->SetMaterial(meshMat);

		auto& cam = renderableSceneShared->GetCamera();
		cam.GetCameraPosition()[2] = -20;

		auto startingGroup = AllocateObject<OShapeGroup>("ShapeGroup", nullptr);
		auto startingSphere = AllocateObject<OSDFSphere>("sphere", startingGroup);
		startingSphere->SetRadius(10);
		startingGroup->AddChild(startingSphere);
		_renderableScene->AddChild(startingGroup);
		_renderableScene->AddChild(meshElement);

		_graphicsDevice->AddScene(renderableSceneShared);
#endif

		//SPP::MakeResidentAllGPUResources();

		std::mutex tickMutex;
		std::condition_variable cv;

		auto LastTime = std::chrono::high_resolution_clock::now();
		float DeltaTime = 0.016f;

		auto ourAppEvents = ApplicationEvents{
			._msgLoop = [this]()
			{
				this->Update();
			} };
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
	}

	void Run()
	{
		app->Run();
	}

	void Update()
	{
		if (!_graphicsDevice) return;

		RECT rect;
		GetClientRect(_mainDXWindow, &rect);

		int32_t WindowSizeX = rect.right - rect.left;
		int32_t WindowSizeY = rect.bottom - rect.top;

		_graphicsDevice->ResizeBuffers(WindowSizeX, WindowSizeY);

		auto& cam = renderableSceneShared->GetCamera();

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
			_graphicsDevice->ResizeBuffers(Width, Height);
		}

		if (_selectionMode == ESelectionMode::Turn)
		{
			if (_keys[0x57])
				cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Forward);
			if (_keys[0x53])
				cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Back);

			if (_keys[0x41])
				cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Left);
			if (_keys[0x44])
				cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Right);
		}

		//
		this->_graphicsDevice->RunFrame();
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

	void MouseDown(int32_t mouseX, int32_t mouseY, uint8_t mouseButton)
	{
		//SPP_QL("md: %d %d %d", mouseX, mouseY, mouseButton);
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

			auto& cam = renderableSceneShared->GetCamera();
			cam.TurnCamera(Vector2(-Delta[0], Delta[1]));
		}		
	}

	void OnResize(int32_t InWidth, int32_t InHeight)
	{
		//_graphicsDevice->ResizeBuffers(InWidth, InHeight);
	}
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

#ifdef _DEBUG
	LoadLibraryA("SPPVulkand.dll");
#else
	LoadLibraryA("SPPVulkan.dll");
#endif

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
	SPP::GRootPath = stdfs::absolute(stdfs::current_path() / "..\\").generic_string();
	SPP::GBinaryPath = SPP::GRootPath + "Binaries\\";
	SPP::GAssetPath = SPP::GRootPath + "Assets\\";

	//SPP::CallPython();
	int ErrorCode = 0;		

	{
		SimpleViewer viewer;
		viewer.Initialize(hInstance);
		viewer.Run();
	}

	return ErrorCode;
}


