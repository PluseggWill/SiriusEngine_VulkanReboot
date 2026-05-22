#include "Vk_PipelineDiagnostics.h"

#include "../Util/Util_Logger.h"

#include <sstream>
#include <string>

namespace {

std::string ShaderStageLabel( VkShaderStageFlagBits aStage ) {
    switch ( aStage ) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return "vertex";
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return "fragment";
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            return "geometry";
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return "compute";
        default:
            return "stage(" + std::to_string( static_cast< uint32_t >( aStage ) ) + ")";
    }
}

std::string TopologyLabel( VkPrimitiveTopology aTopology ) {
    switch ( aTopology ) {
        case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
            return "POINT_LIST";
        case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
            return "LINE_LIST";
        case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
            return "LINE_STRIP";
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
            return "TRIANGLE_LIST";
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
            return "TRIANGLE_STRIP";
        default:
            return "topology(" + std::to_string( static_cast< uint32_t >( aTopology ) ) + ")";
    }
}

std::string PolygonModeLabel( VkPolygonMode aMode ) {
    switch ( aMode ) {
        case VK_POLYGON_MODE_FILL:
            return "FILL";
        case VK_POLYGON_MODE_LINE:
            return "LINE";
        case VK_POLYGON_MODE_POINT:
            return "POINT";
        default:
            return "polygon(" + std::to_string( static_cast< uint32_t >( aMode ) ) + ")";
    }
}

std::string SampleCountLabel( VkSampleCountFlagBits aSamples ) {
    switch ( aSamples ) {
        case VK_SAMPLE_COUNT_1_BIT:
            return "VK_SAMPLE_COUNT_1_BIT";
        case VK_SAMPLE_COUNT_2_BIT:
            return "VK_SAMPLE_COUNT_2_BIT";
        case VK_SAMPLE_COUNT_4_BIT:
            return "VK_SAMPLE_COUNT_4_BIT";
        case VK_SAMPLE_COUNT_8_BIT:
            return "VK_SAMPLE_COUNT_8_BIT";
        default:
            return "samples(" + std::to_string( static_cast< uint32_t >( aSamples ) ) + ")";
    }
}

std::string FormatLabel( VkFormat aFormat ) {
    if ( aFormat == VK_FORMAT_UNDEFINED ) {
        return "undefined";
    }
    std::ostringstream out;
    out << "format(" << static_cast< uint32_t >( aFormat ) << ")";
    return out.str();
}

}  // namespace

void VkPipelineDiagnostics::LogGraphicsPipelineSummary( const Vk_GraphicsPipelineBuildInfo& aInfo, const Vk_PipelineBuilder& aBuilder,
                                                        VkRenderPass aRenderPass ) {
    UtilLogger::Info( "PIPELINE", std::string( "--- " ) + aInfo.myLabel + " pipeline summary ---" );

    if ( aInfo.myVertShaderPath != nullptr ) {
        UtilLogger::Info( "PIPELINE", std::string( "vertSpv=" ) + aInfo.myVertShaderPath );
    }
    if ( aInfo.myFragShaderPath != nullptr ) {
        UtilLogger::Info( "PIPELINE", std::string( "fragSpv=" ) + aInfo.myFragShaderPath );
    }

    for ( const VkPipelineShaderStageCreateInfo& stage : aBuilder.myShaderStages ) {
        const char* entry = ( stage.pName != nullptr ) ? stage.pName : "(null)";
        UtilLogger::Info( "PIPELINE", "stage " + ShaderStageLabel( stage.stage ) + " entry=" + entry );
    }

    UtilLogger::Info( "PIPELINE", "layout setCount=" + std::to_string( aInfo.myPipelineLayoutSetCount ) + " pushConstantRanges="
                                   + std::to_string( aInfo.myPipelineLayoutPushCount ) );
    UtilLogger::Info( "PIPELINE", "renderPass=" + std::to_string( reinterpret_cast< uintptr_t >( aRenderPass ) ) + " subpass=0" );

    const uint32_t width  = aBuilder.myScissor.extent.width;
    const uint32_t height = aBuilder.myScissor.extent.height;
    UtilLogger::Info( "PIPELINE", "viewport=" + std::to_string( width ) + "x" + std::to_string( height ) );

    UtilLogger::Info( "PIPELINE", "colorAttachment " + FormatLabel( aInfo.myColorFormat ) + " depth " + FormatLabel( aInfo.myDepthFormat ) );
    UtilLogger::Info( "PIPELINE", "msaa=" + SampleCountLabel( aBuilder.myMultisampling.rasterizationSamples ) );
    UtilLogger::Info( "PIPELINE", "topology=" + TopologyLabel( aBuilder.myInputAssembly.topology ) + " polygonMode="
                                   + PolygonModeLabel( aBuilder.myRasterizer.polygonMode ) + " cullMode="
                                   + std::to_string( static_cast< uint32_t >( aBuilder.myRasterizer.cullMode ) ) );

    UtilLogger::Info( "PIPELINE", "depthTest=" + std::to_string( aBuilder.myDepthStencil.depthTestEnable ) + " depthWrite="
                                   + std::to_string( aBuilder.myDepthStencil.depthWriteEnable ) + " compareOp="
                                   + std::to_string( static_cast< uint32_t >( aBuilder.myDepthStencil.depthCompareOp ) ) );

    UtilLogger::Info( "PIPELINE", "vertexBindings=" + std::to_string( aBuilder.myVertexInputInfo.vertexBindingDescriptionCount ) + " vertexAttributes="
                                   + std::to_string( aBuilder.myVertexInputInfo.vertexAttributeDescriptionCount ) );

    UtilLogger::Info( "PIPELINE", "colorBlendEnable=" + std::to_string( aBuilder.myColorBlendAttachment.blendEnable ) + " dynamicStateInBuilder=disabled" );
}
