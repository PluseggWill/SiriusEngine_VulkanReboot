#pragma once

#include <cstdint>

// Module: Rhi_Enums — API-agnostic GPU enums for Gfx (no vulkan.h).

enum class Rhi_PipelineBindPoint : uint8_t { Graphics = 0, Compute };

enum class Rhi_ShaderStage : uint32_t {
    Vertex   = 1u << 0,
    Fragment = 1u << 1,
    Compute  = 1u << 2,
};

inline Rhi_ShaderStage operator|( Rhi_ShaderStage a, Rhi_ShaderStage b ) {
    return static_cast< Rhi_ShaderStage >( static_cast< uint32_t >( a ) | static_cast< uint32_t >( b ) );
}

inline Rhi_ShaderStage operator&( Rhi_ShaderStage a, Rhi_ShaderStage b ) {
    return static_cast< Rhi_ShaderStage >( static_cast< uint32_t >( a ) & static_cast< uint32_t >( b ) );
}

enum class Rhi_BufferUsage : uint32_t {
    TransferSrc = 1u << 0,
    TransferDst = 1u << 1,
    Uniform     = 1u << 2,
    Storage     = 1u << 3,
    Vertex      = 1u << 4,
    Index       = 1u << 5,
};

inline Rhi_BufferUsage operator|( Rhi_BufferUsage a, Rhi_BufferUsage b ) {
    return static_cast< Rhi_BufferUsage >( static_cast< uint32_t >( a ) | static_cast< uint32_t >( b ) );
}

enum class Rhi_ImageUsage : uint32_t {
    TransferSrc            = 1u << 0,
    TransferDst            = 1u << 1,
    Sampled                = 1u << 2,
    Storage                = 1u << 3,
    ColorAttachment        = 1u << 4,
    DepthStencilAttachment = 1u << 5,
};

inline Rhi_ImageUsage operator|( Rhi_ImageUsage a, Rhi_ImageUsage b ) {
    return static_cast< Rhi_ImageUsage >( static_cast< uint32_t >( a ) | static_cast< uint32_t >( b ) );
}

enum class Rhi_Format : uint16_t {
    Undefined = 0,
    R8_Unorm,
    R16_Sfloat,
    R32_Sfloat,
    RG16_Sfloat,
    RGBA8_Unorm,
    RGBA16_Sfloat,
    D32_Sfloat,
};

enum class Rhi_ImageLayout : uint8_t {
    Undefined = 0,
    General,
    ColorAttachment,
    DepthStencilAttachment,
    ShaderReadOnly,
    TransferSrc,
    TransferDst,
    Present,
};

enum class Rhi_MemoryUsage : uint8_t { GpuOnly = 0, CpuToGpu, GpuToCpu };

enum class Rhi_PipelineStage : uint32_t {
    TopOfPipe       = 1u << 0,
    DrawIndirect    = 1u << 1,
    VertexShader    = 1u << 2,
    FragmentShader  = 1u << 3,
    ComputeShader   = 1u << 4,
    ColorAttachment = 1u << 5,
    Transfer        = 1u << 6,
    BottomOfPipe    = 1u << 7,
    AllCommands     = 1u << 8,
};

inline Rhi_PipelineStage operator|( Rhi_PipelineStage a, Rhi_PipelineStage b ) {
    return static_cast< Rhi_PipelineStage >( static_cast< uint32_t >( a ) | static_cast< uint32_t >( b ) );
}

enum class Rhi_Access : uint32_t {
    None              = 0,
    ShaderRead        = 1u << 0,
    ShaderWrite       = 1u << 1,
    ColorAttachmentRW = 1u << 2,
    DepthStencilRW    = 1u << 3,
    TransferRead      = 1u << 4,
    TransferWrite     = 1u << 5,
};

inline Rhi_Access operator|( Rhi_Access a, Rhi_Access b ) {
    return static_cast< Rhi_Access >( static_cast< uint32_t >( a ) | static_cast< uint32_t >( b ) );
}
