#pragma once

namespace ct
{
	namespace P2P { class System; }
	namespace Spec { class System; }

	struct CTOpts
	{
		bool register_merge;
	};

	P2P::System* build_system(Spec::System* system);
	CTOpts& get_opts();
}