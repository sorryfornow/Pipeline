#include "./pipeline.h"

//static using namespace ppl;

// error handling
ppl::pipeline_error::pipeline_error(pipeline_error_kind kind):kind_(kind){}

auto ppl::pipeline_error::kind() -> pipeline_error_kind{ return this->kind_; }

const char * ppl::pipeline_error::what() const noexcept{
	switch (this->kind_){
		case ppl::pipeline_error_kind::invalid_node_id:
			return "invalid node ID";
		case ppl::pipeline_error_kind::no_such_slot:
			return "no such slot";
		case ppl::pipeline_error_kind::slot_already_used:
			return "slot already used";
		case ppl::pipeline_error_kind::connection_type_mismatch:
			return "connection type mismatch";
	}
	return "unknown error";
}

// 3.6.2
// The pipeline must be default constructible.
// The pipeline should not be copyable (any attempt to do so should be a compile error).
// The pipeline should be movable; after auto p2 = std::move(p1);,
// p2 should manage all the nodes and connections that p1 used to,
// and p1 should be left in a valid (but unspecified) empty state.
// In this state, the pipeline should logically contain 0 nodes.
// You may provide a destructor to clean up if necessary.

ppl::pipeline::pipeline() {
	this->next_id_ = 1;
	this->connections_ = std::unordered_map<ppl::pipeline::node_id, std::vector<ppl::pipeline::node_id>>{};
	this->nodes_ = std::unordered_map<ppl::pipeline::node_id, std::unique_ptr<ppl::node>>{};
	this->sources_ = std::unordered_set<ppl::pipeline::node_id>{};
	this->sinks_ = std::unordered_set<ppl::pipeline::node_id>{};
	this->node_status_ = std::unordered_map<ppl::pipeline::node_id, ppl::poll>{};
}

ppl::pipeline::pipeline(ppl::pipeline&& other) noexcept {
	this->next_id_ = other.next_id_;
	this->connections_ = std::move(other.connections_);
	this->nodes_ = std::move(other.nodes_);
	this->sources_ = std::move(other.sources_);
	this->sinks_ = std::move(other.sinks_);
	this->node_status_ = std::move(other.node_status_);

	other.next_id_ = 1;
	other.connections_ = std::unordered_map<ppl::pipeline::node_id, std::vector<ppl::pipeline::node_id>>{};
	other.nodes_ = std::unordered_map<ppl::pipeline::node_id, std::unique_ptr<ppl::node>>{};
	other.sources_ = std::unordered_set<ppl::pipeline::node_id>{};
	other.sinks_ = std::unordered_set<ppl::pipeline::node_id>{};
	other.node_status_ = std::unordered_map<ppl::pipeline::node_id, ppl::poll>{};
}

auto ppl::pipeline::operator=(ppl::pipeline&& other) noexcept -> pipeline& {
	this->next_id_ = other.next_id_;
	this->connections_ = std::move(other.connections_);
	this->nodes_ = std::move(other.nodes_);
	this->sources_ = std::move(other.sources_);
	this->sinks_ = std::move(other.sinks_);
	this->node_status_ = std::move(other.node_status_);

	other.next_id_ = 1;
	other.connections_ = std::unordered_map<ppl::pipeline::node_id, std::vector<ppl::pipeline::node_id>>{};
	other.nodes_ = std::unordered_map<ppl::pipeline::node_id, std::unique_ptr<ppl::node>>{};
	other.sources_ = std::unordered_set<ppl::pipeline::node_id>{};
	other.sinks_ = std::unordered_set<ppl::pipeline::node_id>{};
	other.node_status_ = std::unordered_map<ppl::pipeline::node_id, ppl::poll>{};

	return *this;
}


// get_dependencies & is_valid non-const & const


// const version




// non-const version
auto ppl::pipeline::is_valid() -> bool {
	for (auto& [id, conn] : this->connections_){
		if (this->sources_.count(id)){
			continue;
		}
		for (auto& con_id : conn){	// traverse all slots
			if (con_id == id){
				return false;
			}
		}
	}

	// All non-sink nodes must have at least one dependent.
	for (auto& [id, conn] : this->connections_){
		if (this->sinks_.count(id)){
			continue;
		}

		auto has_no_dependent = this->get_dependencies(id).empty();
		if (has_no_dependent){
			return false;
		}
	}

	// There is at least 1 source node.
	if (this->sources_.empty()){
		return false;
	}

	// There is at least 1 sink node.
	if (this->sinks_.empty()){
		return false;
	}

	// There are no subpipelines i.e. completely disconnected sections of the dataflow from the main pipeline.
	// BFS
	std::unordered_set<node_id> visited_node{};
	std::queue<node_id> Q_0{};
	for (auto& sNode: this->sinks_){
		visited_node.insert(sNode);
		Q_0.push(sNode);
		break ;
	}

	while (!Q_0.empty()){
		auto Q_size = Q_0.size();
		for (auto i = 0u; i<Q_size; ++i){
			auto n_id = Q_0.front();
			Q_0.pop();
			auto deps = this->connections_.at(n_id);
			for (auto& dep: deps){
				if (visited_node.count(dep) == 0){
					visited_node.insert(dep);
					Q_0.push(dep);
				}
			}
			auto cons = this->get_dependencies(n_id);
			for (auto& con: cons){
				if (visited_node.count(con.first) == 0){
					visited_node.insert(con.first);
					Q_0.push(con.first);
				}
			}
		}

	}

	if (visited_node.size() != this->nodes_.size()){
		return false;
	}
	for (auto& [id, _] : this->nodes_){
		if (visited_node.count(id) == 0){
			return false;
		}
	}

	// BFS from sink to check circle
	std::unordered_set<node_id> visited{};
	std::queue<node_id> Q{};
	for (auto& sNode: this->sinks_){
		visited.insert(sNode);
		Q.push(sNode);
	}

	while (!Q.empty()){
		auto Q_size = Q.size();
		std::unordered_set<node_id> new_visited;
		for (auto i = 0u; i<Q_size; ++i){
			auto n_id = Q.front();
			Q.pop();
			auto deps = this->connections_.at(n_id);
			for (auto& dep: deps){
				if (new_visited.count(dep) == 0){
					new_visited.insert(dep);
					Q.push(dep);
				}
			}
		}
		for(auto& n_id: new_visited){
			if (visited.count(n_id) == 0){
				visited.insert(n_id);
			} else {
				// There are cycles.
				return false;
			}
		}
	}

	return true;
}



//precondition: this->is_valid()
auto ppl::pipeline::step() ->bool {
	if (!this->is_valid()){
		throw std::runtime_error("pipeline is not valid, no call for step()");
	}

	// According to the poll result:
	// If the node is closed, close all nodes that depend on it.
	// If the node has no value, skip all nodes that depend on it.
	// Otherwise, the node has a value,
	// and all nodes that depend on it should be polled,
	// and so on recursively.
	// The tick ends once every node has been either polled, skipped, or closed.
	// ready, empty, closed
	//
	// Returns: true if all sink nodes are now closed,
	// or false otherwise.
	//
	// Notes: you are allowed to (but don't have to)
	// avoid polling a node if all its dependent sink nodes are closed.

	// topological ordering
	std::queue<node_id> Q{};
	std::unordered_set<node_id> settled{};
	std::unordered_set<node_id> skipped{};
	std::unordered_map<node_id,std::size_t> in_degree{};
	std::unordered_map<node_id,poll> node_status{};

	// calculate in_degree
	for (auto& [id, conn] : this->connections_){
		in_degree[id] = conn.size();
		if (conn.empty() && !this->sources_.count(id)){
			throw std::runtime_error("node is not source but has no dependency");
		}
	}

	for (auto& [nid,node_n]: this->nodes_){
		if (in_degree.at(nid) == 0){
			Q.push(nid);
			if (this->sources_.count(nid) == 0){
				throw std::runtime_error("inD==0 but not source node");
			}
		}
		auto cur_node = node_n.get();
		node_status[nid] = cur_node->poll_next();
	}

	while (!Q.empty()){
		auto cur_n_id = Q.front();
		Q.pop();

		if (settled.count(cur_n_id)){
			continue ;
		}
		settled.insert(cur_n_id);

//		auto cur_node = this->nodes_.at(cur_n_id).get();
		auto cur_poll = node_status.at(cur_n_id);

		if (this->sinks_.count(cur_n_id)){
			continue ;
		}
		// probe next_nodes and update in_degree
		if (cur_poll == poll::closed){
			// if any node closed, check if it is sink_node
			auto cons = this->get_dependencies(cur_n_id);
			for (auto& [next_n_id, _]: cons){


				node_status[next_n_id] = poll::closed;

				in_degree.at(next_n_id) -= 1;
				if (in_degree.at(next_n_id) == 0){
					Q.push(next_n_id);
				}

			}
			// if it is source, do nothing
		} else if (cur_poll == poll::empty){
			// skip all node that depend on it
			auto cons = this->get_dependencies(cur_n_id);
			for (auto& [next_n_id, _]: cons){


				if (node_status.at(next_n_id) != poll::closed){
					node_status[next_n_id] = poll::empty;
					skipped.insert(next_n_id);
				}

				in_degree.at(next_n_id) -= 1;
				if (in_degree.at(next_n_id) == 0){
					Q.push(next_n_id);
				}

			}

		} else if (cur_poll == poll::ready){

			auto cons = this->get_dependencies(cur_n_id);
			for (auto& [next_n_id, _]: cons){

//				auto next_node = this->nodes_.at(next_n_id).get();
				if (node_status.at(next_n_id) != poll::closed && skipped.count(next_n_id) == 0){
					node_status[next_n_id] = poll::ready;
				}

				in_degree.at(next_n_id) -= 1;
				if (in_degree.at(next_n_id) == 0){
					Q.push(next_n_id);
				}
			}

		} else {
			throw std::runtime_error("node poll_next() return invalid value");
		}

	}

	for (auto& x: this->sinks_){
		if (node_status.at(x) != poll::closed){
			return false;
		}
	}

	return true;
}

// Preconditions: is_valid() is true.
// Run the pipeline until all sink nodes are closed. Equivalent to while(!step()) {}, but potentially more efficient.
auto ppl::pipeline::run() -> void {
	if (!this->is_valid()){
		throw std::runtime_error("pipeline is not valid, no call for run()");
	}
	while (!this->step()){
		// do something here while override this function
	}
}


// We specify the output using the [Graphviz "DOT" language][http://graphviz.org/documentation/].
// Nodes are strings like "id name", where name is the result of a call to node::name() and id is a unique integer,
// starting at 1, that is incremented for every successful call to create_node for this pipeline.
// (IDs for nodes that are later removed are skipped.) Edges are specified like "1 node" -> "2 node":
// that is, put an arrow -> between the names of two nodes.
// The output should start with the line digraph G {, be followed by a newline-separated sequence of nodes
// (each indented two spaces), then a blank line, then a newline-separated sequence of edges (again, each indented two spaces),
// and finally end with a single } and trailing newline.
// Nodes and edges should be sorted according to ID.
// If a node is connected to another node more than once, two lines should be outputted.
// An example output might look like:
//
// digraph G {
//  "1 hello"
//  "2 world"
//  "4 foobar"
//
//  "2 world" -> "1 hello"
//  "2 world" -> "4 foobar"
//  "4 foobar" -> "1 hello"
//}
//
// You should be able to run this output through
//[the dot tool provided by Graphviz][http://graphviz.org/download/]
// to get a pictorial representation of the pipeline.


// Preconditions: None.
// Print a graphical representation of the pipeline dependency graph to the given output stream, according to the rules above.
std::ostream& ppl::operator<<(std::ostream & os, ppl::pipeline const & p) {
	os << "digraph G {" << std::endl;
	std::set<ppl::pipeline::node_id> nodes_sorted{};
	for (auto& [id, _]: p.nodes_){
		nodes_sorted.insert(id);
	}
	for (auto& id: nodes_sorted){
		os << "  \"" << id << " " << p.nodes_.at(id)->name() << "\"" << std::endl;
	}
	os << std::endl;
	for (auto& id: nodes_sorted){
		auto cons_unsorted = p.get_dependencies(id);
		std::set<ppl::pipeline::node_id> cons{};
		for (auto& [next_id, _]: cons_unsorted){
			cons.insert(next_id);
		}
		for (auto& next_id: cons){
			os << "  \"" << id << " " << p.nodes_.at(id)->name() << "\" -> \"" << next_id << " " << p.nodes_.at(next_id)->name() << "\"" << std::endl;
		}
	}
	os << "}" << std::endl;
	return os;
}
