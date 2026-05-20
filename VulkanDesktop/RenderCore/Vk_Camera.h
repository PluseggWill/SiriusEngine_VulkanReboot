#pragma once
#include "../Util/Util_InputSnapshot.h"
#include "Vk_Types.h"
#include <glm/glm.hpp>

// Keep in mind that Vulkan expects the data to be aligned, see https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap15.html#interfaces-resources-layout for
// more details
struct GpuCameraData {
    alignas( 16 ) glm::mat4 model;
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
};

class Vk_Camera {
public:
    Vk_Camera();
    ~Vk_Camera();
    void SetLens( const float aFov, const float aNear, const float aFar, const float anAspect );
    void LookAt( const glm::vec3& anEye, const glm::vec3& aCenter, const glm::vec3& aLookup );
    void SetAspect( const float anAspect );

    void ApplyInput( float aDeltaSeconds, const Util_InputSnapshot& aInput, const Util_CameraSettings& aSettings );

    glm::vec3 GetEye() const { return myPosition; }

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
