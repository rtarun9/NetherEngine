#pragma once

namespace nether::scene
{
	enum class Keys : uint8_t
	{
		W, A, S, D,
		ArrowUp, ArrowLeft, ArrowDown, ArrowRight,
		TotalKeys
	};

	// Scene camera.
	class Camera
	{
	public:
		// Not defining any of the special function's (constructor / destructor, etc).
		void HandleInput(const uint8_t keycode, const bool isKeyDown);

		void Update(const float deltaTime);

		DirectX::XMMATRIX GetLookAtMatrix();

	private:
		DirectX::XMFLOAT4 mCameraPosition{0.0f, 0.0f, -10.0f, 1.0f};
		DirectX::XMFLOAT4 mCameraRight{1.0f, 0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT4 mCameraFront{0.0f, 0.0f, 1.0f, 0.0f};

		float mPitch{};
		float mYaw{};

		float mMovementSpeed{50.0f};
		float mRotationSpeed{2.0f};

		std::array<bool, EnumClassValue(Keys::TotalKeys)> mKeys{false};
	};
}


