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
	// which (higher abstraction level) link is this link contained in?
	// which (lower abstraciton level) links does this link contain?
	class ALinkContainment : public AspectWithRef<Link>
	{
		typedef List<Link*> ChildLinks;

		ALinkContainment(Link*);
		~ALinkContainment();

		void set_parent_link(Link*);
		PROP_GET(parent_link, Link*, m_parent_link);

		void add_child_link(Link*);
		void remove_child_link(Link*);
		Link* get_child_link0() const;
		const ChildLinks& get_child_links() const { return m_child_links; }

		Link* get_parent_link(NetType type) const;
		ChildLinks get_child_links(NetType type) const;
		
	protected:
		void remove_child_link_internal(Link*);
		void add_child_link_internal(Link*);

		ChildLinks m_child_links;
		Link* m_parent_link;
	};
}
