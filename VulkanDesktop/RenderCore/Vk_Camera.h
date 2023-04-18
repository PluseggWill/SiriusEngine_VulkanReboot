#pragma once
#include "Vk_Types.h"
#include <glm/glm.hpp>

// Keep in mind that Vulkan expects the data to be aligned, see https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap15.html#interfaces-resources-layout for
// more details
struct GpuCameraData {
    alignas( 16 ) glm::mat4 model;
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
};

class Gfx_Camera {
public:
    Gfx_Camera();
    ~Gfx_Camera();
    void SetLens( const float aFov, const float aNear, const float aFar, const float anAspect );
    void LookAt( const glm::vec3& anEye, const glm::vec3& aCenter, const glm::vec3& aLookup );
    void LookAt( const glm::vec3& anEye );
    void SetPosition( const glm::vec3& aCenter );
    void Move( const glm::vec3& aDirection );
    void Rotate( const glm::vec3& aRotate );
    void SetAspect( const float anAspect );

private:
    void UpdateViewProjMatrix();

public:
    glm::mat4 myView;
    glm::mat4 myProj;

    glm::vec3 myEye;
    glm::vec3 myCenter;
    glm::vec3 myLookUp;
    float     myFov;
    float     myNear;
    float     myFar;
    float     myAspect;
};