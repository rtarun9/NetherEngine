#pragma once

#include "Utils.hpp"

namespace nether
{
	enum class Keys : uint8_t
	{
		W,
		A,
		S,
		D,
		ArrowUp,
		ArrowLeft,
		ArrowDown,
		ArrowRight,
		Total,
	};

	class Camera
	{
	public:
		Camera() = default;

		void Update(float deltaTime);
		void HandleInput(const uint8_t keycode, const bool isKeyDown);
		DirectX::XMMATRIX GetViewMatrix();

	private:
		static constexpr DirectX::XMFLOAT3 WORLD_UP{ 0.0f, 1.0f, 0.0f };
		static constexpr DirectX::XMFLOAT3 WORLD_RIGHT{ 1.0f, 0.0f, 0.0f };
		static constexpr DirectX::XMFLOAT3 WORLD_FRONT{ 0.0f, 0.0f, 1.0f };

	public:
		DirectX::XMFLOAT3 mCameraFront{WORLD_FRONT};
		DirectX::XMFLOAT3 mCameraUp{ WORLD_UP };
		DirectX::XMFLOAT3 mCameraRight{ WORLD_RIGHT };

		DirectX::XMFLOAT3 mCameraTarget{};

		DirectX::XMFLOAT3 mCameraPosition{ WORLD_RIGHT };

		float mMovementSpeed{5.0f};
		float mRotationSpeed{ 1.5f };

		float mPitch{};
		float mYaw{};

		std::array<bool, utils::EnumClassValue(Keys::Total)> mKeys{};
	};
}