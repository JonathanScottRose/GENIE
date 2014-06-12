#pragma once

namespace ct
{
	namespace P2P { class System; }
	namespace Spec { class System; }

	P2P::System* build_system(Spec::System* system);
}