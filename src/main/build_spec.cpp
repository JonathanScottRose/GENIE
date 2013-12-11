#include <unordered_set>

#include "ct/spec.h"
#include "ct/common.h"
#include "tinyxml/tinyxml2.h"
#include "build_spec.h"
#include "globals.h"
#include "io.h"

using namespace tinyxml2;
using namespace ct;
using namespace BuildSpec;

namespace
{
	// Types
	struct AttribDef
	{
		AttribDef(const std::string _name, bool _mandatory)
			: name(_name), mandatory(_mandatory) { }
		std::string name;
		bool mandatory;
	};
	typedef std::vector<AttribDef> RequiredAttribs;
	typedef std::unordered_map<std::string, std::string> Attribs;

	// Variables
	XMLDocument s_xml_doc;
	Spec::Component* s_cur_component = nullptr;
	Spec::Interface* s_cur_interface = nullptr;
	Spec::System* s_cur_system = nullptr;
	Spec::Instance* s_cur_instance = nullptr;
	
	// Forward function declarations
	void xml_check_error(const std::string& prefix);
	Attribs parse_attribs(const RequiredAttribs& req_attribs, const XMLAttribute* input);
	void visit_component(const XMLAttribute* attr);
	void visit_interface(const XMLAttribute* attr);
	void visit_signal(const XMLElement& elem, const XMLAttribute* attr);
	void visit_instance(const XMLAttribute* attr);
	void visit_export(const XMLAttribute* attr);
	void visit_defparam(const XMLElement& elem);
	void visit_linkpoint(const XMLElement& elem, const XMLAttribute* attr);

	// Private function bodies

	Attribs parse_attribs(const RequiredAttribs& req_attribs, const XMLAttribute* input)
	{
		Attribs result;

		// Parse all given attributes
		while (input != nullptr)
		{
			std::string input_name = Util::str_tolower(input->Name());

			// Make sure this attribute is valid
			bool valid = false;
			for (auto& req : req_attribs)
			{
				if (req.name == input_name)
				{
					result[input_name] = input->Value();
					valid = true;
				}
			}

			if (!valid) throw Exception("Invalid attribute: " + input_name);
			
			input = input->Next();
		}

		// Check for missing attributes
		for (auto& i : req_attribs)
		{
			if (i.mandatory && result.count(i.name) == 0)
				throw Exception("Missing attribute: " + i.name);
		}

		return result;
	}

	void xml_check_error(const std::string& prefix)
	{
		if (s_xml_doc.Error())
		{
			std::string msg = prefix + " - ";
			if (s_xml_doc.GetErrorStr1()) msg.append(s_xml_doc.GetErrorStr1());
			if (s_xml_doc.GetErrorStr2()) msg.append(s_xml_doc.GetErrorStr2());
			throw Exception(msg);
		}
	}

	void visit_component(const XMLAttribute* attr)
	{
		if (s_cur_component != nullptr)
			throw Exception("Unexpected <component>");

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("hier", true);
		auto attrs = parse_attribs(req, attr);

		ComponentImpl* impl = new ComponentImpl;
		impl->module_name = attrs["hier"];

		s_cur_component = new Spec::Component(attrs["name"]);
		s_cur_component->set_impl(impl);

		Spec::define_component(s_cur_component);
	}

	void visit_defparam(const XMLAttribute* attr)
	{
		using namespace ct::Spec;

		if (s_cur_instance == nullptr)
			throw Exception("Unexpected <defparam>");

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("value", true);
		auto attrs = parse_attribs(req, attr);

		s_cur_instance->set_param_binding(attrs["name"], attrs["value"]);
	}

	void visit_interface(const XMLAttribute* attr)
	{
		using namespace ct::Spec;

		if (s_cur_component == nullptr)
			throw Exception("Unexpected <interface>");

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("type", true);
		req.emplace_back("clock", false);
		auto attrs = parse_attribs(req, attr);

		s_cur_interface = Interface::factory
		(
			attrs["name"], 
			Interface::type_from_string(attrs["type"]),
			s_cur_component
		);

		if (s_cur_interface->get_type() == Interface::SEND ||
			s_cur_interface->get_type() == Interface::RECV)
		{
			if (attrs.count("clock") == 0)
				throw Exception("Missing clock for interface");

			DataInterface* dif = (DataInterface*)s_cur_interface;
			dif->set_clock(attrs["clock"]);
		}

		s_cur_component->add_interface(s_cur_interface);
	}

	void visit_signal(const XMLElement& elem, const XMLAttribute* attr)
	{
		using namespace ct::Spec;

		if (s_cur_component == nullptr || s_cur_interface == nullptr)
			throw Exception("Unexpected <signal>");

		RequiredAttribs req;
		req.emplace_back("type", true);
		req.emplace_back("width", false);
		req.emplace_back("usertype", false);
		auto attrs = parse_attribs(req, attr);

		Signal::Type stype = Signal::type_from_string(attrs["type"]);
		
		bool has_width = attrs.count("width") > 0;
		bool needs_width = stype == Signal::DATA;
		std::string swidth = "1";

		needs_width |= stype == Signal::HEADER;
		needs_width |= stype == Signal::LINK_ID;
		needs_width |= stype == Signal::LP_ID;
		needs_width |= stype == Signal::CONDUIT_IN;
		needs_width |= stype == Signal::CONDUIT_OUT;
		
		if (needs_width)
		{
			if (!has_width)
				throw Exception("Missing signal width");

			swidth = attrs["width"];
		}
		else if (has_width)
		{
			throw Exception("Unexpected presence of signal width for signal type " + attrs["type"]);
		}

		bool has_usertype = attrs.count("usertype") > 0;
		bool usertype_allowed = s_cur_interface->get_type() == Interface::CONDUIT;
		usertype_allowed |= stype == Signal::DATA;
		usertype_allowed |= stype == Signal::HEADER;

		if (!usertype_allowed && has_usertype)
			throw Exception("Unexpected usertype in signal definition");

		if (std::string(elem.GetText()).empty())
			throw Exception("Missing HDL signal name in signal definition");

		SignalImpl* simpl = new SignalImpl;
		simpl->signal_name = elem.GetText();

		Signal* sig = new Signal(stype, swidth);
		sig->set_impl(simpl);

		if (has_usertype)
			sig->set_subtype(attrs["usertype"]);

		s_cur_interface->add_signal(sig);
	}

	void visit_instance(const XMLAttribute* attr)
	{
		using namespace ct::Spec;

		if (s_cur_system == nullptr)
			throw Exception("<instance> tag found outside of <system> tag");

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("comp", true);
		auto attrs = parse_attribs(req, attr);

		s_cur_instance = new Instance(attrs["name"], attrs["comp"], s_cur_system);
		s_cur_system->add_object(s_cur_instance);
	}

	void visit_link(const XMLAttribute* attr)
	{
		using namespace ct::Spec;

		if (s_cur_system == nullptr)
			throw Exception("<link> tag found outside of <system> tag");

		RequiredAttribs req;
		req.emplace_back("src", false);
		req.emplace_back("dest", false);
		auto attrs = parse_attribs(req, attr);

		const std::string& src = attrs["src"];
		const std::string& dest = attrs["dest"];
		if (src.empty() && dest.empty())
			throw Exception("Link must specify at least src or dest");

		Link* link = new Link(src, dest);
		s_cur_system->add_link(link);
	}

	void visit_export(const XMLAttribute* attr)
	{
		using namespace ct::Spec;

		if (s_cur_system == nullptr)
			throw Exception("<export> tag found outside of <system> tag");

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("type", true);
		auto attrs = parse_attribs(req, attr);

		const std::string& name = attrs["name"];
		Interface::Type iftype = Interface::type_from_string(attrs["type"]);

		Export* ex = new Export(name, iftype, s_cur_system);
		s_cur_system->add_object(ex);
	}

	void visit_system(const XMLAttribute* attr)
	{
		using namespace ct::Spec;

		if (s_cur_system != nullptr)
			throw Exception("<system> tag found nested inside <system> tag");

		RequiredAttribs req;
		req.emplace_back("name", true);
		auto attrs = parse_attribs(req, attr);

		s_cur_system = new System();
		s_cur_system->set_name(attrs["name"]);
		Spec::define_system(s_cur_system);
	}

	void visit_linkpoint(const XMLElement& elem, const XMLAttribute* attr)
	{
		using namespace ct::Spec;

		if (s_cur_component == nullptr || s_cur_interface == nullptr)
			throw Exception("Unexpected <linkpoint>");

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("type", true);
		auto attrs = parse_attribs(req, attr);

		Linkpoint::Type type = Linkpoint::type_from_string(attrs["type"]);
		
		LinkpointImpl* limpl = new LinkpointImpl;
		limpl->encoding = elem.GetText();

		Linkpoint* lp = new Linkpoint(attrs["name"], type, (DataInterface*)s_cur_interface);
		lp->set_impl(limpl);

		s_cur_interface->add_linkpoint(lp);
	}

	// Single instance of XML element visitor
	class : public XMLVisitor
	{
	public:
		bool VisitEnter(const XMLElement& elem, const XMLAttribute* attr)
		{
			std::string e_name = Util::str_tolower(elem.Name());

			if (e_name == "component") visit_component(attr);
			else if (e_name == "interface") visit_interface(attr);
			else if (e_name == "signal") visit_signal(elem, attr);
			else if (e_name == "instance") visit_instance(attr);
			else if (e_name == "link") visit_link(attr);
			else if (e_name == "export") visit_export(attr);
			else if (e_name == "system") visit_system(attr);
			else if (e_name == "defparam") visit_defparam(attr);
			else if (e_name == "linkpoint") visit_linkpoint(elem, attr);
			else throw Exception(("Unknown element: " + e_name).c_str());

			return true;
		}

		bool VisitExit(const XMLElement& elem)
		{
			std::string e_name = Util::str_tolower(elem.Name());

			if (e_name == "component") s_cur_component = nullptr;
			else if (e_name == "interface") s_cur_interface = nullptr;
			else if (e_name == "system") s_cur_system = nullptr;
			else if (e_name == "instance") s_cur_instance = nullptr;

			return true;
		}
	} s_visitor;

	void process_file(const std::string& filename)
	{
		s_xml_doc.LoadFile(filename.c_str());
		xml_check_error("Reading XML file");

		s_xml_doc.Accept(&s_visitor);
	}
}

void BuildSpec::go()
{
	// Go through each .xml file from command line
	for (auto& filename : Globals::inst()->input_files)
	{
		process_file(filename);
	}

	Spec::create_subsystems();

	// Subsystems need an extra ImplAspect to tell them the verilog module name
	for (auto& i : Spec::components())
	{
		Spec::Component* comp = i.second;
		
		auto aspect = (ComponentImpl*)comp->get_impl();
		if (!aspect)
		{
			aspect = new ComponentImpl;
			aspect->module_name = comp->get_name();
			comp->set_impl(aspect);
		}
	}

	// Find any undefined components and look for .xml files
	std::unordered_set<std::string> undef_comps;
	bool check_systems = true;
	while (check_systems)
	{
		for (auto& i : Spec::systems())
		{
			Spec::System* sys = i.second;
			for (auto& obj : sys->objects())
			{
				Spec::Instance* inst = (Spec::Instance*)obj.second;
				if (inst->get_type() != Spec::SysObject::INSTANCE)
					continue;

				auto compname = inst->get_component();
				Spec::Component* comp = Spec::get_component(compname);
				if (!comp)
					undef_comps.insert(compname);
			}
		}

		// Done looking for unspecified components
		check_systems = false;

		for (auto& path : Globals::inst()->component_path)
		{
			for (auto& it = undef_comps.begin(); it != undef_comps.end(); )
			{
				auto comp = *it;

				// See if an .xml file with the component name exists in our path
				std::string fullpath = path + "/" + comp + ".xml";
				if (!Util::fexists(fullpath))
				{
					++it;
					continue;
				}

				// If it does, process it, and just to be safe, because there could be new
				// systems defined in it (and therefore more missing-components), force a recheck
				// from the top of the while loop
				process_file(fullpath);
				undef_comps.erase(it++);
				check_systems = true;
			}
		}
	}

	if (!undef_comps.empty())
	{
		for (auto& comp : undef_comps)
		{
			IO::msg_error("Missing component definition for: " + comp);
		}
		throw Exception("System instantiates undefined components");
	}

	Spec::validate();
}
