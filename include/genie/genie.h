#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <limits>

namespace genie
{
    class Node;

    class APIObject
    {
    protected:
        virtual ~APIObject() = 0;
    };

	class HierObject : virtual public APIObject
	{
	public:
		virtual const std::string& get_name() const = 0;
		virtual std::string get_hier_path(const HierObject* rel_to = nullptr) const = 0;
		virtual HierObject* get_child(const std::string&) const = 0;

	protected:
		~HierObject() = default;
	};

    class Exception : public std::runtime_error
    {
    public:
        Exception(const char* what)
            : std::runtime_error(what) { }
        Exception(const std::string& what)
            : std::runtime_error(what.c_str()) { }
    };

    struct FlowOptions
    {
        bool dump_dot = false;
        std::vector<std::string> dump_dot_networks;

		bool dump_area = false;

		bool dump_reggraph = false;

        bool force_full_merge = false;

		bool no_merge_tree = false;

		bool split_tree = false;

        bool no_topo_opt = false;
        std::vector<std::string> no_topo_opt_systems;

        bool no_mdelay = false;

		unsigned max_logic_depth = 5;
    };

    struct ArchParams
    {
        unsigned lutsize = 6;
        unsigned lutram_width = 20;
        unsigned lutram_depth = 32;
    };

	// Initialize and cleanup library
	void init(FlowOptions* opts = nullptr, ArchParams* arch = nullptr);
	void shutdown();

    // API functions
    Node* create_system(const std::string& name);
    Node* create_module(const std::string& name);
    Node* create_module(const std::string& name, const std::string& hdl_name);

    void do_flow();
}

