#pragma once

#include <vector>
#include <unordered_map>
#include <limits>
#include <functional>

namespace genie
{
namespace impl
{
namespace graph
{
	using VertexID = unsigned;
	using EdgeID = unsigned;
	template<class T> using V2Attr = std::unordered_map<VertexID, T>;
	template<class T> using E2Attr = std::unordered_map<EdgeID, T>;
	template<class T> using Attr2V = std::unordered_map<T, VertexID>;
	template<class T> using Attr2E = std::unordered_map<T, EdgeID>;
	using VList = std::vector<VertexID>;
	using EList = std::vector<EdgeID>;
	using VPair = std::pair<VertexID, VertexID>;

	const VertexID INVALID_V = std::numeric_limits<VertexID>::max();
	const EdgeID INVALID_E = std::numeric_limits<EdgeID>::max();

	class Graph
	{
	protected:
		struct Vertex
		{
			EList edges;
		};

		struct Edge
		{
			VertexID v1;
			VertexID v2;

			Edge() : Edge(INVALID_V, INVALID_V) { };
			Edge(VertexID _v1, VertexID _v2) : v1(_v1), v2(_v2) { }
		};

		/*
		template<class ID, class T>
		class FreeListVec
		{
		public:
			T& get(ID id);
			ID add(const T& obj);
			ID add(T&& obj);
			void del(ID id);
		protected:
			std::vector<T> m_vec;
			std::vector<ID> m_freelist;
		};*/

		using VContType = std::unordered_map<VertexID, Vertex>;
		using EContType = std::unordered_map<EdgeID, Edge>;
			 
		VContType V;
		EContType E;

	public:		
		// A class used to facilitate C++11 range for loops. Implements the required begin() and end()
		// methods that return iterators for making this work.
		//
		// This ultimately allows iteration over Vertex IDs or Edge IDs in the graph. Template parameter
		// T is VertexID or EdgeID. The template parameter C is the type of the container that holds
		// Vertex IDs or Edge IDs (VContType or EContType).
		template<class C, class T>
		class IterContainer
		{
		private:
			Graph& g;

		public:
			class iterator : public std::iterator<std::forward_iterator_tag, T>
			{
			private:
				Graph& g;
				typename C::iterator it;
			public:
				iterator(Graph& g, const typename C::iterator& it) : g(g), it(it) {}
				bool operator!= (const iterator&) const;
				iterator& operator++ ();
				const T& operator*() const;
			};

			IterContainer(Graph& g) : g(g) {}
			iterator begin() const;
			iterator end() const;
		};
		
		Graph();
		//Graph(const Graph&) = default;
		Graph& Graph::operator=(const Graph&);

		// These can be passed to a range-based for loop and support begin() and end() methods
		IterContainer<VContType, VertexID> iter_verts;
		IterContainer<EContType, EdgeID> iter_edges;

		// Retrieve all vertices/edges
		VList verts() const;
		EList edges() const;

		// Creation of new vertices and edges
		VertexID newv();
		void newv(VertexID id);
		EdgeID newe();
		EdgeID newe(VertexID v1, VertexID v2);

		// Existence query
		bool hasv(VertexID v) const;
		bool hase(EdgeID e) const;
		bool hase(VertexID v1, VertexID v2) const;

		// Deletion
		void delv(VertexID v);
		void dele(EdgeID e);		

		// Vertex properties
		EList& edges(VertexID v);
		const EList& edges(VertexID v) const;
		EList edges(VertexID v1, VertexID v2) const;
		EdgeID edge(VertexID v1, VertexID v2) const;
		VList neigh(VertexID v) const;
        VList dir_neigh_r(VertexID v) const;
		VList dir_neigh(VertexID v) const;
		EList dir_edges(VertexID v1, VertexID v2) const;
		EList dir_edges(VertexID self) const;
		EdgeID dir_edge(VertexID v1, VertexID v2) const;

		// Edge properties
		VPair verts(EdgeID e) const;
		VertexID src_vert(EdgeID e) const;
		VertexID sink_vert(EdgeID e) const;
		VertexID otherv(EdgeID e, VertexID self) const;
		void set_otherv(EdgeID e, VertexID self, VertexID newdest);
		EdgeID redge(EdgeID e); // reverse edge, if it exists

		// Connect vertices together
		void connect(VertexID v1, VertexID v2, EdgeID e);
		void connect(VertexID v1, EdgeID e);

		// Merge two vertices src and dest leaving only dest, deleting any duplicated edges
		void mergev(VertexID src, VertexID dest);
		// Same thing but collapse a whole set of vertices (merge into first vertex of list)
		void mergev(const VList&);

		// Return a list of all vertices reachable from t, including t itself
		VList connected_verts(VertexID t);

		// Complement the graph (invert edge existence between all vertices)
		void complement();

		// Add new vertices and edges from another graph
		void union_with(const Graph&);

		// Dump the graph to a .dot file, with an optional edge annotation function
		void dump(const std::string& filename,
			const std::function<std::string(VertexID)>& vfunc = nullptr,
			const std::function<std::string(EdgeID)>& efunc = nullptr
		);

	protected:
		friend class IterContainer<VContType, VertexID>;
		friend class IterContainer<EContType, EdgeID>;

		Vertex& getv(VertexID);
		const Vertex& getv(VertexID) const;
		Edge& gete(EdgeID);
		const Edge& gete(EdgeID) const;
		void remove_e(VertexID v, EdgeID e);

	private:
		VertexID m_next_vid = 0;
		EdgeID m_next_eid = 0;
	}; // end Graph class

	// Algorithm declarations go here for now
	V2Attr<VertexID> multi_way_cut(Graph g, const E2Attr<int>& weights, VList T);
	int min_st_cut(Graph& g, E2Attr<int> weights, VertexID s, VertexID t);
	int connected_comp(Graph& g, V2Attr<unsigned>* vcolor, E2Attr<unsigned>* ecolor);
	bool dijkstra(const Graph& g, VertexID src, VertexID dest, 
		const E2Attr<int>* distances, VList* v_out, EList* e_out);
}
}
}