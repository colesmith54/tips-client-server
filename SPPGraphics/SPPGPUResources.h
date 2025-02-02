// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPReferenceCounter.h"
#include "SPPArrayResource.h"
#include "SPPObject.h"
#include "SPPGraphics.h"
#include "SPPMath.h"
#include "SPPFileSystem.h"
//#include "SPPSceneRendering.h"
#include <vector>
#include <memory>
#include <string>

namespace DirectX
{
    struct DDS_HEADER;
}

namespace SPP
{
    

    enum class EDrawingTopology
    {
        PointList = 0,
        LineList,
        TriangleList,
        TriangleStrip,
        PatchList_4ControlPoints,

        NumValues
    };

    enum class EBlendState
    {
        Disabled = 0,
        Additive,
        AlphaBlend,
        PreMultiplied,
        NoColorWrites,
        PreMultipliedRGB,

        NumValues
    };

    enum class ERasterizerState 
    {
        NoCull = 0,
        BackFaceCull,
        BackFaceCullNoZClip,
        FrontFaceCull,
        NoCullNoMS,
        Wireframe,

        NumValues
    };

    enum class EDepthState 
    {
        Disabled = 0,
        Enabled,
        Reversed,
        WritesEnabled,
        ReversedWritesEnabled,
        Enabled_NoWrites,

        NumValues
    };

    enum class EDepthOp
    {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum class ESamplerState
    {
        Linear = 0,
        LinearClamp,
        LinearBorder,
        Point,
        Anisotropic,
        ShadowMap,
        ShadowMapPCF,
        ReversedShadowMap,
        ReversedShadowMapPCF,

        NumValues
    };

    enum class ECmdListMode
    {
        Graphics = 0,
        Copy,
        Compute,
        NumValues
    };

    struct ImageMeta
    {
        ImageMeta() = default;
        virtual ~ImageMeta() {}
    };
    
    
    struct DDSImageMeta : public ImageMeta
    {
        const DirectX::DDS_HEADER* header = nullptr;
        const uint8_t* bitData = nullptr;
        size_t bitSize = 0;

        DDSImageMeta() = default;
        virtual ~DDSImageMeta() {}
    };

    class GPUResource;
    
    template<typename T>
    class GPUReferencer : public Referencer<T>
    {
    protected:
        virtual void DestroyObject() override
        {
            Referencer<T>::DestroyObject();
        }

    public:
        GPUReferencer() : Referencer<T>()
        {
        }

        GPUReferencer(int line, const char* file, T* obj) : Referencer<T>(line,file,obj)
        {
            SE_ASSERT(IsOnGPUThread());
            static_assert(std::is_base_of_v<GPUResource, T>, "Only for gpu refs");
        }

        GPUReferencer(T* obj) : Referencer<T>(obj)
        {
            SE_ASSERT(IsOnGPUThread());
            static_assert(std::is_base_of_v<GPUResource, T>, "Only for gpu refs");
        }

        GPUReferencer(GPUReferencer<T>& orig) : Referencer<T>(orig) 
        { 
            SE_ASSERT(IsOnGPUThread());
        }
        GPUReferencer(const GPUReferencer<T>& orig) : Referencer<T>(orig) 
        { 
            SE_ASSERT(IsOnGPUThread());
        }

        template<typename K = T>
        GPUReferencer(const GPUReferencer<K>& orig) : Referencer<T>(orig)
        { 
            SE_ASSERT(IsOnGPUThread());
            static_assert(std::is_base_of_v<GPUResource, T>, "Only for gpu refs");
        }

        //template<typename K>
        //GPUReferencer(K* obj = nullptr) : Referencer(obj) 
        //{
        //    static_assert(std::is_base_of_v<GPUResource, K>, "Only for gpu refs");
        //}

        //template<typename K>
        //GPUReferencer(GPUReferencer<K>& orig) : Referencer(orig) { }
    };


    template<typename T>
    GPUReferencer<T> _Make_GPU(int line, const char* file)
    {
        auto Tp = new T();
        Tp->SetDebug(line, file);
        Tp->SetUsingGPUReferencer(true);
        return GPUReferencer<T>(line, file, Tp);
    }

    template<typename T, typename ... Args>
    GPUReferencer<T> _Make_GPU(int line, const char* file, Args&& ... args)
    {
        auto Tp = new T(args...);
        Tp->SetDebug(line, file);
        Tp->SetUsingGPUReferencer(true);
        return GPUReferencer<T>(line, file, Tp);
    }

    #define Make_GPU(T,...) _Make_GPU<T>( __LINE__, __FILE__, ##__VA_ARGS__); 

    class SPP_GRAPHICS_API GPUResource : public ReferenceCounted
    {
        NO_COPY_ALLOWED(GPUResource);

    protected:
        bool _gpuResident = false;
        bool _dying = false;

        int32_t _cppLine = -1;
        std::string _cppFile;

        bool _bIsGPUReferencer = false;

        virtual void _MakeResident() {}
        virtual void _MakeUnresident() {}

    public:
        GPUResource();
        virtual ~GPUResource();

        bool IsUsingGPUReferencer() const 
        {
            return _bIsGPUReferencer;
        }

        void SetUsingGPUReferencer(bool bInValue) 
        {
            _bIsGPUReferencer = bInValue;
        }

        void SetDebug(int line, const char* file)
        {
            _cppLine = line;
            _cppFile = file;
        }

        virtual void SetName(const char *InName) {}

        virtual void NoMoreReferences();

        virtual const char* GetName() const {
            return "GPUResource";
        }
       
        void MakeResident()
        {
            SE_ASSERT(IsOnGPUThread());
            if (!_gpuResident)
            {
                _MakeResident();
                _gpuResident = true;
            }
        }

        void MakeUnresident()
        {
            SE_ASSERT(IsOnGPUThread());
            if (_gpuResident)
            {
                _MakeUnresident();
                _gpuResident = false;
            }
        }

        bool IsGPUResident() const { return _gpuResident; }

        //crazy ugly no verification TODO improve....
        template<typename T>
        T& GetAs()
        {
            return *(T*)this;
        }
    };

    

    #define GLOBAL_RESOURCE(GR) \
            friend struct GR##_REGISTER; \
            static uint32_t _ID;    \
            public:                 \
            static uint32_t GetID();  

    #define REGISTER_GLOBAL_RESOURCE(GR) \
        uint32_t GR::_ID = 0;   \
        uint32_t GR::GetID()    \
        {                       \
            return _ID;         \
        }                       \
        struct GR##_REGISTER    \
        {                       \
            GR##_REGISTER()     \
            {                   \
                GR::_ID = RegisterGlobalResource([](){ return new GR(); });  \
            }                   \
        };                      \
        GR##_REGISTER G##GR##REGISTERED;

    SPP_GRAPHICS_API uint32_t RegisterGlobalResource(std::function< GlobalGraphicsResource*(void) > AllocResource);
    SPP_GRAPHICS_API void UnregisterGlobalResource(GlobalGraphicsResource* InResource);
    SPP_GRAPHICS_API std::vector< std::function< GlobalGraphicsResource* (void) > >& GetGlobalResourceList();
    //SPP_GRAPHICS_API void MakeResidentAllGPUResources();

    class SPP_GRAPHICS_API GPUShader : public GPUResource
    {
    protected:
        EShaderType _type;
        std::string _entryPoint;

    public:
        GPUShader(EShaderType InType) : _type(InType)
        {

        }

        const std::string &GetEntryPoint() 
        {
            return _entryPoint;
        }

        EShaderType GetType() const
        {
            return _type;
        }

        virtual const char* GetName() const override
        {
            return "Shader";
        }

        virtual int32_t GetInstructionCount() const
        {
            return 0;
        }

        virtual bool CompileShaderFromTemplate(const AssetPath& FileName, 
            std::map<std::string, std::string> ReplaceDictionary, 
            const char* EntryPoint = "main", 
            std::string* oErrorMsgs = nullptr)
        {
            std::string loadSrc;
            if (LoadFileToString(*FileName, loadSrc))
            {
                for (auto& [key, value] : ReplaceDictionary)
                {
                    ReplaceInline(loadSrc, key, value);
                }

                return CompileShaderFromString(loadSrc, FileName, EntryPoint, oErrorMsgs);
            }
            else
            {
                return false;
            }
        }
        virtual bool CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr)
        {
            return CompileShaderFromTemplate(FileName, {}, EntryPoint, oErrorMsgs);
        }
        virtual bool CompileShaderFromString(const std::string& ShaderSource, const AssetPath& ReferencePath, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr) = 0;
    };

   
    class SPP_GRAPHICS_API GPUTexture : public GPUResource
    {
    protected:
        int32_t _width = -1;
        int32_t _height = -1;

        uint8_t _dimensions = 2;
        uint8_t _mipLevels = 1;
        uint8_t _faceCount = 1;

        bool _bIsSRGB = false;

        TextureFormat _format = TextureFormat::UNKNOWN;
                
        std::shared_ptr< ImageMeta > _metaInfo;
        std::vector< std::shared_ptr< struct TextureFace > >  _faceData;

        uint32_t _uniqueID = 0;

    public:
        GPUTexture( int32_t Width, int32_t Height, int32_t MipLevelCount, int32_t FaceCount,
            TextureFormat Format );
        GPUTexture( const struct TextureAsset &InTextureAsset);
        virtual ~GPUTexture();
        virtual void PushAsyncUpdate(Vector2i Start, Vector2i Extents, const void* Memory, uint32_t MemorySize) {};

        int32_t GetWidth() const
        {
            return _width;
        }
        int32_t GetHeight() const
        {
            return _height;
        }
        uint32_t GetID() const { return _uniqueID; }
        virtual const char* GetName() const override
        {
            return "Texture";
        }
    };

    struct BufferPageData
    {
        const void* Data;
        uint32_t PageIdx;
    };

    class SPP_GRAPHICS_API GPUBuffer : public GPUResource
    {
    protected:
        GPUBufferType _type = GPUBufferType::Simple;
        std::shared_ptr< ArrayResource > _cpuLink;

    public:
        GPUBuffer(GPUBufferType InType) : _type(InType) {}
        GPUBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData) : _type(InType), _cpuLink(InCpuData) {}
        GPUBuffer(GPUBufferType InType, size_t InPerElementSize, size_t InElementCount) : _type(InType) 
        {
            _cpuLink = std::make_shared< ArrayResource >(InPerElementSize, InElementCount);
        }       

        size_t GetDataSize() const
        {
            SE_ASSERT(_cpuLink);
            return _cpuLink->GetTotalSize();
        }
        uint8_t* GetData() 
        {
            SE_ASSERT(_cpuLink);
            return _cpuLink->GetElementData();
        }
        size_t GetPerElementSize() const
        {
            SE_ASSERT(_cpuLink);
            return _cpuLink->GetPerElementSize();
        }
        size_t GetElementCount() const
        {
            SE_ASSERT(_cpuLink);
            return _cpuLink->GetElementCount();
        }

        virtual void UpdateDirtyRegion(uint32_t Idx, uint32_t Count) { }
        virtual void SetSparsePageMem(BufferPageData* InPages, uint32_t PageCount) { }

        virtual void SetLink(std::shared_ptr < ArrayResource> InLink)
        {
            _cpuLink = InLink;
        }

        virtual const char* GetName() const override
        {
            return "Buffer";
        }
    };

    enum class InputLayoutElementType
    {
        Float3,
        Float2,
        Float,
        UInt32,
        UInt8_4,
        UInt8_3,
    };

    struct InputLayoutElement
    {
        std::string Name;
        InputLayoutElementType Type;
        uint16_t Offset;
    };

    struct VertexAttribute
    {
        InputLayoutElementType Type;
        uint16_t Offset;
    };

    struct VertexStream
    {  
        uint16_t Size;
        std::vector<VertexAttribute> Attributes;
    };

    template <typename T, typename... Args>
    void AddVertexAttributes(std::vector<VertexAttribute> &oAttributes, uintptr_t baseAddr, const T& first, const Args&... args)
    {
        VertexAttribute curAttribute;

        if constexpr (std::is_same_v<T, Vector3>)
        {
            curAttribute.Type = InputLayoutElementType::Float3;
        }
        else if constexpr (std::is_same_v<T, Vector2>)
        {
            curAttribute.Type = InputLayoutElementType::Float2;
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            curAttribute.Type = InputLayoutElementType::UInt32;
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            curAttribute.Type = InputLayoutElementType::Float;
        }
        else if constexpr (std::is_same_v<T, Color4>)
        {
            curAttribute.Type = InputLayoutElementType::UInt8_4;
        }
        else
        {
            struct DummyType {};
            static_assert(std::is_same_v<T, DummyType>, "AddVertexAttributes: Unknown type");
        }

        curAttribute.Offset = (uint32_t)(reinterpret_cast<uintptr_t>(&first) - baseAddr);

        // todo real proper check
        SE_ASSERT(curAttribute.Offset < 128);

        oAttributes.push_back(curAttribute);

        if constexpr (sizeof...(args) >= 1)
        {
            AddVertexAttributes(oAttributes, baseAddr, args...);
        }
    }

    template<class VertexType, typename... VertexMembers>
    VertexStream CreateVertexStream(const VertexType& InType, const VertexMembers&... inMembers)
    {
        VertexStream currentStream = { .Size = sizeof(VertexType) };

        AddVertexAttributes(currentStream.Attributes, reinterpret_cast<uintptr_t>(&InType), inMembers...);

        return currentStream;
    }

    class SPP_GRAPHICS_API GPUInputLayout : public GPUResource
    {
    public:
        GPUInputLayout() 
        {

        }

        virtual const char* GetName() const override
        {
            return "InputLayout";
        }

        virtual void InitializeLayout(const std::vector<VertexStream>& vertexStreams) = 0;
        virtual ~GPUInputLayout() { }
    };

    

    class SPP_GRAPHICS_API PipelineState : public GPUResource
    {
    public:
        PipelineState() {}
        virtual ~PipelineState() { }
    };

    class SPP_GRAPHICS_API GPURenderTarget : public GPUTexture
    {
    public:
        GPURenderTarget(int32_t Width, int32_t Height, TextureFormat Format) :
            GPUTexture(Width, Height, 1, 1, Format) { }
        virtual ~GPURenderTarget() { }
    };


    class SPP_GRAPHICS_API RT_Shader : public RT_Resource
    {
        CLASS_RT_RESOURCE();

    protected:
        GPUReferencer< GPUShader > _shader;
               
        RT_Shader() {}

    public:       
        virtual ~RT_Shader() {}
        virtual void Initialize(EShaderType InType);
        virtual bool CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr);
        virtual bool CompileShaderFromString(const std::string& ShaderSource, const char* ShaderName, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr);

        GPUReferencer< GPUShader > GetGPURef()
        {
            return _shader;
        }
    };

    namespace EMaterialParameterType
    {
        enum ENUM
        {
            Float = 0,
            Float2 = 1,
            Float3 = 2,
            Float4 = 3,
            Texture = 4
        };
    };

    struct IMaterialParameter
    {
        virtual EMaterialParameterType::ENUM GetType() const = 0;
        virtual ~IMaterialParameter() {}
    };

    template<typename T, EMaterialParameterType::ENUM TT>
    struct TMaterialParamter : public IMaterialParameter
    {
        T Value;
        TMaterialParamter() {}
        TMaterialParamter(T InValue) : Value(InValue) { }
        virtual EMaterialParameterType::ENUM GetType() const override { return TT; }
        virtual ~TMaterialParamter() {}
    };

    template struct SPP_GRAPHICS_API TMaterialParamter<float, EMaterialParameterType::Float>;
    template struct SPP_GRAPHICS_API TMaterialParamter<Vector2, EMaterialParameterType::Float2>;
    template struct SPP_GRAPHICS_API TMaterialParamter<Vector3, EMaterialParameterType::Float3>;
    template struct SPP_GRAPHICS_API TMaterialParamter<Vector4, EMaterialParameterType::Float4>;

    using FloatParamter = TMaterialParamter<float, EMaterialParameterType::Float>;
    using Float2Paramter = TMaterialParamter<Vector2, EMaterialParameterType::Float2>;
    using Float3Paramter = TMaterialParamter<Vector3, EMaterialParameterType::Float3>;
    using Float4Paramter = TMaterialParamter<Vector4, EMaterialParameterType::Float4>;

    class SPP_GRAPHICS_API RT_Texture : public RT_Resource, public IMaterialParameter
    {
        CLASS_RT_RESOURCE();

    protected:
        GPUReferencer< GPUTexture > _texture;

        RT_Texture() {}

    public:
        virtual ~RT_Texture() {}
        virtual EMaterialParameterType::ENUM GetType() const override { return EMaterialParameterType::Texture; }
        virtual void Initialize(const struct TextureAsset& TextureAsset);
        GPUReferencer< GPUTexture > GetGPUTexture();
    };

    enum class TexturePurpose : uint8_t
    {
        Diffuse = 0,
        Emissive,
        Metallic,
        Normal,
        Roughness,
        Alpha,
        Lightmap,
        MAX
    };

    SPP_GRAPHICS_API const char* ToString(TexturePurpose InValue);

    //TODO THIS IS WRONG IF CONSTANTS ARE DIFFERENT
    struct ParameterMapKey
    {
        std::vector< std::string > ParamNames;
        std::vector< std::string > Values;

        ParameterMapKey() {}
        ParameterMapKey(const std::map< std::string, std::shared_ptr<IMaterialParameter> >& InMap)
        {
            for (auto& [key, value] : InMap)
            {
                ParamNames.push_back(key);
                auto curType = value->GetType();

                if (curType == EMaterialParameterType::Float)
                {
                    auto paramValue = std::dynamic_pointer_cast<FloatParamter> (value)->Value;
                    Values.push_back(std::string_format("%f", paramValue));
                }
                else if (curType == EMaterialParameterType::Float2)
                {
                    auto paramValue = std::dynamic_pointer_cast<Float2Paramter> (value)->Value;
                    Values.push_back(std::string_format("(%f,%f)", paramValue[0], paramValue[1]));
                }
                else if (curType  == EMaterialParameterType::Float3)
                {
                    auto paramValue = std::dynamic_pointer_cast<Float3Paramter> (value)->Value;
                    Values.push_back(std::string_format("(%f,%f,%f)", paramValue[0], paramValue[1], paramValue[2]));
                }
                else if (curType == EMaterialParameterType::Float4)
                {
                    auto paramValue = std::dynamic_pointer_cast<Float4Paramter> (value)->Value;
                    Values.push_back(std::string_format("(%f,%f,%f,%f)", paramValue[0], paramValue[1], paramValue[2], paramValue[3]));
                }
                else if (curType == EMaterialParameterType::Texture)
                {
                    Values.push_back("TEXT");
                }
            }
            SE_ASSERT(Values.size() == ParamNames.size());
        }

        bool operator<(const ParameterMapKey& compareKey)const
        {
            if (!(ParamNames < compareKey.ParamNames))
            {
                return Values < compareKey.Values;
            }
            else
            {
                return true;
            }
        }
    };

    class SPP_GRAPHICS_API RT_Material : public RT_Resource
    {
        CLASS_RT_RESOURCE();

    protected:
        std::map< std::string, std::shared_ptr<IMaterialParameter> > _parameterMap;

        EBlendState _blendState = EBlendState::Disabled;
        ERasterizerState _rasterizerState = ERasterizerState::BackFaceCull;
        EDepthState _depthState = EDepthState::Enabled;

        uint32_t _updateID = 1;

        RT_Material() {}

    public:
        virtual ~RT_Material() {}

        struct Args
        {
            std::map< std::string, std::shared_ptr<IMaterialParameter> > parameterMap;
        };

        uint32_t GetUpdateID()
        {
            return _updateID;
        }

        void SetMaterialArgs(const Args &InArgs)
        {
            _parameterMap = InArgs.parameterMap;
        }

        auto &GetParameterMap()
        {
            return _parameterMap;
        }
    };

   

    class SPP_GRAPHICS_API RT_Buffer : public RT_Resource
    {
        CLASS_RT_RESOURCE();

    protected:
        GPUReferencer< GPUBuffer > _buffer;
        GPUBufferType _type;

        RT_Buffer(GPUBufferType InType) : _type(InType) {}

    public:
        virtual void Initialize(size_t BufferSize);
        virtual void Initialize(std::shared_ptr< ArrayResource > InCpuData);

        auto GetType() const {
            return _type;
        }

        GPUReferencer< GPUBuffer > GetGPUBuffer()
        {
            return _buffer;
        }
    };
   

    class SPP_GRAPHICS_API RT_ComputeDispatch : public RT_Resource
    {
    protected:        
        std::vector< std::shared_ptr< ArrayResource> > _constants;
        std::vector< GPUReferencer< GPUTexture > > _textures;
        GPUReferencer< GPUShader > _compute;

        RT_ComputeDispatch(GPUReferencer< GPUShader> InCS) : _compute(InCS) { }
    public:

        void SetTextures(const std::vector< GPUReferencer<GPUTexture> > &InTextures)
        {
            _textures = InTextures;
        }

        void SetConstants(const std::vector< std::shared_ptr< ArrayResource> > &InConstants)
        {
            _constants = InConstants;
        }
        virtual void Dispatch(const Vector3i& ThreadGroupCounts) = 0;
    };

   
    struct SimpleColoredLine
    {
        Vector2 lineEnds[2];
        Color3 color;
    };

    class SPP_GRAPHICS_API DebugDrawer
    {
    private:
        std::vector< SimpleColoredLine > _lines;

    public:
        void AddLine(Vector2 Start, Vector2 End, const Color3 InColor = Color3(255, 255, 255));
    }; 
}

