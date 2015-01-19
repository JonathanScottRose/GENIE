#pragma once

#include "genie/common.h"
#include "genie/networks.h"

namespace genie
{
	// Defined elsewhere
	class HierObject;

	// Defined here
	class Link;
	class AEndpoint;
	class ALinkContainment;
	

	// Allows connections to be made of a certain network type
	class AEndpoint : public AspectWithRef<HierObject>
	{
	public:
		typedef std::vector<HierObject*> Objects;
		typedef std::vector<AEndpoint*> Endpoints;
		typedef std::vector<Link*> Links;

		AEndpoint(Dir dir, HierObject* container);
		virtual ~AEndpoint() = 0;
		
		virtual NetType get_type() const = 0;
		PROP_GET(dir, Dir, m_dir);

		virtual void add_link(Link*);
		virtual void remove_link(Link*);
		virtual void remove_all_links();
		virtual bool has_link(Link*) const;
		virtual const Links& links() const;
		virtual Link* get_link0() const;

		virtual AEndpoint* get_remote0() const;
		virtual Endpoints get_remotes() const;
		virtual HierObject* get_remote_obj0() const;
		virtual Objects get_remote_objs() const;
		
		virtual bool is_connected() const;
	
	protected:
		Dir m_dir;
		Links m_links;

		NetTypeDef* get_net_def() const;
	};

	// A connection for a particular network type.
	class Link : public Object
	{
	public:
		Link();
		Link(AEndpoint* src, AEndpoint* sink);
		virtual ~Link();
		
		AEndpoint* get_src() const;
		AEndpoint* get_sink() const;
		AEndpoint* get_other(const AEndpoint*) const;

		void set_src(AEndpoint*);
		void set_sink(AEndpoint*);

		NetType get_type() const;

	protected:
		AEndpoint* m_src;
		AEndpoint* m_sink;
	};

	// An aspect that can be attached to links which says:
	// which (higher abstraction level) links does this link implement?
	// which (lower abstraciton level) links does this link contain?
	class ALinkContainment : public AspectWithRef<Link>
	{
	public:
		typedef List<Link*> Links;

		ALinkContainment(Link*);
		~ALinkContainment();

		void add_parent_link(Link* other) { add_link(other, PARENT); }
		void remove_parent_link(Link* other) { remove_link(other, PARENT); }
		Link* get_parent_link0() const { return get_link0(PARENT); }
		const Links& get_parent_links() const { return m_links[PARENT]; }
		bool has_parent_links() const { return !m_links[PARENT].empty(); }
		Links get_all_parent_links(NetType type) const { return get_all_links(type, PARENT); }

		void add_child_link(Link* other) { add_link(other, CHILD); }
		void remove_child_link(Link* other) { remove_link(other, CHILD); }
		Link* get_child_link0() const { return get_link0(CHILD); }
		const Links& get_child_links() const { return m_links[CHILD]; }
		bool has_child_links() const { return !m_links[CHILD].empty(); }
		Links get_all_child_links(NetType type) const { return get_all_links(type, CHILD); }
				
	protected:
		enum PorC
		{
			PARENT = 0,
			CHILD = 1
		};

		PorC rev_rel(PorC porc) { return (PorC)(1 - porc); }

		void add_link(Link*, PorC);
		void remove_link(Link*, PorC);
		Link* get_link0(PorC) const;
		const Links& get_links(PorC porc) const { return m_links[porc]; }
		bool has_links(PorC porc) const { return !m_links[porc].empty(); }
		Links get_all_links(NetType, PorC) const;

		void remove_link_internal(Link*, PorC);
		void add_link_internal(Link*, PorC);

		Links m_links[2];
	};
}
