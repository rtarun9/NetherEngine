#include "Pch.hpp"

#include "Camera.hpp"

using namespace math;

void nether::Camera::handleInput(const Keys key, const bool isKeyDown) { m_keys[EnumClassValue(key)] = isKeyDown; }

void nether::Camera::update(const float deltaTime)
{
    const float movementSpeed = m_movementSpeed * deltaTime;
    const float rotationSpeed = m_rotationSpeed * deltaTime;

    const math::XMVECTOR cameraForward = math::XMLoadFloat4(&m_cameraForward);
    const math::XMVECTOR cameraRight = math::XMLoadFloat4(&m_cameraRight);
    
    math::XMVECTOR cameraPosition = math::XMLoadFloat4(&m_cameraPosition);

    if (m_keys[EnumClassValue(Keys::W)])
    {
        cameraPosition += cameraForward * movementSpeed;
    }
    else if (m_keys[EnumClassValue(Keys::S)])
    {
        cameraPosition -= cameraForward * movementSpeed;
    }

    if (m_keys[EnumClassValue(Keys::A)])
    {
        cameraPosition -= cameraRight * movementSpeed;
    }
    else if (m_keys[EnumClassValue(Keys::D)])
    {
        cameraPosition += cameraRight * movementSpeed;
    }

    if (m_keys[EnumClassValue(Keys::AUp)])
    {
        m_pitch -= rotationSpeed;
    }
    else if (m_keys[EnumClassValue(Keys::ADown)])
    {
        m_pitch += rotationSpeed;
    }

    if (m_keys[EnumClassValue(Keys::ALeft)])
    {
        m_yaw -= rotationSpeed;
    }
    else if (m_keys[EnumClassValue(Keys::ARight)])
    {
        m_yaw += rotationSpeed;
    }

    math::XMStoreFloat4(&m_cameraPosition, cameraPosition);
}

math::XMMATRIX nether::Camera::getLookAtMatrix()
{
    // Load all XMFLOATX into XMVECTOR's.
    // The target is camera position + camera forward direction (i.e direction it is looking at).

    const DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0.0f);

    const DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const DirectX::XMVECTOR worldRight = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    const DirectX::XMVECTOR worldFront = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

    DirectX::XMVECTOR cameraRight = DirectX::XMVector3Normalize(DirectX::XMVector3TransformCoord(worldRight, rotationMatrix));
    DirectX::XMVECTOR cameraFront = DirectX::XMVector3Normalize(DirectX::XMVector3TransformCoord(worldFront, rotationMatrix));
    DirectX::XMVECTOR cameraUp = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cameraFront, cameraRight));

    const DirectX::XMVECTOR cameraPosition = DirectX::XMLoadFloat4(&m_cameraPosition);

    const DirectX::XMVECTOR cameraTarget = cameraPosition + cameraFront;

    DirectX::XMStoreFloat4(&m_cameraRight, cameraRight);
    DirectX::XMStoreFloat4(&m_cameraForward, cameraFront);

    return DirectX::XMMatrixLookAtLH(cameraPosition, cameraTarget, cameraUp);
}
