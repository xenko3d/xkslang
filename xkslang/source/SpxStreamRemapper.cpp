//
// Copyright (C)

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "glslang/Public/ShaderLang.h"
#include "StandAlone/ResourceLimits.h"

#include "SPIRV/doc.h"
//#include "SPIRV/disassemble.h"
//#include "SPIRV/SPVRemapper.h"

#include "SpxStreamRemapper.h"

using namespace std;
using namespace xkslang;

//=====================================================================================================================================
//=====================================================================================================================================
void SpxStreamRemapper::copyMessagesTo(vector<string>& list)
{
    list.insert(list.end(), errorMessages.begin(), errorMessages.end());
}

bool SpxStreamRemapper::error(const string& txt)
{
    errorMessages.push_back(txt);
    return false;
}

//=====================================================================================================================================
//=====================================================================================================================================
//static const auto spx_inst_fn_nop = [](spv::Op, unsigned) { return false; };
//static const auto spx_op_fn_nop = [](spv::Id&) {};

unsigned int SpxStreamRemapper::currentMergeOperationId = 0;

unsigned int SpxStreamRemapper::GetUniqueMergeOperationId()
{
    return SpxStreamRemapper::currentMergeOperationId++;
}

void SpxStreamRemapper::ResetMergeOperationId()
{
    SpxStreamRemapper::currentMergeOperationId = 0;
}

spv::ExecutionModel SpxStreamRemapper::GetShadingStageExecutionMode(ShadingStageEnum stage)
{
    switch (stage) {
    case ShadingStageEnum::Vertex:           return spv::ExecutionModelVertex;
    case ShadingStageEnum::Pixel:            return spv::ExecutionModelFragment;
    case ShadingStageEnum::TessControl:      return spv::ExecutionModelTessellationControl;
    case ShadingStageEnum::TessEvaluation:   return spv::ExecutionModelTessellationEvaluation;
    case ShadingStageEnum::Geometry:         return spv::ExecutionModelGeometry;
    case ShadingStageEnum::Compute:          return spv::ExecutionModelGLCompute;
    default:
        return spv::ExecutionModelMax;
    }
}
//=====================================================================================================================================
//=====================================================================================================================================

SpxStreamRemapper::SpxStreamRemapper(int verbose) : spirvbin_t(verbose)
{
    status = SpxRemapperStatusEnum::WaitingForMixin;
}

SpxStreamRemapper::~SpxStreamRemapper()
{
    ReleaseAllMaps();
}

SpxStreamRemapper* SpxStreamRemapper::Clone()
{
    SpxStreamRemapper* clonedSpxRemapper = new SpxStreamRemapper();
    clonedSpxRemapper->spv = this->spv;
    clonedSpxRemapper->listAllObjects.resize(listAllObjects.size(), nullptr);

    for (unsigned int i=0; i<listAllObjects.size(); ++i)
    {
        ObjectInstructionBase* obj = listAllObjects[i];
        if (obj != nullptr)
        {
            ObjectInstructionBase* clonedObj = obj->CloneBasicData();
            clonedObj->bytecodeSource = clonedSpxRemapper;
            clonedSpxRemapper->listAllObjects[i] = clonedObj;
        }
    }

    for (unsigned int i = 0; i < listAllObjects.size(); ++i)
    {
        ObjectInstructionBase* obj = listAllObjects[i];
        if (obj == nullptr) continue;

        ObjectInstructionBase* clonedObj = clonedSpxRemapper->listAllObjects[i];
        switch (obj->GetKind())
        {
            case ObjectInstructionTypeEnum::Type:
            {
                TypeInstruction* type = dynamic_cast<TypeInstruction*>(obj);
                if (type->GetTypePointed() != nullptr)
                {
                    TypeInstruction* clonedType = dynamic_cast<TypeInstruction*>(clonedObj);
                    TypeInstruction* pointedType = clonedSpxRemapper->GetTypeById(type->GetTypePointed()->GetId());
                    clonedType->SetTypePointed(pointedType);
                }
                break;
            }
            case ObjectInstructionTypeEnum::Variable:
            {
                VariableInstruction* variable = dynamic_cast<VariableInstruction*>(obj);
                if (variable->GetTypePointed() != nullptr)
                {
                    VariableInstruction* clonedVariable = dynamic_cast<VariableInstruction*>(clonedObj);
                    TypeInstruction* pointedType = clonedSpxRemapper->GetTypeById(variable->GetTypePointed()->GetId());
                    clonedVariable->SetTypePointed(pointedType);
                }
                break;
            }
            case ObjectInstructionTypeEnum::Function:
            {
                FunctionInstruction* function = dynamic_cast<FunctionInstruction*>(obj);
                FunctionInstruction* clonedFunction = dynamic_cast<FunctionInstruction*>(clonedObj);
                if (function->GetOverridingFunction() != nullptr)
                {
                    FunctionInstruction* pointedFunction = clonedSpxRemapper->GetFunctionById(function->GetOverridingFunction()->GetId());
                    clonedFunction->SetOverridingFunction(pointedFunction);
                }
                if (function->GetShaderOwner() != nullptr)
                {
                    ShaderClassData* shaderOwner = clonedSpxRemapper->GetShaderById(function->GetShaderOwner()->GetId());
                    clonedFunction->SetShaderOwner(shaderOwner);
                }
                break;
            }
            case ObjectInstructionTypeEnum::Shader:
            {
                ShaderClassData* shader = dynamic_cast<ShaderClassData*>(obj);
                ShaderClassData* clonedShader = dynamic_cast<ShaderClassData*>(clonedObj);
                shader->tmpClonedShader = clonedShader;
                for (auto it = shader->parentsList.begin(); it != shader->parentsList.end(); it++)
                {
                    ShaderClassData* parent = clonedSpxRemapper->GetShaderById((*it)->GetId());
                    clonedShader->AddParent(parent);
                }
                for (auto it = shader->functionsList.begin(); it != shader->functionsList.end(); it++)
                {
                    FunctionInstruction* function = clonedSpxRemapper->GetFunctionById((*it)->GetId());
                    clonedShader->AddFunction(function);
                }
                for (auto it = shader->shaderTypesList.begin(); it != shader->shaderTypesList.end(); it++)
                {
                    ShaderTypeData* type = *it;
                    TypeInstruction* clonedType = clonedSpxRemapper->GetTypeById(type->type->GetId());
                    TypeInstruction* clonedTypePointer = clonedSpxRemapper->GetTypeById(type->pointerToType->GetId());
                    VariableInstruction* clonedVariable = clonedSpxRemapper->GetVariableById(type->variable->GetId());
                    ShaderTypeData* clonedShaderType = new ShaderTypeData(clonedShader, clonedType, clonedTypePointer, clonedVariable);
                    clonedType->SetShaderOwner(clonedShader);
                    clonedTypePointer->SetShaderOwner(clonedShader);
                    clonedVariable->SetShaderOwner(clonedShader);
                    clonedShader->AddShaderType(clonedShaderType);
                }
                break;
            }
        }
    }

    for (auto it = mapDeclarationName.begin(); it != mapDeclarationName.end(); it++)
    {
        clonedSpxRemapper->mapDeclarationName[it->first] = it->second;
    }

    for (auto it = vecAllShaders.begin(); it != vecAllShaders.end(); it++)
    {
        ShaderClassData* shaderToClone = *it;
        ShaderClassData* clonedShader = clonedSpxRemapper->GetShaderById(shaderToClone->GetId());
        clonedSpxRemapper->vecAllShaders.push_back(clonedShader);

        for (unsigned int i = 0; i < shaderToClone->compositionsList.size(); ++i)
        {
            ShaderComposition& compositionToClone = shaderToClone->compositionsList[i];
            ShaderClassData* shaderType = compositionToClone.shaderType == nullptr? nullptr: clonedSpxRemapper->GetShaderById(compositionToClone.shaderType->GetId());
            ShaderComposition clonedComposition(compositionToClone.compositionShaderId, compositionToClone.compositionShaderOwner, shaderType, compositionToClone.variableName,
                compositionToClone.isArray, compositionToClone.countInstances);
            //clonedComposition.instantiatedShader = compositionToClone.instantiatedShader == nullptr ? nullptr : clonedSpxRemapper->GetShaderById(compositionToClone.instantiatedShader->GetId());
            clonedShader->AddComposition(clonedComposition);
        }
    }

    for (auto it = vecAllFunctions.begin(); it != vecAllFunctions.end(); it++)
    {
        FunctionInstruction* function = clonedSpxRemapper->GetFunctionById((*it)->GetId());
        clonedSpxRemapper->vecAllFunctions.push_back(function);
    }

    clonedSpxRemapper->status = status;
    for (auto it = idPosR.begin(); it != idPosR.end(); it++)
        clonedSpxRemapper->idPosR[it->first] = it->second;
    for (auto it = errorMessages.begin(); it != errorMessages.end(); it++)
        clonedSpxRemapper->errorMessages.push_back(*it);
    
    return clonedSpxRemapper;
}

bool SpxStreamRemapper::RemoveShaderTypeFromBytecodeAndData(ShaderTypeData* shaderTypeToRemove, vector<range_t>& vecStripRanges)
{
    ShaderClassData* shaderOwner = shaderTypeToRemove->shaderOwner;

    vector<bool> listIdsRemoved;
    listIdsRemoved.resize(bound(), false);

    //remove the type belonging to the shader
    {
        bool typeFound = false;
        for (auto itt = shaderOwner->shaderTypesList.begin(); itt != shaderOwner->shaderTypesList.end(); itt++)
        {
            ShaderTypeData* aShaderType = *itt;
            if (shaderTypeToRemove != aShaderType) continue;

            spv::Id id;
            TypeInstruction* type = shaderTypeToRemove->type;
            TypeInstruction* pointerToType = shaderTypeToRemove->pointerToType;
            VariableInstruction* variable = shaderTypeToRemove->variable;

            stripInst(vecStripRanges, type->GetBytecodeStartPosition(), type->GetBytecodeEndPosition());
            id = type->GetId();
            delete listAllObjects[id];
            listAllObjects[id] = nullptr;
            listIdsRemoved[id] = true;

            stripInst(vecStripRanges, pointerToType->GetBytecodeStartPosition(), pointerToType->GetBytecodeEndPosition());
            id = pointerToType->GetId();
            delete listAllObjects[id];
            listAllObjects[id] = nullptr;
            listIdsRemoved[id] = true;

            stripInst(vecStripRanges, variable->GetBytecodeStartPosition(), variable->GetBytecodeEndPosition());
            id = variable->GetId();
            delete listAllObjects[id];
            listAllObjects[id] = nullptr;
            listIdsRemoved[id] = true;

            //remove the element
            typeFound = true;
            iter_swap(itt, shaderOwner->shaderTypesList.end() - 1);
            shaderOwner->shaderTypesList.pop_back();
            break;
        }
        if (!typeFound) return error(string("Failed to find the shaderType to remove: ") + shaderTypeToRemove->type->GetName());
    }

    //remove all decorates related to the objects we removed
    {
        unsigned int start = header_size;
        const unsigned int end = spv.size();
        while (start < end)
        {
            unsigned int wordCount = asWordCount(start);
            spv::Op opCode = asOpCode(start);

            switch (opCode)
            {
                //case spv::OpTypeXlslShaderClass:  //already removed above
                case spv::OpName:
                case spv::OpMemberName:
                case spv::OpDecorate:
                case spv::OpMemberDecorate:
                case spv::OpDeclarationName:
                case spv::OpShaderInheritance:
                case spv::OpShaderCompositionDeclaration:
                case spv::OpShaderCompositionInstance:
                case spv::OpBelongsToShader:
                case spv::OpMethodProperties:
                case spv::OpGSMethodProperties:
                case spv::OpMemberProperties:
                case spv::OpCBufferMemberProperties:
                case spv::OpMemberSemanticName:
                {
                    const spv::Id id = asId(start + 1);
                    if (listIdsRemoved[id]) stripInst(vecStripRanges, start, start + wordCount);
                    break;
                }

                case spv::OpTypeXlslShaderClass:
                {
                    //every decorate and previous Op have been declared before type declaration, we can stop here
                    start = end;
                    break;
                }
            }
            start += wordCount;
        }
    }

    return true;
}

bool SpxStreamRemapper::RemoveShaderFromBytecodeAndData(ShaderClassData* shaderToRemove, vector<range_t>& vecStripRanges)
{
    spv::Id shaderId = shaderToRemove->GetId();
    if (GetShaderById(shaderId) != shaderToRemove) return error(string("Failed to find the shader to remove: ") + shaderToRemove->GetName());

    vector<bool> listIdsRemoved;
    listIdsRemoved.resize(bound(), false);

    //remove the shader from vecAllShaders: swap the shader with the latest elements (to remove it without cost, and to check if it does belong in the list)
    bool shaderFound = false;
    for (auto itsh = vecAllShaders.begin(); itsh != vecAllShaders.end(); itsh++)
    {
        ShaderClassData* shader = *itsh;
        if (shader == shaderToRemove)
        {
            shaderFound = true;
            iter_swap(itsh, vecAllShaders.end() - 1);
            vecAllShaders.pop_back();
            break;
        }
    }
    if (!shaderFound) return error(string("Failed to find the shader to remove: ") + shaderToRemove->GetName());

    //remove all functions belonging to the shader
    {
        unsigned int countFunctionsRemoved = 0;
        for (unsigned int i = 0; i < vecAllFunctions.size(); ++i)
        {
            FunctionInstruction* function = vecAllFunctions[i];
            if (function->shaderOwner == shaderToRemove)
            {
                countFunctionsRemoved++;

                if (i < vecAllFunctions.size() - 1)
                    vecAllFunctions[i] = vecAllFunctions[vecAllFunctions.size() - 1];
                vecAllFunctions.pop_back();

                stripInst(vecStripRanges, function->GetBytecodeStartPosition(), function->GetBytecodeEndPosition());
                spv::Id id = function->GetId();
                delete listAllObjects[id];
                listAllObjects[id] = nullptr;
                listIdsRemoved[id] = true;
            }
        }
        if (countFunctionsRemoved != shaderToRemove->GetCountFunctions())
            return error(string("Discrepancy between shader list of functions and list of all shader functions for shader: ") + shaderToRemove->GetName());
    }

    //remove all types belonging to the shader
    {
        for (unsigned int t = 0; t < shaderToRemove->shaderTypesList.size(); ++t)
        {
            ShaderTypeData* shaderTypeToRemove = shaderToRemove->shaderTypesList[t];

            spv::Id id;
            TypeInstruction* type = shaderTypeToRemove->type;
            TypeInstruction* pointerToType = shaderTypeToRemove->pointerToType;
            VariableInstruction* variable = shaderTypeToRemove->variable;

            stripInst(vecStripRanges, type->GetBytecodeStartPosition(), type->GetBytecodeEndPosition());
            id = type->GetId();
            delete listAllObjects[id];
            listAllObjects[id] = nullptr;
            listIdsRemoved[id] = true;

            stripInst(vecStripRanges, pointerToType->GetBytecodeStartPosition(), pointerToType->GetBytecodeEndPosition());
            id = pointerToType->GetId();
            delete listAllObjects[id];
            listAllObjects[id] = nullptr;
            listIdsRemoved[id] = true;

            stripInst(vecStripRanges, variable->GetBytecodeStartPosition(), variable->GetBytecodeEndPosition());
            id = variable->GetId();
            delete listAllObjects[id];
            listAllObjects[id] = nullptr;
            listIdsRemoved[id] = true;
        }
    }

    //remove the shader's bytecode and object
    {
        stripInst(vecStripRanges, shaderToRemove->GetBytecodeStartPosition());
        spv::Id id = shaderToRemove->GetId();
        delete listAllObjects[id];
        listAllObjects[id] = nullptr;
        listIdsRemoved[id] = true;
    }

    //remove all decorates related to the objects we removed
    {
        unsigned int start = header_size;
        const unsigned int end = spv.size();
        while (start < end)
        {
            unsigned int wordCount = asWordCount(start);
            spv::Op opCode = asOpCode(start);

            switch (opCode)
            {
                //case spv::OpTypeXlslShaderClass:  //already removed above
                case spv::OpName:
                case spv::OpMemberName:
                case spv::OpDecorate:
                case spv::OpMemberDecorate:
                case spv::OpDeclarationName:
                case spv::OpShaderInheritance:
                case spv::OpShaderCompositionDeclaration:
                case spv::OpShaderCompositionInstance:
                case spv::OpBelongsToShader:
                case spv::OpMethodProperties:
                case spv::OpGSMethodProperties:
                case spv::OpMemberProperties:
                case spv::OpCBufferMemberProperties:
                case spv::OpMemberSemanticName:
                {
                    const spv::Id id = asId(start + 1);
                    if (listIdsRemoved[id]) stripInst(vecStripRanges, start, start + wordCount);
                    break;
                }

                case spv::OpTypeXlslShaderClass:
                {
                    //every decorate and previous Op have been declared before type declaration, we can stop here
                    start = end;
                    break;
                }
            }
            start += wordCount;
        }
    }

    return true;
}

void SpxStreamRemapper::ReleaseAllMaps()
{
    unsigned int size = listAllObjects.size();
    for (unsigned int i = 0; i < size; ++i)
    {
        if (listAllObjects[i] != nullptr) delete listAllObjects[i];
    }

    listAllObjects.clear();
    vecAllShaders.clear();
    vecAllFunctions.clear();
    mapDeclarationName.clear();
}

bool SpxStreamRemapper::MixWithShadersFromBytecode(const SpxBytecode& sourceBytecode, const vector<string>& nameOfShadersToMix)
{
    if (status != SpxRemapperStatusEnum::WaitingForMixin) {
        return error("Invalid remapper status");
    }
    status = SpxRemapperStatusEnum::MixinInProgress;

    if (nameOfShadersToMix.size() == 0) {
        return error("List of shader is empty");
    }

    //=============================================================================================================
    // First mixin: set a default header to the bytecode
    if (spv.size() == 0)
    {
        //Initialize a default header
        if (!InitDefaultHeader()) {
            return error("Failed to initialize a default header");
        }

        if (!this->BuildAllMaps()){
            return error("Error parsing the bytecode after having initialized the default header");
        }
    }

    //===============================================================================================================================================
    //===============================================================================================================================================
    //parse and init the bytecode to merge
    SpxStreamRemapper bytecodeToMerge;
    if (!bytecodeToMerge.SetBytecode(sourceBytecode)) return false;

    if (!bytecodeToMerge.BuildAllMaps()) {
        bytecodeToMerge.copyMessagesTo(errorMessages);
        return error(string("Error parsing the bytecode: ") + sourceBytecode.GetName());
    }
    for (auto itsh = bytecodeToMerge.vecAllShaders.begin(); itsh != bytecodeToMerge.vecAllShaders.end(); itsh++)
        (*itsh)->flag1 = 0;

    //===============================================================================================================================================
    //===============================================================================================================================================
    // Build the list of all shaders we want to merge

    //retrieve the shader object from source bytecode
    vector<ShaderClassData*> listShader;
    for (unsigned int is = 0; is < nameOfShadersToMix.size(); ++is)
    {
        const string& nameShaderToMix = nameOfShadersToMix[is];
        ShaderClassData* shaderToMerge = bytecodeToMerge.GetShaderByName(nameShaderToMix);
        if (shaderToMerge == nullptr) {
            return error(string("Shader not found in source bytecode: ") + nameShaderToMix);
        }
        listShader.push_back(shaderToMerge);
    }

    vector<ShaderClassData*> shadersFullDependencies;
    if (!SpxStreamRemapper::GetShadersFullDependencies(&bytecodeToMerge, listShader, shadersFullDependencies)){
        bytecodeToMerge.copyMessagesTo(errorMessages);
        return error(string("Failed to get the shaders dependencies"));
    }

    //We won't add a shader if it already exists in the destination bytecode
    vector<ShaderToMergeData> listShadersToMerge;
    for (auto its = shadersFullDependencies.begin(); its != shadersFullDependencies.end(); its++)
    {
        ShaderClassData* aShaderToMerge = *its;
        if (this->GetShaderByName(aShaderToMerge->GetName()) == nullptr)
        {
            listShadersToMerge.push_back(ShaderToMergeData(aShaderToMerge));
        }
    }

    if (listShadersToMerge.size() == 0){
        //no new shader to merge, can quit there
        status = SpxRemapperStatusEnum::WaitingForMixin;
        return true;
    }

    //===============================================================================================================================================
    //===============================================================================================================================================
    // Merge the shaders
    if (!MergeShadersIntoBytecode(bytecodeToMerge, listShadersToMerge, "")){
        return error("Failed to merge the shaders");
    }

    //===============================================================================================================================================
    //===============================================================================================================================================
    //retrieve the merged shaders from the destination bytecode
    vector<ShaderClassData*> listShadersMerged;
    for (unsigned int is = 0; is<listShadersToMerge.size(); ++is)
    {
        ShaderClassData* shaderToMerge = listShadersToMerge[is].shader;
        ShaderClassData* shaderMerged = shaderToMerge->tmpClonedShader; //GetShaderByName(shaderToMerge->GetName());
        if (shaderMerged == nullptr) return error(string("Cannot retrieve the shader: ") + shaderToMerge->GetName());
        listShadersMerged.push_back(shaderMerged);
    }

    if (!ProcessOverrideAfterMixingNewShaders(listShadersMerged))
    {
        return error("Failed to process overrides after mixin new shaders");
    }
    status = SpxRemapperStatusEnum::WaitingForMixin;
    return true;
}

bool SpxStreamRemapper::ProcessOverrideAfterMixingNewShaders(vector<ShaderClassData*>& listNewShaders)
{
    if (!ValidateSpxBytecodeAndData())
    {
        return error("Error validating the bytecode");
    }

    //=============================================================================================================
    //=============================================================================================================
    // Deal with base and overrides function

    //BEFORE checking for overrides, we first deal with base function calls
    if (!UpdateFunctionCallsHavingUnresolvedBaseAccessor())
    {
        return error("Updating function calls to base target failed");
    }

    //Update the list of overriding methods
    if (!UpdateOverridenFunctionMap(listNewShaders))
    {
        return error("Updating overriding functions failed");
    }

    //target function to the overring functions
    if (!UpdateOpFunctionCallTargetsInstructionsToOverridingFunctions())
    {
        return error("Remapping overriding functions failed");
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

bool SpxStreamRemapper::MergeShadersIntoBytecode(SpxStreamRemapper& bytecodeToMerge, const vector<ShaderToMergeData>& listShadersToMerge, string allInstancesPrefixToAdd)
{
    //if (&bytecodeToMerge == this) {
    //    return error("Cannot merge a bytecode into its own code");
    //}

    if (listShadersToMerge.size() == 0) return true;

    //=============================================================================================================
    //Init destination bytecode hashmap (to compare types and consts hashmap id)
    unordered_map<uint32_t, pairIdPos> destinationBytecodeTypeHashMap;
    if (!this->BuildTypesAndConstsHashmap(destinationBytecodeTypeHashMap)) {
        return error("Merge shaders. Error building type and const hashmap");
    }

    //=============================================================================================================
    //=============================================================================================================
    //Merge all the shaders
    int newId = bound();

    vector<uint32_t> vecNamesAndDecorateToMerge;
    vector<uint32_t> vecXkslDecoratesToMerge;
    vector<uint32_t> vecTypesConstsAndVariablesToMerge;
    vector<uint32_t> vecFunctionsToMerge;
    vector<uint32_t> vecHeaderPropertiesToMerge;

    vector<spv::Id> finalRemapTable;
    finalRemapTable.resize(bytecodeToMerge.bound(), spvUndefinedId);
    //keep the list of merged Ids so that we can look for their name or decorate
    vector<bool> listAllNewIdMerged;
    vector<bool> listIdsWhereToAddNamePrefix;
    listAllNewIdMerged.resize(bytecodeToMerge.bound(), false);
    listIdsWhereToAddNamePrefix.resize(bytecodeToMerge.bound(), false);
    vector<pair<spv::Id, spv::Id>> listOverridenFunctionMergedToBeRemappedWithMergedFunctions;

    for (unsigned int is=0; is<listShadersToMerge.size(); ++is)
    {
        const ShaderToMergeData& shaderToMergeData = listShadersToMerge[is];
        ShaderClassData* shaderToMerge = shaderToMergeData.shader;

        if (shaderToMerge->bytecodeSource != &bytecodeToMerge)
        {
            error(string("Shader: ") + shaderToMerge->GetName() + string(" does not belong to the source to merge"));
            return false;
        }
        shaderToMerge->tmpClonedShader = nullptr;
        bool instantiateTheShader = shaderToMergeData.instantiateShader;

        //check that the shader does not already exist in the destination bytecode
        string shaderToMergeFinalName = shaderToMerge->GetName();
        if (instantiateTheShader) shaderToMergeFinalName = allInstancesPrefixToAdd + shaderToMergeFinalName;

        if (this->GetShaderByName(shaderToMergeFinalName) != nullptr) {
            return error(string("A shader already exists in destination bytecode with the name: ") + shaderToMergeFinalName);
        }

        //=============================================================================================================
        //=============================================================================================================
        //merge all types and variables declared by the shader
        for (unsigned int t = 0; t < shaderToMerge->shaderTypesList.size(); ++t)
        {
            ShaderTypeData* shaderTypeToMerge = shaderToMerge->shaderTypesList[t];

            TypeInstruction* type = shaderTypeToMerge->type;
            TypeInstruction* pointerToType = shaderTypeToMerge->pointerToType;
            VariableInstruction* variable = shaderTypeToMerge->variable;

#ifdef XKSLANG_DEBUG_MODE
            if (finalRemapTable[type->GetId()] != spvUndefinedId) error(string("id: ") + to_string(type->GetId()) + string(" has already been remapped"));
            if (finalRemapTable[pointerToType->GetId()] != spvUndefinedId) error(string("id: ") + to_string(pointerToType->GetId()) + string(" has already been remapped"));
            if (finalRemapTable[variable->GetId()] != spvUndefinedId) error(string("id: ") + to_string(variable->GetId()) + string(" has already been remapped"));
#endif

            finalRemapTable[type->GetId()] = newId++;
            bytecodeToMerge.CopyInstructionToVector(vecTypesConstsAndVariablesToMerge, type->GetBytecodeStartPosition());
            finalRemapTable[pointerToType->GetId()] = newId++;
            bytecodeToMerge.CopyInstructionToVector(vecTypesConstsAndVariablesToMerge, pointerToType->GetBytecodeStartPosition());
            finalRemapTable[variable->GetId()] = newId++;
            bytecodeToMerge.CopyInstructionToVector(vecTypesConstsAndVariablesToMerge, variable->GetBytecodeStartPosition());

            //don't update the name of non-instantiated merged shaders
            if (instantiateTheShader) {
                listIdsWhereToAddNamePrefix[type->GetId()] = true;
                listIdsWhereToAddNamePrefix[pointerToType->GetId()] = true;
                listIdsWhereToAddNamePrefix[variable->GetId()] = true;
            }
        }

        //Add the shader type declaration
        {
            const spv::Id resultId = shaderToMerge->GetId();

#ifdef XKSLANG_DEBUG_MODE
            if (finalRemapTable[resultId] != spvUndefinedId) error(string("id: ") + to_string(resultId) + string(" has already been remapped"));
#endif

            finalRemapTable[resultId] = newId++;
            bytecodeToMerge.CopyInstructionToVector(vecTypesConstsAndVariablesToMerge, shaderToMerge->GetBytecodeStartPosition());

            if (instantiateTheShader) {
                listIdsWhereToAddNamePrefix[resultId] = true;
            }
        }

        //=============================================================================================================
        //=============================================================================================================
        //add all functions' instructions declared by the shader
        for (unsigned int t = 0; t < shaderToMerge->functionsList.size(); ++t)
        {
            FunctionInstruction* functionToMerge = shaderToMerge->functionsList[t];
            //finalRemapTable[functionToMerge->id] = newId++; //done below

            if (instantiateTheShader) {
                listIdsWhereToAddNamePrefix[functionToMerge->GetId()] = true;
            }

            //If the function is already overriden by another function: we'll update the link in the cloned functions as well
            if (functionToMerge->GetOverridingFunction() != nullptr)
            {
                listOverridenFunctionMergedToBeRemappedWithMergedFunctions.push_back(pair<spv::Id, spv::Id>(functionToMerge->GetId(), functionToMerge->GetOverridingFunction()->GetId()));
            }
            
            //For each instructions within the functions bytecode: Remap their results IDs
            {
                unsigned int start = functionToMerge->GetBytecodeStartPosition();
                const unsigned int end = functionToMerge->GetBytecodeEndPosition();
                while (start < end)
                {
                    unsigned int wordCount = bytecodeToMerge.asWordCount(start);
                    spv::Op opCode = bytecodeToMerge.asOpCode(start);

                    unsigned word = start + 1;

                    // Read type and result ID from instruction desc table
                    if (spv::InstructionDesc[opCode].hasType()) {
                        word++;  //spv::Id typeId = bytecodeToMerge.asId(word++);
                    }

                    if (spv::InstructionDesc[opCode].hasResult())
                    {
                        spv::Id resultId = bytecodeToMerge.asId(word++);
#ifdef XKSLANG_DEBUG_MODE
                        if (finalRemapTable[resultId] != spvUndefinedId) error(string("id: ") + to_string(resultId) + string(" has already been remapped"));
#endif
                        finalRemapTable[resultId] = newId++;
                    }

                    start += wordCount;
                }
            }

            //Copy all bytecode instructions from the function
            bytecodeToMerge.CopyInstructionToVector(vecFunctionsToMerge, functionToMerge->GetBytecodeStartPosition(), functionToMerge->GetBytecodeEndPosition());
        }
    } //end shaderToMerge loop

    {
        //update listAllNewIdMerged table (this table defines the name and decorate to fetch and merge)
        unsigned int len = finalRemapTable.size();
        for (unsigned int i = 0; i < len; ++i)
        {
            if (finalRemapTable[i] != spvUndefinedId) listAllNewIdMerged[i] = true;
        }
    }

    //Populate vecNewShadersDecorationPossesingIds with some decorate instructions which might contains unmapped IDs
    vector<uint32_t> vecXkslDecorationsPossesingIds;
    {
        unsigned int start = bytecodeToMerge.header_size;
        const unsigned int end = bytecodeToMerge.spv.size();
        while (start < end)
        {
            unsigned int wordCount = bytecodeToMerge.asWordCount(start);
            spv::Op opCode = bytecodeToMerge.asOpCode(start);

            switch (opCode)
            {
                case spv::OpShaderInheritance:
                case spv::OpShaderCompositionDeclaration:
                case spv::OpShaderCompositionInstance:
                {
                    const spv::Id id = bytecodeToMerge.asId(start + 1);
                    if (listAllNewIdMerged[id])
                    {
                        bytecodeToMerge.CopyInstructionToVector(vecXkslDecorationsPossesingIds, start);
                    }
                    break;
                }

                case spv::OpTypeXlslShaderClass:
                {
                    //all done at this point
                    start = end;
                    break;
                }
            }

            start += wordCount;
        }
    }

    //=============================================================================================================
    //=============================================================================================================
    //Check for all unmapped Ids called within the shader types/consts instructions that we have to merge
    vector<uint32_t> bytecodeWithExtraTypesToMerge;
    {
        //Init by checking all types instructions for unmapped IDs
        vector<uint32_t> bytecodeToCheckForUnmappedIds;
        bytecodeToCheckForUnmappedIds.insert(bytecodeToCheckForUnmappedIds.end(), vecTypesConstsAndVariablesToMerge.begin(), vecTypesConstsAndVariablesToMerge.end());
        bytecodeToCheckForUnmappedIds.insert(bytecodeToCheckForUnmappedIds.end(), vecFunctionsToMerge.begin(), vecFunctionsToMerge.end());
        bytecodeToCheckForUnmappedIds.insert(bytecodeToCheckForUnmappedIds.end(), vecXkslDecorationsPossesingIds.begin(), vecXkslDecorationsPossesingIds.end());
        vector<pairIdPos> listUnmappedIdsToProcess;  //unmapped ids and their pos in the bytecode to merge

        spv::Op opCode;
        unsigned int wordCount;
        vector<spv::Id> listIds;
        spv::Id typeId, resultId;
        unsigned int listIdsLen;
        unsigned int start = 0;
        const unsigned int end = bytecodeToCheckForUnmappedIds.size();
        string errorMsg;
        while (start < end)
        {
            listIds.clear();
            if (!parseInstruction(bytecodeToCheckForUnmappedIds, start, opCode, wordCount, typeId, resultId, listIds, errorMsg)){
                error(errorMsg);
                return error("Merge shaders. Error parsing bytecodeToCheckForUnmappedIds instructions");
            }

            if (typeId != spvUndefinedId) listIds.push_back(typeId);
            listIdsLen = listIds.size();
            for (unsigned int i = 0; i < listIdsLen; ++i)
            {
                const spv::Id id = listIds[i];
                if (finalRemapTable[id] == spvUndefinedId)
                {
                    listUnmappedIdsToProcess.push_back(pairIdPos(id, -1));
                }
            }

            start += wordCount;
        }

        //TypeData* typeToMerge;
        //ConstData* constToMerge;
        //VariableData* variableToAccess;
        while (listUnmappedIdsToProcess.size() > 0)
        {
            pairIdPos& unmappedIdPos = listUnmappedIdsToProcess.back();
            const spv::Id unmappedId = unmappedIdPos.first;
            if (finalRemapTable[unmappedId] != spvUndefinedId)
            {
                listUnmappedIdsToProcess.pop_back();
                continue;
            }

            //Find the position in the code to merge where the unmapped IDs is defined
            ObjectInstructionBase* objectFromUnmappedId = bytecodeToMerge.GetObjectById(unmappedId);
            if (objectFromUnmappedId == nullptr) return error(string("Merge shaders. No object is defined for the unmapped id: ") + to_string(unmappedId));

            bool mappingResolved = false;
            bool mergeTypeOrConst = false;
            //uint32_t instructionPos = object->GetBytecodeStartPosition();
            switch (objectFromUnmappedId->GetKind())
            {
                case ObjectInstructionTypeEnum::Const:
                case ObjectInstructionTypeEnum::Type:
                {
                    mergeTypeOrConst = true;
                    uint32_t typeHash = bytecodeToMerge.hashType(objectFromUnmappedId->GetBytecodeStartPosition());
                    auto hashTypePosIt = destinationBytecodeTypeHashMap.find(typeHash);
                    if (hashTypePosIt != destinationBytecodeTypeHashMap.end())
                    {
                        spv::Id idOfSameTypeFromDestinationBytecode = hashTypePosIt->second.first;

                        if (idOfSameTypeFromDestinationBytecode == spvUndefinedId)
                        {
                            return error(string("Merge shaders. hashmap refers to an invalid Id"));
                        }

                        //The type already exists in the destination bytecode, we can simply remap to it
                        mappingResolved = true;
                        finalRemapTable[unmappedId] = idOfSameTypeFromDestinationBytecode;
                    }
                    break;
                }

                case ObjectInstructionTypeEnum::Variable:
                {
                    //this is a variable from destination bytecode, we retrieve its id in the destination bytecode using its declaration name
                    VariableInstruction* variable = this->GetVariableByName(objectFromUnmappedId->GetName());
                    if (variable == nullptr)
                        return error(string("Merge shaders. No variable exists in destination bytecode with the name: ") + objectFromUnmappedId->GetName());

                    //remap the unmapped ID to the destination variable
                    mappingResolved = true;
                    finalRemapTable[unmappedId] = variable->GetId();
                    break;
                }

                case ObjectInstructionTypeEnum::Function:
                {
                    //this is a function from destination bytecode, we retrieve its id in the destination bytecode using its declaration name and shader owner
                    if (objectFromUnmappedId->GetShaderOwner() == nullptr)
                        return error(string("The function does not have a shader owner: ") + objectFromUnmappedId->GetName());

                    ShaderClassData* shader = this->GetShaderByName(objectFromUnmappedId->GetShaderOwner()->GetName());
                    if (shader == nullptr)
                        return error(string("No shader exists in destination bytecode with the name: ") + objectFromUnmappedId->GetShaderOwner()->GetName());
                    FunctionInstruction* function = shader->GetFunctionByName(objectFromUnmappedId->GetName());
                    if (function == nullptr)
                        return error(string("Shader \"") + shader->GetName() + string("\" does not have a function named \"") + objectFromUnmappedId->GetName() + string("\""));

                    //remap the unmapped ID to the destination variable
                    mappingResolved = true;
                    finalRemapTable[unmappedId] = function->GetId();
                    break;
                }

                case ObjectInstructionTypeEnum::Shader:
                {
                    //This is a shader from destination bytecode, we simply retrieve its Ids
                    ShaderClassData* shader = this->GetShaderByName(objectFromUnmappedId->GetName());
                    if (shader == nullptr)
                        return error(string("Merge shaders. No shader exists in destination bytecode with the name: ") + objectFromUnmappedId->GetName());

                    //remap the unmapped ID to the destination variable
                    mappingResolved = true;
                    finalRemapTable[unmappedId] = shader->GetId();
                    break;
                }

                case ObjectInstructionTypeEnum::HeaderProperty:
                {
                    //import an external lib if we don't already have it
                    HeaderPropertyInstruction* hearderProp = this->GetHeaderPropertyInstructionByOpCodeAndName(objectFromUnmappedId->GetOpCode(), objectFromUnmappedId->GetName());
                    if (hearderProp != nullptr)
                        finalRemapTable[unmappedId] = hearderProp->GetId();
                    else
                    {
                        bytecodeToMerge.CopyInstructionToVector(vecHeaderPropertiesToMerge, objectFromUnmappedId->GetBytecodeStartPosition());
                        finalRemapTable[unmappedId] = newId++;
                        listAllNewIdMerged[unmappedId] = true;
                    }

                    mappingResolved = true;
                    break;
                }

                default:
                    return error(string("Merge shaders. Invalid object. unable to remap unmapped id: ") + to_string(unmappedId));
            }

            if (mappingResolved)
            {
                listUnmappedIdsToProcess.pop_back();
            }
            else
            {
                if (mergeTypeOrConst)
                {
                    //The type doesn't exist yet, we will copy the full instruction, but only after checking that all depending IDs are mapped as well
                    bytecodeToCheckForUnmappedIds.clear();
                    bytecodeToMerge.CopyInstructionToVector(bytecodeToCheckForUnmappedIds, objectFromUnmappedId->GetBytecodeStartPosition());
                    listIds.clear();
                    if (!parseInstruction(bytecodeToCheckForUnmappedIds, 0, opCode, wordCount, typeId, resultId, listIds, errorMsg)){
                        error(errorMsg);
                        return error("Error parsing bytecodeToCheckForUnmappedIds instructions");
                    }

                    if (typeId != spvUndefinedId) listIds.push_back(typeId);
                    listIdsLen = listIds.size();
                    bool canAddTheInstruction = true;
                    for (unsigned int i = 0; i < listIdsLen; ++i)
                    {
                        const spv::Id anotherId = listIds[i];
#ifdef XKSLANG_DEBUG_MODE
                        if (anotherId == unmappedId) return error(string("anotherId == unmappedId: ") + to_string(anotherId) + string(". This should be impossible (bytecode is invalid)"));
#endif
                        if (finalRemapTable[anotherId] == spvUndefinedId)
                        {
                            //we add anotherId to the list of Ids to process (we're depending on it)
                            listUnmappedIdsToProcess.push_back(pairIdPos(anotherId, -1));
                            canAddTheInstruction = false;
                        }
                    }

                    if (canAddTheInstruction)
                    {
                        //the instruction is not depending on another unmapped IDs anymore, we can copy it
                        finalRemapTable[unmappedId] = newId++;
                        listUnmappedIdsToProcess.pop_back();
                        bytecodeToMerge.CopyInstructionToVector(bytecodeWithExtraTypesToMerge, objectFromUnmappedId->GetBytecodeStartPosition());

                        listAllNewIdMerged[unmappedId] = true;
                    }
                }
            }
        }  //end while (listUnmappedIdsToProcess.size() > 0)
    }  //end block

    //Add the extra types we merged at the beginning of our vec of type/const/variable
    if (bytecodeWithExtraTypesToMerge.size() > 0)
    {
        vecTypesConstsAndVariablesToMerge.insert(vecTypesConstsAndVariablesToMerge.begin(), bytecodeWithExtraTypesToMerge.begin(), bytecodeWithExtraTypesToMerge.end());       
    }

    //=============================================================================================================
    //=============================================================================================================
    // get all name, decoration and XKSL data for the new IDs we need to merge
    {
        unsigned int start = bytecodeToMerge.header_size;
        const unsigned int end = bytecodeToMerge.spv.size();
        while (start < end)
        {
            unsigned int wordCount = bytecodeToMerge.asWordCount(start);
            spv::Op opCode = bytecodeToMerge.asOpCode(start);

            switch (opCode)
            {
                case spv::OpName:
                {
                    //We rename the OpName with the prefix (for debug purposes)
                    const spv::Id id = bytecodeToMerge.asId(start + 1);
                    if (listAllNewIdMerged[id])
                    {
                        if (listIdsWhereToAddNamePrefix[id])
                        {
                            //disassemble and reassemble the instruction with a name updated with the prefix
                            string strUpdatedName(allInstancesPrefixToAdd + bytecodeToMerge.literalString(start + 2));
                            spv::Instruction nameInstr(opCode);
                            nameInstr.addIdOperand(id);
                            nameInstr.addStringOperand(strUpdatedName.c_str());
                            nameInstr.dump(vecNamesAndDecorateToMerge);
                        }
                        else
                        {
                            bytecodeToMerge.CopyInstructionToVector(vecNamesAndDecorateToMerge, start);
                        }
                    }
                    break;
                }
                
                case spv::OpMemberName:
                case spv::OpDecorate:
                case spv::OpMemberDecorate:
                {
                    const spv::Id id = bytecodeToMerge.asId(start + 1);
                    if (listAllNewIdMerged[id])
                    {
                        bytecodeToMerge.CopyInstructionToVector(vecNamesAndDecorateToMerge, start);
                    }
                    break;
                }
                
                case spv::OpDeclarationName:
                {
                    //We update the declaration name only for shader classes
                    const spv::Id id = bytecodeToMerge.asId(start + 1);
                    if (listAllNewIdMerged[id])
                    {
                        bool addPrefix = false;
                        if (listIdsWhereToAddNamePrefix[id])
                        {
                            ShaderClassData* shader = bytecodeToMerge.GetShaderById(id);
                            if (shader != nullptr) addPrefix = true;
                        }

                        if (addPrefix)
                        {
                            //disassemble and reassemble the instruction with a name updated with the prefix
                            string strUpdatedName(allInstancesPrefixToAdd + bytecodeToMerge.literalString(start + 2));
                            spv::Instruction nameInstr(opCode);
                            nameInstr.addIdOperand(id);
                            nameInstr.addStringOperand(strUpdatedName.c_str());
                            nameInstr.dump(vecXkslDecoratesToMerge);
                        }
                        else
                        {
                            bytecodeToMerge.CopyInstructionToVector(vecXkslDecoratesToMerge, start);
                        }
                    }
                    break;
                }

                case spv::OpBelongsToShader:
                case spv::OpShaderInheritance:
                case spv::OpShaderCompositionDeclaration:
                case spv::OpShaderCompositionInstance:
                case spv::OpMethodProperties:
                case spv::OpGSMethodProperties:
                case spv::OpMemberProperties:
                case spv::OpCBufferMemberProperties:
                case spv::OpMemberSemanticName:
                {
                    const spv::Id id = bytecodeToMerge.asId(start + 1);
                    if (listAllNewIdMerged[id])
                    {
                        bytecodeToMerge.CopyInstructionToVector(vecXkslDecoratesToMerge, start);
                    }
                    break;
                }
            }

            start += wordCount;
        }
    }

    //=============================================================================================================
    //=============================================================================================================
    //remap IDs for all mergable types / variables / consts / functions / extImport
    if (vecHeaderPropertiesToMerge.size() > 0)
    {
        if (!remapAllIds(vecHeaderPropertiesToMerge, 0, vecHeaderPropertiesToMerge.size(), finalRemapTable))
            return error("remapAllIds failed on vecHeaderPropertiesToMerge");
    }
    if (!remapAllIds(vecTypesConstsAndVariablesToMerge, 0, vecTypesConstsAndVariablesToMerge.size(), finalRemapTable))
        return error("remapAllIds failed on vecTypesConstsAndVariablesToMerge");
    if (!remapAllIds(vecXkslDecoratesToMerge, 0, vecXkslDecoratesToMerge.size(), finalRemapTable))
        return error("remapAllIds failed on vecXkslDecoratesToMerge");
    if (!remapAllIds(vecNamesAndDecorateToMerge, 0, vecNamesAndDecorateToMerge.size(), finalRemapTable))
        return error("remapAllIds failed on vecNamesAndDecorateToMerge");
    if (!remapAllIds(vecFunctionsToMerge, 0, vecFunctionsToMerge.size(), finalRemapTable))
        return error("remapAllIds failed on vecFunctionsToMerge");
    //vecFunctionsToMerge.processOnFullBytecode(
    //    spx_inst_fn_nop,
    //    [&](spv::Id& id)
    //    {
    //        spv::Id newId = finalRemapTable[id];
    //        if (newId == spvUndefinedId) error(string("Invalid remapper Id: ") + to_string(id));
    //        else id = newId;
    //    }
    //);

    //=============================================================================================================
    //=============================================================================================================
    //merge all types / variables / consts in the current bytecode
    setBound(newId);

    //Find the best positions in the bytecode where to merge the new stuff
    unsigned int posToInsertNewHeaderProrerties = header_size;
    unsigned int posToInsertNewNamesAndXkslDecorates = header_size;
    unsigned int posToInsertNewTypesAndConsts = header_size;
    unsigned int posToInsertNewFunctions = spv.size();
    bool firstFunc = true;
    bool firstType = true;
    for (auto ito = listAllObjects.begin(); ito != listAllObjects.end(); ito++)
    {
        ObjectInstructionBase* obj = *ito;
        if (obj == nullptr) continue;

        switch (obj->GetKind())
        {
            case ObjectInstructionTypeEnum::HeaderProperty:
                if (posToInsertNewHeaderProrerties < obj->GetBytecodeEndPosition())
                {
                    posToInsertNewHeaderProrerties = obj->GetBytecodeEndPosition();
                    if (posToInsertNewTypesAndConsts < posToInsertNewHeaderProrerties) posToInsertNewTypesAndConsts = posToInsertNewHeaderProrerties;
                    if (posToInsertNewNamesAndXkslDecorates < posToInsertNewHeaderProrerties) posToInsertNewNamesAndXkslDecorates = posToInsertNewHeaderProrerties;
                }
                break;
            case ObjectInstructionTypeEnum::Type:
            case ObjectInstructionTypeEnum::Variable:
            case ObjectInstructionTypeEnum::Shader:
            case ObjectInstructionTypeEnum::Const:
                if (firstType || posToInsertNewNamesAndXkslDecorates > obj->GetBytecodeStartPosition())
                {
                    posToInsertNewNamesAndXkslDecorates = obj->GetBytecodeStartPosition();
                    if (posToInsertNewTypesAndConsts < posToInsertNewNamesAndXkslDecorates) posToInsertNewTypesAndConsts = posToInsertNewNamesAndXkslDecorates;
                }
                firstType = false;
                break;
            case ObjectInstructionTypeEnum::Function:
                if (firstFunc || posToInsertNewTypesAndConsts > obj->GetBytecodeStartPosition())
                {
                    posToInsertNewTypesAndConsts = obj->GetBytecodeStartPosition();
                }
                firstFunc = false;
                break;
        }
    }
    if (posToInsertNewTypesAndConsts > posToInsertNewFunctions) posToInsertNewTypesAndConsts = posToInsertNewFunctions;
    if (posToInsertNewNamesAndXkslDecorates > posToInsertNewTypesAndConsts) posToInsertNewNamesAndXkslDecorates = posToInsertNewTypesAndConsts;
    if (posToInsertNewHeaderProrerties > posToInsertNewNamesAndXkslDecorates) posToInsertNewHeaderProrerties = posToInsertNewNamesAndXkslDecorates;

    unsigned int posToInsertNewNames = posToInsertNewNamesAndXkslDecorates;
    unsigned int posToInsertNewXkslDecorates = posToInsertNewNamesAndXkslDecorates;
    //We can refine posToInsertNewNames to put it behind all previous XkslDecorate, as long as we leave it between [posToInsertNewHeaderProrerties, posToInsertNewXkslDecorates]
    {
        unsigned int start = posToInsertNewHeaderProrerties;
        const unsigned int end = posToInsertNewXkslDecorates;
        while (start < end)
        {
            unsigned int wordCount = asWordCount(start);
            spv::Op opCode = asOpCode(start);

            switch (opCode) {
                case spv::OpDeclarationName:
                case spv::OpShaderInheritance:
                case spv::OpBelongsToShader:
                case spv::OpShaderCompositionDeclaration:
                case spv::OpShaderCompositionInstance:
                case spv::OpMethodProperties:
                case spv::OpGSMethodProperties:
                case spv::OpMemberProperties:
                case spv::OpCBufferMemberProperties:
                case spv::OpMemberSemanticName:
                {
                    posToInsertNewNames = start;
                    start = end;
                    break;
                }
            }
            start += wordCount;
        }
    }

    //=============================================================================================================
    // merge all data in the destination bytecode
    //merge functions
    spv.insert(spv.begin() + posToInsertNewFunctions, vecFunctionsToMerge.begin(), vecFunctionsToMerge.end());
    //Merge types and variables (we need to merge new types AFTER all previous types)
    spv.insert(spv.begin() + posToInsertNewTypesAndConsts, vecTypesConstsAndVariablesToMerge.begin(), vecTypesConstsAndVariablesToMerge.end());
    //Merge names and decorates
    spv.insert(spv.begin() + posToInsertNewXkslDecorates, vecXkslDecoratesToMerge.begin(), vecXkslDecoratesToMerge.end());
    spv.insert(spv.begin() + posToInsertNewNames, vecNamesAndDecorateToMerge.begin(), vecNamesAndDecorateToMerge.end());
    //merge ext import
    spv.insert(spv.begin() + posToInsertNewHeaderProrerties, vecHeaderPropertiesToMerge.begin(), vecHeaderPropertiesToMerge.end());

    //destination bytecode has been updated: reupdate all maps
    UpdateAllMaps();

    //Set reference between shaders merged and the clone shaders from destination bytecode
    for (unsigned int is = 0; is<listShadersToMerge.size(); ++is)
    {
        ShaderClassData* shaderToMerge = listShadersToMerge[is].shader;
        spv::Id clonedShaderId = finalRemapTable[shaderToMerge->GetId()];
        ShaderClassData* clonedShader = this->GetShaderById(clonedShaderId);
        if (clonedShader == nullptr) return error(string("Cannot retrieve the merged shader: ") + to_string(clonedShaderId));
        shaderToMerge->tmpClonedShader = clonedShader;
    }

    //update overriden references to merged functions
    for (unsigned int i = 0; i < listOverridenFunctionMergedToBeRemappedWithMergedFunctions.size(); ++i)
    {
        spv::Id functionOverridenId = finalRemapTable[listOverridenFunctionMergedToBeRemappedWithMergedFunctions[i].first];
        spv::Id functionOverridingId = finalRemapTable[listOverridenFunctionMergedToBeRemappedWithMergedFunctions[i].second];
        FunctionInstruction* functionOverriden = this->GetFunctionById(functionOverridenId);
        FunctionInstruction* functionOverriding = this->GetFunctionById(functionOverridingId);
        if (functionOverriden == nullptr) return error(string("Cannot retrieve the merged function: ") + to_string(functionOverridenId));
        if (functionOverriding == nullptr) return error(string("Cannot retrieve the merged function: ") + to_string(functionOverridingId));
        functionOverriden->SetOverridingFunction(functionOverriding);
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

bool SpxStreamRemapper::SetBytecode(const SpxBytecode& bytecode)
{
    const vector<uint32_t>& spx = bytecode.getBytecodeStream();
    spv.clear();
    spv.insert(spv.end(), spx.begin(), spx.end());

    if (!ValidateSpxBytecodeAndData())
        return error(string("Invalid bytecode: ") + bytecode.GetName());

    return true;
}

bool SpxStreamRemapper::ValidateHeader()
{
    if (spv.size() < header_size) {
        return error("file too short");
    }

    if (magic() != spv::MagicNumber) {
        error("bad magic number");
    }

    // field 1 = version
    // field 2 = generator magic
    // field 3 = result <id> bound

    if (schemaNum() != 0) {
        error("bad schema, must be 0");
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

bool SpxStreamRemapper::ProcessBytecodeAndDataSanityCheck()
{  
    vector<unsigned int> listAllTypesConstsAndVariablesPosition;
    vector<unsigned int> listAllResultIdsPosition;
    listAllTypesConstsAndVariablesPosition.resize(bound(), 0);
    listAllResultIdsPosition.resize(bound(), 0);

    vector<spv::Id> listIds;
    unsigned int start = header_size;
    const unsigned int end = spv.size();
    while (start < end)
    {
        unsigned int wordCount = asWordCount(start);
        spv::Op opCode = asOpCode(start);

        if (wordCount == 0) return error("wordCount is 0 for instruction at pos: " + to_string(start));

        bool hasAType = false;
        spv::Id instructionTypeId = 0;
        bool hasAResultId = false;
        spv::Id instructionResultId = 0;
        
        //if the instruction has a type, check that this type is already defined
        if (spv::InstructionDesc[opCode].hasType())
        {
            hasAType = true;
            unsigned int posTypeId = start + 1;
            if (posTypeId >= spv.size()) return error("TypeId position is out of bound for instruction at pos: " + to_string(start));

            instructionTypeId = spv[start + 1];
            if (instructionTypeId == 0 || instructionTypeId >= listAllTypesConstsAndVariablesPosition.size()) return error("instruction TypeId is out of bound: " + to_string(instructionTypeId));
            if (listAllTypesConstsAndVariablesPosition[instructionTypeId] == 0) return error("an instruction is using an undefined type: " + to_string(instructionTypeId));
        }

        //if the instruction has a resultId, check this resultId
        if (spv::InstructionDesc[opCode].hasResult())
        {
            hasAResultId = true;
            unsigned int posResultId = hasAType ? start + 2 : start + 1;
            if (posResultId >= spv.size()) return error("ResultId position is out of bound for instruction at pos: " + to_string(start));

            instructionResultId = spv[posResultId];
            if (instructionResultId == 0 || instructionResultId >= listAllResultIdsPosition.size()) return error("instruction ResultId is out of bound: " + to_string(instructionResultId));
            if (listAllResultIdsPosition[instructionResultId] != 0) return error("the instruction result Id is already defined: " + to_string(instructionResultId));
            listAllResultIdsPosition[instructionResultId] = start;
        }

        //Check more details for instruction defining types/consts/variables
        bool isOpTypeConstOrVariableInstruction = false;
        spv::Id typeConstVariableResultId = 0;
        if (isConstOp(opCode)) {
            if (start + 2 >= spv.size()) return error("resultId position is out of bound for instruction at pos: " + to_string(start));
            isOpTypeConstOrVariableInstruction = true;
            typeConstVariableResultId = asId(start + 2);
        }
        else if (isTypeOp(opCode)) {
            if (start + 1 >= spv.size()) return error("resultId position is out of bound for instruction at pos: " + to_string(start));
            isOpTypeConstOrVariableInstruction = true;
            typeConstVariableResultId = asId(start + 1);
        }
        else if (isVariableOp(opCode)) {
            if (start + 2 >= spv.size()) return error("resultId position is out of bound for instruction at pos: " + to_string(start));
            isOpTypeConstOrVariableInstruction = true;
            typeConstVariableResultId = asId(start + 2);
        }

        //for every types/consts/variables: if the instruction is refering to other IDs, check that those IDs are already defined.
        if (isOpTypeConstOrVariableInstruction)
        {
            if (typeConstVariableResultId == 0 || typeConstVariableResultId != instructionResultId)
                return error("typeConstVariableResultId is inconsistent with the instruction resultId: " + to_string(typeConstVariableResultId));

            if (typeConstVariableResultId >= listAllTypesConstsAndVariablesPosition.size()) return error("resultId is out of bound: " + to_string(typeConstVariableResultId));

            if (listAllTypesConstsAndVariablesPosition[typeConstVariableResultId] != 0) return error("the Id is already used by another type: " + to_string(typeConstVariableResultId));
            listAllTypesConstsAndVariablesPosition[typeConstVariableResultId] = start;

            if (listAllObjects[typeConstVariableResultId] == nullptr) return error("The type, const or variable has no object data defined. Id: " + to_string(typeConstVariableResultId));

            //Check that all IDs refers to by the type/const/variable are already defined
            {
                unsigned int wordCountBis;
                spv::Op opCodeBis;
                spv::Id typeId;
                spv::Id resultId;
                listIds.clear();
                if (!parseInstruction(start, opCodeBis, wordCountBis, typeId, resultId, listIds)) return error("Error parsing the instruction");
                if (opCodeBis != opCode) return error("Inconsistency in the opCode parsed");
                if (wordCountBis != wordCount) return error("Inconsistency in the wordCount parsed");

                if (typeId != spvUndefinedId) listIds.push_back(typeId);
                for (unsigned k = 0; k < listIds.size(); ++k)
                {
                    spv::Id anId = listIds[k];

                    if (anId >= listAllTypesConstsAndVariablesPosition.size()) return error("anId is out of bound: " + to_string(anId));
                    if (listAllTypesConstsAndVariablesPosition[anId] == 0) return error("a type refers to an Id not defined yet: " + to_string(anId));
                }
            }
        }

        start += wordCount;
    }

    return true;
}

bool SpxStreamRemapper::ValidateSpxBytecodeAndData()
{
    if (!ValidateHeader()) return error("Failed to validate the header");

#ifdef XKSLANG_DEBUG_MODE
    //Debug sanity check: make sure that 2 shaders or 2 variables does not share the same name
    unordered_map<string, int> shaderVariablesDeclarationName;
    unordered_map<string, int> shadersFunctionsDeclarationName;
    unordered_map<string, int> shadersDeclarationName;

    for (auto itsh = vecAllShaders.begin(); itsh != vecAllShaders.end(); itsh++)
    {
        ShaderClassData* aShader = *itsh;
        if (shadersDeclarationName.find(aShader->GetName()) != shadersDeclarationName.end())
                return error(string("A shader already exists with the name: ") + aShader->GetName());
        shadersDeclarationName[aShader->GetName()] = 1;

        //2 different shaders cannot share an identical variable name
        for (auto itt = aShader->shaderTypesList.begin(); itt != aShader->shaderTypesList.end(); itt++)
        {
            ShaderTypeData* type = *itt;
            if (shaderVariablesDeclarationName.find(type->variable->GetName()) != shaderVariablesDeclarationName.end())
                return error(string("A variable already exists with the name: ") + type->variable->GetName());
            shaderVariablesDeclarationName[type->variable->GetName()] = 1;
        }
    }
#endif

    return true;
}

bool SpxStreamRemapper::AddComposition(const string& shaderName, const string& variableName, SpxStreamRemapper* source, vector<string>& msgs)
{
    if (status != SpxRemapperStatusEnum::WaitingForMixin) {
        return error("Invalid remapper status");
    }
    status = SpxRemapperStatusEnum::MixinInProgress;

    //===================================================================================================================
    //===================================================================================================================
    //Find the shader to instantiate from source bytecode
    ShaderClassData* shaderCompositionTarget = this->GetShaderByName(shaderName);
    if (shaderCompositionTarget == nullptr) return error(string("No shader exists in destination bytecode with the name: ") + shaderName);

    //find the shader's composition target
    ShaderComposition* compositionTarget = shaderCompositionTarget->GetShaderCompositionByName(variableName);
    if (compositionTarget == nullptr) return error(string("No composition exists in shader: ") + shaderName + string(", with the name: ") + variableName);

    if (!compositionTarget->isArray && compositionTarget->countInstances > 0) return error(string("The composition has already been instanciated"));

    int compositionTargetId = compositionTarget->compositionShaderId;

    ShaderClassData* shaderTypeToInstantiate = compositionTarget->shaderType;
    if (shaderTypeToInstantiate == nullptr) return error("The composition does not define any shader type");

    ShaderClassData* mainShaderTypeMergedToMergeFromSource = source->GetShaderByName(shaderTypeToInstantiate->GetName());
    if (mainShaderTypeMergedToMergeFromSource == nullptr) return error(string("No shader exists in the source mixer with the name: ") + shaderTypeToInstantiate->GetName());

    //===================================================================================================================
    //===================================================================================================================
    //Get the list of all shaders to instantiate or merge
    vector<ShaderClassData*> listShader;
    vector<ShaderClassData*> shadersFullDependencies;
    listShader.push_back(mainShaderTypeMergedToMergeFromSource);
    if (!SpxStreamRemapper::GetShadersFullDependencies(source, listShader, shadersFullDependencies)) {
        source->copyMessagesTo(errorMessages);
        return error(string("Failed to get the shaders dependencies"));
    }

    vector<ShaderToMergeData> listShadersToMerge;
    for (auto its = shadersFullDependencies.begin(); its != shadersFullDependencies.end(); its++)
    {
        ShaderClassData* aShaderToMerge = *its;
        if (aShaderToMerge->dependencyType == ShaderClassData::ShaderDependencyTypeEnum::StaticFunctionCall)
        {
            //if calling a shader static function only: we don't make a new instance
            if (this->GetShaderByName(aShaderToMerge->GetName()) == nullptr)
            {
                listShadersToMerge.push_back(ShaderToMergeData(aShaderToMerge, false));
            }
        }
        else
        {
            //we will make a new instance of the shader in the destination bytecode
            listShadersToMerge.push_back(ShaderToMergeData(aShaderToMerge, true));
        }
    }

    //===================================================================================================================
    //===================================================================================================================
    //prefix to rename the shader, its variables and functions
    //Since we're using the shader name to identify them and retrieve them throughout mixin,
    //we need to make sure that the new instances will have a unique name and can't conflict with existing shader or previously created instances
    unsigned int prefixId = GetUniqueMergeOperationId();
    string namePrefix = string("o") + to_string(prefixId) + string("S") + to_string(compositionTarget->compositionShaderOwner->GetId()) + string("C") + to_string(compositionTargetId) + string("_");

    //Merge (duplicate) all shaders targeted by by the composition into the current bytecode
    if (!MergeShadersIntoBytecode(*source, listShadersToMerge, namePrefix))
    {
        error(string("failed to clone the shader for the composition: ") + compositionTarget->compositionShaderOwner->GetName() + string(".") + compositionTarget->variableName);
        return false;
    }
    ShaderClassData* mainShaderTypeMerged = mainShaderTypeMergedToMergeFromSource->tmpClonedShader;
    if (mainShaderTypeMerged == nullptr)
        return error(string("Merge shader function is expected to return a reference on the merged shader"));

    //===================================================================================================================
    //After having instantiated a shader, we set the original shader name to all cloned cbuffers having a STAGE property (so that we can merge them later on)
    for (auto its = listShadersToMerge.begin(); its != listShadersToMerge.end(); its++)
    {
        const ShaderToMergeData& shaderInstantiated = *its;
        if (shaderInstantiated.instantiateShader)
        {
            ShaderClassData* originalShader = shaderInstantiated.shader;
            ShaderClassData* instantiatedShader = originalShader->tmpClonedShader;
            if (instantiatedShader == nullptr) return error(string("Cannot retrieve the instantiated shader after having added some compositions"));
            string originalShaderName = originalShader->GetName();

            for (auto itt = instantiatedShader->shaderTypesList.begin(); itt != instantiatedShader->shaderTypesList.end(); itt++)
            {
                ShaderTypeData* aShaderType = *itt;
                if (aShaderType->isCBufferType())
                {
                    CBufferTypeData* cbuffer = aShaderType->GetCBufferData();
                    if (cbuffer->isStage)
                    {
                        cbuffer->shaderOwnerName = originalShaderName;
                    }
                }
            }
        }
    }

    //===================================================================================================================
    //===================================================================================================================
    // - update our composition data, and add the instances instruction
    {
        //Analyse the function's bytecode, to add all new functions called
        unsigned int start = header_size;
        const unsigned int end = spv.size();
        bool compositionUpdated = false;
        while (start < end)
        {
            unsigned int wordCount = asWordCount(start);
            spv::Op opCode = asOpCode(start);

            switch (opCode)
            {
                case spv::OpShaderCompositionDeclaration:
                {
                    spv::Id shaderId = asId(start + 1);
                    int compositionId = asLiteralValue(start + 2);

                    if (shaderId == shaderCompositionTarget->GetId() && compositionId == compositionTargetId)
                    {
                        //increase number of instances
                        compositionTarget->countInstances++;
                        setLiteralValue(start + 5, compositionTarget->countInstances);

                        //Add the instance information in the bytecode
                        spv::Id mergedShaderTypeId = mainShaderTypeMerged->GetId();
                        spv::Instruction compInstanceInstr(spv::OpShaderCompositionInstance);
                        compInstanceInstr.addIdOperand(shaderId);
                        compInstanceInstr.addImmediateOperand(compositionId);
                        compInstanceInstr.addImmediateOperand(compositionTarget->countInstances - 1);
                        compInstanceInstr.addIdOperand(mergedShaderTypeId);

                        vector<unsigned int> instructionBytecode;
                        compInstanceInstr.dump(instructionBytecode);
                        spv.insert(spv.begin() + (start + wordCount), instructionBytecode.begin(), instructionBytecode.end());

                        compositionUpdated = true;
                        start = end;  //can stop here: there is only one composition to update
                    }
                    break;
                }
            }
            start += wordCount;
        }

        if (!compositionUpdated) return error("The composition has not been updated");

        if (!UpdateAllMaps()) return error("Failed to update all maps");
    }

    if (errorMessages.size() > 0) return false;
    status = SpxRemapperStatusEnum::WaitingForMixin;
    return true;
}


void SpxStreamRemapper::GetStagesPipeline(vector<ShadingStageEnum>& pipeline)
{
    pipeline = { ShadingStageEnum::Vertex, ShadingStageEnum::TessControl, ShadingStageEnum::TessEvaluation, ShadingStageEnum::Geometry , ShadingStageEnum::Pixel };
}

bool SpxStreamRemapper::InitializeCompilationProcess(vector<XkslMixerOutputStage>& outputStages)
{
    //if (status != SpxRemapperStatusEnum::AAA) return error("Invalid remapper status");
    status = SpxRemapperStatusEnum::MixinBeingCompiled_Initialized;

    if (outputStages.size() == 0) return error("no output stages defined");
    if (!ValidateHeader()) return error("Failed to validate the header");

#ifdef XKSLANG_DEBUG_MODE
    //check that the output stages are in the correct order
    vector<ShadingStageEnum> stagePipeline;
    SpxStreamRemapper::GetStagesPipeline(stagePipeline);

    int lastStageMatched = -1;
    for (unsigned int i = 0; i<outputStages.size(); ++i)
    {
        ShadingStageEnum outputStage = outputStages[i].outputStage->stage;

        lastStageMatched++;
        while (lastStageMatched < (int)stagePipeline.size())
        {
            if (stagePipeline[lastStageMatched] == outputStage) break;
            lastStageMatched++;
        }

        if (lastStageMatched == stagePipeline.size())
            return error(string("The output stage is unknown or not in the correct order: ") + GetShadingStageLabel(outputStage));
    }

#endif

    //===================================================================================================================
    //For each output stages, we search the entryPoint function in the bytecode
    for (unsigned int iStage = 0; iStage < outputStages.size(); iStage++)
    {
        FunctionInstruction* entryFunction = GetShaderFunctionForEntryPoint(outputStages[iStage].outputStage->entryPointName);
        if (entryFunction == nullptr) error(string("Entry point not found: ") + outputStages[iStage].outputStage->entryPointName);
        outputStages[iStage].entryFunction = entryFunction;
    }
    if (errorMessages.size() > 0) return false;

    return true;
}

SpxStreamRemapper::ConstInstruction* SpxStreamRemapper::FindConstFromList(const vector<ConstInstruction*>& listConsts, spv::Op opCode, spv::Id typeId, const vector<unsigned int>& values)
{
    for (auto it = listConsts.begin(); it != listConsts.end(); it++)
    {
        ConstInstruction* aConst = *it;

        if (aConst->GetOpCode() == opCode && aConst->GetTypeId() == typeId)
        {
#ifdef XKSLANG_DEBUG_MODE
            //some sanity check
            if (asOpCode(aConst->GetBytecodeStartPosition()) != opCode) { error("Corrupted bytecode: the const opCode is not matching the bytecode"); return nullptr; }
#endif

            unsigned int wordCount = asWordCount(aConst->GetBytecodeStartPosition());
            unsigned int countValues = wordCount - 3;
            if (countValues == values.size())
            {
                bool gotIt = true;

                unsigned int startingPos = aConst->GetBytecodeStartPosition() + 3;
                for (unsigned int k = 0; k < countValues; ++k)
                {
                    if (asLiteralValue(startingPos + k) != values[k])
                    {
                        gotIt = false;
                        break;
                    }
                }

                if (gotIt)
                {
                    return aConst;
                }
            }
        }
    }

    return nullptr;
}

spv::Id SpxStreamRemapper::GetOrCreateTypeDefaultConstValue(spv::Id& newId, TypeInstruction* type, const vector<ConstInstruction*>& listAllConsts,
    vector<spv::Instruction>& listNewConstInstructionsToAdd, int iterationCounter)
{
    if (iterationCounter > 10) { error("Too many nested type (infinite type redirection in the bytecode?)"); return 0; }

    const spv::Op& typeOpCode = type->opCode;
    const spv::Id& typeId = type->GetId();
    spv::Id typeConstId = 0;

#ifdef XKSLANG_DEBUG_MODE
    //some sanity check
    if (asOpCode(type->GetBytecodeStartPosition()) != typeOpCode) {error("Corrupted bytecode: type opcode is not matching the bytecode"); return 0;}
#endif

    spv::Instruction newConstInstruction(spv::OpUndef);

    switch (typeOpCode)
    {
        case spv::OpTypeInt:
        case spv::OpTypeFloat:
        {
            unsigned int width = asLiteralValue(type->GetBytecodeStartPosition() + 2);
            unsigned int countOperands = (width == 16? 1 : width >> 5);

#ifdef XKSLANG_DEBUG_MODE
            if (!(width == 16 || width % 32 == 0)) { error("Invalid width size for the type"); return 0; }
#endif

            newConstInstruction.SetResultTypeAndOpCode(newId++, typeId, spv::OpConstant);
            for (unsigned int k = 0; k < countOperands; k++) newConstInstruction.addImmediateOperand(0);
        }
        break;

        case spv::OpTypeBool:
        {
            newConstInstruction.SetResultTypeAndOpCode(newId++, typeId, spv::OpConstantFalse);
        }
        break;

        case spv::OpTypeMatrix:
        case spv::OpTypeVector:
        case spv::OpTypeArray:
        {
            spv::Id vectorElementTypeId = asId(type->GetBytecodeStartPosition() + 2);

            int countElems = 0;
            if (typeOpCode == spv::OpTypeArray)
            {
                //array size is set with a const instruction
                spv::Id sizeConstTypeId = asLiteralValue(type->GetBytecodeStartPosition() + 3);
                ConstInstruction* constObject = GetConstById(sizeConstTypeId);
                if (constObject == nullptr) { error("cannot get const object for Id: " + to_string(sizeConstTypeId)); return 0; }

                spv::Id constTypeId = constObject->GetTypeId();
                TypeInstruction* constTypeObject = GetTypeById(constTypeId);
                if (constTypeObject == nullptr) { error("no type exist for const typeId: " + to_string(constTypeId)); return 0; }
                spv::Op typeOpCode = asOpCode(constTypeObject->bytecodeStartPosition);
                unsigned int width = asLiteralValue(constTypeObject->bytecodeStartPosition + 2);
                if (typeOpCode != spv::OpTypeInt) { error("the type of the const defining the size of the array has an invalid type (OpTypeInt expected)"); return 0; }
                if (width != 32) { error("the size of the type of the const defining the size of the array is incorrect (32 expected)"); return 0; }

                countElems = asLiteralValue(constObject->GetBytecodeStartPosition() + 3);
            }
            else
            {
                countElems = asLiteralValue(type->GetBytecodeStartPosition() + 3);
            }

#ifdef XKSLANG_DEBUG_MODE
            if (countElems <= 0) { error("Invalid OpTypeVector size"); return 0; }
#endif
            TypeInstruction* vectorElemType = GetTypeById(vectorElementTypeId);
            if (vectorElemType == nullptr) { error("failed to find the vector type for id: " + to_string(vectorElementTypeId)); return 0; }

            spv::Id elemConstValueId = GetOrCreateTypeDefaultConstValue(newId, vectorElemType, listAllConsts, listNewConstInstructionsToAdd, iterationCounter + 1);
            if (elemConstValueId == 0) return 0;
            
            newConstInstruction.SetResultTypeAndOpCode(newId++, typeId, spv::OpConstantComposite);
            for (int k = 0; k < countElems; ++k) newConstInstruction.addIdOperand(elemConstValueId);
        }
        break;

        case spv::OpTypeStruct:
        {
            int wordCount = asWordCount(type->GetBytecodeStartPosition());
            int countElems = wordCount - 2;

#ifdef XKSLANG_DEBUG_MODE
            if (countElems <= 0) { error("Invalid OpTypeStruct size"); return 0; }
#endif

            unsigned int posElemStart = type->GetBytecodeStartPosition() + 2;
            vector<spv::Id> vecStructElemsConstsId;
            for (int k = 0; k < countElems; ++k)
            {
                spv::Id structElemTypeId = asId(posElemStart + k);
                TypeInstruction* structElemType = GetTypeById(structElemTypeId);
                if (structElemType == nullptr) { error("failed to find the struct element type for id: " + to_string(structElemTypeId)); return 0; }

                spv::Id elemConstValueId = GetOrCreateTypeDefaultConstValue(newId, structElemType, listAllConsts, listNewConstInstructionsToAdd, iterationCounter + 1);
                if (elemConstValueId == 0) return 0;
                vecStructElemsConstsId.push_back(elemConstValueId);
            }

            newConstInstruction.SetResultTypeAndOpCode(newId++, typeId, spv::OpConstantComposite);
            for (unsigned int k = 0; k < vecStructElemsConstsId.size(); ++k) newConstInstruction.addIdOperand(vecStructElemsConstsId[k]);
        }
        break;

        default:       
            error("Cannot create a default const value, unprocessed or invalid type");
            break;
    }

    if (newConstInstruction.getOpCode() == spv::OpUndef)
    {
        error("No const instruction has been created");
        return 0;
    }

    //check if a duplicate const already exists in our list of consts
    ConstInstruction* existingConst = FindConstFromList(listAllConsts, newConstInstruction.getOpCode(), newConstInstruction.getTypeId(), newConstInstruction.GetOperands());
    if (existingConst != nullptr)
    {
        newId--;
        typeConstId = existingConst->GetResultId();
    }
    else
    {
        //check that the new const has not already been recursively created in this function

        int index = -1;
        for (unsigned int n = 0; n < listNewConstInstructionsToAdd.size(); n++)
        {
            const spv::Instruction& anInstruction = listNewConstInstructionsToAdd[n];
            if (anInstruction.getOpCode() == newConstInstruction.getOpCode() && anInstruction.getTypeId() == newConstInstruction.getTypeId())
            {
                bool sameInstruction = true;
                unsigned int numOperands = newConstInstruction.getNumOperands();
                if (anInstruction.getNumOperands() == numOperands)
                {
                    for (unsigned int op = 0; op < numOperands; ++op)
                    {
                        if (anInstruction.getImmediateOperand(op) != newConstInstruction.getImmediateOperand(op)) {
                            sameInstruction = false;
                            break;
                        }
                    }
                }

                if (sameInstruction)
                {
                    index = n;
                    break;
                }
            }
        }

        if (index != -1)
        {
            newId--;
            typeConstId = listNewConstInstructionsToAdd[index].getResultId();
        }
        else
        {
            listNewConstInstructionsToAdd.push_back(newConstInstruction);
            typeConstId = newConstInstruction.getResultId();
        }
    }

    return typeConstId;
}

//Remove unused cbuffers, merge used cbuffers havind the same name, take resources type out of the cbuffer
bool SpxStreamRemapper::ProcessCBuffers(vector<XkslMixerOutputStage>& outputStages)
{
    if (status != SpxRemapperStatusEnum::MixinBeingCompiled_StreamReschuffled && status != SpxRemapperStatusEnum::MixinBeingCompiled_StreamsAndCBuffersAnalysed) return error("Invalid remapper status");
    status = SpxRemapperStatusEnum::MixinBeingCompiled_CBuffersValidated;
    bool success = true;

    unsigned int positionFirstOpFunctionInstruction = 0;

    //=========================================================================================================================
    //=========================================================================================================================
    //get ALL SHADERs' cbuffers
    vector<CBufferTypeData*> listAllShaderCBuffers;
    for (auto itsh = vecAllShaders.begin(); itsh != vecAllShaders.end(); itsh++) {
        ShaderClassData* shader = *itsh;
        for (auto it = shader->shaderTypesList.begin(); it != shader->shaderTypesList.end(); it++){
            ShaderTypeData* shaderType = *it;
            if (shaderType->isCBufferType())
            {
                CBufferTypeData* cbufferData = shaderType->type->cbufferData;
#ifdef XKSLANG_DEBUG_MODE
                if (cbufferData == nullptr) { error("a cbuffer type has no cbuffer data: " + shaderType->type->GetName()); break; }
                if (cbufferData->cbufferMembersData != nullptr) return error("a cbuffer members data has already been initialized");
#endif
                cbufferData->correspondingShaderType = shaderType;
                cbufferData->isUsed = false;
                cbufferData->cbufferMembersData = nullptr;
                cbufferData->posOpNameType = -1;
                cbufferData->posOpNameVariable = -1;

                listAllShaderCBuffers.push_back(cbufferData);
            }
        }
    }
    if (listAllShaderCBuffers.size() == 0) return true;  //no cbuffer at all, we can return immediatly

    //flag the USED cbuffers and allocate object necessary to store their data
    vector<CBufferTypeData*> vectorUsedCbuffers;
    vectorUsedCbuffers.resize(bound(), nullptr);
    bool anyCBufferUsed = false;
    {
        for (unsigned int iStage = 0; iStage < outputStages.size(); iStage++)
        {
            XkslMixerOutputStage* outputStage = &(outputStages[iStage]);
            for (auto itcb = outputStage->listCBuffersAccessed.begin(); itcb != outputStage->listCBuffersAccessed.end(); itcb++)
            {
                anyCBufferUsed = true;
                ShaderTypeData* shaderType = *itcb;
                CBufferTypeData* cbufferData = shaderType->type->cbufferData;

#ifdef XKSLANG_DEBUG_MODE
                if (shaderType->type->GetId() >= vectorUsedCbuffers.size()) { error("cbuffer type id is out of bound. Id: " + to_string(shaderType->type->GetId())); break; }
                if (cbufferData->cbufferCountMembers <= 0) { error("invalid count members for cbuffer: " + shaderType->type->GetName()); break; }
#endif

                cbufferData->isUsed = true;
                if (vectorUsedCbuffers[shaderType->type->GetId()] == nullptr)
                {
                    cbufferData->cbufferMembersData = new TypeStructMemberArray();
                    cbufferData->cbufferMembersData->members.resize(cbufferData->cbufferCountMembers);

                    //set both type and variable IDs
                    vectorUsedCbuffers[shaderType->type->GetId()] = cbufferData;
                    vectorUsedCbuffers[shaderType->variable->GetId()] = cbufferData;
                }
            }
        }

        if (errorMessages.size() > 0) success = false;
    }

    //=========================================================================================================================
    //=========================================================================================================================
    //Retrieve information from the bytecode for all USED cbuffers, and their members
    unsigned int posLatestMemberNameOrDecorate = header_size;
    unsigned int posFirstOpNameOrDecorate = spv.size();
    if (success && anyCBufferUsed)
    {
        unsigned int start = header_size;
        const unsigned int end = spv.size();
        while (start < end)
        {
            unsigned int wordCount = asWordCount(start);
            spv::Op opCode = asOpCode(start);

            switch (opCode)
            {
                case spv::OpName:
                {
                    if (posFirstOpNameOrDecorate > start) posFirstOpNameOrDecorate = start;

                    spv::Id id = asId(start + 1);
                    if (vectorUsedCbuffers[id] != nullptr)
                    {
                        //ShaderTypeData* cbuffer = fdsfsdf;
                        CBufferTypeData* cbufferData = vectorUsedCbuffers[id];

                        if (start + wordCount > posLatestMemberNameOrDecorate) posLatestMemberNameOrDecorate = start + wordCount;
                        
                        if (cbufferData->correspondingShaderType->type->GetId() == id) cbufferData->posOpNameType = start;
                        else if (cbufferData->correspondingShaderType->variable->GetId() == id) cbufferData->posOpNameVariable = start;
                    }
                    break;
                }

                /*case spv::OpMemberProperties:
                {
                    const spv::Id id = asId(start + 1);
                    if (vectorUsedCbuffers[id] != nullptr)
                    {
                        const ShaderTypeData* cbuffer = vectorUsedCbuffers[id];
                        CBufferTypeData* cbufferData = cbuffer->type->cbufferData;

                        unsigned int index = asLiteralValue(start + 2);
                        string memberName = literalString(start + 3);
#ifdef XKSLANG_DEBUG_MODE
                        if (cbuffer->type->GetId() != id) { error("Invalid instruction Id"); break; }
                        if (index >= cbufferData->cbufferMembersData->countMembers()) { error("Invalid member index"); break; }
#endif
                        //we're only interested by the member's stage property
                        int countProperties = wordCount - 3;
                        for (int a = 0; a < countProperties; ++a)
                        {
                            int prop = asLiteralValue(start + 3 + a);
                            switch (prop)
                            {
                                case spv::XkslPropertyEnum::PropertyStage:
                                    cbufferData->cbufferMembersData->members[index].isStage = true;
                                    break;
                            }
                        }
                    }
                    break;
                }*/

                case spv::OpMemberName:
                {
                    spv::Id id = asId(start + 1);
                    if (vectorUsedCbuffers[id] != nullptr)
                    {
                        CBufferTypeData* cbufferData = vectorUsedCbuffers[id];

                        if (start + wordCount > posLatestMemberNameOrDecorate) posLatestMemberNameOrDecorate = start + wordCount;

                        unsigned int index = asLiteralValue(start + 2);
                        string memberName = literalString(start + 3);
#ifdef XKSLANG_DEBUG_MODE
                        if (cbufferData->correspondingShaderType->type->GetId() != id) { error("Invalid instruction Id"); break; }
                        if (index >= cbufferData->cbufferMembersData->countMembers()) { error("Invalid member index"); break; }
#endif
                        cbufferData->cbufferMembersData->members[index].declarationName = memberName;
                    }
                    break;
                }

                case spv::OpCBufferMemberProperties:
                {
                    const spv::Id typeId = asId(start + 1);

                    if (vectorUsedCbuffers[typeId] != nullptr)
                    {
                        CBufferTypeData* cbufferData = vectorUsedCbuffers[typeId];

                        if (start + wordCount > posLatestMemberNameOrDecorate) posLatestMemberNameOrDecorate = start + wordCount;

                        spv::XkslPropertyEnum cbufferType = (spv::XkslPropertyEnum)asLiteralValue(start + 2);
                        spv::XkslPropertyEnum cbufferStage = (spv::XkslPropertyEnum)asLiteralValue(start + 3);
                        unsigned int countMembers = asLiteralValue(start + 4);
                        unsigned int remainingBytes = wordCount - 5;

#ifdef XKSLANG_DEBUG_MODE
                        if (cbufferData->correspondingShaderType->type->GetId() != typeId) { error("Invalid instruction Id"); break; }
                        if (cbufferType != cbufferData->cbufferType) { error("Invalid cbuffer type property"); break; }
                        if (countMembers != cbufferData->cbufferCountMembers) { error("Invalid cbuffer count members property"); break; }
#endif

                        if (countMembers != cbufferData->cbufferMembersData->countMembers()) { error("Inconsistent number of members"); break; }
                        if (remainingBytes != countMembers * 2) { error("OpCBufferMemberProperties instruction has an invalid number of bytes"); break; }
                        
                        for (unsigned int m = 0; m < countMembers; m++)
                        {
                            unsigned int k = start + 5 + (m * 2);
                            unsigned int memberSize = asLiteralValue(k);
                            unsigned int memberAlignment = asLiteralValue(k + 1);

                            cbufferData->cbufferMembersData->members[m].memberSize = memberSize;
                            cbufferData->cbufferMembersData->members[m].memberAlignment = memberAlignment;
                        }
                    }
                    break;
                }

                case spv::OpMemberDecorate:
                {
                    if (posFirstOpNameOrDecorate > start) posFirstOpNameOrDecorate = start;

                    spv::Id id = asId(start + 1);
                    if (vectorUsedCbuffers[id] != nullptr)
                    {
                        CBufferTypeData* cbufferData = vectorUsedCbuffers[id];
                        if (start + wordCount > posLatestMemberNameOrDecorate) posLatestMemberNameOrDecorate = start + wordCount;

                        const unsigned int index = asLiteralValue(start + 2);
                        const spv::Decoration dec = (spv::Decoration)asLiteralValue(start + 3);
#ifdef XKSLANG_DEBUG_MODE
                        if (cbufferData->correspondingShaderType->type->GetId() != id) { error("Invalid instruction Id"); break; }
                        if (index >= cbufferData->cbufferMembersData->countMembers()) { error("Invalid member index"); break; }
#endif
                        if (dec == spv::DecorationOffset)
                        {
#ifdef XKSLANG_DEBUG_MODE
                            if (wordCount <= 4) { error("Invalid wordCount"); break; }
#endif
                            const unsigned int offsetValue = asLiteralValue(start + 4);
                            cbufferData->cbufferMembersData->members[index].memberOffset = offsetValue;
                        }
                        else
                        {
#ifdef XKSLANG_DEBUG_MODE
                            if (wordCount <= 3) { error("Invalid wordCount"); break; }
#endif
                            unsigned int countRemainingBytes = wordCount - 3;
                            std::vector<unsigned int>& listMemberDecoration = cbufferData->cbufferMembersData->members[index].listMemberDecoration;
                            listMemberDecoration.push_back(countRemainingBytes);
                            for (unsigned int i=0; i<countRemainingBytes; i++)
                                listMemberDecoration.push_back(asLiteralValue(start + 3 + i));
                        }
                    }
                    break;
                }

                case spv::OpTypeStruct:
                {
                    spv::Id id = asId(start + 1);
                    if (vectorUsedCbuffers[id] != nullptr)
                    {
                        CBufferTypeData* cbufferData = vectorUsedCbuffers[id];

                        int countMembers = wordCount - 2;
#ifdef XKSLANG_DEBUG_MODE
                        if (cbufferData->correspondingShaderType->type->GetId() != id) { error("Invalid instruction Id"); break; }
                        if (countMembers != cbufferData->cbufferMembersData->members.size()) { error("inconsistent number of members between OpTypeStruct instruction and cbuffer properties"); break; }
#endif
                        for (int m = 0; m < countMembers; ++m)
                        {
                            TypeStructMember& member = cbufferData->cbufferMembersData->members[m];
                            spv::Id memberTypeId = asLiteralValue(start + 2 + m);
                            member.memberTypeId = memberTypeId;

                            //get the member type object
                            TypeInstruction* memberType = GetTypeById(memberTypeId);
                            if (memberType == nullptr) { error("failed to find the member type for memberTypeId: " + to_string(memberTypeId)); break; }
                            member.memberType = memberType;

                            //check if the member type is a resource
                            member.isResourceType = IsResourceType(memberType->opCode);
                        }
                    }
                    break;
                }

                case spv::OpFunction:
                {
                    //all information retrieved at this point: can safely stop here
                    if (positionFirstOpFunctionInstruction == 0) positionFirstOpFunctionInstruction = start;
                    start = end;
                    break;
                }
            }
            start += wordCount;
        }

        if (positionFirstOpFunctionInstruction == 0) positionFirstOpFunctionInstruction = header_size;
        if (posFirstOpNameOrDecorate > posLatestMemberNameOrDecorate) posFirstOpNameOrDecorate = posLatestMemberNameOrDecorate;
        if (errorMessages.size() > 0) success = false;
    }

#ifdef XKSLANG_DEBUG_MODE
    //some sanity check in debug mode
    if (success)
    {
        for (unsigned int i = 0; i < listAllShaderCBuffers.size(); i++)
        {
            CBufferTypeData* cbufferData = listAllShaderCBuffers[i];
            if (cbufferData->cbufferMembersData != nullptr)
            {
                if (cbufferData->cbufferCountMembers != cbufferData->cbufferMembersData->countMembers()) {error("inconsistent number of members");}
                for (unsigned int m = 0; m < cbufferData->cbufferMembersData->countMembers(); ++m)
                {
                    if (cbufferData->cbufferMembersData->members[m].memberSize < 0) { error("undefined size for a cbuffer member"); }
                    if (!IsPow2(cbufferData->cbufferMembersData->members[m].memberAlignment)) { error("invalid member alignment"); }
                    if (cbufferData->cbufferMembersData->members[m].memberTypeId == spvUndefinedId) { error("undefined member type id for a cbuffer member"); }
                    if (cbufferData->cbufferMembersData->members[m].memberType == nullptr) { error("undefined member type for a cbuffer member"); }
                }
            }
        }
        if (errorMessages.size() > 0) success = false;
    }
#endif

    //=========================================================================================================================
    //Stuff necessary to insert new bytecode
    BytecodeUpdateController bytecodeUpdateController;
    BytecodeChunk* bytecodeNewNamesAndDecocates = bytecodeUpdateController.InsertNewBytecodeChunckAt(posLatestMemberNameOrDecorate, BytecodeUpdateController::InsertionConflictBehaviourEnum::InsertFirst);
    spv::Id newBoundId = bound();

    unsigned int maxConstValueNeeded = 0;
    vector<spv::Id> mapIndexesWithConstValueId; //map const value with their typeId
    vector<CBufferTypeData*> listUntouchedCbuffers;   //this list will contain all cbuffers being kept as they are
    vector<TypeStructMemberArray*> listNewCbuffers;   //this list will contain all new cbuffer information
    vector<TypeStructMember> listResourcesNewAccessVariables;     //this list will contain access variable for all resources moved out from the cbuffer

    vector<CBufferTypeData*>& vectorCbuffersToRemap = vectorUsedCbuffers;  //just reusing an existing vector to avoid creating a new one...
    std::fill(vectorCbuffersToRemap.begin(), vectorCbuffersToRemap.end(), nullptr);

    if (success && anyCBufferUsed)
    {
        //=========================================================================================================================
        //=========================================================================================================================
        //merge all USED cbuffers having an undefined type, or sharing the same declaration name
        for (unsigned int i = 0; i < listAllShaderCBuffers.size(); i++)
        {
            CBufferTypeData* cbufferA = listAllShaderCBuffers[i];

            if (cbufferA->isUsed == false) continue; //cbuffer A is not used
            bool mergingUndefinedCbuffers = cbufferA->cbufferType == spv::CBufferUndefined;

            vector<CBufferTypeData*> someCBuffersToMerge;
            someCBuffersToMerge.push_back(cbufferA); //for consistency purpose, we will recreate every used cbuffers, even if it's not merged with another one

            //==========================================================================================
            //check if we find some USED cbuffers which have to be merged (all defined cbuffer sharing the same name, or all global cbuffer (undefined))
            for (unsigned int j = i + 1; j < listAllShaderCBuffers.size(); j++)
            {
                CBufferTypeData* cbufferB = listAllShaderCBuffers[j];

                if (cbufferB->isUsed == false) continue; //cbuffer B is not used
                bool isBUndefinedCbuffer = cbufferB->cbufferType == spv::CBufferUndefined;

                if (mergingUndefinedCbuffers)
                {
                    //merge all undefined cbuffers
                    if (someCBuffersToMerge.size() == 0) someCBuffersToMerge.push_back(cbufferA);
                    if (isBUndefinedCbuffer) someCBuffersToMerge.push_back(cbufferB);
                }
                else
                {
                    //merge defined cbuffers, only if they share the same name
                    if (!isBUndefinedCbuffer && cbufferA->cbufferName == cbufferB->cbufferName)
                    {
                        if (someCBuffersToMerge.size() == 0) someCBuffersToMerge.push_back(cbufferA);
                        someCBuffersToMerge.push_back(cbufferB);
                    }
                }
            }

            //==========================================================================================
            //merge the cbuffers
            if (someCBuffersToMerge.size() > 0)
            {
                bool onlyMergeUsedMembers = (mergingUndefinedCbuffers? true: false); //either we merge the whole cbuffer (for defined cbuffers), or only the members used (for undefined cbuffers)

                if (onlyMergeUsedMembers)
                {
                    //flag all Variables accessing a cbuffer that we're merging
                    {
                        for (auto it = listAllObjects.begin(); it != listAllObjects.end(); ++it) {
                            ObjectInstructionBase* obj = *it;
                            if (obj != nullptr && obj->GetKind() == ObjectInstructionTypeEnum::Variable) {
                                VariableInstruction* variable = dynamic_cast<VariableInstruction*>(obj);
                                variable->tmpFlag = 0;
                            }
                        }
                        for (auto itcb = someCBuffersToMerge.begin(); itcb != someCBuffersToMerge.end(); itcb++){
                            CBufferTypeData* cbufferToMerge = *itcb;
                            cbufferToMerge->correspondingShaderType->variable->tmpFlag = 1;
                            for (unsigned int m = 0; m < cbufferToMerge->cbufferMembersData->members.size(); m++) cbufferToMerge->cbufferMembersData->members[m].isUsed = false;
                        }
                    }

                    //Detect which members from the cbuffers to merge are used
                    unsigned int start = positionFirstOpFunctionInstruction;
                    const unsigned int end = spv.size();
                    while (start < end)
                    {
                        unsigned int wordCount = asWordCount(start);
                        spv::Op opCode = asOpCode(start);

                        switch (opCode)
                        {
                            case spv::OpAccessChain:
                            {
                                spv::Id structIdAccessed = asId(start + 3);

                                VariableInstruction* variable = GetVariableById(structIdAccessed);
                                //if (variable == nullptr) { error("Failed to find the variable object for id: " + to_string(structIdAccessed)); break; }

                                //are we accessing the a cbuffer variable
                                if (variable != nullptr && variable->tmpFlag == 1)
                                {
                                    //spv::Id resultId = asId(start + 2);
                                    spv::Id indexConstId = asId(start + 4);

                                    ConstInstruction* constObject = GetConstById(indexConstId);
                                    if (constObject == nullptr) { error("cannot get const object for Id: " + to_string(indexConstId)); break; }
                                    int memberIndexValue = constObject->valueS32;
#ifdef XKSLANG_DEBUG_MODE
                                    if (variable->shaderOwner == nullptr) { error("the accessed variable does not belong to a shader: " + variable->GetName()); break; }
#endif
                                    ShaderTypeData* cbufferToMerge = variable->shaderOwner->GetShaderTypeDataForVariable(variable);
                                    CBufferTypeData* cbufferData = cbufferToMerge->type->cbufferData;
#ifdef XKSLANG_DEBUG_MODE
                                    if (cbufferToMerge == nullptr) { error("cannot get the cbuffer shader type for the variable: " + variable->GetName()); break; }
                                    if (cbufferData->cbufferMembersData == nullptr) { error("the cbuffer members data have not been initialized for: " + variable->GetName()); break; }
                                    if (memberIndexValue < 0 || memberIndexValue >= (int)cbufferData->cbufferMembersData->members.size()) {error("the member access index is out of bound"); break;}
#endif
                                    cbufferData->cbufferMembersData->members[memberIndexValue].isUsed = true;
                                }

                                break;
                            }
                        }
                        start += wordCount;
                    }

                    if (errorMessages.size() > 0) { success = false; break; }
                } //end of if (onlyMergeUsedMembers)
                
                TypeStructMemberArray* combinedCbuffer = new TypeStructMemberArray();
                combinedCbuffer->structTypeId = newBoundId++;
                combinedCbuffer->structPointerTypeId = newBoundId++;
                combinedCbuffer->structVariableTypeId = newBoundId++;
                combinedCbuffer->tmpTargetedBytecodePosition = 0;

                //==========================================================================================
                //create the list with all members from the cbuffers we're merging
                for (auto itcb = someCBuffersToMerge.begin(); itcb != someCBuffersToMerge.end(); itcb++)
                {
                    CBufferTypeData* cbufferToMerge = *itcb;
                    ShaderTypeData* cbufferShaderType = cbufferToMerge->correspondingShaderType;

                    vectorCbuffersToRemap[cbufferShaderType->variable->GetId()] = cbufferToMerge; //store the cbuffer to remap
                    bool isMemberStaged = cbufferToMerge->isStage;

                    //check the best position where to insert the new cbuffer in the bytecode
                    if (cbufferShaderType->type->bytecodeEndPosition > combinedCbuffer->tmpTargetedBytecodePosition) combinedCbuffer->tmpTargetedBytecodePosition = cbufferShaderType->type->bytecodeEndPosition;

                    const unsigned int countMembersInBufferToMerge = cbufferToMerge->cbufferMembersData->members.size();
                    for (unsigned int m = 0; m < countMembersInBufferToMerge; m++)
                    {
                        TypeStructMember& memberToMerge = cbufferToMerge->cbufferMembersData->members[m];
                        if (onlyMergeUsedMembers && memberToMerge.isUsed == false){
                            continue;
                        }

                        if (memberToMerge.isResourceType)
                        {
                            //a resource won't be merged into a cbuffer, but moved out into the global space
                            memberToMerge.newStructMemberIndex = -1;
                            memberToMerge.newStructTypeId = 0;
                            memberToMerge.newStructVariableAccessTypeId = 0;
                            memberToMerge.declarationName = cbufferToMerge->shaderOwnerName + "_" + memberToMerge.declarationName;
                            memberToMerge.variableAccessTypeId = newBoundId++; //id of the new variable we'll create

                            listResourcesNewAccessVariables.push_back(memberToMerge);
                        }
                        else
                        {
                            int memberNewIndex = -1;
                            bool memberAlreadyAdded = false;
                            if (isMemberStaged)
                            {
                                for (unsigned int pm = 0; pm < combinedCbuffer->members.size(); pm++)
                                {
                                    TypeStructMember& anotherMember = combinedCbuffer->members[pm];
                                    if (anotherMember.isStage)
                                    {
                                        if (anotherMember.shaderOwnerName == cbufferToMerge->shaderOwnerName && anotherMember.declarationName == memberToMerge.declarationName)
                                        {
                                            memberAlreadyAdded = true;
                                            memberNewIndex = pm;
                                            break;
                                        }
                                    }
                                }
                            }

                            if (memberAlreadyAdded)
                            {
                                //member already added, nothing else to do
                            }
                            else
                            {
                                memberNewIndex = combinedCbuffer->members.size();
                                spv::Id memberTypeId = memberToMerge.memberTypeId;
                                combinedCbuffer->members.push_back(TypeStructMember());
                                TypeStructMember& newMember = combinedCbuffer->members.back();

                                newMember.structMemberIndex = memberNewIndex;
                                newMember.memberTypeId = memberTypeId;
                                newMember.isStage = isMemberStaged;
                                newMember.listMemberDecoration = memberToMerge.listMemberDecoration;
                                newMember.declarationName = memberToMerge.declarationName;
                                newMember.shaderOwnerName = cbufferToMerge->shaderOwnerName;
                        
                                //compute the member offset (depending on the previous member's offset, its size, plus the new member's alignment
                                newMember.memberSize = memberToMerge.memberSize;
                                newMember.memberAlignment = memberToMerge.memberAlignment;
                                int memberOffset = 0;
                                if (memberNewIndex > 0)
                                {
                                    int previousMemberOffset = combinedCbuffer->members[memberNewIndex - 1].memberOffset;
                                    int previousMemberSize = combinedCbuffer->members[memberNewIndex - 1].memberSize;

                                    memberOffset = previousMemberOffset + previousMemberSize;
                                    int memberAlignment = newMember.memberAlignment;
                                    //round to pow2
                                    memberOffset = (memberOffset + memberAlignment - 1) & (~(memberAlignment - 1));
                                }
                                newMember.memberOffset = memberOffset;
                            }

                            //link reference from previous member to new one
                            memberToMerge.newStructMemberIndex = memberNewIndex;
                            memberToMerge.newStructTypeId = combinedCbuffer->structTypeId;
                            memberToMerge.newStructVariableAccessTypeId = combinedCbuffer->structVariableTypeId;
                        }
                    }
                }

                if (combinedCbuffer->members.size() == 0)
                {
                    delete combinedCbuffer;
                    combinedCbuffer = nullptr;
                }
                else
                {
                    listNewCbuffers.push_back(combinedCbuffer);

                    if (combinedCbuffer->members.size() > maxConstValueNeeded) maxConstValueNeeded = combinedCbuffer->members.size();

                    //name of the combined cbuffer is the name of the first cbuffer (all merged cbuffers have the same name)
                    combinedCbuffer->declarationName = someCBuffersToMerge[0]->cbufferName;

                    //update the new member's name with a more explicit name (ex. var1 --> ShaderA.var1, if the member is stage we use the original shader name)
                    for (unsigned int pm = 0; pm < combinedCbuffer->members.size(); pm++)
                    {
                        TypeStructMember& anotherMember = combinedCbuffer->members[pm];
                        anotherMember.declarationName = anotherMember.shaderOwnerName + "_" + anotherMember.declarationName;
                    }
                }
                
                //all cbuffers merged will be removed
                for (auto itcb = someCBuffersToMerge.begin(); itcb != someCBuffersToMerge.end(); itcb++)
                {
                    CBufferTypeData* cbufferToMerge = *itcb;
                    cbufferToMerge->isUsed = false;
                }
            }  //end of if (listCBuffersToMerge.size() > 0)
            else
            {
                //the cbuffer is used but not merged with anyone else: we still add it in this list, so that we can just rename it (for a cleaner bytecode)
                listUntouchedCbuffers.push_back(cbufferA);
            }
        } //end of for (unsigned int i = 0; i < listAllShaderCBuffers.size(); i++) (checking all used cbuffers)

        if (errorMessages.size() > 0) success = false;
    }

    if (success)
    {
        //===================================================================================================================
        //===================================================================================================================
        //get or build the maps of const values for the new cbuffers we're adding
        if (maxConstValueNeeded > 0)
        {
            //Build the map of const value with the existing const object Id, for each index of the global stream struct
            mapIndexesWithConstValueId.resize(maxConstValueNeeded, spvUndefinedId);
            spv::Id idOpTypeIntS32 = spvUndefinedId;
            unsigned int posLastConstEnd = 0;

            for (auto it = listAllObjects.begin(); it != listAllObjects.end(); ++it)
            {
                ObjectInstructionBase* obj = *it;
                if (obj != nullptr && obj->GetKind() == ObjectInstructionTypeEnum::Const)
                {
                    ConstInstruction* constObject = dynamic_cast<ConstInstruction*>(obj);
                    if (constObject->bytecodeEndPosition > posLastConstEnd) posLastConstEnd = constObject->bytecodeEndPosition;

                    if (constObject->isS32)
                    {
                        int constValueS32 = constObject->valueS32;
                        if (constValueS32 >= 0 && constValueS32 < (int)mapIndexesWithConstValueId.size()) mapIndexesWithConstValueId[constValueS32] = constObject->GetId();
                        if (idOpTypeIntS32 == spvUndefinedId) idOpTypeIntS32 = constObject->GetTypeId();
                    }
                }
            }

            BytecodeChunk* bytecodeNewConsts = bytecodeUpdateController.InsertNewBytecodeChunckAt(posLastConstEnd, BytecodeUpdateController::InsertionConflictBehaviourEnum::ReturnNull);
            if (bytecodeNewConsts == nullptr) return error("Failed to insert a new bytecode chunk. position is already used: " + to_string(posLastConstEnd));

            //make the missing consts and OpTypeInt
            if (idOpTypeIntS32 == spvUndefinedId)
            {
                spv::Instruction typeInt32(newBoundId++, spv::NoType, spv::OpTypeInt);
                typeInt32.addImmediateOperand(32);
                typeInt32.addImmediateOperand(1);
                typeInt32.dump(bytecodeNewConsts->bytecode);
                idOpTypeIntS32 = typeInt32.getResultId();
            }
            for (unsigned int i = 0; i < mapIndexesWithConstValueId.size(); ++i)
            {
                if (mapIndexesWithConstValueId[i] == spvUndefinedId)
                {
                    spv::Instruction constant(newBoundId++, idOpTypeIntS32, spv::OpConstant);
                    constant.addImmediateOperand(i);
                    constant.dump(bytecodeNewConsts->bytecode);
                    mapIndexesWithConstValueId[i] = constant.getResultId();
                }
            }

            if (errorMessages.size() > 0) success = false;
        }
    }

    if (success)
    {
        //===================================================================================================================
        //===================================================================================================================
        //Create the resources access variables
        for (auto itrav = listResourcesNewAccessVariables.begin(); itrav != listResourcesNewAccessVariables.end(); itrav++)
        {
            //for each resources: create the pointer type (if need), and the uniform access variable (to remove the cbuffer accessChain and directly load the variable)
            //example: a texture created on global space would be access through variable id (19)
            // 17:                    OpTypeImage 8(float)2D sampled format : Unknown
            // 18:                    OpTypePointer UniformConstant 17
            // 19(Texture0) : 18(ptr) OpVariable UniformConstant
            //....
            // 20:            17      OpLoad 19(Texture0)
            //While the same texture created within a cbuffer is accessed this way:
            // 51:                    OpTypeImage 29(float)2D sampled format : Unknown
            // 53:                    OpTypePointer UniformConstant 51
            //....
            // 10:            53(ptr) OpAccessChain 57(ResourceGroup_var) 54
            // 11:            51      OpLoad 10

            TypeStructMember& memberToMoveOut = *itrav;
            TypeInstruction* resourceType = memberToMoveOut.memberType;

            TypeInstruction* resourcePointerType = GetTypePointingTo(resourceType);
            VariableInstruction* resourceVariable = GetVariablePointingTo(resourcePointerType);

            BytecodeChunk* bytecodeResourceVariable = nullptr;

            spv::Id pointerTypeId = 0;
            if (resourcePointerType == nullptr)
            {
                bytecodeResourceVariable = bytecodeUpdateController.InsertNewBytecodeChunckAt(resourceType->bytecodeEndPosition, BytecodeUpdateController::InsertionConflictBehaviourEnum::InsertLast);
                if (bytecodeResourceVariable == nullptr) { error("Failed to insert a new bytecode chunk"); break; }

                //make the pointer type
                pointerTypeId = newBoundId++;
                spv::Instruction pointer(pointerTypeId, spv::NoType, spv::OpTypePointer);
                pointer.addImmediateOperand(spv::StorageClass::StorageClassUniformConstant);
                pointer.addIdOperand(resourceType->GetId());
                pointer.dump(bytecodeResourceVariable->bytecode);
            }
            else
            {
                //check that the storage class is valid (Note: otherwise we can also create a new one)
                pointerTypeId = resourcePointerType->GetId();
                spv::StorageClass pointerStorageClass = (spv::StorageClass)asLiteralValue(resourcePointerType->GetBytecodeStartPosition() + 2);
                if (pointerStorageClass != spv::StorageClass::StorageClassUniformConstant) { error("Invalid storage class, expected StorageClassUniform"); break; }

                bytecodeResourceVariable = bytecodeUpdateController.InsertNewBytecodeChunckAt(resourcePointerType->bytecodeEndPosition, BytecodeUpdateController::InsertionConflictBehaviourEnum::InsertLast);
                if (bytecodeResourceVariable == nullptr) { error("Failed to insert a new bytecode chunk"); break; }
            }
            if (resourceVariable != nullptr) { error("a variable already exists for the resource type"); break; }

            //make the variable
            {
                spv::Instruction variable(memberToMoveOut.variableAccessTypeId, pointerTypeId, spv::OpVariable);
                variable.addImmediateOperand(spv::StorageClass::StorageClassUniformConstant);
                variable.dump(bytecodeResourceVariable->bytecode);

#ifdef XKSLANG_ADD_NAMES_AND_DEBUG_DATA_INTO_BYTECODE
                string variableName = memberToMoveOut.declarationName;
                spv::Instruction variableNameInstr(spv::OpName);
                variableNameInstr.addIdOperand(variable.getResultId());
                variableNameInstr.addStringOperand(variableName.c_str());
                variableNameInstr.dump(bytecodeNewNamesAndDecocates->bytecode);
#endif
                
                //add the variable's descriptorSet 0
                spv::Instruction variableDecorateInstr(spv::OpDecorate);
                variableDecorateInstr.addIdOperand(memberToMoveOut.variableAccessTypeId);
                variableDecorateInstr.addImmediateOperand(spv::DecorationDescriptorSet);
                variableDecorateInstr.addImmediateOperand(0);
                variableDecorateInstr.dump(bytecodeNewNamesAndDecocates->bytecode);
            }
        }
        
        //===================================================================================================================
        //===================================================================================================================
        //Create the new cbuffer structs
        for (auto itcb = listNewCbuffers.begin(); itcb != listNewCbuffers.end(); itcb++)
        {
            TypeStructMemberArray* cbuffer = *itcb;
            string cbufferName = cbuffer->declarationName;
            string cbufferVarName = cbufferName + "_var";

            BytecodeChunk* bytecodeMergedCBufferType = bytecodeUpdateController.InsertNewBytecodeChunckAt(cbuffer->tmpTargetedBytecodePosition, BytecodeUpdateController::InsertionConflictBehaviourEnum::InsertLast);
            if (bytecodeMergedCBufferType == nullptr) { error("Failed to insert a new bytecode chunk"); break; }

            //make the cbuffer struct type
            {
                spv::Instruction cbufferType(cbuffer->structTypeId, spv::NoType, spv::OpTypeStruct);
                for (unsigned int m = 0; m < cbuffer->members.size(); ++m)
                {
                    const TypeStructMember& aStreamMember = cbuffer->members[m];
                    cbufferType.addIdOperand(aStreamMember.memberTypeId);
                }
                cbufferType.dump(bytecodeMergedCBufferType->bytecode);

#ifdef XKSLANG_ADD_NAMES_AND_DEBUG_DATA_INTO_BYTECODE
                //cbuffer struct name
                spv::Instruction cbufferStructName(spv::OpName);
                cbufferStructName.addIdOperand(cbufferType.getResultId());
                cbufferStructName.addStringOperand(cbufferName.c_str());
                cbufferStructName.dump(bytecodeNewNamesAndDecocates->bytecode);
#endif

                //cbuffer struct decorate (block / cbuffer)
                spv::Instruction structDecorateInstr(spv::OpDecorate);
                structDecorateInstr.addIdOperand(cbuffer->structTypeId);
                structDecorateInstr.addImmediateOperand(spv::DecorationBlock);
                structDecorateInstr.dump(bytecodeNewNamesAndDecocates->bytecode);

                //cbuffer properties (block / cbuffer)
                spv::Instruction structCBufferPropertiesInstr(spv::OpCBufferMemberProperties);
                structCBufferPropertiesInstr.addIdOperand(cbuffer->structTypeId);
                structCBufferPropertiesInstr.addImmediateOperand(spv::CBufferDefined);
                structCBufferPropertiesInstr.addImmediateOperand(spv::CBufferUnstage);
                structCBufferPropertiesInstr.addImmediateOperand(cbuffer->countMembers());
                for (unsigned int m = 0; m < cbuffer->members.size(); ++m)
                {
                    //add size and alignment for each members
                    structCBufferPropertiesInstr.addImmediateOperand(cbuffer->members[m].memberSize);
                    structCBufferPropertiesInstr.addImmediateOperand(cbuffer->members[m].memberAlignment);
                }
                structCBufferPropertiesInstr.dump(bytecodeNewNamesAndDecocates->bytecode);
            }

            //make the pointer type
            {
                spv::Instruction pointer(cbuffer->structPointerTypeId, spv::NoType, spv::OpTypePointer);
                pointer.addImmediateOperand(spv::StorageClass::StorageClassUniform);
                pointer.addIdOperand(cbuffer->structTypeId);
                pointer.dump(bytecodeMergedCBufferType->bytecode);
            }
            
            //make the variable
            {
                spv::Instruction variable(cbuffer->structVariableTypeId, cbuffer->structPointerTypeId, spv::OpVariable);
                variable.addImmediateOperand(spv::StorageClass::StorageClassUniform);
                variable.dump(bytecodeMergedCBufferType->bytecode);

#ifdef XKSLANG_ADD_NAMES_AND_DEBUG_DATA_INTO_BYTECODE
                spv::Instruction variableName(spv::OpName);
                variableName.addIdOperand(variable.getResultId());
                variableName.addStringOperand(cbufferVarName.c_str());
                variableName.dump(bytecodeNewNamesAndDecocates->bytecode);
#endif
            }

#ifdef XKSLANG_ADD_NAMES_AND_DEBUG_DATA_INTO_BYTECODE
            //Add the members name
            for (unsigned int memberIndex = 0; memberIndex < cbuffer->members.size(); ++memberIndex)
            {
                const TypeStructMember& cbufferMember = cbuffer->members[memberIndex];

                string memberName = cbufferMember.declarationName;
                //validate the member name (for example rename Shader<8>_var to Shader_8__var)
                unsigned int memberNameSize = memberName.size();
                for (unsigned int k = 0; k < memberNameSize; k++){
                    char c = memberName[k];
                    if (c == '<' || c == '>' || c == ',') memberName[k] = '_';
                }

                //member name
                spv::Instruction memberNameInstr(spv::OpMemberName);
                memberNameInstr.addIdOperand(cbuffer->structTypeId);
                memberNameInstr.addImmediateOperand(memberIndex);
                memberNameInstr.addStringOperand(memberName.c_str()); //use declaration name
                memberNameInstr.dump(bytecodeNewNamesAndDecocates->bytecode);
            }
#endif

            //Add the members decorate
            for (unsigned int memberIndex = 0; memberIndex < cbuffer->members.size(); ++memberIndex)
            {
                const TypeStructMember& cbufferMember = cbuffer->members[memberIndex];

                //member decorate (offset)
                spv::Instruction memberOffsetDecorateInstr(spv::OpMemberDecorate);
                memberOffsetDecorateInstr.addIdOperand(cbuffer->structTypeId);
                memberOffsetDecorateInstr.addImmediateOperand(cbufferMember.structMemberIndex);
                memberOffsetDecorateInstr.addImmediateOperand(spv::DecorationOffset);
                memberOffsetDecorateInstr.addImmediateOperand(cbufferMember.memberOffset);
                memberOffsetDecorateInstr.dump(bytecodeNewNamesAndDecocates->bytecode);
            
                //member extra decorate (if any)
                if (cbufferMember.listMemberDecoration.size() > 0)
                {
                    unsigned int cur = 0;
                    while (cur < cbufferMember.listMemberDecoration.size())
                    {
                        unsigned int decorateCount = cbufferMember.listMemberDecoration[cur];
                        cur++;
#ifdef XKSLANG_DEBUG_MODE
                        if (cur + decorateCount > cbufferMember.listMemberDecoration.size()) { error("Invalid member decorate instructions"); break; }
#endif

                        spv::Instruction memberDecorateInstr(spv::OpMemberDecorate);
                        memberDecorateInstr.addIdOperand(cbuffer->structTypeId);
                        memberDecorateInstr.addImmediateOperand(cbufferMember.structMemberIndex);
                        for (unsigned int k = 0; k < decorateCount; k++)
                            memberDecorateInstr.addImmediateOperand(cbufferMember.listMemberDecoration[cur + k]);
                        memberDecorateInstr.dump(bytecodeNewNamesAndDecocates->bytecode);

                        cur += decorateCount;
                    }
                }
            }
        }

#ifdef XKSLANG_ADD_NAMES_AND_DEBUG_DATA_INTO_BYTECODE
        if (listUntouchedCbuffers.size() > 0)
        {
            //rename the cbuffers
            for (auto itcb = listUntouchedCbuffers.begin(); itcb != listUntouchedCbuffers.end(); itcb++)
            {
                CBufferTypeData* cbuffer = *itcb;

                //remove their initial name instruction (if any)
                unsigned int posToInsert = 0;
                if (cbuffer->posOpNameType > 0)
                {
                    if (posToInsert == 0) posToInsert = cbuffer->posOpNameType;
                    int wordCount = asWordCount(cbuffer->posOpNameType);
                    if (bytecodeUpdateController.AddPortionToRemove(cbuffer->posOpNameType, wordCount) == nullptr) { error("Failed to insert a portion to remove"); break; }
                }
                if (cbuffer->posOpNameVariable > 0)
                {
                    if (posToInsert == 0) posToInsert = cbuffer->posOpNameVariable;
                    int wordCount = asWordCount(cbuffer->posOpNameVariable);
                    if (bytecodeUpdateController.AddPortionToRemove(cbuffer->posOpNameVariable, wordCount) == nullptr) { error("Failed to insert a portion to remove"); break; }
                }
                if (posToInsert == 0) posToInsert = posFirstOpNameOrDecorate;
                BytecodeChunk* bytecodeNameInsertion = bytecodeUpdateController.InsertNewBytecodeChunckAt(posToInsert, BytecodeUpdateController::InsertionConflictBehaviourEnum::InsertFirst);

                //set the new type and variable name
                string cbufferName = cbuffer->cbufferName;
                string cbufferVarName = cbufferName + "_var";

                spv::Instruction cbufferStructName(spv::OpName);
                cbufferStructName.addIdOperand(cbuffer->correspondingShaderType->type->GetId());
                cbufferStructName.addStringOperand(cbufferName.c_str());
                cbufferStructName.dump(bytecodeNameInsertion->bytecode);

                spv::Instruction variableName(spv::OpName);
                variableName.addIdOperand(cbuffer->correspondingShaderType->variable->GetId());
                variableName.addStringOperand(cbufferVarName.c_str());
                variableName.dump(bytecodeNameInsertion->bytecode);
            }
        }
#endif

        if (errorMessages.size() > 0) success = false;
    }

    if (success)
    {
        vector<spv::Id> mapAccessChainResultIdsToRemap;
        mapAccessChainResultIdsToRemap.resize(bound(), 0);
        bool anyAccessChainResultIdToRemap = false;

        //===================================================================================================================
        //===================================================================================================================
        //Update all accesses to previous cbuffer to the new ones
        if (listNewCbuffers.size() > 0 || listResourcesNewAccessVariables.size() > 0)
        {
            unsigned int start = positionFirstOpFunctionInstruction;
            const unsigned int end = spv.size();
            while (start < end)
            {
                unsigned int wordCount = asWordCount(start);
                spv::Op opCode = asOpCode(start);

                switch (opCode)
                {
                    case spv::OpAccessChain:
                    {
                        spv::Id structIdAccessed = asId(start + 3);

                        //are we accessing a cbuffer that we just merged?
                        if (vectorCbuffersToRemap[structIdAccessed] != nullptr)
                        {
                            CBufferTypeData* cbufferData = vectorCbuffersToRemap[structIdAccessed];
                            TypeStructMemberArray* cbufferMembersData = cbufferData->cbufferMembersData;

                            spv::Id typeId = asId(start + 1);
                            spv::Id resultId = asId(start + 2);
                            spv::Id indexConstId = asId(start + 4);

                            ConstInstruction* constObject = GetConstById(indexConstId);
                            if (constObject == nullptr) { error(string("cannot get const object for Id: ") + to_string(indexConstId)); break; }
                            int memberIndexInOriginalCbuffer = constObject->valueS32;

#ifdef XKSLANG_DEBUG_MODE
                            if (!constObject->isS32) { error("const object is not a valid S32"); break; }
                            if (cbufferMembersData == nullptr) { error("the original shaderType cbuffer has not initialized its members"); break; }
                            if (memberIndexInOriginalCbuffer < 0 || memberIndexInOriginalCbuffer >= (int)cbufferMembersData->members.size()) { error("memberIndexInOriginalCbuffer is out of bound"); break; }
#endif
                            const TypeStructMember& structMember = cbufferMembersData->members[memberIndexInOriginalCbuffer];

                            if (structMember.isResourceType)
                            {
                                //the resource has been moved outside the cbuffer: we remove the access chain instruction
                                spv::Id accessChainResultId = asId(start + 2);
                                if (bytecodeUpdateController.AddPortionToRemove(start, wordCount) == nullptr) { error("Failed to insert a portion to remove"); break; }

                                //Then we will need to remap the access chain id into the resource variable id
                                mapAccessChainResultIdsToRemap[accessChainResultId] = structMember.variableAccessTypeId;
                                anyAccessChainResultIdToRemap = true;
                            }
                            else
                            {
                                spv::Id newStructAccessId = structMember.newStructVariableAccessTypeId;
                                int memberIndexInNewCbuffer = structMember.newStructMemberIndex;

                                //could eventually be optimized, but we shouldn't have too many new cbuffers to bother for now
                                TypeStructMemberArray* newCbuffer = nullptr;
                                for (unsigned int i = 0; i < listNewCbuffers.size(); ++i)
                                {
                                    if (listNewCbuffers[i]->structVariableTypeId == newStructAccessId) {
                                        newCbuffer = listNewCbuffers[i];
                                        break;
                                    }
                                }
                                if (newCbuffer == nullptr) { error("unable to find the new cbuffer"); break; }
#ifdef XKSLANG_DEBUG_MODE
                                if (memberIndexInNewCbuffer < 0 || memberIndexInNewCbuffer >= (int)mapIndexesWithConstValueId.size()) { error("memberIndexInNewCbuffer is out of bound"); break; }
#endif
                                spv::Id memberIndexInNewCbufferConstTypeId = mapIndexesWithConstValueId[memberIndexInNewCbuffer]; //get the const type id for new index

                                BytecodePortionToReplace& portionToReplace = bytecodeUpdateController.SetNewPortionToReplace(start + 1);
                                portionToReplace.SetNewValues({ typeId, resultId, newStructAccessId, memberIndexInNewCbufferConstTypeId });
                            }
                        }
                        break;
                    }
                }
                start += wordCount;
            }
        }
        if (errorMessages.size() > 0) success = false;

        if (success && anyAccessChainResultIdToRemap)
        {
            //Remap all OpLoad instruction refering to a resource we moved out from the cbuffer
            //Note: could we have any other kind of instruction?
            unsigned int start = positionFirstOpFunctionInstruction;
            const unsigned int end = spv.size();
            while (start < end)
            {
                unsigned int wordCount = asWordCount(start);
                spv::Op opCode = asOpCode(start);

                switch (opCode)
                {
                    case spv::OpLoad:
                    {
                        spv::Id idLoaded = asId(start + 3);
#ifdef XKSLANG_DEBUG_MODE
                        if (idLoaded >= mapAccessChainResultIdsToRemap.size()) { error("idLoaded is out of bound"); break; }
#endif
                        if (mapAccessChainResultIdsToRemap[idLoaded] != 0)
                        {
                            spv::Id newId = mapAccessChainResultIdsToRemap[idLoaded];
                            bytecodeUpdateController.SetNewAtomicValueUpdate(start + 3, newId);
                        }
                        break;
                    }
                }
                start += wordCount;
            }

            if (errorMessages.size() > 0) success = false;
        }
    }

    //=========================================================================================================================
    //delete allocated data
    for (unsigned int i = 0; i < listAllShaderCBuffers.size(); i++)
    {
        CBufferTypeData* cbufferData = listAllShaderCBuffers[i];
        if (cbufferData->cbufferMembersData != nullptr)
        {
            delete cbufferData->cbufferMembersData;
            cbufferData->cbufferMembersData = nullptr;
        }
    }
    for (auto itcb = listNewCbuffers.begin(); itcb != listNewCbuffers.end(); itcb++)
    {
        TypeStructMemberArray* cbuffer = *itcb;
        delete cbuffer;
    }

    //=========================================================================================================================
    //=========================================================================================================================
    //apply all changes
    if (success)
    {
        //add the new cbuffers into the bytecode
        setBound(newBoundId);

        //apply the bytecode update controller
        if (!ApplyBytecodeUpdateController(bytecodeUpdateController)) error("failed to update the bytecode update controller");

        //bytecode has been updated: reupdate all maps
        if (!UpdateAllMaps()) error("failed to update all maps");

        if (errorMessages.size() > 0) success = false;
    }

    //=========================================================================================================================
    //=========================================================================================================================
    //remove all unused cbuffers
    if (success)
    {
        vector<range_t> vecStripRanges;
        for (auto itcb = listAllShaderCBuffers.begin(); itcb != listAllShaderCBuffers.end(); itcb++)
        {
            CBufferTypeData* cbuffer = *itcb;
            if (cbuffer->isUsed == false)
            {
                if (!RemoveShaderTypeFromBytecodeAndData(cbuffer->correspondingShaderType, vecStripRanges))
                {
                    return error(string("Failed to remove the unused cbuffer type: ") + cbuffer->correspondingShaderType->type->GetName());
                }
            }
        }
    
        stripBytecode(vecStripRanges);
        if (!UpdateAllMaps()) return error("failed to update all maps");
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

//parse an OpStruct instruction to get its list of membes typeId
bool SpxStreamRemapper::GetStructTypeMembersTypeIdList(TypeInstruction* structType, vector<spv::Id>& membersTypeList)
{
#ifdef XKSLANG_DEBUG_MODE
    if (structType == nullptr) return error("null parameter");
#endif

    unsigned int start = structType->bytecodeStartPosition;
    unsigned int wordCount = asWordCount(start);

    spv::Op opCode = asOpCode(start);
    if (opCode != spv::OpTypeStruct) return error(string("Invalid OpCode for the stream struct type: ") + OpcodeString(opCode));

    int countMembers = wordCount - 2;
#ifdef XKSLANG_DEBUG_MODE
    if (countMembers < 0) return error("corrupted bytecode: countMembers is negative");
#endif

    for (int m = 0; m < countMembers; ++m)
    {
        spv::Id memberTypeId = asLiteralValue(start + 2 + m);
        membersTypeList.push_back(memberTypeId);

#ifdef XKSLANG_DEBUG_MODE
        if (GetTypeById(memberTypeId) == nullptr) return error("The struct member type is undefined: " + to_string(memberTypeId));
#endif
    }

    return true;
}

bool SpxStreamRemapper::RemoveAllUnusedShaders(vector<XkslMixerOutputStage>& outputStages)
{
    //if (status != SpxRemapperStatusEnum::AAA) return error("Invalid remapper status");
    status = SpxRemapperStatusEnum::MixinBeingCompiled_UnusedShaderRemoved;

    if (outputStages.size() == 0) return error("no output stages defined");
    if (!ValidateHeader()) return error("Failed to validate the header");

    //===================================================================================================================
    //===================================================================================================================
    //Find all unused shader classes from the bytecode, depending on the outputStages entryPoint functions
    {
        //Set all shaders and functions flag to 0
        for (auto itsh = vecAllShaders.begin(); itsh != vecAllShaders.end(); itsh++) {
            ShaderClassData* shader = *itsh;
            shader->flag1 = 0;
            for (auto itsf = shader->functionsList.begin(); itsf != shader->functionsList.end(); itsf++) {
                FunctionInstruction* aFunction = *itsf;
                aFunction->flag1 = 0;
            }
        }

        //====================================================================
        //Analyse all functions called, starting from all entryPoint functions
        vector<FunctionInstruction*> vectorFunctionsCalled;
        vector<FunctionInstruction*> vectorAllFunctionsCalled;
        vector<ShaderClassData*> vectorShadersOwningAllFunctionsCalled;
        for (unsigned int i = 0; i<outputStages.size(); ++i)
        {
            if (outputStages[i].entryFunction == nullptr) return error("An entry point function is null.");
            vectorFunctionsCalled.push_back(outputStages[i].entryFunction);
        }

        while (vectorFunctionsCalled.size() > 0)
        {
            FunctionInstruction* aFunctionCalled = vectorFunctionsCalled.back();
            vectorFunctionsCalled.pop_back();
            if (aFunctionCalled->flag1 == 1) continue; //we already processed this function
            vectorAllFunctionsCalled.push_back(aFunctionCalled);

            //Flag the function and the shader owning it
            aFunctionCalled->flag1 = 1;
            if (aFunctionCalled->shaderOwner->flag1 == 0)
                vectorShadersOwningAllFunctionsCalled.push_back(aFunctionCalled->shaderOwner);
            aFunctionCalled->shaderOwner->flag1 = 1;

            //Analyse the function's bytecode, to add all new functions called
            unsigned int start = aFunctionCalled->bytecodeStartPosition;
            const unsigned int end = aFunctionCalled->bytecodeEndPosition;
            while (start < end)
            {
                unsigned int wordCount = asWordCount(start);
                spv::Op opCode = asOpCode(start);

                switch (opCode)
                {
                    case spv::OpFunctionCall:
                    case spv::OpFunctionCallBaseResolved:
                    case spv::OpFunctionCallThroughStaticShaderClassCall:
                    {
                        spv::Id functionCalledId = asId(start + 3);
                        FunctionInstruction* anotherFunctionCalled = GetFunctionById(functionCalledId);
                        if (anotherFunctionCalled->flag1 == 0) vectorFunctionsCalled.push_back(anotherFunctionCalled); //we'll analyse the function later
                        break;
                    }

                    case spv::OpFunctionCallBaseUnresolved:
                    {
                        error(string("An unresolved function base call has been found in function: ") + aFunctionCalled->GetFullName());
                        break;
                    }

                    case spv::OpFunctionCallThroughCompositionVariable:
                    {
                        error(string("An unresolved function call through composition has been found in function: ") + aFunctionCalled->GetFullName());
                        break;
                    }
                }
                start += wordCount;
            }
        }
        if (errorMessages.size() > 0) return false;

        //====================================================================
        //For all shader flagged, flag their whole family (composition instances and parents) as well
        vector<ShaderClassData*> vectorAllShadersInvolved;
        while (vectorShadersOwningAllFunctionsCalled.size() > 0)
        {
            ShaderClassData* aShaderInvolved = vectorShadersOwningAllFunctionsCalled.back();
            vectorShadersOwningAllFunctionsCalled.pop_back();
            if (aShaderInvolved->flag1 == 2) continue;

            aShaderInvolved->flag1 = 2;
            vectorAllShadersInvolved.push_back(aShaderInvolved);

            //add the shader parents
            for (auto itsh = aShaderInvolved->parentsList.begin(); itsh != aShaderInvolved->parentsList.end(); itsh++)
            {
                ShaderClassData* aShaderParentInvolved = *itsh;
                if (aShaderParentInvolved->flag1 != 2) vectorShadersOwningAllFunctionsCalled.push_back(aShaderParentInvolved);
            }

            //add the composition shader instances
            for (auto itc = aShaderInvolved->compositionsList.begin(); itc != aShaderInvolved->compositionsList.end(); itc++)
            {
                const ShaderComposition& aComposition = *itc;
                if (aComposition.countInstances > 0)
                {
                    vector<ShaderClassData*> vecCompositionShaderInstances;
                    if (!GetAllShaderInstancesForComposition(&aComposition, vecCompositionShaderInstances)) {
                        error(string("Failed to retrieve the instances for the composition: ") + aComposition.variableName + string(" from shader: ") + aComposition.compositionShaderOwner->GetName());
                    }
                    else
                    {
                        for (unsigned int ii = 0; ii < vecCompositionShaderInstances.size(); ii++)
                        {
                            ShaderClassData* aShaderCompositionInstanced = vecCompositionShaderInstances[ii];
                            if (aShaderCompositionInstanced->flag1 != 2) vectorShadersOwningAllFunctionsCalled.push_back(aShaderCompositionInstanced);
                        }
                    }
                }
            }
        }
        if (errorMessages.size() > 0) return false;
    }

    //===================================================================================================================
    //===================================================================================================================
    //Remove unused shaders
    {
        //Get the list of all unused (unflagged) shaders
        vector<ShaderClassData*> listUnusedShaders;
        for (auto itsh = vecAllShaders.begin(); itsh != vecAllShaders.end(); itsh++) {
            ShaderClassData* shader = *itsh;
            if (shader->flag1 != 2) listUnusedShaders.push_back(shader);
        }

        //remove all unused shaders from the bytecode and the data structure
        vector<range_t> vecStripRanges;
        for (auto itsh = listUnusedShaders.begin(); itsh != listUnusedShaders.end(); itsh++) {
            ShaderClassData* unusedShader = *itsh;
            if (!RemoveShaderFromBytecodeAndData(unusedShader, vecStripRanges))
            {
                return error(string("Failed to remove the unused shader: ") + unusedShader->GetName());
            }
        }

        stripBytecode(vecStripRanges);
        if (!UpdateAllMaps()) return error("failed to update all maps");
    }

    return true;
}

bool SpxStreamRemapper::ApplyCompositionInstancesToBytecode()
{
    //if (status != SpxRemapperStatusEnum::AAA) return error("Invalid remapper status");
    status = SpxRemapperStatusEnum::MixinBeingCompiled_CompositionInstancesProcessed;

    //===================================================================================================================
    //===================================================================================================================
    //Duplicate all foreach loops depending on the composition instances

    //======================================================================================
    //Build the list of all foreach loops, plus the list of all composition instances data
    int maxForEachLoopsNestedLevel;
    vector<CompositionForEachLoopData> vecAllForeachLoops;
    if (!GetAllCompositionForEachLoops(vecAllForeachLoops, maxForEachLoopsNestedLevel)) {
        return error(string("Failed to retrieve all composition foreach loops for the composition: "));
    }

    //======================================================================================
    //duplicate the foreach loops for all instances
    {
        for (int nestedLevelToProcess = maxForEachLoopsNestedLevel; nestedLevelToProcess >= 0; nestedLevelToProcess--)
        {
            list<CompositionForEachLoopData*> listForeachLoopsToMergeIntoBytecode;

            int maxId = bound();
            vector<bool> IdsToRemapTable;
            IdsToRemapTable.resize(bound(), false);

            for (auto itfe = vecAllForeachLoops.begin(); itfe != vecAllForeachLoops.end(); itfe++)
            {
                CompositionForEachLoopData* forEachLoopToUnroll = &(*itfe);
                if (forEachLoopToUnroll->nestedLevel != nestedLevelToProcess) continue;

                ShaderComposition* compositionToUnroll = forEachLoopToUnroll->composition;
                
                bool anythingToUnroll = true;
                if (compositionToUnroll->countInstances <= 0) anythingToUnroll = false;  //if no instances set, nothing to unroll
                if (forEachLoopToUnroll->firstLoopInstuctionStart >= forEachLoopToUnroll->lastLoopInstuctionEnd) anythingToUnroll = false;  //if no bytecode within the forloop, nothing to unroll

                if (anythingToUnroll)
                {
                    vector<uint32_t> duplicatedBytecode;
                    vector<uint32_t> foreachLoopBytecode;
                    foreachLoopBytecode.insert(foreachLoopBytecode.end(), spv.begin() + forEachLoopToUnroll->firstLoopInstuctionStart, spv.begin() + forEachLoopToUnroll->lastLoopInstuctionEnd);

                    //get the list of all resultIds to remapp from the foreach loop bytecode
                    {
                        unsigned int start = 0;
                        const unsigned int end = foreachLoopBytecode.size();
                        while (start < end)
                        {
                            spv::Op opCode = spirvbin_t::opOpCode(foreachLoopBytecode[start]);
                            unsigned int wordCount = spirvbin_t::opWordCount(foreachLoopBytecode[start]);
                            unsigned int word = start + 1;
                        
                            // any type?
                            if (spv::InstructionDesc[opCode].hasType()) {
                                word++;
                            }

                            // any result id to remap?
                            if (spv::InstructionDesc[opCode].hasResult()) {
                                spv::Id resultId = foreachLoopBytecode[word];
#ifdef XKSLANG_DEBUG_MODE
                                if (resultId < 0 || resultId >= IdsToRemapTable.size())
                                { error(string("unroll foreach composition: result id is out of bound. Id: ") + to_string(resultId)); break; }
#endif
                                IdsToRemapTable[resultId] = true;
                            }

                            start += wordCount;
                        }
                    }

                    //Get all instances set for the composition
                    vector<ShaderClassData*> vecCompositionShaderInstances;
                    if (!GetAllShaderInstancesForComposition(compositionToUnroll, vecCompositionShaderInstances)) {
                        return error(string("Failed to retrieve the instances for the composition: ") + compositionToUnroll->variableName + string(" from shader: ") + compositionToUnroll->compositionShaderOwner->GetName());
                    }

                    //duplicate the foreach loop bytecode for all instances (compositions are already sorted by their num)
                    unsigned int countInstances = vecCompositionShaderInstances.size();
                    for (unsigned int instanceNum = 0; instanceNum < countInstances; ++instanceNum)
                    {
                        ShaderClassData* compositionShaderInstance = vecCompositionShaderInstances[instanceNum];

                        //Clone the loop bytecode at the end of the duplicated bytecode
                        {
                            vector<spv::Id> remapTable;
                            remapTable.resize(bound(), spvUndefinedId);
                            for (unsigned int id=0; id<IdsToRemapTable.size(); ++id) {
                                if (IdsToRemapTable[id]) remapTable[id] = maxId++;
                                else remapTable[id] = id;
                            }

                            unsigned int start = duplicatedBytecode.size();
                            duplicatedBytecode.insert(duplicatedBytecode.end(), foreachLoopBytecode.begin(), foreachLoopBytecode.end());
                            const unsigned int end = duplicatedBytecode.size();

                            //remap all Ids
                            if (!remapAllIds(duplicatedBytecode, start, end, remapTable))
                                return error("remapAllIds failed on duplicatedBytecode");

                            //for all call to a function through the composition variable we're instancing, update the instance num
                            while (start < end)
                            {
                                unsigned int wordCount = opWordCount(duplicatedBytecode[start]);
                                spv::Op opCode = opOpCode(duplicatedBytecode[start]);
                                switch (opCode)
                                {
                                    case spv::OpFunctionCallThroughCompositionVariable:
                                    {
                                        spv::Id shaderId = duplicatedBytecode[start + 4];
                                        int compositionId = duplicatedBytecode[start + 5];
                                        int initialInstanceNum = duplicatedBytecode[start + 6];

                                        if (compositionToUnroll->compositionShaderOwner->GetId() == shaderId && compositionToUnroll->compositionShaderId == compositionId)
                                        {
                                            duplicatedBytecode[start + 6] = instanceNum;
                                        }

                                        break;
                                    }
                                }
                                start += wordCount;
                            }
                        }
                    }

                    forEachLoopToUnroll->foreachDuplicatedBytecode = duplicatedBytecode;

                    //insert the foreach loop, sorted by their reversed apparition in the bytecode
                    auto itList = listForeachLoopsToMergeIntoBytecode.begin();
                    while (itList != listForeachLoopsToMergeIntoBytecode.end())
                    {
                        if (forEachLoopToUnroll->foreachLoopStart > (*itList)->foreachLoopStart) break;
                    }
                    listForeachLoopsToMergeIntoBytecode.insert(itList, forEachLoopToUnroll);

                }  //end of if (anythingToUnroll)
            } //for (auto itfe = vecAllForeachLoops.begin(); itfe != vecAllForeachLoops.end(); itfe++)

            //we finished unrolling all foreach loops for the nested level, we can apply them to the bytecode
            unsigned int insertionPos = spv.size();
            for (auto itfe = listForeachLoopsToMergeIntoBytecode.begin(); itfe != listForeachLoopsToMergeIntoBytecode.end(); itfe++)
            {
                CompositionForEachLoopData* forEachLoopToMerge = *itfe;
                if (forEachLoopToMerge->foreachLoopEnd > insertionPos) return error("foreach loops are not sorted correctly");
                insertionPos = forEachLoopToMerge->foreachLoopEnd;
                
                spv.insert(spv.begin() + insertionPos, forEachLoopToMerge->foreachDuplicatedBytecode.begin(), forEachLoopToMerge->foreachDuplicatedBytecode.end());
            }

            //we updated the bytecode, so refetch all foreach loops
            setBound(maxId);
            if (!GetAllCompositionForEachLoops(vecAllForeachLoops, maxForEachLoopsNestedLevel)) {
                return error(string("Failed to retrieve all composition foreach loops for the composition: "));
            }

        } //end for (int nestedLevelToProcess = maxForEachLoopsNestedLevel; nestedLevelToProcess >= 0; nestedLevelToProcess--)
    } 

    //======================================================================================
    //remove all foreach loops
    if (vecAllForeachLoops.size() > 0)
    {
        vector<range_t> vecStripRanges;
        for (auto itfe = vecAllForeachLoops.begin(); itfe != vecAllForeachLoops.end(); itfe++)
        {
            const CompositionForEachLoopData& forEachLoop = *itfe;
            if (forEachLoop.nestedLevel == 0)  //nested level > 0 are included inside level 0
            {
                vecStripRanges.push_back(range_t(forEachLoop.foreachLoopStart, forEachLoop.foreachLoopEnd));
            }
        }

        stripBytecode(vecStripRanges);
        if (errorMessages.size() > 0) return error("ApplyAllCompositions: failed to remove the foreach loops");
        //Update all maps (update objects position in the bytecode)
        if (!UpdateAllMaps()) return error("ApplyAllCompositions: failed to update all maps");
    }

    //===================================================================================================================
    //===================================================================================================================
    //updates all OpFunctionCallThroughCompositionVariable instructions calling through the composition that we're instancing   
    {
        vector<range_t> vecStripRanges;
        unsigned int start = header_size;
        const unsigned int end = spv.size();
        while (start < end)
        {
            unsigned int wordCount = asWordCount(start);
            spv::Op opCode = asOpCode(start);

            switch (opCode)
            {
                case spv::OpFunctionCallThroughCompositionVariable:
                {
                    spv::Id shaderId = asId(start + 4);
                    int compositionId = asLiteralValue(start + 5);
                    unsigned int instancesNum = asLiteralValue(start + 6);

                    //===================================================
                    //Find the composition data
                    ShaderClassData* compositionShaderOwner = this->GetShaderById(shaderId);
                    if (compositionShaderOwner == nullptr) { error(string("No shader exists with the Id: ") + to_string(shaderId)); break; }

                    ShaderComposition* composition = compositionShaderOwner->GetShaderCompositionById(compositionId);
                    if (composition == nullptr) { error(string("Shader: ") + compositionShaderOwner->GetName() + string(" has no composition for id: ") + to_string(compositionId)); break; }

                    // If an OpFunctionCallThroughCompositionVariable instructions has no composition instance we ignore it
                    // (it won't generate an error as long as the instruction is not called by any compilation output functions)
                    if (composition->countInstances == 0) break;

                    vector<ShaderClassData*> vecCompositionShaderInstances;
                    if (!GetAllShaderInstancesForComposition(composition, vecCompositionShaderInstances)) {
                        return error(string("Failed to retrieve the instances for the composition: ") + composition->variableName + string(" from shader: ") + composition->compositionShaderOwner->GetName());
                    }

                    if (instancesNum >= vecCompositionShaderInstances.size()) { error(string("Invalid instanceNum number: ") + to_string(instancesNum)); break; }
                    ShaderClassData* clonedShader = vecCompositionShaderInstances[instancesNum];

                    //===================================================
                    //Get the original function called
                    spv::Id originalFunctionId = asId(start + 3);
                    FunctionInstruction* functionToReplace = GetFunctionById(originalFunctionId);
                    if (functionToReplace == nullptr) {
                        error(string("OpFunctionCallThroughCompositionVariable: targeted Id is not a known function. Id: ") + to_string(originalFunctionId));
                        return true;
                    }

                    //We retrieve the cloned function using the function name
                    FunctionInstruction* functionTarget = GetTargetedFunctionByNameWithinShaderAndItsFamily(clonedShader, functionToReplace->GetName());
                    if (functionTarget == nullptr) {
                        error(string("OpFunctionCallThroughCompositionVariable: cannot retrieve the function in the cloned shader. Function name: ") + functionToReplace->GetName());
                        return true;
                    }

                    //If the function is overriden by another, change the target
                    if (functionTarget->GetOverridingFunction() != nullptr) functionTarget = functionTarget->GetOverridingFunction();

                    int wordCount = asWordCount(start);
                    vecStripRanges.push_back(range_t(start + 4, start + 6 + 1));   //will remove composition variable ids from the bytecode
                    setOpAndWordCount(start, spv::OpFunctionCall, wordCount - 3);  //change instruction OpFunctionCallThroughCompositionVariable to OpFunctionCall
                    setId(start + 3, functionTarget->GetId());                     //replace the function called id

                    break;
                }
            }
            start += wordCount;
        }

        if (vecStripRanges.size() > 0)
        {
            //remove the stripped bytecode first (to have a consistent bytecode)
            stripBytecode(vecStripRanges);

            if (errorMessages.size() > 0) {
                return error("ApplyAllCompositions: failed to retarget the functions called by the compositions");
            }

            //Update all maps (update objects position in the bytecode)
            if (!UpdateAllMaps()) return error("ApplyAllCompositions: failed to update all maps");
        }
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

//For every call to a function using base accessor (base.function()), we check if we need to redirect to another overriding method
bool SpxStreamRemapper::UpdateFunctionCallsHavingUnresolvedBaseAccessor()
{
    unsigned int start = header_size;
    const unsigned int end = spv.size();
    while (start < end)
    {
        unsigned int wordCount = asWordCount(start);
        spv::Op opCode = asOpCode(start);

        switch (opCode)
        {
            case spv::OpFunctionCallBaseUnresolved:
            {
                setOpCode(start, spv::OpFunctionCallBaseResolved); // change OpFunctionCallBaseUnresolved to OpFunctionCallBaseResolved

                spv::Id functionCalledId = asId(start + 3);

                FunctionInstruction* functionCalled = GetFunctionById(functionCalledId);
                if (functionCalled != nullptr)
                {
                    if (functionCalled->GetOverridingFunction() != nullptr)
                    {
                        spv::Id overridingFunctionId = functionCalled->GetOverridingFunction()->GetResultId();
                        spv[start + 3] = overridingFunctionId;
                    }
                }
                else
                    error(string("OpFunctionCallBaseUnresolved: targeted Id is not a known function. Id: ") + to_string(functionCalledId));

                break;
            }
        }
        start += wordCount;
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

bool SpxStreamRemapper::UpdateOpFunctionCallTargetsInstructionsToOverridingFunctions()
{
    vector<FunctionInstruction*> vecFunctionIdBeingOverriden;
    vecFunctionIdBeingOverriden.resize(bound(), nullptr);
    bool anyOverridingFunction = false;
    for (auto itfn = vecAllFunctions.begin(); itfn != vecAllFunctions.end(); itfn++)
    {
        FunctionInstruction* function = *itfn;
        if (function->GetOverridingFunction() != nullptr)
        {
            vecFunctionIdBeingOverriden[function->GetResultId()] = function->GetOverridingFunction();
            anyOverridingFunction = true;
        }
    }

    if (!anyOverridingFunction) return true; //nothing to override

    unsigned int start = header_size;
    const unsigned int end = spv.size();
    while (start < end)
    {
        unsigned int wordCount = asWordCount(start);
        spv::Op opCode = asOpCode(start);

        switch (opCode)
        {
            // call to base function (OpFunctionCallBaseResolved, OpFunctionCallBaseUnresolved, OpFunctionCallThroughStaticShaderClassCall) are ignored

            case spv::OpFunctionCall:
            case spv::OpFunctionCallThroughCompositionVariable:
            {
                spv::Id functionCalledId = asId(start + 3);
#ifdef XKSLANG_DEBUG_MODE
                if (functionCalledId < 0 || functionCalledId >= vecFunctionIdBeingOverriden.size()){
                    error(string("function call Id is out of bound. Id: ") + to_string(functionCalledId));
                    break;
                }
#endif
                FunctionInstruction* overridingFunction = vecFunctionIdBeingOverriden[functionCalledId];
                if (overridingFunction != nullptr)
                {
                    spv::Id overridingFunctionId = overridingFunction->GetResultId();
                    spv[start + 3] = overridingFunctionId;
                }

                break;
            }
        }
        start += wordCount;
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

void SpxStreamRemapper::GetShaderChildrenList(ShaderClassData* shader, vector<ShaderClassData*>& children)
{
    children.clear();
    for (auto itsh = vecAllShaders.begin(); itsh != vecAllShaders.end(); itsh++)
    {
        ShaderClassData* aShader = *itsh;
        if (aShader == shader) continue;

        if (aShader->HasParent(shader)) children.push_back(aShader);
    }
}

//Return the full dependency of the shader from 
// - the complete family tree (parents of all shaders)
// - the composition type and instances of all shaders
// - for all shaders: parse the function's bytecode and detect if they're calling any other shaders
bool SpxStreamRemapper::GetShadersFullDependencies(SpxStreamRemapper* bytecodeSource, const vector<ShaderClassData*>& listShaders, vector<ShaderClassData*>& fullDependencies)
{
    fullDependencies.clear();

    //setup all shaders
    for (auto itsh = bytecodeSource->vecAllShaders.begin(); itsh != bytecodeSource->vecAllShaders.end(); itsh++){
        ShaderClassData* aShader = *itsh;
        aShader->flag = 0;
        aShader->dependencyType = ShaderClassData::ShaderDependencyTypeEnum::Undefined;
    }

    //setup list of initial shader
    vector<ShaderClassData*> vecCompositionShaderInstances;
    vector<ShaderClassData*> listShaderToValidate;
    for (auto itsh = listShaders.begin(); itsh != listShaders.end(); itsh++) {
        ShaderClassData* aShaderToValidate = *itsh;
        aShaderToValidate->dependencyType = ShaderClassData::ShaderDependencyTypeEnum::Other;
        listShaderToValidate.push_back(aShaderToValidate);
    }

    while (listShaderToValidate.size() > 0)
    {
        ShaderClassData* aShader = listShaderToValidate.back();
        listShaderToValidate.pop_back();
        if (aShader->flag != 0) continue;  //the shader has already been treated
        aShader->flag = 1;

#ifdef XKSLANG_DEBUG_MODE
        if (aShader->bytecodeSource != bytecodeSource){  //double check that the bytecode comes from the same source
            return bytecodeSource->error(string("Shader: ") + aShader->GetName() + string(" does not belong to the bytecode source"));
        }
        if (aShader->dependencyType == ShaderClassData::ShaderDependencyTypeEnum::Undefined) {
            return bytecodeSource->error(string("unset dependencyType for Shader: ") + aShader->GetName());
        }
#endif

        fullDependencies.push_back(aShader);
        
        //add all parents
        for (auto itsh = aShader->parentsList.begin(); itsh != aShader->parentsList.end(); itsh++)
        {
            ShaderClassData* aShaderParent = *itsh;
            aShaderParent->dependencyType = ShaderClassData::ShaderDependencyTypeEnum::Other;
            if (aShaderParent->flag == 0) listShaderToValidate.push_back(aShaderParent);
        }

        //add all compositions type
        for (auto itc = aShader->compositionsList.begin(); itc != aShader->compositionsList.end(); itc++)
        {
            const ShaderComposition& aComposition = *itc;
            aComposition.shaderType->dependencyType = ShaderClassData::ShaderDependencyTypeEnum::Other;
            if (aComposition.shaderType->flag == 0) listShaderToValidate.push_back(aComposition.shaderType);

            //add the composition instances
            if (aComposition.countInstances > 0)
            {
                if (!bytecodeSource->GetAllShaderInstancesForComposition(&aComposition, vecCompositionShaderInstances)) {
                    return bytecodeSource->error(string("Failed to retrieve the instances for the composition: ") + aComposition.variableName +
                        string(" from shader: ") + aComposition.compositionShaderOwner->GetName());
                }

                for (unsigned int i = 0; i < vecCompositionShaderInstances.size(); ++i)
                {
                    ShaderClassData* aShaderCompositionInstance = vecCompositionShaderInstances[i];
                    aShaderCompositionInstance->dependencyType = ShaderClassData::ShaderDependencyTypeEnum::Other;
                    if (aShaderCompositionInstance->flag == 0) listShaderToValidate.push_back(aShaderCompositionInstance);
                }
            }
        }

        //if a function has been overrided by a subclass, include the subclass as well
        for (auto itf = aShader->functionsList.begin(); itf != aShader->functionsList.end(); itf++)
        {
            FunctionInstruction* aFunctionFromShader = *itf;
            FunctionInstruction* overridingFunction = aFunctionFromShader->GetOverridingFunction();
            if (overridingFunction != nullptr){
                overridingFunction->shaderOwner->dependencyType = ShaderClassData::ShaderDependencyTypeEnum::Other;
                if (overridingFunction->shaderOwner->flag == 0) listShaderToValidate.push_back(overridingFunction->shaderOwner);
            }
        }

        //check the functions's bytecode, looking for function calls to static class
        for (auto itf = aShader->functionsList.begin(); itf != aShader->functionsList.end(); itf++)
        {
            FunctionInstruction* aFunctionFromShader = *itf;
            
            unsigned int start = aFunctionFromShader->bytecodeStartPosition;
            const unsigned int end = aFunctionFromShader->bytecodeEndPosition;
            while (start < end)
            {
                unsigned int wordCount = bytecodeSource->asWordCount(start);
                spv::Op opCode = bytecodeSource->asOpCode(start);

                switch (opCode)
                {
                    case spv::OpFunctionCall:
                    case spv::OpFunctionCallBaseResolved:
                    case spv::OpFunctionCallBaseUnresolved:
                    case spv::OpFunctionCallThroughStaticShaderClassCall:
                    case spv::OpFunctionCallThroughCompositionVariable:
                    {
                        spv::Id functionCalledId = bytecodeSource->asId(start + 3);
                        FunctionInstruction* functionCalled = bytecodeSource->GetFunctionById(functionCalledId);
                        if (functionCalled == nullptr) {
                            return bytecodeSource->error(string("Failed to retrieve the function for Id: ") + to_string(functionCalledId));
                        }

                        ShaderClassData* functionShaderOwner = functionCalled->shaderOwner;
                        if (functionShaderOwner != nullptr && functionShaderOwner->flag == 0)
                        {
                            //if we're calling a static function, set the dependency type to static (unless it was already set to another dependency type)
                            if (functionCalled->IsStatic() && functionShaderOwner->dependencyType == ShaderClassData::ShaderDependencyTypeEnum::Undefined)
                                functionShaderOwner->dependencyType = ShaderClassData::ShaderDependencyTypeEnum::StaticFunctionCall;
                            else
                                functionShaderOwner->dependencyType = ShaderClassData::ShaderDependencyTypeEnum::Other;
                            listShaderToValidate.push_back(functionShaderOwner);
                        }

                        break;
                    }
                }
                start += wordCount;
            }
        }
    }

    return true;
}

void SpxStreamRemapper::GetShaderFamilyTree(ShaderClassData* shaderFromFamily, vector<ShaderClassData*>& shaderFamilyTree)
{
    shaderFamilyTree.clear();

    for (auto itsh = vecAllShaders.begin(); itsh != vecAllShaders.end(); itsh++) (*itsh)->flag = 0;

    vector<ShaderClassData*> children;
    vector<ShaderClassData*> listShaderToValidate;
    listShaderToValidate.push_back(shaderFromFamily);
    while (listShaderToValidate.size() > 0)
    {
        ShaderClassData* aShader = listShaderToValidate.back();
        listShaderToValidate.pop_back();

        if (aShader->flag != 0) continue;  //the shader has already been added
        shaderFamilyTree.push_back(aShader);
        aShader->flag = 1;

        //Add all parents
        for (auto itsh = aShader->parentsList.begin(); itsh != aShader->parentsList.end(); itsh++)
            listShaderToValidate.push_back(*itsh);

        //Add all children
        GetShaderChildrenList(aShader, children);
        for (auto itsh = children.begin(); itsh != children.end(); itsh++)
            listShaderToValidate.push_back(*itsh);
    }
}

static bool shaderLevelSortFunction(SpxStreamRemapper::ShaderClassData* sa, SpxStreamRemapper::ShaderClassData* sb)
{
    return (sa->level < sb->level);
}

bool SpxStreamRemapper::UpdateOverridenFunctionMap(vector<ShaderClassData*>& listShadersMerged)
{
    if (listShadersMerged.size() == 0) return true;

    //we need to know the shader level before building the overriding map
    if (!ComputeShadersLevel())
        return error(string("Failed to build the shader level"));

    //sort the shaders according to their level
    sort(listShadersMerged.begin(), listShadersMerged.end(), shaderLevelSortFunction);

    for (auto itshaderMerged = listShadersMerged.begin(); itshaderMerged != listShadersMerged.end(); itshaderMerged++)
    {
        ShaderClassData* shaderWithOverridingFunctions = *itshaderMerged;
        vector<ShaderClassData*> shaderFamilyTree;
        vector<FunctionInstruction*> listOverridingFunctions;

        //get the list of all functions having the override attribute
        for (auto itOverridingFn = shaderWithOverridingFunctions->functionsList.begin(); itOverridingFn != shaderWithOverridingFunctions->functionsList.end(); itOverridingFn++)
        {
            FunctionInstruction* shaderFunction = *itOverridingFn;
            if (shaderFunction->GetOverrideAttributeState() == FunctionInstruction::OverrideAttributeStateEnum::Defined)
                listOverridingFunctions.push_back(shaderFunction);
        }
        if (listOverridingFunctions.size() == 0) continue;

        //the overriding functions will override every functions having the same name, from the whole parents tree
        GetShaderFamilyTree(shaderWithOverridingFunctions, shaderFamilyTree);
        for (auto itOverridingFn = listOverridingFunctions.begin(); itOverridingFn != listOverridingFunctions.end(); itOverridingFn++)
        {
            FunctionInstruction* overridingFunction = *itOverridingFn;
            overridingFunction->SetOverrideAttributeState(FunctionInstruction::OverrideAttributeStateEnum::Processed);
            overridingFunction->SetOverridingFunction(nullptr); //the overriding function overrides itself by default (this could have been set to a different function previously)
            const string& overringFunctionName = overridingFunction->GetMangledName();

            for (auto itShaderFromTree = shaderFamilyTree.begin(); itShaderFromTree != shaderFamilyTree.end(); itShaderFromTree++)
            {
                ShaderClassData* shaderFromFamily = *itShaderFromTree;
                if (shaderFromFamily == shaderWithOverridingFunctions) continue;

                for (auto itFn = shaderFromFamily->functionsList.begin(); itFn != shaderFromFamily->functionsList.end(); itFn++)
                {
                    FunctionInstruction* aFunction = *itFn;
                    if (overringFunctionName == aFunction->GetMangledName())
                    {
                        //Got a function to be overriden !
                        aFunction->SetOverridingFunction(overridingFunction);
                    }
                }
            }
        }
    }

    if (errorMessages.size() > 0) return false;

    return true;
}

void SpxStreamRemapper::GetMixinBytecode(vector<uint32_t>& bytecodeStream)
{
    bytecodeStream.clear();
    bytecodeStream.insert(bytecodeStream.end(), spv.begin(), spv.end());
}

bool SpxStreamRemapper::GetFunctionLabelInstructionPosition(FunctionInstruction* function, unsigned int& labelPos)
{
    unsigned int start = function->bytecodeStartPosition;
    const unsigned int end = function->bytecodeEndPosition;
    while (start < end)
    {
        unsigned int wordCount = asWordCount(start);
        spv::Op opCode = asOpCode(start);
        switch (opCode)
        {
            case spv::OpLabel:
            {
                labelPos = start;
                return true;
            }
        }

        start += wordCount;
    }

    return false;
}

bool SpxStreamRemapper::GetFunctionLabelAndReturnInstructionsPosition(FunctionInstruction* function, unsigned int& labelPos, unsigned int& returnPos)
{
    int state = 0;
    unsigned int start = function->bytecodeStartPosition;
    const unsigned int end = function->bytecodeEndPosition;
    while (start < end)
    {
        unsigned int wordCount = asWordCount(start);
        spv::Op opCode = asOpCode(start);
        switch (opCode)
        {
            case spv::OpLabel:
            {
                if (state != 0) return error("Failed to parse the function: expecting OpLabel instruction");
                labelPos = start;
                state = 1;
                break;
            }
            case spv::OpReturn:
            case spv::OpReturnValue:
            {
                if (state != 1) return error("Failed to parse the function: expecting OpReturn or OpReturnValue instruction");
                returnPos = start;
                state = 2;
                break;
            }
        }

        start += wordCount;
    }

    if (state != 2) return error("Failed to parse the function: missing some opInstructions");
    return true;
}

SpxStreamRemapper::FunctionInstruction* SpxStreamRemapper::GetShaderFunctionForEntryPoint(string entryPointName)
{
    // Search for the entry point function (assume the first function with the name is the one)
    FunctionInstruction* entryPointFunction = nullptr;
    for (auto it = vecAllFunctions.begin(); it != vecAllFunctions.end(); it++)
    {
        FunctionInstruction* func = *it;
        if (func->shaderOwner == nullptr) continue;  //we're looking for a function owned by a shader

        string mangledFunctionName = func->GetMangledName();
        string unmangledFunctionName = mangledFunctionName.substr(0, mangledFunctionName.find('('));
        if (unmangledFunctionName == entryPointName)
        {
            entryPointFunction = func;
            //has the function been overriden?
            if (entryPointFunction->GetOverridingFunction() != nullptr) entryPointFunction = entryPointFunction->GetOverridingFunction();
            break;
        }
    }

    return entryPointFunction;
}

bool SpxStreamRemapper::RemoveAndConvertSPXExtensions()
{
    if (status != SpxRemapperStatusEnum::MixinBeingCompiled_UnusedShaderRemoved) return error("Invalid remapper status");
    status = SpxRemapperStatusEnum::MixinBeingCompiled_ConvertedToSPV;

    //===================================================================================================================
    //Convert SPIRX to SPIRV (remove all SPIRX extended instructions). So we don't need to repeat it for every stages
    vector<range_t> vecStripRanges;
    unsigned int start = header_size;
    const unsigned int end = spv.size();
    while (start < end)
    {
        unsigned int wordCount = asWordCount(start);
        spv::Op opCode = asOpCode(start);

        switch (opCode)
        {
            case spv::OpEntryPoint:
            {
                //remove all current entry points (if there is any): we will set a new one for each stages
                stripInst(vecStripRanges, start);
                break;
            }
            case spv::OpFunctionCallBaseResolved:
            case spv::OpFunctionCallThroughStaticShaderClassCall:
            {
                //change OpCode to OpFunctionCall
                setOpCode(start, spv::OpFunctionCall);
                break;
            }
            case spv::OpFunctionCallThroughCompositionVariable:
            {
                error(string("Found unresolved OpFunctionCallThroughCompositionVariable at: ") + to_string(start));
                break;
            }
            case spv::OpFunctionCallBaseUnresolved:
            {
                error(string("A function base call has an unresolved state at: ") + to_string(start));
                break;
            }
            case spv::OpForEachCompositionStartLoop:
            {
                error(string("A foreach start loop has not been processed at: ") + to_string(start));
                break;
            }
            case spv::OpForEachCompositionEndLoop:
            {
                error(string("A foreach end loop has not been processed at: ") + to_string(start));
                break;
            }
            case spv::OpBelongsToShader:
            case spv::OpDeclarationName:
            case spv::OpTypeXlslShaderClass:
            case spv::OpShaderInheritance:
            case spv::OpShaderCompositionDeclaration:
            case spv::OpShaderCompositionInstance:
            case spv::OpMethodProperties:
            case spv::OpGSMethodProperties:
            case spv::OpMemberProperties:
            case spv::OpCBufferMemberProperties:
            case spv::OpMemberSemanticName:
            {
                stripInst(vecStripRanges, start);
                break;
            }
        }
        start += wordCount;
    }

    if (errorMessages.size() > 0) return false;

    stripBytecode(vecStripRanges);
    if (!UpdateAllMaps()) return error("RemoveAndConvertSPXExtensions: failed to update all maps");

    if (errorMessages.size() > 0) return false;
    return true;
}

//Mixin is finalized: no more updates will be brought to the mixin bytecode after
bool SpxStreamRemapper::GenerateBytecodeForAllStages(vector<XkslMixerOutputStage>& outputStages)
{
    if (status != SpxRemapperStatusEnum::MixinBeingCompiled_ConvertedToSPV) return error("Invalid remapper status");
    status = SpxRemapperStatusEnum::MixinBeingCompiled_SPXBytecodeRemoved;

    if (outputStages.size() == 0) return error("no output stages defined");

    //===================================================================================================================
    //===================================================================================================================
    //Get the IDs of all cbuffers (plus their pointer and variable types)
    //We keep all cbuffers, even if some are not used by the output stage (unused cbuffers have already been removed)
    vector<spv::Id> listCBufferIds;

    /*
    {
        for (auto it = listAllObjects.begin(); it != listAllObjects.end(); ++it)
        {
            ObjectInstructionBase* obj = *it;
            if (obj == nullptr) continue;

            switch (obj->GetKind())
            {
                case ObjectInstructionTypeEnum::Type:
                {
                    TypeInstruction* type = dynamic_cast<TypeInstruction*>(obj);
                    if (type->IsCBuffer()) listCBufferIds.push_back(type->GetId());
                    else
                    {
                        if (type->pointerTo != nullptr && type->pointerTo->IsCBuffer()) listCBufferIds.push_back(type->GetId());
                    }
                    break;
                }

                case ObjectInstructionTypeEnum::Variable:
                {
                    VariableInstruction* variable = dynamic_cast<VariableInstruction*>(obj);
                    if (variable->variableTo != nullptr && variable->variableTo->pointerTo != nullptr && variable->variableTo->pointerTo->IsCBuffer())
                        listCBufferIds.push_back(variable->GetId());
                }
            }
        }
    }
    */

    //===================================================================================================================
    // Generate the SPIRV bytecode for all stages
    for (unsigned int i = 0; i<outputStages.size(); ++i)
    {
        XkslMixerOutputStage& outputStage = outputStages[i];

        //Clean the bytecode of all unused stuff!
        if (!GenerateBytecodeForStage(outputStage, listCBufferIds))
        {
            error("Failed to set and clean the stage bytecode");
            outputStage.outputStage->resultingBytecode.clear();
            break;
        }
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

bool SpxStreamRemapper::GenerateBytecodeForStage(XkslMixerOutputStage& stage, vector<spv::Id>& listObjectIdsToKeep)
{
    if (status != SpxRemapperStatusEnum::MixinBeingCompiled_SPXBytecodeRemoved) return error("Invalid remapper status");
    FunctionInstruction* entryFunction = stage.entryFunction;
    if (entryFunction == nullptr) return error("The stage has no entry function");

    //======================================================================================================
    //Find all functions being called by the stage
    vector<FunctionInstruction*> listAllFunctionsCalledByTheStage;
    {
        //Set all functions flag to 0 (to check a function only once)
        for (auto itsf = vecAllFunctions.begin(); itsf != vecAllFunctions.end(); itsf++) {
            FunctionInstruction* aFunction = *itsf;
            aFunction->flag1 = 0;
        }

        vector<FunctionInstruction*> vectorFunctionsToCheck;
        vectorFunctionsToCheck.push_back(stage.entryFunction);
        stage.entryFunction->flag1 = 1;
        while (vectorFunctionsToCheck.size() > 0)
        {
            FunctionInstruction* aFunctionCalled = vectorFunctionsToCheck.back();
            vectorFunctionsToCheck.pop_back();
            listAllFunctionsCalledByTheStage.push_back(aFunctionCalled);

            //check if the function is calling any other function
            unsigned int start = aFunctionCalled->bytecodeStartPosition;
            const unsigned int end = aFunctionCalled->bytecodeEndPosition;
            while (start < end)
            {
                unsigned int wordCount = asWordCount(start);
                spv::Op opCode = asOpCode(start);

                switch (opCode)
                {
                    case spv::OpFunctionCall:
                    case spv::OpFunctionCallBaseResolved:
                    case spv::OpFunctionCallThroughStaticShaderClassCall:
                    {
                        //pile the function to go check it later
                        spv::Id functionCalledId = asId(start + 3);
                        FunctionInstruction* anotherFunctionCalled = GetFunctionById(functionCalledId);
                        if (anotherFunctionCalled == nullptr) return error("Failed to find the function for id: " + to_string(functionCalledId));
                        if (anotherFunctionCalled->flag1 == 0) {
                            anotherFunctionCalled->flag1 = 1;
                            vectorFunctionsToCheck.push_back(anotherFunctionCalled); //we'll analyse the function later
                        }
                        break;
                    }

                    case spv::OpFunctionCallThroughCompositionVariable:
                    case spv::OpFunctionCallBaseUnresolved:
                    {
                        return error(string("An unresolved function call has been found in function: ") + aFunctionCalled->GetFullName());
                        break;
                    }
                }
                start += wordCount;
            }
        }
    }

    //======================================================================================================
    //Init the list of IDs that we need to keep
    vector<bool> listIdsUsed;
    listIdsUsed.resize(bound(), false);

    //Keep some IDs
    for (auto it = listObjectIdsToKeep.begin(); it != listObjectIdsToKeep.end(); it++)
    {
        const spv::Id& id = *it;
#ifdef XKSLANG_DEBUG_MODE
        if (id >= listIdsUsed.size()) return error("An Id is out of bound");
#endif
        listIdsUsed[id] = true;
    }

    //======================================================================================================
    //======================================================================================================
    //Build the header
    vector<uint32_t> header;
    header.insert(header.begin(), spv.begin(), spv.begin() + header_size);

    //======================================================================================================
    //Add stage capabilities and execution mode (cf TGlslangToSpvTraverser::TGlslangToSpvTraverser function)
    vector<spv::Capability> capabilities;
    vector<SPVHeaderStageExecutionMode> executionModes;
    {
        switch (stage.outputStage->stage)
        {
            case ShadingStageEnum::Vertex:
                capabilities.push_back(spv::CapabilityShader);
                break;
            case ShadingStageEnum::Geometry:
                capabilities.push_back(spv::CapabilityGeometry);
                return error("stage not processed yet"); //(cf TGlslangToSpvTraverser::TGlslangToSpvTraverser function)
                break;
            case ShadingStageEnum::TessControl:
                capabilities.push_back(spv::CapabilityTessellation);
                return error("stage not processed yet"); //(cf TGlslangToSpvTraverser::TGlslangToSpvTraverser function)
                break;
            case ShadingStageEnum::TessEvaluation:
                capabilities.push_back(spv::CapabilityTessellation);
                return error("stage not processed yet"); //(cf TGlslangToSpvTraverser::TGlslangToSpvTraverser function)
                break;
            case ShadingStageEnum::Pixel:
                capabilities.push_back(spv::CapabilityShader);
                executionModes.push_back(SPVHeaderStageExecutionMode(spv::ExecutionModeOriginUpperLeft));  //default for now!!
                break;
            case ShadingStageEnum::Compute:
                capabilities.push_back(spv::CapabilityShader);
                return error("stage not processed yet"); //(cf TGlslangToSpvTraverser::TGlslangToSpvTraverser function)
                break;
            default:
                return error("unknown stage");
        }
        for (auto itc = capabilities.begin(); itc != capabilities.end(); itc++)
        {
            const spv::Capability& capability = *itc;
            spv::Instruction capInst(0, 0, spv::OpCapability);
            capInst.addImmediateOperand(capability);
            capInst.dump(header);
        }
    }

    //Add the entry point instruction
    spv::ExecutionModel model = GetShadingStageExecutionMode(stage.outputStage->stage);
    if (model == spv::ExecutionModelMax) return error("Unknown stage");
    spv::Instruction entryPointInstr(spv::OpEntryPoint);
    entryPointInstr.addImmediateOperand(model);
    entryPointInstr.addIdOperand(entryFunction->GetResultId());
    entryPointInstr.addStringOperand(stage.outputStage->entryPointName.c_str());
    entryPointInstr.dump(header);

    //add the execution modes
    for (auto itc = executionModes.begin(); itc != executionModes.end(); itc++)
    {
        const SPVHeaderStageExecutionMode& execMode = *itc;
        spv::Instruction instrExecMode(spv::OpExecutionMode);
        instrExecMode.addIdOperand(entryFunction->GetResultId());
        instrExecMode.addImmediateOperand(execMode.mode);
        if (execMode.value1 >= 0) instrExecMode.addImmediateOperand(execMode.value1);
        if (execMode.value2 >= 0) instrExecMode.addImmediateOperand(execMode.value2);
        if (execMode.value3 >= 0) instrExecMode.addImmediateOperand(execMode.value3);
        instrExecMode.dump(header);
    }

    //======================================================================================================
    //======================================================================================================
    // Add some header data from original spv files
    {
        unsigned int start = header_size;
        const unsigned int end = spv.size();
        while (start < end)
        {
            unsigned int wordCount = asWordCount(start);
            spv::Op opCode = asOpCode(start);

            //By default: copy memoryModel and external import (this might change later depending on the stage)
            switch (opCode)
            {
                case spv::OpMemoryModel:
                {
                    header.insert(header.end(), spv.begin() + start, spv.begin() + (start + wordCount));
                    break;
                }
                case spv::OpExtInstImport:
                {
                    spv::Id resultId = asId(start + 1);
                    listIdsUsed[resultId] = true;
                    header.insert(header.end(), spv.begin() + start, spv.begin() + (start + wordCount));
                    break;
                }

                case spv::OpName:
                case spv::OpMemberName:
                case spv::OpFunction:
                case spv::OpConstant:
                case spv::OpTypeInt:
                {
                    start = end;
                    break;
                }
            }

            start += wordCount;
        }
    }

    //======================================================================================================
    //parse all functions called by the stage to get all IDs needed
    for (auto itf = listAllFunctionsCalledByTheStage.begin(); itf != listAllFunctionsCalledByTheStage.end(); itf++)
    {
        FunctionInstruction* aFunctionCalled = *itf;

        spv::Op opCode;
        unsigned int wordCount;
        unsigned int start = aFunctionCalled->GetBytecodeStartPosition();
        const unsigned int end = aFunctionCalled->GetBytecodeEndPosition();
        while (start < end)
        {
            //add all IDs in the list of IDs to keep
            if (!flagAllIdsFromInstruction(start, opCode, wordCount, listIdsUsed))
                return error("Error flagging all IDs from the instruction");
        
            start += wordCount;
        }
    }

    //======================================================================================================
    //For all IDs used: if it's defining a type, variable or const: check if it's depending on another type no included in the list yet
    {
        vector<spv::Id> idToCheckForExtraIds;
        for (unsigned int id = 0; id < listIdsUsed.size(); ++id)
        {
            if (listIdsUsed[id] == true)  //the id is used
            {
                if (listAllObjects[id] != nullptr)  //the id has been recognized as a data object
                {
                    ObjectInstructionBase* obj = listAllObjects[id];
                    switch (obj->GetKind())
                    {
                        case ObjectInstructionTypeEnum::Type:
                        case ObjectInstructionTypeEnum::Variable:
                        case ObjectInstructionTypeEnum::Const:
                            idToCheckForExtraIds.push_back(id);
                            break;
                    }
                }
            }
        }

        spv::Op opCode;
        unsigned int wordCount;
        vector<spv::Id> listIds;
        spv::Id typeId, resultId;
        while (idToCheckForExtraIds.size() > 0)
        {
            spv::Id idToCheckForUnreferencedIds = idToCheckForExtraIds.back();
            idToCheckForExtraIds.pop_back();

            ObjectInstructionBase* obj = listAllObjects[idToCheckForUnreferencedIds];
            listIds.clear();
            if (!parseInstruction(obj->bytecodeStartPosition, opCode, wordCount, typeId, resultId, listIds))
                return error("Error parsing the instruction");

#ifdef XKSLANG_DEBUG_MODE
            if (resultId != idToCheckForUnreferencedIds) return error("The type, variable or const does not match its parsed instruction");  //sanity check
#endif

            if (typeId != spvUndefinedId && listIdsUsed[typeId] == false) listIds.push_back(typeId);
            for (unsigned int k = 0; k < listIds.size(); ++k)
            {
                const spv::Id& anotherId = listIds[k];
                if (listIdsUsed[anotherId] == false)
                {
                    //we'll need this other id
                    listIdsUsed[anotherId] = true;
                    
                    //If this other id is a type, const, or variable: we will need to check it as well
                    if (listAllObjects[anotherId] != nullptr)
                    {
                        ObjectInstructionBase* anotherObj = listAllObjects[anotherId];
                        switch (anotherObj->GetKind())
                        {
                            case ObjectInstructionTypeEnum::Type:
                            case ObjectInstructionTypeEnum::Variable:
                            case ObjectInstructionTypeEnum::Const:
                                idToCheckForExtraIds.push_back(anotherId);
                                break;
                        }
                    }
                }
            }
        }
    }

    //======================================================================================================
    //copy the header
    vector<uint32_t>& stageBytecode = stage.outputStage->resultingBytecode.getWritableBytecodeStream();
    stageBytecode.clear();
    stageBytecode.insert(stageBytecode.end(), header.begin(), header.end());

#ifdef XKSLANG_ADD_NAMES_AND_DEBUG_DATA_INTO_BYTECODE
    //======================================================================================================
    //copy all names and decorate matching our IDS
    {
        unsigned int start = header_size;
        const unsigned int end = spv.size();
        while (start < end)
        {
            unsigned int wordCount = asWordCount(start);
            spv::Op opCode = asOpCode(start);

            switch (opCode)
            {
                case spv::OpName:
                case spv::OpMemberName:
                case spv::OpDecorate:
                case spv::OpMemberDecorate:
                {
                    const spv::Id id = asId(start + 1);
                    if (listIdsUsed[id] == true)
                    {
                        auto its = spv.begin() + start;
                        stageBytecode.insert(stageBytecode.end(), its, its + wordCount);
                    }
                    break;
                }
                
                case spv::OpTypeInt:
                case spv::OpTypeVoid:
                case spv::OpTypePointer:
                case spv::OpFunction:
                {
                    //can stop parsing at this point
                    start = end;
                    break;
                }
            }

            start += wordCount;
        }
    }
#endif

    //======================================================================================================
    //Copy all types matching our IDS
    {
        unsigned int start = header_size;
        const unsigned int end = spv.size();
        while (start < end)
        {
            unsigned int wordCount = asWordCount(start);
            spv::Op opCode = asOpCode(start);

            spv::Id resultId = spvUndefinedId;
            if (isConstOp(opCode))
            {
                resultId = asId(start + 2);
            }
            else if (isTypeOp(opCode))
            {
                resultId = asId(start + 1);
            }
            else if (isVariableOp(opCode))
            {
                resultId = asId(start + 2);
            }

            if (resultId != spvUndefinedId)
            {
                if (listIdsUsed[resultId] == true)
                {
                    auto its = spv.begin() + start;
                    stageBytecode.insert(stageBytecode.end(), its, its + wordCount);
                }
            }

            //stop at first OpFunction instruction
            if (opCode == spv::OpFunction) start = end;
    
            start += wordCount;
        }
    }

    //======================================================================================================
    //copy all functions' bytecode
    for (auto itf = listAllFunctionsCalledByTheStage.begin(); itf != listAllFunctionsCalledByTheStage.end(); itf++)
    {
        FunctionInstruction* aFunctionCalled = *itf;
        stageBytecode.insert(stageBytecode.end(), spv.begin() + aFunctionCalled->GetBytecodeStartPosition(), spv.begin() + aFunctionCalled->GetBytecodeEndPosition());
    }

    //======================================================================================================
    //Remap all IDs
    {
        int newId = 1;
        vector<spv::Id> remapTable;
        remapTable.resize(bound(), spvUndefinedId);
        for (spv::Id id = 0; id < listIdsUsed.size(); ++id)
        {
            if (listIdsUsed[id] == true) remapTable[id] = newId++;
        }

        if (!remapAllIds(stageBytecode, header_size, stageBytecode.size(), remapTable))
            return error("Failed to remap all IDs");

        setBound(stageBytecode, newId);
    }

    return true;
}

bool SpxStreamRemapper::SpxStreamRemapper::InitDefaultHeader()
{
    if (spv.size() > 0) {
        return error("Bytecode must by empty");
    }

    int uniqueId = 1;
    spv.push_back(MagicNumber);
    spv.push_back(Version);
    spv.push_back(builderNumber);
    spv.push_back(0);
    spv.push_back(0);

    //default memory model
    spv::Instruction memInst(0, 0, spv::OpMemoryModel);
    memInst.addImmediateOperand(spv::AddressingModelLogical);
    memInst.addImmediateOperand(spv::MemoryModelGLSL450);
    memInst.dump(spv);

    //default imports
    spv::Instruction defaultImport(uniqueId++, spv::NoType, spv::OpExtInstImport);
    defaultImport.addStringOperand("GLSL.std.450");
    defaultImport.dump(spv);

    setBound(uniqueId);

    return true;
}

bool SpxStreamRemapper::BuildConstsHashmap(unordered_map<uint32_t, pairIdPos>& mapHashPos)
{
    mapHashPos.clear();

    //We build the hashmap table for all types and consts
    //except for OpTypeXlslShaderClass types: (this type is only informational, never used as a type or result)
    unsigned int start = header_size;
    const unsigned int end = spv.size();
    while (start < end)
    {
        unsigned int wordCount = asWordCount(start);
        spv::Op opCode = asOpCode(start);

        spv::Id id = spvUndefinedId;
        if (isConstOp(opCode))
        {
            id = asId(start + 2);
        }
        else if (opCode == spv::OpFunction)
        {
            break; //no more consts after this point
        }

        if (id != spvUndefinedId)
        {
            uint32_t hashval = hashType(start);

#ifdef XKSLANG_DEBUG_MODE
            if (hashval == spirvbin_t::unused) error("Failed to get the hashval for a const. constId: " + to_string(id));
#endif

            //we don't mind if we have several candidates with the same hashType (several identical consts)
            mapHashPos[hashval] = pairIdPos(id, start);
        }

        start += wordCount;
    }

    return true;
}

bool SpxStreamRemapper::BuildTypesAndConstsHashmap(unordered_map<uint32_t, pairIdPos>& mapHashPos)
{
    mapHashPos.clear();

    //We build the hashmap table for all types and consts
    //except for OpTypeXlslShaderClass types: (this type is only informational, never used as a type or result)
    unsigned int start = header_size;
    const unsigned int end = spv.size();
    while (start < end)
    {
        unsigned int wordCount = asWordCount(start);
        spv::Op opCode = asOpCode(start);

        spv::Id id = spvUndefinedId;
        if (opCode != spv::OpTypeXlslShaderClass)
        {
            if (isConstOp(opCode))
            {
                id = asId(start + 2);
            }
            else if (isTypeOp(opCode))
            {
                id = asId(start + 1);
            }
            else if (opCode == spv::OpFunction)
            {
                break; //no more consts after this point
            }
        }

        if (id != spvUndefinedId)
        {
            uint32_t hashval = hashType(start);

#ifdef XKSLANG_DEBUG_MODE
            if (hashval == spirvbin_t::unused) error("Failed to get the hashval for a const or type. Id: " + to_string(id));
            if (mapHashPos.find(hashval) != mapHashPos.end())
            {
                // Warning: might cause some conflicts sometimes?
                //return error(string("2 types have the same hashmap value. Ids: ") + to_string(mapHashPos[hashval].first) + string(", ") + to_string(id));
                id = spvUndefinedId;  //by precaution we invalidate the id: we should not choose between them
                //hashval = hashType(start);
            }
#endif
            mapHashPos[hashval] = pairIdPos(id, start);
        }

        start += wordCount;
    }

    return true;
}

//Update the start and end position for all objects
bool SpxStreamRemapper::UpdateAllObjectsPositionInTheBytecode()
{
    //Parse the list of all object data
    vector<ParsedObjectData> listParsedObjectsData;
    bool res = BuildDeclarationNameMapsAndObjectsDataList(listParsedObjectsData);
    if (!res) {
        return error("Failed to build maps");
    }

    uint32_t maxResultId = bound();
    unsigned int countParsedObjects = listParsedObjectsData.size();
    for (unsigned int i = 0; i < countParsedObjects; ++i)
    {
        ParsedObjectData& parsedData = listParsedObjectsData[i];
        spv::Id resultId = parsedData.resultId;

        if (resultId <= 0 || resultId == spv::NoResult || resultId >= maxResultId)
            return error(string("The parsed object has an invalid resultId: ") + to_string(resultId));
        if (listAllObjects[resultId] == nullptr)
            return error(string("The parsed object is not registerd in the list of all objects. ResultId: ") + to_string(resultId));

        listAllObjects[resultId]->SetBytecodeRangePositions(parsedData.bytecodeStartPosition, parsedData.bytecodeEndPosition);
    }

    return true;
}

bool SpxStreamRemapper::UpdateAllMaps()
{
    //Parse the list of all object data
    vector<ParsedObjectData> listParsedObjectsData;
    bool res = this->BuildDeclarationNameMapsAndObjectsDataList(listParsedObjectsData);
    if (!res) {
        return error("Failed to build maps");
    }

    //Resize our vector of ALL objects
    uint32_t maxResultId = bound();
    if (maxResultId < listAllObjects.size()){
        return error("We lost some objects since last update");
    }
    listAllObjects.resize(maxResultId, nullptr);

    //======================================================================================================
    //create and store all NEW objects in corresponding maps
    vector<bool> vectorIdsToDecorate;
    vectorIdsToDecorate.resize(maxResultId, false);

    unsigned int countParsedObjects = listParsedObjectsData.size();
    for (unsigned int i = 0; i < countParsedObjects; ++i)
    {
        ParsedObjectData& parsedData = listParsedObjectsData[i];
        spv::Id resultId = parsedData.resultId;

        if (resultId <= 0 || resultId == spv::NoResult || resultId >= maxResultId)
            return error(string("The parsed object has an invalid resultId: ") + to_string(resultId));

        if (listAllObjects[resultId] != nullptr)
        {
            //the object already exist, we just update its position in the bytecode
            listAllObjects[resultId]->SetBytecodeRangePositions(parsedData.bytecodeStartPosition, parsedData.bytecodeEndPosition);
        }
        else
        {
            ObjectInstructionBase* newObject = CreateAndAddNewObjectFor(parsedData);
            if (newObject == nullptr) {
                return error("Failed to create the PSX objects from parsed data");
            }
            vectorIdsToDecorate[resultId] = true;
        }
    }

    //decorate all objects
    res = DecorateObjects(vectorIdsToDecorate);
    if (!res) {
        return error("Failed to decorate objects");
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

bool SpxStreamRemapper::BuildAllMaps()
{
    //cout << "BuildAllMaps!!" << endl;

    ReleaseAllMaps();
    
    //Parse the list of all object data
    vector<ParsedObjectData> listParsedObjectsData;
    bool res = this->BuildDeclarationNameMapsAndObjectsDataList(listParsedObjectsData);
    if (!res) {
        return error("Failed to build maps");
    }

    //======================================================================================================
    //create and store all objects in corresponding maps
    int maxResultId = bound();
    listAllObjects.resize(maxResultId, nullptr);
    vector<bool> vectorIdsToDecorate;
    vectorIdsToDecorate.resize(maxResultId, false);

    unsigned int countParsedObjects = listParsedObjectsData.size();
    for (unsigned int i = 0; i < countParsedObjects; ++i)
    {
        ParsedObjectData& parsedData = listParsedObjectsData[i];
        
        ObjectInstructionBase* newObject = CreateAndAddNewObjectFor(parsedData);
        if (newObject == nullptr) {
            return error("Failed to create the PSX objects from parsed data");
        }

        vectorIdsToDecorate[newObject->GetResultId()] = true;
    }

    if (errorMessages.size() > 0) return false;

    //decorate all objects
    res = DecorateObjects(vectorIdsToDecorate);
    if (!res) {
        return error("Failed to decorate objects");
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

bool SpxStreamRemapper::DecorateObjects(vector<bool>& vectorIdsToDecorate)
{
    //======================================================================================================
    // Decorate objects attributes and relations

    //some decorate must be processed after we finish processing some others
    vector<int> vectorInstructionsToProcessAtTheEnd;

    unsigned int start = header_size;
    const unsigned int end = spv.size();
    while (start < end)
    {
        unsigned int wordCount = asWordCount(start);
        spv::Op opCode = asOpCode(start);

        switch (opCode)
        {
            /*case spv::OpMemberProperties:
            {
                //if the member has a stream property, define the type as a stream struct
                const spv::Id typeId = asId(start + 1);
                if (typeId < 0 || typeId >= vectorIdsToDecorate.size()) break;
                if (!vectorIdsToDecorate[typeId]) break;

                break;
            }*/

            case spv::Op::OpCBufferMemberProperties:
            {
                //we look for decorate with Block property (defining a cbuffer)
                const spv::Id typeId = asId(start + 1);
                if (typeId < 0 || typeId >= vectorIdsToDecorate.size()) break;
                if (!vectorIdsToDecorate[typeId]) break;
                vectorInstructionsToProcessAtTheEnd.push_back(start);
                break;
            }

            case spv::Op::OpBelongsToShader:
            {
                const spv::Id shaderId = asId(start + 1);
                const spv::Id objectId = asId(start + 2);

                if (shaderId < 0 || shaderId >= vectorIdsToDecorate.size()) break;
                if (!vectorIdsToDecorate[shaderId]) break;

                ShaderClassData* shaderOwner = GetShaderById(shaderId);
                if (shaderOwner == nullptr) { error(string("undeclared shader owner for Id: ") + to_string(shaderId)); break; }

                FunctionInstruction* function = GetFunctionById(objectId);
                if (function != nullptr) {
                    //a function is defined as being owned by a shader
#ifdef XKSLANG_DEBUG_MODE
                    if (function->GetShaderOwner() != nullptr) { error(string("The function already has a shader owner: ") + function->GetMangledName()); break; }
                    if (shaderOwner->HasFunction(function))
                    {
                        error(string("The shader: ") + shaderOwner->GetName() + string(" already possesses the function: ") + function->GetMangledName()); break;
                    }
                    function->SetFullName(shaderOwner->GetName() + string(".") + function->GetMangledName());
#endif
                    function->SetShaderOwner(shaderOwner);
                    shaderOwner->AddFunction(function);
                }
                else
                {
                    //a type is defined as being owned by a shader
                    TypeInstruction* type = GetTypeById(objectId);
                    if (type != nullptr)
                    {
                        //A type belongs to a shader, we fetch the necessary data
                        //A shader type is defined by: the type, a pointer type to it, and a variable to access it
                        TypeInstruction* typePointer = GetTypePointingTo(type);
                        VariableInstruction* variable = GetVariablePointingTo(typePointer);

                        if (typePointer == nullptr) { error(string("cannot find the pointer type to the shader type: ") + type->GetName()); break; }
                        if (variable == nullptr) { error(string("cannot find the variable for shader type: ") + type->GetName()); break; }
#ifdef XKSLANG_DEBUG_MODE
                        if (type->GetShaderOwner() != nullptr) { error(string("The type already has a shader owner: ") + type->GetName()); break; }
                        if (typePointer->GetShaderOwner() != nullptr) { error(string("The typePointer already has a shader owner: ") + typePointer->GetName()); break; }
                        if (variable->GetShaderOwner() != nullptr) { error(string("The variable already has a shader owner: ") + variable->GetName()); break; }
                        if (shaderOwner->HasType(type)) { error(string("The shader: ") + shaderOwner->GetName() + string(" already possesses the type: ") + type->GetName()); break; }
#endif
                        ShaderTypeData* shaderType = new ShaderTypeData(shaderOwner, type, typePointer, variable);
                        type->SetShaderOwner(shaderOwner);
                        typePointer->SetShaderOwner(shaderOwner);
                        variable->SetShaderOwner(shaderOwner);
                        shaderOwner->AddShaderType(shaderType);
                    }
                    else
                    {
                        error(string("unprocessed OpBelongsToShader instruction, invalid objectId: ") + to_string(objectId));
                    }
                }
                break;
            }

            case spv::OpShaderInheritance:
            {
                //A shader inherits from some shader parents
                const spv::Id shaderId = asId(start + 1);

                if (shaderId < 0 || shaderId >= vectorIdsToDecorate.size()) break;
                if (!vectorIdsToDecorate[shaderId]) break;

                ShaderClassData* shader = GetShaderById(shaderId);
                if (shader == nullptr) { error(string("undeclared shader for Id: ") + to_string(shaderId)); break; }

                int countParents = wordCount - 2;
                for (int p = 0; p < countParents; ++p)
                {
                    const spv::Id parentShaderId = asId(start + 2 + p);
                    ShaderClassData* shaderParent = GetShaderById(parentShaderId);
                    if (shaderParent == nullptr) { error(string("undeclared parent shader for Id: ") + to_string(parentShaderId)); break; }

//#ifdef XKSLANG_DEBUG_MODE
                    //A shader can inherit several time of the same shader, we just add it once
                    //if (shader->HasParent(shaderParent)) { error(string("shader: ") + shader->GetName() + string(" already inherits from parent: ") + shaderParent->GetName()); break; }
//#endif
                    if (!shader->HasParent(shaderParent))
                    {
                        shader->AddParent(shaderParent);
                    }
                }
                break;
            }

            case spv::OpShaderCompositionDeclaration:
            {
                const spv::Id shaderId = asId(start + 1);

                if (shaderId < 0 || shaderId >= vectorIdsToDecorate.size()) break;
                if (!vectorIdsToDecorate[shaderId]) break;

                ShaderClassData* shaderCompositionOwner = GetShaderById(shaderId);
                if (shaderCompositionOwner == nullptr) { error(string("undeclared shader id: ") + to_string(shaderId)); break; }

                int compositionId = asLiteralValue(start + 2);

                const spv::Id shaderCompositionTypeId = asId(start + 3);
                ShaderClassData* shaderCompositionType = GetShaderById(shaderCompositionTypeId);
                if (shaderCompositionType == nullptr) { error(string("undeclared shader type id: ") + to_string(shaderCompositionTypeId)); break; }

                bool isArray = asLiteralValue(start + 4) == 0? false: true;

                int count = asLiteralValue(start + 5);

                const string compositionVariableName = literalString(start + 6);

                ShaderComposition shaderComposition(compositionId, shaderCompositionOwner, shaderCompositionType, compositionVariableName, isArray, count);
                shaderCompositionOwner->AddComposition(shaderComposition);
                break;
            }

            case spv::OpGSMethodProperties:
            {
                return error(string("OpGSMethodProperties: unprocessed yet"));
            }

            case spv::Op::OpMethodProperties:
            {
                //a function is defined with some properties
                const spv::Id functionId = asId(start + 1);

                if (functionId < 0 || functionId >= vectorIdsToDecorate.size()) break;
                if (!vectorIdsToDecorate[functionId]) break;

                FunctionInstruction* function = GetFunctionById(functionId);
                if (function == nullptr) { error(string("undeclared function id: ") + to_string(functionId)); break; }

                int countProperties = wordCount - 2;
                for (int a = 0; a < countProperties; ++a)
                {
                    int prop = asLiteralValue(start + 2 + a);
                    switch (prop)
                    {
                    case spv::XkslPropertyEnum::PropertyMethodOverride:
                        function->ParsedOverrideAttribute();
                        break;
                    case spv::XkslPropertyEnum::PropertyMethodStatic:
                        function->ParsedStaticAttribute();
                        break;
                    }
                }
                break;
            }
        }

        start += wordCount;
    }

    for (unsigned int i = 0; i < vectorInstructionsToProcessAtTheEnd.size(); i++)
    {
        unsigned int start = vectorInstructionsToProcessAtTheEnd[i];
        spv::Op opCode = asOpCode(start);

        switch (opCode)
        {
            case spv::OpCBufferMemberProperties:
            {
                const spv::Id typeId = asId(start + 1);
                spv::XkslPropertyEnum cbufferType = (spv::XkslPropertyEnum)asLiteralValue(start + 2);
                spv::XkslPropertyEnum cbufferStage = (spv::XkslPropertyEnum)asLiteralValue(start + 3);

#ifdef XKSLANG_DEBUG_MODE
                if (cbufferType != spv::CBufferUndefined && cbufferType != spv::CBufferDefined) { error("Invalid cbuffer type property"); break; }
                if (cbufferStage != spv::CBufferStage && cbufferStage != spv::CBufferUnstage) { error("Invalid cbuffer stage property"); break; }
#endif

                int countMembers = asLiteralValue(start + 4);
                TypeInstruction* type = GetTypeById(typeId);
                if (type == nullptr) return error("Cannot find the type for Id: " + to_string(typeId));
                if (countMembers <= 0) { error("Invalid number of members for the cbuffer: " + type->GetName()); break; }
                
                string shaderOwnerName = "";
                if (type->shaderOwner != nullptr) shaderOwnerName = type->shaderOwner->GetName();

                CBufferTypeData* cbufferData = new CBufferTypeData(shaderOwnerName, type->GetName(), cbufferType, cbufferStage == spv::CBufferStage ? true : false, countMembers);
                type->SetCBufferData(cbufferData);
                break;
            }
        }
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

SpxStreamRemapper::ObjectInstructionBase* SpxStreamRemapper::CreateAndAddNewObjectFor(ParsedObjectData& parsedData)
{
    spv::Id resultId = parsedData.resultId;
    if (resultId <= 0 || resultId == spv::NoResult || resultId >= listAllObjects.size()) {
        error(string("The object has an invalid resultId: ") + to_string(resultId));
        return nullptr;
    }
    if (listAllObjects[resultId] != nullptr) {
        error(string("An object with the same resultId already exists. resultId: ") + to_string(resultId));
        return nullptr;
    }

    string declarationName;
    bool hasDeclarationName = GetDeclarationNameForId(parsedData.resultId, declarationName);

    bool declarationNameRequired = true;
    ObjectInstructionBase* newObject = nullptr;
    switch (parsedData.kind)
    {
        case ObjectInstructionTypeEnum::HeaderProperty:
        {
            declarationNameRequired = false;
            string headerPropName = "";
            switch (parsedData.opCode)
            {
            case spv::Op::OpExtInstImport:
                headerPropName = literalString(parsedData.bytecodeStartPosition + 2);
                break;
            default:
                error(string("HeaderProperty has an unknown OpCode: ") + to_string(parsedData.opCode));
                return nullptr;
            }
            newObject = new HeaderPropertyInstruction(parsedData, headerPropName, this);
            break;
        }
        case ObjectInstructionTypeEnum::Const:
        {
            declarationNameRequired = false;
            
            bool isS32 = false; int valueS32 = 0;
            //if the const if a signed 32 bits, prefetch its numeric value
            {
                spv::Id constTypeId = asId(parsedData.bytecodeStartPosition + 1);
                TypeInstruction* constType = GetTypeById(constTypeId);
                if (constType == nullptr) {
                    error(string("no type exist for const typeId: ") + to_string(constTypeId));
                    return nullptr;
                }
                spv::Op typeOpCode = asOpCode(constType->bytecodeStartPosition);
                if (typeOpCode == spv::OpTypeInt)
                {
                    int literalNumber = asLiteralValue(constType->bytecodeStartPosition + 2);
                    int sign = asLiteralValue(constType->bytecodeStartPosition + 3);
                    if (literalNumber == 32 && sign == 1) {
                        isS32 = true;
                        valueS32 = asLiteralValue(parsedData.bytecodeStartPosition + 3);
                    }
                }
            }

            newObject = new ConstInstruction(parsedData, declarationName, this, isS32, valueS32);
            break;
        }
        case ObjectInstructionTypeEnum::Shader:
        {
            declarationNameRequired = true;
            ShaderClassData* shader = new ShaderClassData(parsedData, declarationName, this);
            vecAllShaders.push_back(shader);
            newObject = shader;
            break;
        }
        case ObjectInstructionTypeEnum::Type:
        {
            declarationNameRequired = false;
            TypeInstruction* type = new TypeInstruction(parsedData, declarationName, this);
            if (isPointerTypeOp(parsedData.opCode))
            {
                //create the link to the type pointed by the pointer (already created at this stage)
                TypeInstruction* pointedType = GetTypeById(parsedData.targetId);
                if (pointedType == nullptr) {
                    error(string("Cannot find the typeId: ") + to_string(parsedData.targetId) + string(", pointed by pointer Id: ") + to_string(resultId));
                    delete type;
                    return nullptr;
                }
                type->SetTypePointed(pointedType);
            }
            newObject = type;

            break;
        }
        case ObjectInstructionTypeEnum::Variable:
        {
            declarationNameRequired = false;
            
            //create the link to the type pointed by the variable (already created at this stage)
            TypeInstruction* pointedType = GetTypeById(parsedData.typeId);
            if (pointedType == nullptr) {
                error(string("Cannot find the typeId: ") + to_string(parsedData.typeId) + string(", pointed by variable Id: ") + to_string(resultId));
                return nullptr;
            }

            VariableInstruction* variable = new VariableInstruction(parsedData, declarationName, this);
            variable->SetTypePointed(pointedType);
            newObject = variable;
            break;
        }
        case ObjectInstructionTypeEnum::Function:
        {
            declarationNameRequired = false;  //some functions can be declared outside a shader definition, they don't belong to a shader then
            if (!hasDeclarationName || declarationName.size() == 0)
                declarationName = "__globalFunction__" + to_string(vecAllFunctions.size());

            FunctionInstruction* function = new FunctionInstruction(parsedData, declarationName, this);
            vecAllFunctions.push_back(function);

            newObject = function;
            break;
        }
    }

    listAllObjects[resultId] = newObject;

    if (newObject == nullptr) {
        error("Unknown parsed data kind");
        return nullptr;
    }

    if (declarationNameRequired && !hasDeclarationName) {
        error("Object requires a declaration name");
        return nullptr;
    }

    return newObject;
}

bool SpxStreamRemapper::ComputeShadersLevel()
{
    //===================================================================================================================
    // Set the shader levels (also detects cyclic shader inheritance)
    for (auto itsh = vecAllShaders.begin(); itsh != vecAllShaders.end(); itsh++)
        (*itsh)->level = -1;

    bool allShaderSet = false;
    while (!allShaderSet)
    {
        allShaderSet = true;
        bool anyShaderUpdated = false;

        for (auto itsh = vecAllShaders.begin(); itsh != vecAllShaders.end(); itsh++)
        {
            ShaderClassData* shader = *itsh;
            if (shader->level != -1) continue;  //shader already set

            if (shader->parentsList.size() == 0)
            {
                shader->level = 0; //shader has no parent
                anyShaderUpdated = true;
                continue;
            }

            int maxParentLevel = -1;
            for (unsigned int p = 0; p<shader->parentsList.size(); ++p)
            {
                int parentLevel = shader->parentsList[p]->level;
                if (parentLevel == -1)
                {
                    //parent not set yet: got to wait
                    allShaderSet = false;
                    maxParentLevel = -1;
                    break;
                }
                if (parentLevel > maxParentLevel) maxParentLevel = parentLevel;
            }

            if (maxParentLevel >= 0)
            {
                shader->level = maxParentLevel + 1; //shader level
                anyShaderUpdated = true;
            }
        }

        if (!anyShaderUpdated)
        {
            return error("Cyclic inheritance detected among shaders");
        }
    }

    return true;
}

bool SpxStreamRemapper::BuildDeclarationNameMapsAndObjectsDataList(vector<ParsedObjectData>& listParsedObjectsData)
{
    mapDeclarationName.clear();
    idPosR.clear();
    listParsedObjectsData.clear();

    int fnStart = 0;
    spv::Id fnResId = spv::NoResult;
    spv::Id fnTypeId = spv::NoResult;
    spv::Op fnOpCode;

    unsigned int start = header_size;
    const unsigned int end = spv.size();
    while (start < end)
    {
        unsigned int wordCount = asWordCount(start);
        spv::Op opCode = asOpCode(start);

        unsigned end = start + wordCount;

        //Fill idPosR map (used by hashType)
        unsigned word = start+1;
        spv::Id typeId = spv::NoType;
        spv::Id resultId = spv::NoResult;
        if (spv::InstructionDesc[opCode].hasType())
            typeId = asId(word++);
        if (spv::InstructionDesc[opCode].hasResult()) {
            resultId = asId(word++);
            idPosR[resultId] = start;
        }

        if (opCode == spv::Op::OpName)
        {
        /*
#ifdef XKSLANG_DEBUG_MODE
            const spv::Id target = asId(start + 1);
            const string name = literalString(start + 2);

            if (mapDeclarationName.find(target) == mapDeclarationName.end())
                mapDeclarationName[target] = name;
#endif*/
        }
        else if (opCode == spv::Op::OpDeclarationName)
        {
            const spv::Id target = asId(start + 1);
            const string  name = literalString(start + 2);
            mapDeclarationName[target] = name;
        }
        else if (isConstOp(opCode))
        {
            listParsedObjectsData.push_back(ParsedObjectData(ObjectInstructionTypeEnum::Const, opCode, resultId, typeId, start, end));
        }
        else if (isTypeOp(opCode))
        {
            if (opCode == spv::OpTypeXlslShaderClass)
            {
                listParsedObjectsData.push_back(ParsedObjectData(ObjectInstructionTypeEnum::Shader, opCode, resultId, typeId, start, end));
            }
            else
            {
                ParsedObjectData data = ParsedObjectData(ObjectInstructionTypeEnum::Type, opCode, resultId, typeId, start, end);
                if (isPointerTypeOp(opCode))
                {
                    spv::Id targetId = asId(start + 3);
                    data.SetTargetId(targetId);
                }
                listParsedObjectsData.push_back(data);
            }
        }
        else if (isVariableOp(opCode))
        {
            listParsedObjectsData.push_back(ParsedObjectData(ObjectInstructionTypeEnum::Variable, opCode, resultId, typeId, start, end));
        }
        else if (opCode == spv::Op::OpFunction)
        {
            if (fnStart != 0) error("nested function found");
            fnStart = start;
            fnTypeId = typeId;
            fnResId = resultId;
            fnOpCode = opCode;
        }
        else if (opCode == spv::Op::OpFunctionEnd)
        {
            if (fnStart == 0) error("function end without function start");
            if (fnResId == spv::NoResult) error("function has no result iD");
            listParsedObjectsData.push_back(ParsedObjectData(ObjectInstructionTypeEnum::Function, fnOpCode, fnResId, fnTypeId, fnStart, end));
            fnStart = 0;
        }
        else if (opCode == spv::Op::OpExtInstImport)
        {
            listParsedObjectsData.push_back(ParsedObjectData(ObjectInstructionTypeEnum::HeaderProperty, opCode, resultId, typeId, start, end));
        }

        start += wordCount;
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

//===========================================================================================================================//
//===========================================================================================================================//
string SpxStreamRemapper::GetDeclarationNameForId(spv::Id id)
{
    auto it = mapDeclarationName.find(id);
    if (it == mapDeclarationName.end())
    {
        error(string("Id: ") + to_string(id) + string(" has no declaration name"));
        return string("");
    }
    return it->second;
}

bool SpxStreamRemapper::GetDeclarationNameForId(spv::Id id, string& name)
{
    auto it = mapDeclarationName.find(id);
    if (it == mapDeclarationName.end())
        return false;

    name = it->second;
    return true;
}

SpxStreamRemapper::HeaderPropertyInstruction* SpxStreamRemapper::GetHeaderPropertyInstructionByOpCodeAndName(const spv::Op opCode, const string& name)
{
    if (name.size() == 0) return nullptr;

    for (auto it = listAllObjects.begin(); it != listAllObjects.end(); ++it)
    {
        ObjectInstructionBase* obj = *it;
        if (obj != nullptr && obj->GetKind() == ObjectInstructionTypeEnum::HeaderProperty && obj->GetOpCode() == opCode && obj->GetName() == name)
        {
            HeaderPropertyInstruction* headerProp = dynamic_cast<HeaderPropertyInstruction*>(obj);
            return headerProp;
        }
    }
    return nullptr;
}

SpxStreamRemapper::FunctionInstruction* SpxStreamRemapper::GetTargetedFunctionByNameWithinShaderAndItsFamily(ShaderClassData* shader, const string& name)
{
    FunctionInstruction* function = shader->GetFunctionByName(name);
    if (function != nullptr) {
        if (function->GetOverridingFunction() != nullptr) return function->GetOverridingFunction();
        return function;
    }

    //Look for the function within the shader parents
    for (unsigned int i = 0; i < shader->parentsList.size(); ++i)
    {
        function = GetTargetedFunctionByNameWithinShaderAndItsFamily(shader->parentsList[i], name);
        if (function != nullptr) return function;
    }
    return nullptr;
}

SpxStreamRemapper::ShaderClassData* SpxStreamRemapper::GetShaderByName(const string& name)
{
    if (name.size() == 0) return nullptr;

    for (auto it = vecAllShaders.begin(); it != vecAllShaders.end(); ++it)
    {
        ShaderClassData* shader = *it;
        if (shader->GetName() == name)
        {
            return shader;
        }
    }
    return nullptr;
}

SpxStreamRemapper::ShaderComposition* SpxStreamRemapper::GetCompositionById(spv::Id shaderId, int compositionId)
{
    ShaderClassData* shader = GetShaderById(shaderId);
    if (shader == nullptr) return nullptr;
    return shader->GetShaderCompositionById(compositionId);
}

bool SpxStreamRemapper::GetAllCompositionForEachLoops(vector<CompositionForEachLoopData>& vecForEachLoops, int& maxForEachLoopsNestedLevel)
{
    vecForEachLoops.clear();
    maxForEachLoopsNestedLevel = -1;

    int currentForEachLoopNestedLevel = -1;
    vector<CompositionForEachLoopData> pileForeachLoopsCurrentlyParsed;
    unsigned int start = header_size;
    const unsigned int end = spv.size();
    while (start < end)
    {
        unsigned int wordCount = asWordCount(start);
        spv::Op opCode = asOpCode(start);

        switch (opCode)
        {
            case spv::OpForEachCompositionStartLoop:
            {
                spv::Id shaderId = asId(start + 1);
                int compositionId = asLiteralValue(start + 2);

                ShaderComposition* composition = GetCompositionById(shaderId, compositionId);
                if (composition == nullptr) {
                    error(string("Composition not found for ShaderId: ") + to_string(shaderId) + string(" with composition id: ") + to_string(compositionId));
                    break;
                }
                if (!composition->isArray) { error("Foreach loop only works with array compositions."); break; }

                currentForEachLoopNestedLevel++;
                if (currentForEachLoopNestedLevel > maxForEachLoopsNestedLevel) maxForEachLoopsNestedLevel = currentForEachLoopNestedLevel;
                pileForeachLoopsCurrentlyParsed.push_back(CompositionForEachLoopData(composition, currentForEachLoopNestedLevel, start, 0, start + wordCount, 0));

                break;
            }

            case spv::OpForEachCompositionEndLoop:
            {
                if (currentForEachLoopNestedLevel < 0) { error("A foreach end loop instruction is missing a start instruction"); break; }
                currentForEachLoopNestedLevel--;

                CompositionForEachLoopData compositionForEachLoop = pileForeachLoopsCurrentlyParsed.back();
                pileForeachLoopsCurrentlyParsed.pop_back();

                compositionForEachLoop.lastLoopInstuctionEnd = start;
                compositionForEachLoop.foreachLoopEnd = start + wordCount;
                vecForEachLoops.push_back(compositionForEachLoop);

                break;
            }
        }

        start += wordCount;
    }

    if (currentForEachLoopNestedLevel >= 0) error("A foreach start loop instruction is missing an end instruction");
    if (errorMessages.size() > 0) return false;
    return true;
}

bool SpxStreamRemapper::GetListAllFunctionCallInstructions(vector<FunctionCallInstructionData>& listFunctionCallInstructions)
{
    listFunctionCallInstructions.clear();
    for (auto itf = vecAllFunctions.begin(); itf != vecAllFunctions.end(); itf++)
    {
        FunctionInstruction* functionCalling = *itf;
        functionCalling->bytecodeStartPosition;

        unsigned int start = functionCalling->GetBytecodeStartPosition();
        const unsigned int end = functionCalling->GetBytecodeEndPosition();
        while (start < end)
        {
            unsigned int wordCount = asWordCount(start);
            spv::Op opCode = asOpCode(start);

            switch (opCode)
            {
                case spv::OpFunctionCall:
                case spv::OpFunctionCallBaseResolved:
                case spv::OpFunctionCallBaseUnresolved:
                case spv::OpFunctionCallThroughStaticShaderClassCall:
                case spv::OpFunctionCallThroughCompositionVariable:
                {
                    spv::Id functionCalledId = asId(start + 3);
                    FunctionInstruction* functionCalled = GetFunctionById(functionCalledId);
                    if (functionCalled == nullptr) return error(string("Failed to retrieve the function for Id: ") + to_string(functionCalledId));

                    listFunctionCallInstructions.push_back(FunctionCallInstructionData(functionCalling, functionCalled, opCode, start));
                    
                    break;
                }
            }

            start += wordCount;
        }
    }

    return true;
}

bool SpxStreamRemapper::GetAllShaderInstancesForComposition(const ShaderComposition* composition, vector<ShaderClassData*>& instances)
{
    spv::Id compositionShaderOwnerId = composition->compositionShaderOwner->GetId();
    int compositionShaderId = composition->compositionShaderId;

    int countInstancesExpected = composition->countInstances;
    instances.resize(countInstancesExpected, nullptr);
    int countInstancesFound = 0;

    //look for the instances in the bytecode
    unsigned int start = header_size;
    const unsigned int end = spv.size();
    while (start < end)
    {
        unsigned int wordCount = asWordCount(start);
        spv::Op opCode = asOpCode(start);

        switch (opCode)
        {
            case spv::OpShaderCompositionInstance:
            {
                const spv::Id shaderOwnerId = asId(start + 1);
                const int compositionId = asLiteralValue(start + 2);

                if (shaderOwnerId == compositionShaderOwnerId && compositionId == compositionShaderId)
                {
                    int instanceNum = asLiteralValue(start + 3);
                    if (instanceNum < 0 || instanceNum >= countInstancesExpected)
                        return error(string("composition instance has an invalid num: ") + to_string(instanceNum));
                    if (instances[instanceNum] != nullptr)
                        return error(string("Found 2 instances for the same composition having the same num: ") + to_string(instanceNum));

                    const spv::Id shaderInstanceId = asId(start + 4);
                    ShaderClassData* shaderInstance = GetShaderById(shaderInstanceId);
                    if (shaderInstance == nullptr)
                        return error(string("unfound composition shader instance for id: ") + to_string(shaderInstanceId));
                    instances[instanceNum] = shaderInstance;
                    countInstancesFound++;
                }

                break;
            }

            case spv::OpTypeXlslShaderClass:
            {
                //all declaration must be set before type declaration: we can stop parsing the rest of the bytecode
                start = end;
                break;
            }
        }
        start += wordCount;
    }

    if (countInstancesExpected != countInstancesFound) {
        return error(string("Invalid number of instances found. Expected number: ") + to_string(countInstancesExpected) + string(". Instances found: ") + to_string(countInstancesFound));
    }

    return true;
}

SpxStreamRemapper::ShaderClassData* SpxStreamRemapper::GetShaderById(spv::Id id)
{
    if (id >= listAllObjects.size()) return nullptr;
    ObjectInstructionBase* obj = listAllObjects[id];

    if (obj != nullptr && obj->GetKind() == ObjectInstructionTypeEnum::Shader)
    {
        ShaderClassData* shader = dynamic_cast<ShaderClassData*>(obj);
        return shader;
    }
    return nullptr;
}

SpxStreamRemapper::VariableInstruction* SpxStreamRemapper::GetVariableByName(const string& name)
{
    if (name.size() == 0) return nullptr;

    for (auto it = listAllObjects.begin(); it != listAllObjects.end(); ++it)
    {
        ObjectInstructionBase* obj = *it;
        if (obj != nullptr && obj->GetKind() == ObjectInstructionTypeEnum::Variable && obj->GetName() == name)
        {
            VariableInstruction* variable = dynamic_cast<VariableInstruction*>(obj);
            return variable;
        }
    }
    return nullptr;
}

SpxStreamRemapper::ObjectInstructionBase* SpxStreamRemapper::GetObjectById(spv::Id id)
{
    if (id >= listAllObjects.size()) return nullptr;
    ObjectInstructionBase* obj = listAllObjects[id];
    return obj;
}

SpxStreamRemapper::FunctionInstruction* SpxStreamRemapper::GetFunctionById(spv::Id id)
{
    if (id >= listAllObjects.size()) return nullptr;
    ObjectInstructionBase* obj = listAllObjects[id];

    if (obj != nullptr && obj->GetKind() == ObjectInstructionTypeEnum::Function)
    {
        FunctionInstruction* function = dynamic_cast<FunctionInstruction*>(obj);
        return function;
    }
    return nullptr;
}

SpxStreamRemapper::ConstInstruction* SpxStreamRemapper::GetConstById(spv::Id id)
{
    if (id >= listAllObjects.size()) return nullptr;
    ObjectInstructionBase* obj = listAllObjects[id];

    if (obj != nullptr && obj->GetKind() == ObjectInstructionTypeEnum::Const)
    {
        ConstInstruction* constInstr = dynamic_cast<ConstInstruction*>(obj);
        return constInstr;
    }
    return nullptr;
}

SpxStreamRemapper::TypeInstruction* SpxStreamRemapper::GetTypeById(spv::Id id)
{
    if (id >= listAllObjects.size()) return nullptr;
    ObjectInstructionBase* obj = listAllObjects[id];

    if (obj != nullptr && obj->GetKind() == ObjectInstructionTypeEnum::Type)
    {
        TypeInstruction* type = dynamic_cast<TypeInstruction*>(obj);
        return type;
    }
    return nullptr;
}

SpxStreamRemapper::VariableInstruction* SpxStreamRemapper::GetVariableById(spv::Id id)
{
    if (id >= listAllObjects.size()) return nullptr;
    ObjectInstructionBase* obj = listAllObjects[id];

    if (obj != nullptr && obj->GetKind() == ObjectInstructionTypeEnum::Variable)
    {
        VariableInstruction* variable = dynamic_cast<VariableInstruction*>(obj);
        return variable;
    }
    return nullptr;
}

SpxStreamRemapper::TypeInstruction* SpxStreamRemapper::GetTypePointingTo(TypeInstruction* targetType)
{
    if (targetType == nullptr) return nullptr;

    TypeInstruction* res = nullptr;
    for (auto it = listAllObjects.begin(); it != listAllObjects.end(); ++it)
    {
        ObjectInstructionBase* obj = *it;
        if (obj != nullptr && obj->GetKind() == ObjectInstructionTypeEnum::Type)
        {
            TypeInstruction* aType = dynamic_cast<TypeInstruction*>(obj);
            if (aType->GetTypePointed() == targetType)
            {
                if (res != nullptr) error("found 2 types pointing to the same type");
                res = aType;
            }
        }
    }
    return res;
}

SpxStreamRemapper::VariableInstruction* SpxStreamRemapper::GetVariablePointingTo(TypeInstruction* targetType)
{
    if (targetType == nullptr) return nullptr;

    VariableInstruction* res = nullptr;
    for (auto it = listAllObjects.begin(); it != listAllObjects.end(); ++it)
    {
        ObjectInstructionBase* obj = *it;
        if (obj != nullptr && obj->GetKind() == ObjectInstructionTypeEnum::Variable)
        {
            VariableInstruction* variable = dynamic_cast<VariableInstruction*>(obj);
            if (variable->GetTypePointed() == targetType)
            {
                if (res != nullptr) error("found 2 variables pointing to the same type");
                res = variable;
            }
        }
    }
    return res;
}

void SpxStreamRemapper::stripBytecode(vector<range_t>& ranges)
{
    if (ranges.empty()) // nothing to do
        return;

    // Sort strip ranges in order of traversal
    sort(ranges.begin(), ranges.end());

    // Allocate a new binary big enough to hold old binary
    // We'll step this iterator through the strip ranges as we go through the binary
    auto strip_it = ranges.begin();

    int strippedPos = 0;
    for (unsigned word = 0; word < unsigned(spv.size()); ++word) {
        if (strip_it != ranges.end() && word >= strip_it->second)
            ++strip_it;

        if (strip_it == ranges.end() || word < strip_it->first || word >= strip_it->second)
            spv[strippedPos++] = spv[word];
    }

    spv.resize(strippedPos);
}

bool SpxStreamRemapper::remapAllIds(vector<uint32_t>& bytecode, unsigned int begin, unsigned int end, const vector<spv::Id>& remapTable)
{
    string errorMsg;
    if (!remapAllIds(bytecode, begin, end, remapTable, errorMsg))
    {
        error(errorMsg);
        return false;
    }
    return true;
}

bool SpxStreamRemapper::remapAllIds(vector<uint32_t>& bytecode, unsigned int begin, unsigned int end, const vector<spv::Id>& remapTable, string& errorMsg)
{
    unsigned int pos = begin;
    unsigned int wordCount = 0;
    while (pos < end)
    {
        if (!remapAllInstructionIds(bytecode, pos, wordCount, remapTable, errorMsg)) return false;
        pos += wordCount;
    }

    return true;
}

// inspired by spirvbin_t::processInstruction. Allow us to remap the instruction Ids
bool SpxStreamRemapper::remapAllInstructionIds(vector<uint32_t>& bytecode, unsigned int word, unsigned int& wordCount, const vector<spv::Id>& remapTable, string& errorMsg)
{
    const unsigned int instructionStart = word;
    wordCount = opWordCount(bytecode[instructionStart]);
    spv::Op opCode = opOpCode(bytecode[instructionStart]);
    const unsigned int nextInst = word++ + wordCount;

#ifdef XKSLANG_DEBUG_MODE
    if (nextInst > bytecode.size()) { errorMsg = "nextInst is out of bound"; return false; }
    if (wordCount == 0) { errorMsg = "wordcount equals 0"; return false; }
#endif

    // Base for computing number of operands; will be updated as more is learned
    unsigned int numOperands = wordCount - 1;

    // Read type and result ID from instruction desc table
    if (spv::InstructionDesc[opCode].hasType())
    {
#ifdef XKSLANG_DEBUG_MODE
        if (remapTable[bytecode[word]] == 0 || remapTable[bytecode[word]] == spvUndefinedId) {
            errorMsg = "Fail to remap the instructions IDs, no new value remapped for Id: " + to_string(bytecode[word]);
            return false;
        }
#endif
        bytecode[word] = remapTable[bytecode[word]];
        word++;
        --numOperands;
    }

    if (spv::InstructionDesc[opCode].hasResult())
    {
#ifdef XKSLANG_DEBUG_MODE
        if (remapTable[bytecode[word]] == 0 || remapTable[bytecode[word]] == spvUndefinedId) {
            errorMsg = "Fail to remap the instructions IDs, no new value remapped for Id: " + to_string(bytecode[word]);
            return false;
        }
#endif
        bytecode[word] = remapTable[bytecode[word]];
        word++;
        --numOperands;
    }

    // Extended instructions: currently, assume everything is an ID.
    // TODO: add whatever data we need for exceptions to that
    if (opCode == spv::OpExtInst)
    {
#ifdef XKSLANG_DEBUG_MODE
        if (remapTable[bytecode[word]] == 0 || remapTable[bytecode[word]] == spvUndefinedId) {
            errorMsg = "Fail to remap the instructions IDs, no new value remapped for Id: " + to_string(bytecode[word]);
            return false;
        }
#endif
        bytecode[word] = remapTable[bytecode[word]];

        word += 2; // instruction set, and instruction from set
        numOperands -= 2;

        for (unsigned op = 0; op < numOperands; ++op)
        {
#ifdef XKSLANG_DEBUG_MODE
            if (remapTable[bytecode[word]] == 0 || remapTable[bytecode[word]] == spvUndefinedId) {
                errorMsg = "Fail to remap the instructions IDs, no new value remapped for Id: " + to_string(bytecode[word]);
                return false;
            }
#endif
            bytecode[word] = remapTable[bytecode[word]];
            word++;
        }

        return true;
    }

    // Store IDs from instruction in our map
    for (int op = 0; numOperands > 0; ++op, --numOperands) {
        switch (spv::InstructionDesc[opCode].operands.getClass(op)) {
        case spv::OperandId:
        case spv::OperandScope:
        case spv::OperandMemorySemantics:
#ifdef XKSLANG_DEBUG_MODE
            if (remapTable[bytecode[word]] == 0 || remapTable[bytecode[word]] == spvUndefinedId){
                errorMsg = "Fail to remap the instructions IDs, no new value remapped for Id: " + to_string(bytecode[word]);
                return false;
            }
#endif
            bytecode[word] = remapTable[bytecode[word]];
            word++;
            break;

        case spv::OperandVariableIds:
            for (unsigned i = 0; i < numOperands; ++i)
            {
#ifdef XKSLANG_DEBUG_MODE
                if (remapTable[bytecode[word]] == 0 || remapTable[bytecode[word]] == spvUndefinedId) {
                    errorMsg = "Fail to remap the instructions IDs, no new value remapped for Id: " + to_string(bytecode[word]);
                    return false;
                }
#endif
                bytecode[word] = remapTable[bytecode[word]];
                word++;
            }
            return true;

        case spv::OperandVariableLiterals:
            // for clarity
            // if (opCode == spv::OpDecorate && asDecoration(word - 1) == spv::DecorationBuiltIn) {
            //     ++word;
            //     --numOperands;
            // }
            // word += numOperands;
            return true;

        case spv::OperandVariableLiteralId: {
            if (opCode == spv::OpSwitch)
            {
                /// // word-2 is the position of the selector ID.  OpSwitch Literals match its type.
                /// // In case the IDs are currently being remapped, we get the word[-2] ID from
                /// // the circular idBuffer.
                /// const unsigned literalSizePos = (idBufferPos + idBufferSize - 2) % idBufferSize;
                /// const unsigned literalSize = idTypeSizeInWords(idBuffer[literalSizePos]);
                /// const unsigned numLiteralIdPairs = (nextInst - word) / (1 + literalSize);
                /// 
                /// for (unsigned arg = 0; arg<numLiteralIdPairs; ++arg) {
                ///     word += literalSize;  // literal
                ///     spv[word] = remapTable[spv[word]];   // label
                ///     word++;
                /// }
                errorMsg = "Unplugged operand for now.. to check later";
                return false;
            }
            else {
                errorMsg = "invalid OperandVariableLiteralId instruction";
                return false;
            }

            return true;
        }

        case spv::OperandLiteralString: {
            const int stringWordCount = literalStringWords(literalString(bytecode, word));
            word += stringWordCount;
            numOperands -= (stringWordCount - 1); // -1 because for() header post-decrements
            break;
        }

        // Execution mode might have extra literal operands.  Skip them.
        case spv::OperandExecutionMode:
            return true;

        // Single word operands we simply ignore, as they hold no IDs
        case spv::OperandLiteralNumber:
        case spv::OperandSource:
        case spv::OperandExecutionModel:
        case spv::OperandAddressing:
        case spv::OperandMemory:
        case spv::OperandStorage:
        case spv::OperandDimensionality:
        case spv::OperandSamplerAddressingMode:
        case spv::OperandSamplerFilterMode:
        case spv::OperandSamplerImageFormat:
        case spv::OperandImageChannelOrder:
        case spv::OperandImageChannelDataType:
        case spv::OperandImageOperands:
        case spv::OperandFPFastMath:
        case spv::OperandFPRoundingMode:
        case spv::OperandLinkageType:
        case spv::OperandAccessQualifier:
        case spv::OperandFuncParamAttr:
        case spv::OperandDecoration:
        case spv::OperandBuiltIn:
        case spv::OperandSelect:
        case spv::OperandLoop:
        case spv::OperandFunction:
        case spv::OperandMemoryAccess:
        case spv::OperandGroupOperation:
        case spv::OperandKernelEnqueueFlags:
        case spv::OperandKernelProfilingInfo:
        case spv::OperandCapability:
        case spv::XkslShaderDataProperty:
            ++word;
            break;

        case spv::XkslShaderDataProperties:
            return true;

        default:
            errorMsg = "Unhandled Operand Class";
            return false;
        }
    }

    return true;
}

bool SpxStreamRemapper::flagAllIdsFromInstruction(unsigned int word, spv::Op& opCode, unsigned int& wordCount, std::vector<bool>& listIdsUsed)
{
    wordCount = opWordCount(spv[word]);
    opCode = opOpCode(spv[word]);
    const unsigned int nextInst = word + wordCount;
    word++;

#ifdef XKSLANG_DEBUG_MODE
    if (nextInst > spv.size()) return error("nextInst is out of bound");
    if (wordCount == 0) return error("wordcount equals 0");
#endif

    // Base for computing number of operands; will be updated as more is learned
    unsigned int numOperands = wordCount - 1;

    // Read type and result ID from instruction desc table
    if (spv::InstructionDesc[opCode].hasType()) {
        //listIds.push_back(asId(word++));
        listIdsUsed[asId(word++)] = true;
        --numOperands;
    }

    if (spv::InstructionDesc[opCode].hasResult()) {
        //listIds.push_back(asId(word++));
        listIdsUsed[asId(word++)] = true;
        --numOperands;
    }

    // Extended instructions: currently, assume everything is an ID.
    // TODO: add whatever data we need for exceptions to that
    if (opCode == spv::OpExtInst) {
        listIdsUsed[asId(word)] = true;  //Add ExtInstImport Id

        word += 2; // instruction set, and instruction from set
        numOperands -= 2;

        for (unsigned op = 0; op < numOperands; ++op)
            listIdsUsed[asId(word++)] = true;

        return true;
    }

    // Store IDs from instruction in our map
    for (int op = 0; numOperands > 0; ++op, --numOperands)
    {
        const spv::OperandParameters& operands = spv::InstructionDesc[opCode].operands;
#ifdef XKSLANG_DEBUG_MODE
        if (op >= operands.getNum()) {
            return ("op is out of bound");
            return false;
        }
#endif
        switch (operands.getClass(op))
        {
        case spv::OperandId:
        case spv::OperandScope:
        case spv::OperandMemorySemantics:
            listIdsUsed[asId(word++)] = true;
            break;

        case spv::OperandVariableIds:
            for (unsigned i = 0; i < numOperands; ++i)
                listIdsUsed[asId(word++)] = true;
            return true;

        case spv::OperandVariableLiterals:
            // for clarity
            // if (opCode == spv::OpDecorate && asDecoration(word - 1) == spv::DecorationBuiltIn) {
            //     ++word;
            //     --numOperands;
            // }
            // word += numOperands;
            return true;

        case spv::OperandVariableLiteralId: {
            if (opCode == spv::OpSwitch)
            {
                /// // word-2 is the position of the selector ID.  OpSwitch Literals match its type.
                /// // In case the IDs are currently being remapped, we get the word[-2] ID from
                /// // the circular idBuffer.
                /// const unsigned literalSizePos = (idBufferPos + idBufferSize - 2) % idBufferSize;
                /// const unsigned literalSize = idTypeSizeInWords(idBuffer[literalSizePos]);
                /// const unsigned numLiteralIdPairs = (nextInst - word) / (1 + literalSize);
                /// 
                /// for (unsigned arg = 0; arg<numLiteralIdPairs; ++arg) {
                ///     word += literalSize;  // literal
                ///     listIds.push_back(spv[word++]);   // label
                /// }
                return ("Unplugged operand for now.. to check later");
                return false;
            }
            else {
                return ("invalid OperandVariableLiteralId instruction");
                return false;
            }

            return true;
        }

        case spv::OperandLiteralString: {
            const int stringWordCount = literalStringWords(literalString(spv, word));
            word += stringWordCount;
            numOperands -= (stringWordCount - 1); // -1 because for() header post-decrements
            break;
        }

        // Execution mode might have extra literal operands.  Skip them.
        case spv::OperandExecutionMode:
            return true;

        // Single word operands we simply ignore, as they hold no IDs
        case spv::OperandLiteralNumber:
        case spv::OperandSource:
        case spv::OperandExecutionModel:
        case spv::OperandAddressing:
        case spv::OperandMemory:
        case spv::OperandStorage:
        case spv::OperandDimensionality:
        case spv::OperandSamplerAddressingMode:
        case spv::OperandSamplerFilterMode:
        case spv::OperandSamplerImageFormat:
        case spv::OperandImageChannelOrder:
        case spv::OperandImageChannelDataType:
        case spv::OperandImageOperands:
        case spv::OperandFPFastMath:
        case spv::OperandFPRoundingMode:
        case spv::OperandLinkageType:
        case spv::OperandAccessQualifier:
        case spv::OperandFuncParamAttr:
        case spv::OperandDecoration:
        case spv::OperandBuiltIn:
        case spv::OperandSelect:
        case spv::OperandLoop:
        case spv::OperandFunction:
        case spv::OperandMemoryAccess:
        case spv::OperandGroupOperation:
        case spv::OperandKernelEnqueueFlags:
        case spv::OperandKernelProfilingInfo:
        case spv::OperandCapability:
            ++word;
            break;

        default:
            return "Unhandled Operand Class";
        }
    }

    return true;
}

// inspired by spirvbin_t::processInstruction. Allow us to store the instruction data and ids
bool SpxStreamRemapper::parseInstruction(const vector<uint32_t>& bytecode, unsigned int word, spv::Op& opCode, unsigned int& wordCount, spv::Id& type, spv::Id& result, vector<spv::Id>& listIds, string& errorMsg)
{
    const unsigned int instructionStart = word;
    wordCount = opWordCount(bytecode[instructionStart]);
    opCode = opOpCode(bytecode[instructionStart]);
    const unsigned int nextInst = word++ + wordCount;

#ifdef XKSLANG_DEBUG_MODE
    if (nextInst > bytecode.size()) {errorMsg = "nextInst is out of bound"; return false;}
    if (wordCount == 0) { errorMsg = "wordcount equals 0"; return false; }
#endif

    // Base for computing number of operands; will be updated as more is learned
    unsigned int numOperands = wordCount - 1;

    // Read type and result ID from instruction desc table
    if (spv::InstructionDesc[opCode].hasType()) {
        //listIds.push_back(asId(word++));
        type = bytecode[word++];
        --numOperands;
    }
    else type = spvUndefinedId;

    if (spv::InstructionDesc[opCode].hasResult()) {
        //listIds.push_back(asId(word++));
        result = bytecode[word++];
        --numOperands;
    }
    else result = spvUndefinedId;

    // Extended instructions: currently, assume everything is an ID.
    // TODO: add whatever data we need for exceptions to that
    if (opCode == spv::OpExtInst) {
        listIds.push_back(bytecode[word]);  //Add ExtInstImport Id

        word += 2; // instruction set, and instruction from set
        numOperands -= 2;

        for (unsigned op = 0; op < numOperands; ++op)
            listIds.push_back(bytecode[word++]); // ID

        return true;
    }

    // Store IDs from instruction in our map
    for (int op = 0; numOperands > 0; ++op, --numOperands)
    {
        const spv::OperandParameters& operands = spv::InstructionDesc[opCode].operands;
#ifdef XKSLANG_DEBUG_MODE
        if (op >= operands.getNum()) {
            errorMsg = "op is out of bound";
            return false;
        }
#endif
        switch (operands.getClass(op))
        {
            case spv::OperandId:
            case spv::OperandScope:
            case spv::OperandMemorySemantics:
                listIds.push_back(bytecode[word++]);
                break;

            case spv::OperandVariableIds:
                for (unsigned i = 0; i < numOperands; ++i)
                    listIds.push_back(bytecode[word++]);
                return true;

            case spv::OperandVariableLiterals:
                // for clarity
                // if (opCode == spv::OpDecorate && asDecoration(word - 1) == spv::DecorationBuiltIn) {
                //     ++word;
                //     --numOperands;
                // }
                // word += numOperands;
                return true;

            case spv::OperandVariableLiteralId: {
                if (opCode == spv::OpSwitch)
                {
                    /// // word-2 is the position of the selector ID.  OpSwitch Literals match its type.
                    /// // In case the IDs are currently being remapped, we get the word[-2] ID from
                    /// // the circular idBuffer.
                    /// const unsigned literalSizePos = (idBufferPos + idBufferSize - 2) % idBufferSize;
                    /// const unsigned literalSize = idTypeSizeInWords(idBuffer[literalSizePos]);
                    /// const unsigned numLiteralIdPairs = (nextInst - word) / (1 + literalSize);
                    /// 
                    /// for (unsigned arg = 0; arg<numLiteralIdPairs; ++arg) {
                    ///     word += literalSize;  // literal
                    ///     listIds.push_back(bytecode[word++]);   // label
                    /// }
                    errorMsg = "Unplugged operand for now.. to check later";
                    return false;
                }
                else {
                    errorMsg = "invalid OperandVariableLiteralId instruction";
                    return false;
                }

                return true;
            }

            case spv::OperandLiteralString: {
                const int stringWordCount = literalStringWords(literalString(bytecode, word));
                word += stringWordCount;
                numOperands -= (stringWordCount - 1); // -1 because for() header post-decrements
                break;
            }

            // Execution mode might have extra literal operands.  Skip them.
            case spv::OperandExecutionMode:
                return true;

            // Single word operands we simply ignore, as they hold no IDs
            case spv::OperandLiteralNumber:
            case spv::OperandSource:
            case spv::OperandExecutionModel:
            case spv::OperandAddressing:
            case spv::OperandMemory:
            case spv::OperandStorage:
            case spv::OperandDimensionality:
            case spv::OperandSamplerAddressingMode:
            case spv::OperandSamplerFilterMode:
            case spv::OperandSamplerImageFormat:
            case spv::OperandImageChannelOrder:
            case spv::OperandImageChannelDataType:
            case spv::OperandImageOperands:
            case spv::OperandFPFastMath:
            case spv::OperandFPRoundingMode:
            case spv::OperandLinkageType:
            case spv::OperandAccessQualifier:
            case spv::OperandFuncParamAttr:
            case spv::OperandDecoration:
            case spv::OperandBuiltIn:
            case spv::OperandSelect:
            case spv::OperandLoop:
            case spv::OperandFunction:
            case spv::OperandMemoryAccess:
            case spv::OperandGroupOperation:
            case spv::OperandKernelEnqueueFlags:
            case spv::OperandKernelProfilingInfo:
            case spv::OperandCapability:
                ++word;
                break;

            default:
                errorMsg = "Unhandled Operand Class";
                return false;
        }
    }

    return true;
}

// inspired by spirvbin_t::processInstruction. Allow us to store the instruction data and ids
bool SpxStreamRemapper::parseInstruction(unsigned int word, spv::Op& opCode, unsigned int& wordCount, spv::Id& type, spv::Id& result, vector<spv::Id>& listIds)
{
    string errorMsg;
    if (!parseInstruction(spv, word, opCode, wordCount, type, result, listIds, errorMsg))
    {
        error(errorMsg);
        return false;
    }
    return true;
}

//=====================================================================================================================================
//=====================================================================================================================================
BytecodePortionToRemove* BytecodeUpdateController::AddPortionToRemove(unsigned int position, unsigned int count)
{
    if (count == 0) return nullptr;

    auto itListPos = listSortedPortionsToRemove.begin();
    while (itListPos != listSortedPortionsToRemove.end())
    {
        if (position == itListPos->position) return nullptr;
        if (position > itListPos->position) break; //we sort the list from higher to smaller position

        if (position + count > itListPos->position) return nullptr; //we're overlapping
        itListPos++;
    }

    itListPos = listSortedPortionsToRemove.insert(itListPos, BytecodePortionToRemove(position, count));
    return &(*itListPos);
}

BytecodeChunk* BytecodeUpdateController::InsertNewBytecodeChunckAt(unsigned int position, InsertionConflictBehaviourEnum conflictBehaviour, unsigned int countBytesToOverlap)
{
    auto itListPos = listSortedChunksToInsert.begin();
    while (itListPos != listSortedChunksToInsert.end())
    {
        if (position == itListPos->insertionPos)
        {
            bool canBreak = true;
            switch (conflictBehaviour)
            {
                case InsertionConflictBehaviourEnum::InsertFirst:
                    canBreak = false;
                    break;
                case InsertionConflictBehaviourEnum::InsertLast:
                    canBreak = true;
                    break;
                case InsertionConflictBehaviourEnum::ReturnNull:
                    return nullptr;
            }
            if (canBreak) break;
        }

        if (position > itListPos->insertionPos) break; //we sort the list from higher to smaller position
        itListPos++;
    }
    itListPos = listSortedChunksToInsert.insert(itListPos, BytecodeChunk(position, countBytesToOverlap));
    return &(*itListPos);
}

/*BytecodeChunk* BytecodeUpdateController::GetOrCreateBytecodeChunckAt(unsigned int position, unsigned int countBytesToOverlap)
{
    auto itListPos = listSortedChunksToInsert.begin();
    while (itListPos != listSortedChunksToInsert.end())
    {
        if (position == itListPos->insertionPos) {
            if (countBytesToOverlap != itListPos->countInstructionsToOverlap) return nullptr;  //the overlap parameters are different, return null
            return &(*itListPos);
        }

        if (position > itListPos->insertionPos) break; //we sort the list from higher to smaller position
        itListPos++;
    }
    itListPos = listSortedChunksToInsert.insert(itListPos, BytecodeChunk(position, countBytesToOverlap));
    return &(*itListPos);
}*/

BytecodeChunk* BytecodeUpdateController::GetBytecodeChunkAt(unsigned int position)
{
    for (auto it = listSortedChunksToInsert.begin(); it != listSortedChunksToInsert.end(); it++)
    {
        if (position == it->insertionPos) return &(*it);
        if (position < it->insertionPos) break; //the list is sorted from higher to smaller position
    }

    return nullptr;
}

bool SpxStreamRemapper::ApplyBytecodeUpdateController(BytecodeUpdateController& bytecodeUpdateController)
{
    unsigned int bytecodeOriginalSize = spv.size();

    //first : update all values and portions
    for (auto itau = bytecodeUpdateController.listAtomicUpdates.begin(); itau != bytecodeUpdateController.listAtomicUpdates.end(); itau++)
    {
        const BytecodeValueToReplace& atomicValueUpdate = *itau;

#ifdef XKSLANG_DEBUG_MODE
        if (atomicValueUpdate.pos >= bytecodeOriginalSize) { error("pos is out of bound"); break; }
#endif

        spv[atomicValueUpdate.pos] = atomicValueUpdate.value;
    }
    for (auto itau = bytecodeUpdateController.listPortionsToUpdates.begin(); itau != bytecodeUpdateController.listPortionsToUpdates.end(); itau++)
    {
        const BytecodePortionToReplace& portionUpdate = *itau;

#ifdef XKSLANG_DEBUG_MODE
        if (portionUpdate.pos + portionUpdate.values.size() >= bytecodeOriginalSize) { error("portion to update is out of bound"); break; }
#endif

        auto itdest = spv.begin() + portionUpdate.pos;
        for (auto itp = portionUpdate.values.begin(); itp != portionUpdate.values.end(); itp++)
        {
            *itdest++ = *itp;
        }
    }

    //Insert all new chuncks in the bytecode, update positions accordingly of chuncks to remove
    for (auto itbc = bytecodeUpdateController.listSortedChunksToInsert.begin(); itbc != bytecodeUpdateController.listSortedChunksToInsert.end(); itbc++)
    {
        const BytecodeChunk& bytecodeChunck = *itbc;
        if (bytecodeChunck.bytecode.size() == 0) continue;

        int countBytesInserted = 0;

#ifdef XKSLANG_DEBUG_MODE
        if (bytecodeChunck.insertionPos > bytecodeOriginalSize) { error("bytecode chunck is out of bound"); break; }
        if (bytecodeChunck.countInstructionsToOverlap > bytecodeChunck.bytecode.size()) { error("bytecode chunck overlaps too many bytes"); break; }
#endif

        if (bytecodeChunck.countInstructionsToOverlap == 0)
        {
            spv.insert(spv.begin() + bytecodeChunck.insertionPos, bytecodeChunck.bytecode.begin(), bytecodeChunck.bytecode.end());
            countBytesInserted = bytecodeChunck.bytecode.size();
        }
        else
        {
            //We overlap some bytes, and maybe add some more
            unsigned int countOverlaps = bytecodeChunck.countInstructionsToOverlap;
            int countRemaining = (int)bytecodeChunck.bytecode.size() - (int)countOverlaps;

#ifdef XKSLANG_DEBUG_MODE
            if (countRemaining < 0) { error("invalid number of overlapping bytes"); break; }
            if (bytecodeChunck.insertionPos + countOverlaps >= spv.size()) { error("bytecode chunck overlaps too many bytes"); break; }
#endif

            for (unsigned int i = 0; i < countOverlaps; ++i) spv[bytecodeChunck.insertionPos + i] = bytecodeChunck.bytecode[i];
            if (countRemaining > 0){
                spv.insert(spv.begin() + (bytecodeChunck.insertionPos + countOverlaps), bytecodeChunck.bytecode.begin() + countOverlaps, bytecodeChunck.bytecode.end());
            }
            countBytesInserted = countRemaining;
        }

        if (countBytesInserted > 0)
        {
            //for all portions to remove coming AFTER the position we just inserted into, we update their position
            for (auto itpr = bytecodeUpdateController.listSortedPortionsToRemove.begin(); itpr != bytecodeUpdateController.listSortedPortionsToRemove.end(); itpr++)
            {
                BytecodePortionToRemove& portionToRemove = *itpr;

                if (portionToRemove.position >= bytecodeChunck.insertionPos)
                {
                    portionToRemove.position += countBytesInserted;
                }
                else
                {
                    if (portionToRemove.position + portionToRemove.count > bytecodeChunck.insertionPos){ error("A portion to remove overlap an insertion chunck"); }
                    break;
                }
            }
        }
    }

    //Remove all portions to remove
    for (auto itpr = bytecodeUpdateController.listSortedPortionsToRemove.begin(); itpr != bytecodeUpdateController.listSortedPortionsToRemove.end(); itpr++)
    {
        BytecodePortionToRemove& portionToRemove = *itpr;
        spv.erase(spv.begin() + portionToRemove.position, spv.begin() + (portionToRemove.position + portionToRemove.count));
    }

    if (errorMessages.size() > 0) return false;
    return true;
}

bool SpxStreamRemapper::IsResourceType(const spv::Op& opCode)
{
    switch (opCode)
    {
    case spv::OpTypeImage:
    case spv::OpTypeSampler:
    case spv::OpTypeSampledImage:
        return true;
    }

    return false;
}