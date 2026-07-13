#pragma once
#include "../Util/Util_InputSnapshot.h"
#include "Vk_Types.h"
#include <glm/glm.hpp>

// std140 UBO, set 0 / eVk_CameraBinding — current + previous view/proj for temporal reprojection.
struct GpuCameraData {
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
    alignas( 16 ) glm::mat4 prevViewProj;
    alignas( 16 ) glm::mat4 currViewProj;
    alignas( 16 ) glm::vec4 temporalJitterAndFlags;  // xy=jitter(px), z=historyValid(0/1), w=reserved
};

// std140 UBO slice in per-frame instance slab (Set 2 / UNIFORM_BUFFER_DYNAMIC — S1 verify task).
struct GpuObjectData {
    alignas( 16 ) glm::mat4 model;
    alignas( 4 ) uint32_t materialIndex = 0;
    alignas( 4 ) uint32_t _pad0         = 0;
    alignas( 4 ) uint32_t _pad1         = 0;
    alignas( 4 ) uint32_t _pad2         = 0;
};

static_assert( sizeof( GpuObjectData ) == 80, "GpuObjectData must be std140-compatible (80 bytes)" );

// Z-up fly camera: yaw about +Z, pitch about camera right; writes myView/myProj for GpuCameraData.
class Vk_Camera {
public:
    Vk_Camera();
    ~Vk_Camera();
    void SetLens( const float aFov, const float aNear, const float aFar, const float anAspect );
    void LookAt( const glm::vec3& anEye, const glm::vec3& aCenter, const glm::vec3& aLookup );
    void SetAspect( const float anAspect );

    void ApplyInput( float aDeltaSeconds, const Util_InputSnapshot& aInput, const Util_CameraSettings& aSettings );

    glm::vec3 GetEye() const {
        return myPosition;
    }

private:
    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;
    void      SyncOrientationFromLookDirection( const glm::vec3& aForward );
    void      UpdateViewProjMatrix();

public:
    glm::mat4 myView;
    glm::mat4 myProj;

    // Derived each UpdateViewProjMatrix(); read-only for lighting / UBO.
    glm::vec3 myEye;
    glm::vec3 myCenter;
    glm::vec3 myLookUp;
    float     myFov;
    float     myNear;
    float     myFar;
    float     myAspect;

private:
    glm::vec3 myPosition;
    float     myYaw;
    float     myPitch;
    glm::vec3 myWorldUp;

    static constexpr float MAX_PITCH_RADIANS = glm::radians( 89.0f );
};
