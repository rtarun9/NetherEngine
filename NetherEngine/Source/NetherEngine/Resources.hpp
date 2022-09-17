#pragma once

namespace nether
{
	// Required because of const char *values in the semantic name of the input element desc's, as it is constantly null when Shader object is returned from the Compile function.
	struct InputElementDesc
	{
		std::string semanticName{};
		D3D12_INPUT_ELEMENT_DESC inputElementDesc{};
	};
}