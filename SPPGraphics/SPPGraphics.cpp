// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphics.h"
#include "SPPPlatformCore.h"
#include "SPPLogging.h"
#include "ThreadPool.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	LogEntry LOG_GRAPHICS("GRAPHICS");

	std::unique_ptr<ThreadPool> GPUThreaPool;

	static std::thread::id GPUThread;

	void IntializeGraphicsThread()
	{
		SPP_LOG(LOG_GRAPHICS, LOG_INFO, "IntializeGraphics");
		GPUThreaPool = std::make_unique< ThreadPool >("GPUThread", 1);
		RunOnRTAndWait([]()
			{
				GPUThread = std::this_thread::get_id();
			});
	}

	void ShutdownGraphicsThread()
	{
		GPUThreaPool.reset();
	}

	bool IsOnGPUThread()
	{
		// make sure its an initialized one
		return (GPUThread == std::this_thread::get_id()) ||
			(GPUThread == std::thread::id());
	}

	GPU_CALL gpu_coroutine_promise::get_return_object() noexcept
	{
		return GPU_CALL(coro_handle::from_promise(*this));
	}

	GPU_CALL::GPU_CALL(coro_handle InHandle)
	{
		if (IsOnGPUThread())
		{
			InHandle.resume();
			SE_ASSERT(InHandle.done());
		}
		else
		{
			RunOnRT([InHandle]()
				{
					InHandle.resume();
					SE_ASSERT(InHandle.done());
				});
		}
	}

	GraphicsDevice::~GraphicsDevice()
	{
		SE_ASSERT(_resources.empty());
	}

	void GraphicsDevice::PushResource(GPUResource* InResource)
	{
		_resources.push_back(InResource);
	}
	void GraphicsDevice::PopResource(GPUResource* InResource)
	{
		_resources.remove(InResource);
	}

	void GraphicsDevice::PushRenderThreadResource(std::shared_ptr< RT_Resource > InResource)
	{
		SE_ASSERT(IsOnCPUThread());
		_renderThreadResources.push_back(InResource);
	}
}