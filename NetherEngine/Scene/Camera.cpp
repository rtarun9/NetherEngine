#include "Pch.hpp"

#include "Camera.hpp"

// For the DX operator overloads.
using namespace DirectX;

namespace nether::scene
{
	void Camera::HandleInput(const uint8_t keycode, const bool isKeyDown)
	{
		switch (keycode)
		{
		case 'W':
		{
			mKeys[EnumClassValue(Keys::W)] = isKeyDown;
		}break;

		case 'A':
		{
			mKeys[EnumClassValue(Keys::A)] = isKeyDown;
		}break;

		case 'S':
		{
			mKeys[EnumClassValue(Keys::S)] = isKeyDown;
		}break;

		case 'D':
		{
			mKeys[EnumClassValue(Keys::D)] = isKeyDown;
		}break;

		case VK_UP:
		{
			mKeys[EnumClassValue(Keys::ArrowUp)] = isKeyDown;
		}break;

		case VK_LEFT:
		{
			mKeys[EnumClassValue(Keys::ArrowLeft)] = isKeyDown;
		}break;

		case VK_DOWN:
		{
			mKeys[EnumClassValue(Keys::ArrowDown)] = isKeyDown;
		}break;

		case VK_RIGHT:
		{
			mKeys[EnumClassValue(Keys::ArrowRight)] = isKeyDown;
		}break;
		}
	}

	void Camera::Update(const float deltaTime)
	{
		const float movementSpeed = deltaTime * mMovementSpeed;
		const float rotationSpeed = deltaTime * mRotationSpeed;

		const DirectX::XMVECTOR cameraRight = DirectX::XMLoadFloat4(&mCameraRight);
		const DirectX::XMVECTOR cameraFront = DirectX::XMLoadFloat4(&mCameraFront);
		
		DirectX::XMVECTOR cameraPosition = DirectX::XMLoadFloat4(&mCameraPosition);
		
		if (mKeys[EnumClassValue(Keys::W)])
		{
			cameraPosition += cameraFront * movementSpeed;
		}
		else if (mKeys[EnumClassValue(Keys::S)])
		{
			cameraPosition -= cameraFront * movementSpeed;
		}

		if (mKeys[EnumClassValue(Keys::A)])
		{
			cameraPosition -= cameraRight * movementSpeed;
		}
		else if (mKeys[EnumClassValue(Keys::D)])
		{
			cameraPosition += cameraRight * movementSpeed;
		}

		if (mKeys[EnumClassValue(Keys::ArrowUp)])
		{
			mPitch -= rotationSpeed;
		}
		else if (mKeys[EnumClassValue(Keys::ArrowDown)])
		{
			mPitch += rotationSpeed;
		}

		if (mKeys[EnumClassValue(Keys::ArrowLeft)])
		{
			mYaw -= rotationSpeed;
		}
		else if (mKeys[EnumClassValue(Keys::ArrowRight)])
		{
			mYaw += rotationSpeed;
		}

		DirectX::XMStoreFloat4(&mCameraPosition, cameraPosition);
	}

	DirectX::XMMATRIX Camera::GetLookAtMatrix() 
	{
		const DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(mPitch, mYaw, 0.0f);
		
		const DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		const DirectX::XMVECTOR worldRight = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		const DirectX::XMVECTOR worldFront = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

		DirectX::XMVECTOR cameraRight = DirectX::XMVector3Normalize(DirectX::XMVector3TransformCoord(worldRight, rotationMatrix));
		DirectX::XMVECTOR cameraFront = DirectX::XMVector3Normalize(DirectX::XMVector3TransformCoord(worldFront, rotationMatrix));
		DirectX::XMVECTOR cameraUp = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cameraFront, cameraRight));

		const DirectX::XMVECTOR cameraPosition = DirectX::XMLoadFloat4(&mCameraPosition);

		const DirectX::XMVECTOR cameraTarget = cameraPosition + cameraFront;

		DirectX::XMStoreFloat4(&mCameraRight, cameraRight);
		DirectX::XMStoreFloat4(&mCameraFront, cameraFront);

		return DirectX::XMMatrixLookAtLH(cameraPosition, cameraTarget, cameraUp);
	}
}
