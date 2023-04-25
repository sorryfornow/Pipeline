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


// 3.6.3
template <typename N, typename... Args>
    requires ppl::concrete_node<N> and std::constructible_from<N, Args...>
        auto ppl::pipeline::create_node(Args&&... args) -> node_id {

			auto node_xx = std::make_unique<N>(std::forward<Args>(args)...).get();
	        auto node_x = std::forward<std::unique_ptr<node>>(node_xx);

			auto id_x = this->next_id_++; // increase next_id_ after use the old value

	        // if source node
	        if (node_x->is_source()){
		        this->sources_.insert(id_x);
	        }
	        // if sink node
	        if (node_x->is_sink()){
		        this->sinks_.insert(id_x);
	        }

	        if (this->nodes_.count(id_x) != 0){
		        throw ppl::pipeline_error(ppl::pipeline_error_kind::slot_already_used);
	        }
	        this->nodes_.insert({id_x, std::move(node_x)});
	        // get the tuple size of input_type
//	        auto s = std::tuple_size<decltype(N::input_type)>::value;
	        size_t ss = node_x->get_input_size();
	        // initialize connections_ with size of input_type
	        if (ss == 0){
		        this->connections_[id_x] = std::vector<node_id>{};
	        }
	        else{
		        this->connections_[id_x] = std::vector<node_id>(ss, id_x);
	        }
	        // Note: if connections_[id][x] = id, then the current slot x is not connected to any node

//	        this->connections_[id] = std::vector<node_id>(sizeof...(Args),id);

			return id_x;
		}

void ppl::pipeline::erase_node(ppl::pipeline::node_id n_id) {

	if (this->nodes_.count(n_id) == 0){
//		throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
		return ;
	}

	nodes_[n_id] = nullptr;	// release memory

	// remove all connections
	this->nodes_.erase(n_id);
	this->connections_.erase(n_id);
	for (auto& [id, conn] : this->connections_){
		for (auto& con_id : conn){	// traverse all slots
			if (con_id == n_id){
				con_id = id;	// reset the slot
				// break;
			}
		}
	}
	// remove from sources_ and sinks_
	this->sources_.erase(n_id);
	this->sinks_.erase(n_id);

}

auto ppl::pipeline::get_node(const ppl::pipeline::node_id n_id) const -> ppl::node*{
	if (this->nodes_.count(n_id) == 0){
//		throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
		return nullptr;
	}
	return this->nodes_.at(n_id).get();
}

auto ppl::pipeline::get_node(const ppl::pipeline::node_id n_id) -> ppl::node*{
	if (this->nodes_.count(n_id) == 0){
		//		throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
		return nullptr;
	}
	return this->nodes_.at(n_id).get();
}


// link the output of src_id to the slot-th input of dst_id
void ppl::pipeline::connect(const ppl::pipeline::node_id src_id, const ppl::pipeline::node_id dst_id, const int slot) {
	// if src is sink or dst is source, throw error
	if (this->nodes_.at(src_id)->is_sink() || this->nodes_.at(dst_id)->is_source()){
		throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
	}

	if (this->nodes_.count(src_id) == 0 || this->nodes_.count(dst_id) == 0){
		throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
	}

	auto src_node = get_node(src_id);
	auto dst_node = get_node(dst_id);

	bool slot_is_full = true;
	for (auto& [id, conn] : this->connections_){
		for (auto con_id : conn){	// traverse all slots
			if (con_id == dst_id){
				slot_is_full = false;
				// break;
			}
		}
	}
	if (slot_is_full){
		throw ppl::pipeline_error(ppl::pipeline_error_kind::slot_already_used);
	}
	if (static_cast<std::size_t>(slot) >= this->connections_[dst_id].size()){
		throw ppl::pipeline_error(ppl::pipeline_error_kind::no_such_slot);
	}

	// type mismatching
//	auto src_out = src_node->get_output_type();
//	auto dst_in = dst_node->get_input_type(slot);
//	if (src_out != dst_in){
//		throw ppl::pipeline_error(ppl::pipeline_error_kind::connection_type_mismatch);
//	}

	// node.connect usage need to be done here
	dst_node->connect(src_node, slot);

	this->connections_[dst_id].at(static_cast<std::size_t>(slot)) = src_id;

	// no need to change source/sink status since they are different class to component

}

void ppl::pipeline::disconnect(const ppl::pipeline::node_id src_id, const ppl::pipeline::node_id dst_id) {
	if (this->nodes_.count(src_id) == 0 || this->nodes_.count(dst_id) == 0){
		throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
	}

	int s = -1;
	for (auto slot = 0u; slot < this->connections_[dst_id].size(); ++slot){
		if (this->connections_[dst_id].at(slot) == src_id){
			this->connections_[dst_id].at(slot) = dst_id;	// reset
			s = static_cast<int>(slot);
		}
	}

	if (s == -1){
//		throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
		return ;
	}

//	auto src_node = get_node(src_id);
	auto dst_node = get_node(dst_id);
	dst_node->connect(nullptr, s);

}



// get_dependencies & is_valid non-const & const

// non-const
auto ppl::pipeline::get_dependencies(const ppl::pipeline::node_id src) -> std::vector<std::pair<node_id, int>>  {
	if (this->nodes_.count(src) == 0){
		throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
	}
	std::vector<std::pair<node_id, int>> dependencies{};
	if (this->nodes_.at(src)->is_sink()){
		return dependencies;
	}
	for (const auto& [id, conn] : this->connections_){
		if (id == src){
			continue ;
		}
		for (auto i = 0u; i<conn.size();++i){
			if (conn.at(i) == src){
				dependencies.emplace_back( std::make_pair(id, static_cast<int>(i)) );
			}
		}
	}
	return dependencies;
}

// const version
auto ppl::pipeline::get_dependencies(const ppl::pipeline::node_id src) const -> std::vector<std::pair<node_id, int>>  {
	if (this->nodes_.count(src) == 0){
		throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
	}
	std::vector<std::pair<node_id, int>> dependencies{};
	if (this->nodes_.at(src)->is_sink()){
		return dependencies;
	}
	for (const auto& [id, conn] : this->connections_){
		if (id == src){
			continue ;
		}
		for (auto i = 0u; i<conn.size();++i){
			if (conn.at(i) == src){
				dependencies.emplace_back( std::make_pair(id, static_cast<int>(i)) );
			}
		}
	}
	return dependencies;
}


auto ppl::pipeline::is_valid() const -> bool {
	// All source slots for all nodes must be filled.
	// All non-sink nodes must have at least one dependent.
	// There is at least 1 source node.
	// There is at least 1 sink node.
	// There are no subpipelines i.e. completely disconnected sections of the dataflow from the main pipeline.
	// There are no cycles.
	//
	// Returns: true if all above conditions hold, and false otherwise.

	// All source slots for all nodes must be filled.
	for (auto& [id, conn] : this->connections_){
		if (this->nodes_.at(id)->is_source()){
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
		if (this->nodes_.at(id)->is_sink()){
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


// non-const version
auto ppl::pipeline::is_valid() -> bool {
	for (auto& [id, conn] : this->connections_){
		if (this->nodes_.at(id)->is_source()){
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
		if (this->nodes_.at(id)->is_sink()){
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
		if (conn.empty() && !this->nodes_.at(id)->is_source()){
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
std::ostream & ppl::operator<<(std::ostream & os, ppl::pipeline const & p) {
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
