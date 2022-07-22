#include "Vk_Camera.h"
//#include <glm/gtx/hash.hpp>
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera() {
    myEye    = glm::vec3( 2.0f, 2.0f, 2.0f );
    myCenter = glm::vec3( 0.0f, 0.0f, 0.0f );
    myLookUp = glm::vec3( 0.0f, 0.0f, 1.0f );
    myFov    = 45.0f;
    myNear   = 0.1f;
    myFar    = 10.0f;
    myAspect = 800 / 600;

    UpdateViewProjMatrix();
}

Camera::~Camera(){}

void Camera::SetLens(const float aFov, const float aNear, const float aFar, const float anAspect) {
    myFov = aFov;
    myNear = aNear;
    myFar = aFar;
    myAspect = anAspect;

    myProj = glm::perspective( glm::radians( myFov ), myAspect, myNear, myFar );
    myProj[ 1 ][ 1 ] *= -1;
}

void Camera::LookAt(const glm::vec3& anEye, const glm::vec3& aCenter, const glm::vec3& aLookup) {
    myEye = anEye;
    myCenter = aCenter;
    myLookUp = aLookup;

    UpdateViewProjMatrix();
}

void Camera::UpdateViewProjMatrix() {
    myView = glm::lookAt( myEye, myCenter, myLookUp );
    myProj = glm::perspective( glm::radians( myFov ), myAspect, myNear, myFar );
    myProj[ 1 ][ 1 ] *= -1;
}

void Camera::LookAt(const glm::vec3& aCenter)
{
    myCenter = aCenter;
    
    UpdateViewProjMatrix();
}

void Camera::SetPosition(const glm::vec3& anEye) {
    glm::vec3 diff = anEye - myEye;
    myView         = glm::translate( myView, diff );

    myEye = anEye;
}

void Camera::Move(const glm::vec3& aDirection) {
    myEye += aDirection;

    myView = glm::translate( myView, aDirection );
}

void Camera::Rotate(const glm::vec3& aRotate) {
    
}

void Camera::SetAspect(const float anAspect) {
    myAspect = anAspect;

    UpdateViewProjMatrix();
}