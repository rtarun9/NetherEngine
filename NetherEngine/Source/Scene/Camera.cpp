#include "Pch.hpp"

#include "Camera.hpp"

using namespace DirectX;

namespace nether
{
	void Camera::Update(float deltaTime)
	{
		// Load the XMFLOATx to XMVECTOR's.
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
			mPitch -= rotationSpeed;
		}
		else if (mKeys[utils::EnumClassValue(Keys::ArrowDown)])
		{
			mPitch += rotationSpeed;
		}

		if (mKeys[utils::EnumClassValue(Keys::ArrowLeft)])
		{
			mYaw -= rotationSpeed;
		}
		else if (mKeys[utils::EnumClassValue(Keys::ArrowRight)])
		{
			mYaw += rotationSpeed;
		}

		// Store the position back into the member variable. 
		// Pitch and yaw are directly modified, so no need to save the values into the member variables.
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
		// Load member variables into SIMD compatible XMVector's.

		DirectX::XMVECTOR cameraPosition = DirectX::XMLoadFloat3(&mCameraPosition);
		DirectX::XMVECTOR cameraFront = DirectX::XMLoadFloat3(&mCameraFront);
		DirectX::XMVECTOR cameraUp = DirectX::XMLoadFloat3(&mCameraUp);
		DirectX::XMVECTOR cameraRight = DirectX::XMLoadFloat3(&mCameraRight);
		DirectX::XMVECTOR cameraTarget = DirectX::XMLoadFloat3(&mCameraTarget);
		
		const DirectX::XMVECTOR worldUp = DirectX::XMLoadFloat3(&WORLD_UP);
		const DirectX::XMVECTOR worldRight = DirectX::XMLoadFloat3(&WORLD_RIGHT);
		const DirectX::XMVECTOR worldFront = DirectX::XMLoadFloat3(&WORLD_FRONT);

		const DirectX::XMMATRIX rotationMatrix =  DirectX::XMMatrixRotationRollPitchYaw(mPitch, mYaw, 0.0f);

		// Compute the basis vectors for the look at matrix.
		cameraTarget = DirectX::XMVector3Normalize(DirectX::XMVector3TransformCoord(worldFront, rotationMatrix));
		cameraRight = DirectX::XMVector3Normalize(DirectX::XMVector3TransformCoord(worldRight, rotationMatrix));
		cameraFront = DirectX::XMVector3Normalize(DirectX::XMVector3TransformCoord(worldFront, rotationMatrix));

		cameraUp = DirectX::XMVector3Cross(cameraFront, cameraRight);

		// Some clarification why vector subtraction is not used here : cameraPosition is a point, and not a vector while camera target is a vector.
		// Basically finding the vector centered (vector's arent centered but for visualization assume the base of vector is at cameraPosition), and looking / going towards the camera position.
		cameraTarget = cameraPosition + cameraTarget;

		// Store the XMVECTOR's back to XMFlOATX's.
		DirectX::XMStoreFloat3(&mCameraFront, cameraFront);
		DirectX::XMStoreFloat3(&mCameraUp, cameraUp);
		DirectX::XMStoreFloat3(&mCameraRight, cameraRight);
		DirectX::XMStoreFloat3(&mCameraTarget, cameraTarget);
		DirectX::XMStoreFloat3(&mCameraPosition, cameraPosition);

		return DirectX::XMMatrixLookAtLH(cameraPosition, cameraTarget, cameraUp);
	}
}