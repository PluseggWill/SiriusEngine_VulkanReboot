#pragma once

// Init-time render policy snapshot (App / BindEngineConfig → RenderCore). Prefer over live EngineConfig queries.
struct Vk_RenderFeatures {
    bool myHybridDeferred               = false;
    bool myDescriptorLayoutMismatchTest = false;
    bool myValidationEnabled            = false;
};
