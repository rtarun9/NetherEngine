#include "Pch.hpp"

#include "Camera.hpp"

using namespace DirectX;

namespace nether
{
	void Camera::Update(float deltaTime)
	{
		DirectX::XMVECTOR cameraPosition = DirectX::XMLoadFloat3(&mCameraPosition);
		const DirectX::XMVECTOR cameraFront = DirectX::XMLoadFloat3(&mCameraFront);
		const DirectX::XMVECTOR cameraRight = DirectX::XMLoadFloat3(&mCameraRight);

		const float movementSpeed = deltaTime * mMovementSpeed;
		const float rotationSpeed = deltaTime * mRotationSpeed;

		if (mKeys[utils::EnumClassValue(Keys::W)])
		{
			cameraPosition += cameraFront * movementSpeed;
		}
		else if (mKeys[utils::EnumClassValue(Keys::S)])
		{
			cameraPosition -= cameraFront * movementSpeed;
		}

		if (mKeys[utils::EnumClassValue(Keys::A)])
		{
			cameraPosition -= cameraRight * movementSpeed;
		}
		else if (mKeys[utils::EnumClassValue(Keys::D)])
		{
			cameraPosition += cameraRight * movementSpeed;
		}

		if (mKeys[utils::EnumClassValue(Keys::ArrowUp)])
		{
			mPitch -= mRotationSpeed * rotationSpeed;
		}
		else if (mKeys[utils::EnumClassValue(Keys::ArrowDown)])
		{
			mPitch += mRotationSpeed * rotationSpeed;
		}

		if (mKeys[utils::EnumClassValue(Keys::ArrowLeft)])
		{
			mYaw -= mRotationSpeed * rotationSpeed;
		}
		else if (mKeys[utils::EnumClassValue(Keys::ArrowRight)])
		{
			mYaw += mRotationSpeed * rotationSpeed;
		}

		DirectX::XMStoreFloat3(&mCameraPosition, cameraPosition);
	}

	void Camera::HandleInput(const uint8_t keycode, const bool isKeyDown)
	{
		switch (keycode)
		{
		case 'W':
		{
			mKeys[utils::EnumClassValue(Keys::W)] = isKeyDown;
		}break;

		case 'A':
		{
			mKeys[utils::EnumClassValue(Keys::A)] = isKeyDown;
		}break;

		case 'S':
		{
			mKeys[utils::EnumClassValue(Keys::S)] = isKeyDown;
		}break;

		case 'D':
		{
			mKeys[utils::EnumClassValue(Keys::D)] = isKeyDown;
		}break;

		case VK_UP:
		{
			mKeys[utils::EnumClassValue(Keys::ArrowUp)] = isKeyDown;
		}break;

		case VK_LEFT:
		{
			mKeys[utils::EnumClassValue(Keys::ArrowLeft)] = isKeyDown;
		}break;

		case VK_DOWN:
		{
			mKeys[utils::EnumClassValue(Keys::ArrowDown)] = isKeyDown;
		}break;

		case VK_RIGHT:
		{
			mKeys[utils::EnumClassValue(Keys::ArrowRight)] = isKeyDown;
		}break;
		}
	}

	DirectX::XMMATRIX Camera::GetViewMatrix() 
	{
		DirectX::XMVECTOR cameraPosition = DirectX::XMLoadFloat3(&mCameraPosition);
		DirectX::XMVECTOR cameraFront = DirectX::XMLoadFloat3(&mCameraFront);
		DirectX::XMVECTOR cameraUp = DirectX::XMLoadFloat3(&mCameraUp);
		DirectX::XMVECTOR cameraRight = DirectX::XMLoadFloat3(&mCameraRight);
		DirectX::XMVECTOR cameraTarget = DirectX::XMLoadFloat3(&mCameraTarget);
		
		const DirectX::XMVECTOR worldUp = DirectX::XMLoadFloat3(&WORLD_UP);
		const DirectX::XMVECTOR worldRight = DirectX::XMLoadFloat3(&WORLD_RIGHT);
		const DirectX::XMVECTOR worldFront = DirectX::XMLoadFloat3(&WORLD_FRONT);

		const DirectX::XMMATRIX rotationMatrix =  DirectX::XMMatrixRotationRollPitchYaw(mPitch, mYaw, 0.0f);

		cameraTarget = DirectX::XMVector3TransformCoord(worldFront, rotationMatrix);
		cameraTarget = DirectX::XMVector3Normalize(cameraTarget);

		cameraRight = DirectX::XMVector3TransformCoord(worldRight, rotationMatrix);
		cameraRight = DirectX::XMVector3Normalize(cameraRight);

		cameraFront = DirectX::XMVector3TransformCoord(worldFront, rotationMatrix);
		cameraFront = DirectX::XMVector3Normalize(cameraFront);

		cameraUp = DirectX::XMVector3Cross(cameraFront, cameraRight);

		cameraTarget = cameraPosition + cameraTarget;

		DirectX::XMStoreFloat3(&mCameraFront, cameraFront);
		DirectX::XMStoreFloat3(&mCameraUp, cameraUp);
		DirectX::XMStoreFloat3(&mCameraRight, cameraRight);
		DirectX::XMStoreFloat3(&mCameraTarget, cameraTarget);
		DirectX::XMStoreFloat3(&mCameraPosition, cameraPosition);

		return DirectX::XMMatrixLookAtLH(cameraPosition, cameraTarget, cameraUp);
	}
}