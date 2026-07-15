#include "Gfx_RenderCamera.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

Gfx_RenderCamera::Gfx_RenderCamera() {
    myPosition = glm::vec3( 10.0f, 0.1f, 16.0f );
    myWorldUp  = glm::vec3( 0.0f, 0.0f, 1.0f );
    myYaw      = 0.0f;
    myPitch    = 0.0f;
    myFov      = 45.0f;
    myNear     = 0.1f;
    myFar      = 256.0f;
    myAspect   = 800.0f / 600.0f;

    LookAt( myPosition, myPosition + glm::vec3( -1.0f, 0.0f, 0.0f ), myWorldUp );
}

Gfx_RenderCamera::~Gfx_RenderCamera() {}

void Gfx_RenderCamera::SetLens( const float aFov, const float aNear, const float aFar, const float anAspect ) {
    myFov    = aFov;
    myNear   = aNear;
    myFar    = aFar;
    myAspect = anAspect;

    UpdateViewProjMatrix();
}

void Gfx_RenderCamera::LookAt( const glm::vec3& anEye, const glm::vec3& aCenter, const glm::vec3& aLookup ) {
    myPosition = anEye;
    myWorldUp  = aLookup;
    SyncOrientationFromLookDirection( glm::normalize( aCenter - anEye ) );
    UpdateViewProjMatrix();
}

void Gfx_RenderCamera::SetAspect( const float anAspect ) {
    myAspect = anAspect;
    UpdateViewProjMatrix();
}

glm::vec3 Gfx_RenderCamera::GetForward() const {
    const float cosPitch = std::cos( myPitch );
    return glm::normalize( glm::vec3( cosPitch * std::sin( myYaw ), cosPitch * std::cos( myYaw ), std::sin( myPitch ) ) );
}

glm::vec3 Gfx_RenderCamera::GetRight() const {
    const glm::vec3 forward = GetForward();
    glm::vec3       right   = glm::cross( forward, myWorldUp );
    if ( glm::dot( right, right ) < 1e-8f )
        right = glm::vec3( 1.0f, 0.0f, 0.0f );
    return glm::normalize( right );
}

void Gfx_RenderCamera::SyncOrientationFromLookDirection( const glm::vec3& aForward ) {
    const glm::vec3 forward = glm::normalize( aForward );
    myPitch                 = std::asin( glm::clamp( forward.z, -1.0f, 1.0f ) );
    myYaw                   = std::atan2( forward.x, forward.y );
}

void Gfx_RenderCamera::UpdateViewProjMatrix() {
    const glm::vec3 forward = GetForward();
    myEye                   = myPosition;
    myCenter                = myPosition + forward;
    myLookUp                = myWorldUp;

    myView = glm::lookAt( myEye, myCenter, myLookUp );
    // CONTRACT: glm::perspective = OpenGL clip Z [-1,1]; shadow uses ZO [0,1] (vulkan-clip-depth.mdc).
    myProj = glm::perspective( glm::radians( myFov ), myAspect, myNear, myFar );
    myProj[ 1 ][ 1 ] *= -1;  // Vulkan NDC: Y points down (GLM perspective is Y-up OpenGL style)
}

void Gfx_RenderCamera::ApplyInput( float aDeltaSeconds, const Util_InputSnapshot& aInput, const Util_CameraSettings& aSettings ) {
    if ( aDeltaSeconds <= 0.0f )
        return;

    if ( aInput.myMouseDeltaX != 0.0f || aInput.myMouseDeltaY != 0.0f ) {
        myYaw += aInput.myMouseDeltaX * aSettings.myMouseSensitivity;
        myPitch -= aInput.myMouseDeltaY * aSettings.myMouseSensitivity;
        myPitch = glm::clamp( myPitch, -MAX_PITCH_RADIANS, MAX_PITCH_RADIANS );
    }

    glm::vec3 moveDir( 0.0f );
    glm::vec3 forwardFlat( GetForward().x, GetForward().y, 0.0f );
    if ( glm::dot( forwardFlat, forwardFlat ) < 1e-8f )
        forwardFlat = glm::vec3( std::sin( myYaw ), std::cos( myYaw ), 0.0f );
    else
        forwardFlat = glm::normalize( forwardFlat );
    const glm::vec3 right = GetRight();

    if ( aInput.myMoveForward )
        moveDir += forwardFlat;
    if ( aInput.myMoveBack )
        moveDir -= forwardFlat;
    if ( aInput.myMoveRight )
        moveDir += right;
    if ( aInput.myMoveLeft )
        moveDir -= right;
    if ( aInput.myMoveUp )
        moveDir += myWorldUp;
    if ( aInput.myMoveDown )
        moveDir -= myWorldUp;

    if ( glm::dot( moveDir, moveDir ) > 0.0f ) {
        myPosition += glm::normalize( moveDir ) * aSettings.myMoveSpeed * aDeltaSeconds;
    }

    UpdateViewProjMatrix();
}
