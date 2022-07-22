#pragma once
#include <glm/glm.hpp>

class Camera {
public:
    Camera();
    ~Camera();
    void SetLens( const float aFov, const float aNear, const float aFar, const float anAspect );
    void LookAt( const glm::vec3& anEye, const glm::vec3& aCenter, const glm::vec3& aLookup );
    void LookAt( const glm::vec3& anEye );
    void SetPosition( const glm::vec3& aCenter );
    void Move (const glm::vec3& aDirection);
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