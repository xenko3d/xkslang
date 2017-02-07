//
// Copyright (C) 2014-2015 LunarG, Inc.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

//
// Disassembler for SPIR-V.
//

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <iomanip>
#include <stack>
#include <sstream>
#include <cstring>

#include "disassemble.h"
#include "doc.h"

namespace spv {
    extern "C" {
        // Include C-based headers that don't have a namespace
        #include "GLSL.std.450.h"
#ifdef AMD_EXTENSIONS
        #include "GLSL.ext.AMD.h"
#endif
#ifdef NV_EXTENSIONS
        #include "GLSL.ext.NV.h"
#endif
    }
}
const char* GlslStd450DebugNames[spv::GLSLstd450Count];

namespace spv {

#ifdef AMD_EXTENSIONS
static const char* GLSLextAMDGetDebugNames(const char*, unsigned);
#endif

#ifdef NV_EXTENSIONS
static const char* GLSLextNVGetDebugNames(const char*, unsigned);
#endif

static void Kill(std::ostream& out, const char* message)
{
    out << std::endl << "Disassembly failed: " << message << std::endl;
    exit(1);
}

// used to identify the extended instruction library imported when printing
enum ExtInstSet {
    GLSL450Inst,
#ifdef AMD_EXTENSIONS
    GLSLextAMDInst,
#endif
#ifdef NV_EXTENSIONS
    GLSLextNVInst,
#endif
    OpenCLExtInst,
};

// Container class for a single instance of a SPIR-V stream, with methods for disassembly.
class SpirvStream {
public:
    SpirvStream(std::ostream& out, const std::vector<unsigned int>& stream) : out(out), stream(stream), word(0), nextNestedControl(0) { }
    virtual ~SpirvStream() { }

    void validate();
    void processInstructions();

protected:
    SpirvStream(const SpirvStream&);
    SpirvStream& operator=(const SpirvStream&);
    Op getOpCode(int id) const { return idInstruction[id] ? (Op)(stream[idInstruction[id]] & OpCodeMask) : OpNop; }

    // Output methods
    void outputIndent();
    void formatId(Id id, std::stringstream&);
    void outputResultId(Id id);
    void outputTypeId(Id id);
    void outputId(Id id);
    void outputProperty(int propId);
    void outputMask(OperandClass operandClass, unsigned mask);
    void disassembleImmediates(int numOperands);
    void disassembleIds(int numOperands);
    void disassembleProperties(int numOperands);
    int disassembleString();
    void disassembleInstruction(Id resultId, Id typeId, Op opCode, int numOperands);

    // Data
    std::ostream& out;                       // where to write the disassembly
    const std::vector<unsigned int>& stream; // the actual word stream
    int size;                                // the size of the word stream
    int word;                                // the next word of the stream to read

    // map each <id> to the instruction that created it
    Id bound;
    std::vector<unsigned int> idInstruction;  // the word offset into the stream where the instruction for result [id] starts; 0 if not yet seen (forward reference or function parameter)

    std::vector<std::string> idDescriptor;    // the best text string known for explaining the <id>

    // schema
    unsigned int schema;

    // stack of structured-merge points
    std::stack<Id> nestedControl;
    Id nextNestedControl;         // need a slight delay for when we are nested
};

void SpirvStream::validate()
{
    size = (int)stream.size();
    if (size < 4)
        Kill(out, "stream is too short");

    // Magic number
    if (stream[word++] != MagicNumber) {
        out << "Bad magic number";
        return;
    }

    // Version
    out << "// Module Version " << std::hex << stream[word++] << std::endl;

    // Generator's magic number
    out << "// Generated by (magic number): " << std::hex << stream[word++] << std::dec << std::endl;

    // Result <id> bound
    bound = stream[word++];
    idInstruction.resize(bound);
    idDescriptor.resize(bound);
    out << "// Id's are bound by " << bound << std::endl;
    out << std::endl;

    // Reserved schema, must be 0 for now
    schema = stream[word++];
    if (schema != 0)
        Kill(out, "bad schema, must be 0");
}

// Loop over all the instructions, in order, processing each.
// Boiler plate for each is handled here directly, the rest is dispatched.
void SpirvStream::processInstructions()
{
    // Instructions
    while (word < size) {
        int instructionStart = word;

        // Instruction wordCount and opcode
        unsigned int firstWord = stream[word];
        unsigned wordCount = firstWord >> WordCountShift;
        Op opCode = (Op)(firstWord & OpCodeMask);
        int nextInst = word + wordCount;
        ++word;

        // Presence of full instruction
        if (nextInst > size)
            Kill(out, "stream instruction terminated too early");

        // Base for computing number of operands; will be updated as more is learned
        unsigned numOperands = wordCount - 1;

        // Type <id>
        Id typeId = 0;
        if (InstructionDesc[opCode].hasType()) {
            typeId = stream[word++];
            --numOperands;
        }

        // Result <id>
        Id resultId = 0;
        if (InstructionDesc[opCode].hasResult()) {
            resultId = stream[word++];
            --numOperands;

            // save instruction for future reference
            idInstruction[resultId] = instructionStart;
        }

        outputResultId(resultId);
        outputTypeId(typeId);
        outputIndent();

        // Hand off the Op and all its operands
        disassembleInstruction(resultId, typeId, opCode, numOperands);
        if (word != nextInst) {
            out << " ERROR, incorrect number of operands consumed.  At " << word << " instead of " << nextInst << " instruction start was " << instructionStart;
            word = nextInst;
        }
        out << std::endl;
    }
}

void SpirvStream::outputIndent()
{
    for (int i = 0; i < (int)nestedControl.size(); ++i)
        out << "  ";
}

void SpirvStream::formatId(Id id, std::stringstream& idStream)
{
    if (id != 0) {
        // On instructions with no IDs, this is called with "0", which does not
        // have to be within ID bounds on null shaders.
        if (id >= bound)
            Kill(out, "Bad <id>");

        idStream << id;
        if (idDescriptor[id].size() > 0)
            idStream << "(" << idDescriptor[id] << ")";
    }
}

void SpirvStream::outputResultId(Id id)
{
    const int width = 16;
    std::stringstream idStream;
    formatId(id, idStream);
    out << std::setw(width) << std::right << idStream.str();
    if (id != 0)
        out << ":";
    else
        out << " ";

    if (nestedControl.size() && id == nestedControl.top())
        nestedControl.pop();
}

void SpirvStream::outputTypeId(Id id)
{
    const int width = 12;
    std::stringstream idStream;
    formatId(id, idStream);
    out << std::setw(width) << std::right << idStream.str() << " ";
}

void SpirvStream::outputProperty(int propId)
{
    out << XkslPropertyString(propId);
}

void SpirvStream::outputId(Id id)
{
    if (id >= bound)
        Kill(out, "Bad <id>");

    out << id;
    if (idDescriptor[id].size() > 0)
        out << "(" << idDescriptor[id] << ")";
}

void SpirvStream::outputMask(OperandClass operandClass, unsigned mask)
{
    if (mask == 0)
        out << "None";
    else {
        for (int m = 0; m < OperandClassParams[operandClass].ceiling; ++m) {
            if (mask & (1 << m))
                out << OperandClassParams[operandClass].getName(m) << " ";
        }
    }
}

void SpirvStream::disassembleImmediates(int numOperands)
{
    for (int i = 0; i < numOperands; ++i) {
        out << stream[word++];
        if (i < numOperands - 1)
            out << " ";
    }
}

void SpirvStream::disassembleProperties(int numOperands)
{
    for (int i = 0; i < numOperands; ++i) {
        outputProperty(stream[word++]);
        if (i < numOperands - 1)
            out << " ";
    }
}

void SpirvStream::disassembleIds(int numOperands)
{
    for (int i = 0; i < numOperands; ++i) {
        outputId(stream[word++]);
        if (i < numOperands - 1)
            out << " ";
    }
}

// return the number of operands consumed by the string
int SpirvStream::disassembleString()
{
    int startWord = word;

    out << " \"";

    const char* wordString;
    bool done = false;
    do {
        unsigned int content = stream[word];
        wordString = (const char*)&content;
        for (int charCount = 0; charCount < 4; ++charCount) {
            if (*wordString == 0) {
                done = true;
                break;
            }
            out << *(wordString++);
        }
        ++word;
    } while (! done);

    out << "\"";

    return word - startWord;
}

void SpirvStream::disassembleInstruction(Id resultId, Id /*typeId*/, Op opCode, int numOperands)
{
    // Process the opcode

    out << (OpcodeString(opCode) + 2);  // leave out the "Op"

    if (opCode == OpLoopMerge || opCode == OpSelectionMerge)
        nextNestedControl = stream[word];
    else if (opCode == OpBranchConditional || opCode == OpSwitch) {
        if (nextNestedControl) {
            nestedControl.push(nextNestedControl);
            nextNestedControl = 0;
        }
    } else if (opCode == OpExtInstImport) {
        idDescriptor[resultId] = (const char*)(&stream[word]);
    }
    else {
        if (resultId != 0 && idDescriptor[resultId].size() == 0) {
            switch (opCode) {
            case OpTypeInt:
                idDescriptor[resultId] = "int";
                break;
            case OpTypeFloat:
                idDescriptor[resultId] = "float";
                break;
            case OpTypeBool:
                idDescriptor[resultId] = "bool";
                break;
            case OpTypeStruct:
                idDescriptor[resultId] = "struct";
                break;
            case OpTypeXlslShaderClass:
                idDescriptor[resultId] = "XkslShaderClass";
                break;
            case OpTypePointer:
                idDescriptor[resultId] = "ptr";
                break;
            case OpTypeVector:
                if (idDescriptor[stream[word]].size() > 0)
                    idDescriptor[resultId].append(idDescriptor[stream[word]].begin(), idDescriptor[stream[word]].begin() + 1);
                idDescriptor[resultId].append("vec");
                switch (stream[word + 1]) {
                case 2:   idDescriptor[resultId].append("2");   break;
                case 3:   idDescriptor[resultId].append("3");   break;
                case 4:   idDescriptor[resultId].append("4");   break;
                case 8:   idDescriptor[resultId].append("8");   break;
                case 16:  idDescriptor[resultId].append("16");  break;
                case 32:  idDescriptor[resultId].append("32");  break;
                default: break;
                }
                break;
            default:
                break;
            }
        }
    }

    // Process the operands.  Note, a new context-dependent set could be
    // swapped in mid-traversal.

    // Handle images specially, so can put out helpful strings.
    if (opCode == OpTypeImage) {
        out << " ";
        disassembleIds(1);
        out << " " << DimensionString((Dim)stream[word++]);
        out << (stream[word++] != 0 ? " depth" : "");
        out << (stream[word++] != 0 ? " array" : "");
        out << (stream[word++] != 0 ? " multi-sampled" : "");
        switch (stream[word++]) {
        case 0: out << " runtime";    break;
        case 1: out << " sampled";    break;
        case 2: out << " nonsampled"; break;
        }
        out << " format:" << ImageFormatString((ImageFormat)stream[word++]);

        if (numOperands == 8) {
            out << " " << AccessQualifierString(stream[word++]);
        }
        return;
    }

    // Handle all the parameterized operands
    for (int op = 0; op < InstructionDesc[opCode].operands.getNum() && numOperands > 0; ++op) {
        out << " ";
        OperandClass operandClass = InstructionDesc[opCode].operands.getClass(op);
        switch (operandClass) {
        case OperandId:
        case OperandScope:
        case OperandMemorySemantics:
            disassembleIds(1);
            --numOperands;
            // Get names for printing "(XXX)" for readability, *after* this id
            if (opCode == OpName)
                idDescriptor[stream[word - 1]] = (const char*)(&stream[word]);
            break;
        case OperandVariableIds:
            disassembleIds(numOperands);
            return;
        case OperandImageOperands:
            outputMask(OperandImageOperands, stream[word++]);
            --numOperands;
            disassembleIds(numOperands);
            return;
        case OperandOptionalLiteral:
        case OperandVariableLiterals:
            if ((opCode == OpDecorate && stream[word - 1] == DecorationBuiltIn) ||
                (opCode == OpMemberDecorate && stream[word - 1] == DecorationBuiltIn)) {
                out << BuiltInString(stream[word++]);
                --numOperands;
                ++op;
            }
            disassembleImmediates(numOperands);
            return;
        case OperandVariableIdLiteral:
            while (numOperands > 0) {
                out << std::endl;
                outputResultId(0);
                outputTypeId(0);
                outputIndent();
                out << "     Type ";
                disassembleIds(1);
                out << ", member ";
                disassembleImmediates(1);
                numOperands -= 2;
            }
            return;
        case OperandVariableLiteralId:
            while (numOperands > 0) {
                out << std::endl;
                outputResultId(0);
                outputTypeId(0);
                outputIndent();
                out << "     case ";
                disassembleImmediates(1);
                out << ": ";
                disassembleIds(1);
                numOperands -= 2;
            }
            return;
        case OperandLiteralNumber:
            disassembleImmediates(1);
            --numOperands;
            if (opCode == OpExtInst) {
                ExtInstSet extInstSet = GLSL450Inst;
                const char* name = idDescriptor[stream[word - 2]].c_str();
                if (0 == memcmp("OpenCL", name, 6)) {
                    extInstSet = OpenCLExtInst;
#ifdef AMD_EXTENSIONS
                } else if (strcmp(spv::E_SPV_AMD_shader_ballot, name) == 0 ||
                           strcmp(spv::E_SPV_AMD_shader_trinary_minmax, name) == 0 ||
                           strcmp(spv::E_SPV_AMD_shader_explicit_vertex_parameter, name) == 0 ||
                           strcmp(spv::E_SPV_AMD_gcn_shader, name) == 0) {
                    extInstSet = GLSLextAMDInst;
#endif
#ifdef NV_EXTENSIONS
                }else if (strcmp(spv::E_SPV_NV_sample_mask_override_coverage, name) == 0 ||
                          strcmp(spv::E_SPV_NV_geometry_shader_passthrough, name) == 0) {
                    extInstSet = GLSLextNVInst;
#endif
                }
                unsigned entrypoint = stream[word - 1];
                if (extInstSet == GLSL450Inst) {
                    if (entrypoint < GLSLstd450Count) {
                        out << "(" << GlslStd450DebugNames[entrypoint] << ")";
                    }
#ifdef AMD_EXTENSIONS
                } else if (extInstSet == GLSLextAMDInst) {
                    out << "(" << GLSLextAMDGetDebugNames(name, entrypoint) << ")";
#endif
#ifdef NV_EXTENSIONS
                }
                else if (extInstSet == GLSLextNVInst) {
                    out << "(" << GLSLextNVGetDebugNames(name, entrypoint) << ")";
#endif
                }
            }
            break;
        case OperandOptionalLiteralString:
        case OperandLiteralString:
            numOperands -= disassembleString();
            break;
        case OperandProperties:
            disassembleProperties(numOperands);
            return;
            break;
        default:
            assert(operandClass >= OperandSource && operandClass < OperandOpcode);

            if (OperandClassParams[operandClass].bitmask)
                outputMask(operandClass, stream[word++]);
            else
                out << OperandClassParams[operandClass].getName(stream[word++]);
            --numOperands;

            break;
        }
    }

    return;
}

static void GLSLstd450GetDebugNames(const char** names)
{
    for (int i = 0; i < GLSLstd450Count; ++i)
        names[i] = "Unknown";

    names[GLSLstd450Round]                   = "Round";
    names[GLSLstd450RoundEven]               = "RoundEven";
    names[GLSLstd450Trunc]                   = "Trunc";
    names[GLSLstd450FAbs]                    = "FAbs";
    names[GLSLstd450SAbs]                    = "SAbs";
    names[GLSLstd450FSign]                   = "FSign";
    names[GLSLstd450SSign]                   = "SSign";
    names[GLSLstd450Floor]                   = "Floor";
    names[GLSLstd450Ceil]                    = "Ceil";
    names[GLSLstd450Fract]                   = "Fract";
    names[GLSLstd450Radians]                 = "Radians";
    names[GLSLstd450Degrees]                 = "Degrees";
    names[GLSLstd450Sin]                     = "Sin";
    names[GLSLstd450Cos]                     = "Cos";
    names[GLSLstd450Tan]                     = "Tan";
    names[GLSLstd450Asin]                    = "Asin";
    names[GLSLstd450Acos]                    = "Acos";
    names[GLSLstd450Atan]                    = "Atan";
    names[GLSLstd450Sinh]                    = "Sinh";
    names[GLSLstd450Cosh]                    = "Cosh";
    names[GLSLstd450Tanh]                    = "Tanh";
    names[GLSLstd450Asinh]                   = "Asinh";
    names[GLSLstd450Acosh]                   = "Acosh";
    names[GLSLstd450Atanh]                   = "Atanh";
    names[GLSLstd450Atan2]                   = "Atan2";
    names[GLSLstd450Pow]                     = "Pow";
    names[GLSLstd450Exp]                     = "Exp";
    names[GLSLstd450Log]                     = "Log";
    names[GLSLstd450Exp2]                    = "Exp2";
    names[GLSLstd450Log2]                    = "Log2";
    names[GLSLstd450Sqrt]                    = "Sqrt";
    names[GLSLstd450InverseSqrt]             = "InverseSqrt";
    names[GLSLstd450Determinant]             = "Determinant";
    names[GLSLstd450MatrixInverse]           = "MatrixInverse";
    names[GLSLstd450Modf]                    = "Modf";
    names[GLSLstd450ModfStruct]              = "ModfStruct";
    names[GLSLstd450FMin]                    = "FMin";
    names[GLSLstd450SMin]                    = "SMin";
    names[GLSLstd450UMin]                    = "UMin";
    names[GLSLstd450FMax]                    = "FMax";
    names[GLSLstd450SMax]                    = "SMax";
    names[GLSLstd450UMax]                    = "UMax";
    names[GLSLstd450FClamp]                  = "FClamp";
    names[GLSLstd450SClamp]                  = "SClamp";
    names[GLSLstd450UClamp]                  = "UClamp";
    names[GLSLstd450FMix]                    = "FMix";
    names[GLSLstd450Step]                    = "Step";
    names[GLSLstd450SmoothStep]              = "SmoothStep";
    names[GLSLstd450Fma]                     = "Fma";
    names[GLSLstd450Frexp]                   = "Frexp";
    names[GLSLstd450FrexpStruct]             = "FrexpStruct";
    names[GLSLstd450Ldexp]                   = "Ldexp";
    names[GLSLstd450PackSnorm4x8]            = "PackSnorm4x8";
    names[GLSLstd450PackUnorm4x8]            = "PackUnorm4x8";
    names[GLSLstd450PackSnorm2x16]           = "PackSnorm2x16";
    names[GLSLstd450PackUnorm2x16]           = "PackUnorm2x16";
    names[GLSLstd450PackHalf2x16]            = "PackHalf2x16";
    names[GLSLstd450PackDouble2x32]          = "PackDouble2x32";
    names[GLSLstd450UnpackSnorm2x16]         = "UnpackSnorm2x16";
    names[GLSLstd450UnpackUnorm2x16]         = "UnpackUnorm2x16";
    names[GLSLstd450UnpackHalf2x16]          = "UnpackHalf2x16";
    names[GLSLstd450UnpackSnorm4x8]          = "UnpackSnorm4x8";
    names[GLSLstd450UnpackUnorm4x8]          = "UnpackUnorm4x8";
    names[GLSLstd450UnpackDouble2x32]        = "UnpackDouble2x32";
    names[GLSLstd450Length]                  = "Length";
    names[GLSLstd450Distance]                = "Distance";
    names[GLSLstd450Cross]                   = "Cross";
    names[GLSLstd450Normalize]               = "Normalize";
    names[GLSLstd450FaceForward]             = "FaceForward";
    names[GLSLstd450Reflect]                 = "Reflect";
    names[GLSLstd450Refract]                 = "Refract";
    names[GLSLstd450FindILsb]                = "FindILsb";
    names[GLSLstd450FindSMsb]                = "FindSMsb";
    names[GLSLstd450FindUMsb]                = "FindUMsb";
    names[GLSLstd450InterpolateAtCentroid]   = "InterpolateAtCentroid";
    names[GLSLstd450InterpolateAtSample]     = "InterpolateAtSample";
    names[GLSLstd450InterpolateAtOffset]     = "InterpolateAtOffset";
}

#ifdef AMD_EXTENSIONS
static const char* GLSLextAMDGetDebugNames(const char* name, unsigned entrypoint)
{
    if (strcmp(name, spv::E_SPV_AMD_shader_ballot) == 0) {
        switch (entrypoint) {
        case SwizzleInvocationsAMD:         return "SwizzleInvocationsAMD";
        case SwizzleInvocationsMaskedAMD:   return "SwizzleInvocationsMaskedAMD";
        case WriteInvocationAMD:            return "WriteInvocationAMD";
        case MbcntAMD:                      return "MbcntAMD";
        default:                            return "Bad";
        }
    } else if (strcmp(name, spv::E_SPV_AMD_shader_trinary_minmax) == 0) {
        switch (entrypoint) {
        case FMin3AMD:      return "FMin3AMD";
        case UMin3AMD:      return "UMin3AMD";
        case SMin3AMD:      return "SMin3AMD";
        case FMax3AMD:      return "FMax3AMD";
        case UMax3AMD:      return "UMax3AMD";
        case SMax3AMD:      return "SMax3AMD";
        case FMid3AMD:      return "FMid3AMD";
        case UMid3AMD:      return "UMid3AMD";
        case SMid3AMD:      return "SMid3AMD";
        default:            return "Bad";
        }
    } else if (strcmp(name, spv::E_SPV_AMD_shader_explicit_vertex_parameter) == 0) {
        switch (entrypoint) {
        case InterpolateAtVertexAMD:    return "InterpolateAtVertexAMD";
        default:                        return "Bad";
        }
    }
    else if (strcmp(name, spv::E_SPV_AMD_gcn_shader) == 0) {
        switch (entrypoint) {
        case CubeFaceIndexAMD:      return "CubeFaceIndexAMD";
        case CubeFaceCoordAMD:      return "CubeFaceCoordAMD";
        case TimeAMD:               return "TimeAMD";
        default:
            break;
        }
    }

    return "Bad";
}
#endif


#ifdef NV_EXTENSIONS
static const char* GLSLextNVGetDebugNames(const char* name, unsigned entrypoint)
{
    if (strcmp(name, spv::E_SPV_NV_sample_mask_override_coverage) == 0 ||
        strcmp(name, spv::E_SPV_NV_geometry_shader_passthrough) == 0) {
        switch (entrypoint) {
        case OverrideCoverageNV:          return "OverrideCoverageNV";
        case PassthroughNV:               return "PassthroughNV";
        case GeometryShaderPassthroughNV: return "GeometryShaderPassthroughNV";
        default:                          return "Bad";
        }
    }
    return "Bad";
}
#endif

void Disassemble(std::ostream& out, const std::vector<unsigned int>& stream)
{
    if (stream.size() == 0) return;
    SpirvStream SpirvStream(out, stream);
    spv::Parameterize();
    GLSLstd450GetDebugNames(GlslStd450DebugNames);
    SpirvStream.validate();
    SpirvStream.processInstructions();
}

}; // end namespace spv
