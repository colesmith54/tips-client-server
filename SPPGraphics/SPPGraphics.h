// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPEngine.h"
#include "SPPArrayResource.h"
#include "SPPMath.h"
#include "SPPSTLUtils.h"
#include "ThreadPool.h"

#include <coroutine>
#include <vector>
#include <memory>
#include <condition_variable>
#include <future>

#if _WIN32 && !defined(SPP_GRAPHICS_STATIC)
	#ifdef SPP_GRAPHICSE_EXPORT
		#define SPP_GRAPHICS_API __declspec(dllexport)
	#else
		#define SPP_GRAPHICS_API __declspec(dllimport)
	#endif
#else
	#define SPP_GRAPHICS_API 
#endif


namespace SPP
{
    SPP_GRAPHICS_API extern std::unique_ptr<class ThreadPool> GPUThreaPool;

    SPP_GRAPHICS_API void IntializeGraphicsThread();
    SPP_GRAPHICS_API void ShutdownGraphicsThread();

    SPP_GRAPHICS_API bool IsOnGPUThread();

    template<class F, class... Args>
    auto RunOnRT(F&& f, Args&&... args)->std::future< typename std::invoke_result_t<F, Args...> >
    {
        SE_ASSERT(GPUThreaPool);
        return GPUThreaPool->enqueue(std::forward<F>(f), std::forward<Args>(args)...);
    }

    template<class F, class... Args>
    auto RunOnRTAndWait(F&& f, Args&&... args)->std::invoke_result_t<F, Args...> 
    {
        SE_ASSERT(GPUThreaPool);
        return GPUThreaPool->enqueue(std::forward<F>(f), std::forward<Args>(args)...).get();
    }
    
    class GPU_CALL;
    class gpu_coroutine_promise
    {
    public:
        gpu_coroutine_promise() {}

        using coro_handle = std::coroutine_handle<gpu_coroutine_promise>;

        auto initial_suspend() noexcept
        {
            return std::suspend_always{};
        }
        auto final_suspend() noexcept
        {
            return std::suspend_always{};
        }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
        void result() {};

        GPU_CALL get_return_object() noexcept;
    };

    inline uint32_t getGroupCount(uint32_t InValue, uint32_t threadCount)
    {
        return (InValue + threadCount - 1) / threadCount;
    }

    class GPU_CALL
    {
    public:
        using promise_type = gpu_coroutine_promise;
        using coro_handle = std::coroutine_handle<promise_type>;

        GPU_CALL(coro_handle InHandle);
        ~GPU_CALL() {}
    };

    struct PassCache
    {
        virtual ~PassCache() {}
    };

    enum class EShaderType
    {
        Pixel = 0,
        Vertex,
        Compute,
        Hull,
        Domain,
        Mesh,
        Amplification,
        NumValues,
    };

    enum class GPUBufferType
    {
        Simple,
        Array,
        Index,
        Vertex,
        Sparse
    };

    enum class TextureFormat
    {
        UNKNOWN,
        R8,

        RGB_888,
        RGBA_8888,
        BGRA_8888,
        RGBA_BC7,
        DDS_UNKNOWN,
        RG_BC5,
        GRAY_BC4,
        RGB_BC1,
        D24_S8,
        D32,
        D32_S8,
        S8,

        R16G16F,
        R16G16B16A16F,

        R32F,
        R32G32B32A32F,
        R32G32B32A32
    };

    enum class VertexInputTypes
    {
        StaticMesh = 0,
        SkeletalMesh,
        Particle,
        MAX
    };    

    class GraphicsDevice;

    template<typename T>
    class GPUReferencer;

    #define Make_RT_Resource(T,...) _Make_RT_Resource<T>( __LINE__, __FILE__, ##__VA_ARGS__); 
    #define CLASS_RT_RESOURCE() \
            template<typename T, typename ... Args> \
            friend std::shared_ptr<T> _Make_RT_Resource(int line, const char* file, Args&& ... args);

    class SPP_GRAPHICS_API RT_Resource : public inheritable_enable_shared_from_this< RT_Resource >
    {
        CLASS_RT_RESOURCE();

    protected:
        RT_Resource()
        {
            SE_ASSERT(IsOnCPUThread());
        }

    public:        
       
        virtual ~RT_Resource()
        {
            SE_ASSERT(IsMainActive());
            SE_ASSERT(IsOnGPUThread());
        }
    };      
  
    class SPP_GRAPHICS_API GlobalGraphicsResource
    {
    public:
        GlobalGraphicsResource();
        virtual ~GlobalGraphicsResource();
    };

    class SPP_GRAPHICS_API GraphicsDevice
    {
    protected:
        std::vector< class RT_RenderScene* > _renderScenes;

        virtual void INTERNAL_AddScene(class RT_RenderScene* InScene);
        virtual void INTERNAL_RemoveScene(class RT_RenderScene* InScene);

        std::array< std::unique_ptr< class GlobalGraphicsResource >, 30 > _globalResources;

        std::list< class GPUResource* > _resources;
        std::list< std::weak_ptr< RT_Resource > > _renderThreadResources;

        std::future<bool> _currentFrame;

        std::atomic_bool bFrameActive{ false };
              
    public:
        virtual ~GraphicsDevice();

        template<typename T>
        T* GetGlobalResource()
        {
            return (T*)(_globalResources[T::GetID()].get());
        }

        virtual void PushResource(class GPUResource* InResource);
        virtual void PopResource(class GPUResource* InResource);

        virtual void PushRenderThreadResource(std::shared_ptr< RT_Resource > InResource);

        virtual void Initialize(int32_t InitialWidth, int32_t InitialHeight, void* OSWindow) = 0;
        virtual void Shutdown() = 0;
        virtual void ResizeBuffers(int32_t NewWidth, int32_t NewHeight) = 0;

        virtual int32_t GetDeviceWidth() const = 0;
        virtual int32_t GetDeviceHeight() const = 0;

        virtual void DyingResource(class GPUResource* InResourceToKill) = 0;

        GPU_CALL AddScene(class RT_RenderScene* InScene);
        GPU_CALL RemoveScene(class RT_RenderScene* InScene);

        virtual void Flush() {}
        virtual void SyncGPUData();
        virtual void BeginFrame();
        virtual void Draw();
        virtual void EndFrame();
        virtual void MoveToNextFrame() { };

        void RunFrame();
               
        virtual GPUReferencer< class GPUShader > _gxCreateShader(EShaderType InType) = 0;
        virtual GPUReferencer< class GPUTexture > _gxCreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData = nullptr, std::shared_ptr< struct ImageMeta > InMetaInfo = nullptr) = 0;

        virtual GPUReferencer< class GPUTexture > _gxCreateTexture(const struct TextureAsset& TextureAsset) = 0;

        virtual GPUReferencer< class GPUBuffer > _gxCreateBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData = nullptr) = 0;
        virtual GPUReferencer< class GPUBuffer > _gxCreateBuffer(GPUBufferType InType, size_t InSize) = 0;

        //virtual GPUReferencer< GPUInputLayout > CreateInputLayout() = 0;
        //virtual GPUReferencer< GPUTexture > CreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData = nullptr, std::shared_ptr< ImageMeta > InMetaInfo = nullptr) = 0;
        //virtual GPUReferencer< GPURenderTarget > CreateRenderTarget(int32_t Width, int32_t Height, TextureFormat Format) = 0;
        //virtual std::shared_ptr< class RT_Material > CreateMaterial() = 0;

        virtual std::shared_ptr< class RT_Texture > CreateTexture() = 0;
        virtual std::shared_ptr< class RT_Shader > CreateShader() = 0;
        virtual std::shared_ptr< class RT_Buffer > CreateBuffer(GPUBufferType InType) = 0;

        virtual std::shared_ptr< class RT_Material > CreateMaterial() = 0;

        //virtual std::shared_ptr< class RT_ComputeDispatch > CreateComputeDispatch(GPUReferencer< GPUShader> InCS) = 0;
        virtual std::shared_ptr< class RT_RenderScene > CreateRenderScene() = 0;
        
        virtual std::shared_ptr< class RT_RenderableMesh > CreateRenderableMesh() = 0;
        virtual std::shared_ptr< class RT_StaticMesh > CreateStaticMesh() = 0;

        virtual std::shared_ptr< class RT_SunLight > CreateSunLight() = 0;
        virtual std::shared_ptr< class RT_PointLight > CreatePointLight() = 0;

        virtual std::shared_ptr< class RT_RenderableSVVO > CreateRenderableSVVO() = 0;

        //virtual std::shared_ptr< class RT_RenderableMesh > CreateSkinnedMesh() = 0;

        //virtual std::shared_ptr< class RT_Material> GetDefaultMaterial() = 0;

        virtual std::shared_ptr< class RT_RenderableSignedDistanceField > CreateSignedDistanceField() = 0;

        virtual void DrawDebugText(const Vector2i& InPosition, const char* Text, const Color3& InColor = Color3(255,255,255)) {}
    };

 


    struct IGraphicsInterface
    {
        virtual void CreateGraphicsDevice() = 0;
        virtual void DestroyraphicsDevice() = 0;

        virtual GraphicsDevice* GetGraphicsDevice() = 0;
    };

    // global graphics interface
    SPP_GRAPHICS_API IGraphicsInterface* GGI();
    SPP_GRAPHICS_API void SET_GGI(IGraphicsInterface* InGraphicsIterface);


    template<typename T, typename ... Args>
    std::shared_ptr<T> _Make_RT_Resource(int line, const char* file, Args&& ... args)
    {
        auto Sp = std::shared_ptr<T>(new T(args...));
        GGI()->GetGraphicsDevice()->PushRenderThreadResource(Sp);
        return Sp;
    }
}
