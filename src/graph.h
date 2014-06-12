#pragma once

#include <vector>
#include <unordered_map>
#include <limits>
#include <functional>

namespace ct
{
	namespace Graphs
	{
		typedef unsigned int VertexID;
		typedef unsigned int EdgeID;
		template<class T> using VAttr = std::unordered_map<VertexID, T>;
		template<class T> using EAttr = std::unordered_map<EdgeID, T>;
		typedef std::vector<VertexID> VList;
		typedef std::vector<EdgeID> EList;
		typedef std::pair<VertexID, VertexID> VPair;

		const VertexID INVALID_V = std::numeric_limits<VertexID>::max();
		const EdgeID INVALID_E = std::numeric_limits<EdgeID>::max();

		class Graph
		{
		protected:
			struct Vertex
			{
				EList edges;

				void remove_e(EdgeID eid);
			};

			struct Edge
			{
				VertexID v1;
				VertexID v2;

				Edge() : Edge(INVALID_V, INVALID_V) { };
				Edge(VertexID _v1, VertexID _v2) : v1(_v1), v2(_v2) { }
			};

			typedef std::unordered_map<VertexID, Vertex> VContType;
			typedef std::unordered_map<EdgeID, Edge> EContType;
			 
			VContType V;
			EContType E;

		public:
			// Creation of new stuff
			VertexID newv();
			EdgeID newe();
			EdgeID newe(VertexID v1, VertexID v2);
			
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
			
			IterContainer<VContType, VertexID> verts();
			IterContainer<EContType, EdgeID> edges();

			// Deletion
			void delv(VertexID v);
			void dele(EdgeID e);		

			// Vertex properties
			EList& edges(VertexID v);
			const EList& edges(VertexID v) const;
			EList edges(VertexID v1, VertexID v2);
			EdgeID edge(VertexID v1, VertexID v2);
			VList neigh(VertexID v);
			EList dir_edges(VertexID v1, VertexID v2);
			EdgeID dir_edge(VertexID v1, VertexID v2);

			// Edge properties
			VPair verts(EdgeID e);
			VertexID otherv(EdgeID e, VertexID self);
			void set_otherv(EdgeID e, VertexID self, VertexID newdest);
			EdgeID redge(EdgeID e);

			void connect(VertexID v1, VertexID v2, EdgeID e);
			void connect(VertexID v1, EdgeID e);

			void mergev(VertexID src, VertexID dest);
			void dump(const std::string& filename, 
				const std::function<std::string(EdgeID)>& efunc = std::function<std::string(EdgeID)>());

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
		};

		// Algorithm definitions go here for now
		VAttr<VertexID> multi_way_cut(Graph g, EAttr<int>& weights, VList T);
		int min_st_cut(Graph& g, EAttr<int> weights, VertexID s, VertexID t);
	}
}
