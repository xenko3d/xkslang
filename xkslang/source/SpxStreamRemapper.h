//
// Copyright (C) 

#ifndef XKSLANG_XKSL_SPX_STREAM_REMAPPER_H__
#define XKSLANG_XKSL_SPX_STREAM_REMAPPER_H__

#include <string>
#include <vector>
#include <stack>
#include <sstream>

#include "SPIRV/SPVRemapper.h"

#include "define.h"
#include "SpxBytecode.h"

namespace xkslang
{
//==============================================================================================================//
//===========================================  SpxStreamRemapper  ==============================================//
//==============================================================================================================//

enum class SpxRemapperStatusEnum
{
    WaitingForMixin,
    MixinInProgress,
    MixinBeingFinalized,
    MixinFinalized
};

class SpxStreamRemapper : public spv::spirvbin_t
{
    class FunctionData;
    class ShaderClassData
    {
    public:
        spv::Id id;
        std::string name;
        int level;
        std::vector<ShaderClassData*> parentsList;
        std::vector<FunctionData*> functionsList;

        ShaderClassData(spv::Id id, std::string name): id(id), name(name), level(-1){}
    };

    class FunctionData
    {
    public:
        spv::Id id;
        std::string mangledName;
        ShaderClassData* owner;
        bool hasOverride;  //has the override attribute
        FunctionData* overridenBy;  //the function is being overriden by another function

        FunctionData(spv::Id id, std::string mangledName) : id(id), mangledName(mangledName), owner(nullptr), hasOverride(false), overridenBy(nullptr){}
    };

public:
    SpxStreamRemapper(int verbose = 0);
    virtual ~SpxStreamRemapper();

    bool MixWithSpxBytecode(const SpxBytecode& bytecode);
    bool FinalizeMixin();

    bool GetMappedSpxBytecode(SpxBytecode& bytecode);
    bool GenerateSpvStageBytecode(ShadingStage stage, std::string entryPointName, SpvBytecode& output);

    virtual void error(const std::string& txt) const;
    bool error(const std::string& txt);
    void copyMessagesTo(std::vector<std::string>& list);

    spv::ExecutionModel GetShadingStageExecutionMode(ShadingStage stage);

private:
    bool MergeWithBytecode(const SpxBytecode& bytecode);
    bool SetBytecode(const SpxBytecode& bytecode);

    void ClearAllMaps();
    void BuildDeclarationNameMap();
    bool BuildAllMaps();
    bool BuildOverridenFunctionMap();

    bool BuildAndSetShaderStageHeader(ShadingStage stage, FunctionData* entryFunction, std::string unmangledFunctionName);
    bool RemapAllOverridenFunctions();
    bool ConvertSpirxToSpirVBytecode();

private:
    SpxRemapperStatusEnum status;

    std::vector<std::string> errorMessages;
   
    //std::unordered_map<spv::Id, int> declaredShaderAndTheirLevel;           // list of xksl shader type declared in the bytecode, and their level
    //std::unordered_map<spv::Id, bool> fnWithOverrideAttribute;              // list of all methods having the override attributes
    //std::unordered_map<spv::Id, spv::Id> fnOwnerShader;                     // list of all methods, linked with the shader declaring it
    //std::unordered_map<spv::Id, std::vector<spv::Id>> shaderParentsList;    // list of all parents inherited by the shader
    //std::unordered_map<spv::Id, std::vector<spv::Id>> shaderFunctionsList;  // list of all functions declared within a shader

    std::unordered_map<spv::Id, std::string> mapDeclarationName;            // delaration name (user defined name) for methods, shaders and variables
    std::unordered_map<spv::Id, ShaderClassData*> mapShadersById;
    std::unordered_map<std::string, ShaderClassData*> mapShadersByName;
    std::unordered_map<spv::Id, FunctionData*> mapFunctionsById;

    std::string GetDeclarationNameForId(spv::Id id);
    bool GetDeclarationNameForId(spv::Id id, std::string& name);
    ShaderClassData* GetShaderByName(const std::string& name);
    ShaderClassData* GetShaderById(spv::Id id);
    FunctionData* GetFunctionById(spv::Id id);
    FunctionData* IsFunction(spv::Id id);

    void stripBytecode(std::vector<range_t>& ranges);
};

}  // namespace xkslang

#endif  // XKSLANG_XKSL_SPX_STREAM_REMAPPER_H__
