#pragma once

#include <cstdint>

// Module: Rhi — opaque GPU handles for Gfx (no vulkan.h).
// Backend maps these to Vulkan in RenderCore/Vk_RhiBackend.
// CONTRACT: Device / CommandList are move-only; Destroy nulls the moved-from handle.

struct Rhi_Device {
    void* myImpl = nullptr;

    Rhi_Device()                               = default;
    Rhi_Device( const Rhi_Device& )            = delete;
    Rhi_Device& operator=( const Rhi_Device& ) = delete;

    Rhi_Device( Rhi_Device&& aOther ) noexcept : myImpl( aOther.myImpl ) {
        aOther.myImpl = nullptr;
    }

    Rhi_Device& operator=( Rhi_Device&& aOther ) noexcept {
        if ( this != &aOther ) {
            myImpl        = aOther.myImpl;
            aOther.myImpl = nullptr;
        }
        return *this;
    }

    explicit operator bool() const {
        return myImpl != nullptr;
    }
};

struct Rhi_CommandList {
    void* myImpl = nullptr;

    Rhi_CommandList()                                    = default;
    Rhi_CommandList( const Rhi_CommandList& )            = delete;
    Rhi_CommandList& operator=( const Rhi_CommandList& ) = delete;

    Rhi_CommandList( Rhi_CommandList&& aOther ) noexcept : myImpl( aOther.myImpl ) {
        aOther.myImpl = nullptr;
    }

    Rhi_CommandList& operator=( Rhi_CommandList&& aOther ) noexcept {
        if ( this != &aOther ) {
            myImpl        = aOther.myImpl;
            aOther.myImpl = nullptr;
        }
        return *this;
    }

    explicit operator bool() const {
        return myImpl != nullptr;
    }
};

struct Rhi_Buffer {
    uint64_t myId = 0;

    explicit operator bool() const {
        return myId != 0;
    }
};

struct Rhi_Texture {
    uint64_t myId = 0;

    explicit operator bool() const {
        return myId != 0;
    }
};

struct Rhi_Sampler {
    uint64_t myId = 0;

    explicit operator bool() const {
        return myId != 0;
    }
};

struct Rhi_Pipeline {
    uint64_t myId = 0;

    explicit operator bool() const {
        return myId != 0;
    }
};

struct Rhi_PipelineLayout {
    uint64_t myId = 0;

    explicit operator bool() const {
        return myId != 0;
    }
};

struct Rhi_DescriptorSet {
    uint64_t myId = 0;

    explicit operator bool() const {
        return myId != 0;
    }
};

struct Rhi_ShaderModule {
    uint64_t myId = 0;

    explicit operator bool() const {
        return myId != 0;
    }
};

struct Rhi_RenderPass {
    uint64_t myId = 0;

    explicit operator bool() const {
        return myId != 0;
    }
};

struct Rhi_Framebuffer {
    uint64_t myId = 0;

    explicit operator bool() const {
        return myId != 0;
    }
};
