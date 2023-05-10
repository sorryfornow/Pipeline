#ifndef COMP6771_PIPELINE_H
#define COMP6771_PIPELINE_H


#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <typeindex>
#include <memory>

namespace ppl {
	
	// Errors that may occur in a pipeline.
	enum class pipeline_error_kind {
		// An expired node ID was provided.
		invalid_node_id,
		// Attempting to bind a non-existant slot.
		no_such_slot,
		// Attempting to bind to a slot that is already filled.
		slot_already_used,
		// The output type and input types for a connection don't match.
		connection_type_mismatch,
	};

	struct pipeline_error : std::exception {
	 private:
		pipeline_error_kind kind_;

	 public:
		explicit pipeline_error(pipeline_error_kind);
		auto kind() -> pipeline_error_kind;
		[[nodiscard]] const char* what() const noexcept override;
	};

	template <typename Input, std::size_t... Is>
	[[nodiscard]] auto create_types_array(std::index_sequence<Is...>) {
		return std::array<std::type_index, sizeof...(Is)>{std::type_index(typeid(std::tuple_element_t<Is, Input>))...};
	}

	template <typename Input>
	[[nodiscard]] const auto& input_types() {
		using idx = std::make_index_sequence<std::tuple_size_v<Input>>;
		static const auto input_types_array = create_types_array<Input>(idx{});
		return input_types_array;
	}

	// The result of a poll_next() operation.
	enum class poll {
		// A value is available.
		ready,
		// No value is available this time, but there might be one later.
		empty,
		// No value is available, and there never will be again:
		// every future poll for this node will return `poll::closed` again.
		closed,
	};

	class node {
	 public:
		[[nodiscard]] virtual auto name() const -> std::string = 0;
		virtual ~node() = default;

	 private:
		virtual auto poll_next() -> poll = 0;
		virtual void connect(const node* source, int slot) = 0;

		// You may add any other virtual functions you feel you may want here.
		[[nodiscard]] virtual auto get_output_type() const -> const std::type_index  = 0;
		[[nodiscard]] virtual auto get_input_type(int slot) const -> const std::type_index  = 0;
		[[nodiscard]] virtual auto get_all_input_type_idx() const -> std::vector<std::type_index> = 0;
//		virtual auto is_source() const -> bool =0;
//		virtual auto is_sink() const -> bool =0;

		// virtual auto set_poll(poll p) -> void;

		friend class pipeline;
	};

	// producer

	// requires(!std::is_void_v<Output>)
	template<typename Output>
	struct producer : node {
		using output_type = Output;
		// you must specialise the producer type for when Output is void.
		virtual auto value() const -> const output_type& = 0; // only when `Output` is not `void`
		// when `Output` is `void`, this function does not exist
	};

	template<>
	struct producer<void> : node {
		using output_type = void;
		// only when `Output` is `void`
	};

	template<typename Input, typename Output>
	//	    requires (std::tuple_size_v<Input> >= 0)
	struct component : producer<Output> {
		using input_type = Input;
		using output_type = Output;
		input_type input_;
		poll poll_status_ = poll::empty;

		// Returns: The number of input slots this component has.


		// self-defined
		auto get_output_type() const -> const std::type_index override {
			return std::type_index(typeid(Output));

		}
//
		auto get_input_type(const int slot) const -> const std::type_index override {
			auto& array = input_types<input_type>();
			return array[static_cast<std::size_t>(slot)];
		}

		auto get_all_input_type_idx() const -> std::vector<std::type_index> override {
			const auto& input_types_array = input_types<input_type>();
			return std::vector<std::type_index>(input_types_array.begin(), input_types_array.end());
		}


		// Connect source as the input to the given slot.
		// If source is nullptr, signifies that this node should disconnect the existing connection for slot.
		// A later call to connect with a non-null pointer will later fill that slot again.
		// Preconditions: slot is a valid index, and source is either a pointer to a producer of the correct type, or
		// nullptr.

		auto connect([[maybe_unused]]const node* source, [[maybe_unused]]const int slot) -> void override {
//			if (slot < 0 || slot >= static_cast<int>(std::tuple_size_v<input_type>)) {
//				throw pipeline_error(pipeline_error_kind::no_such_slot);
//			}
//			if (source == nullptr) {
//				// std::get<slot>(input_) = nullptr;
//			} else {
//				// erase node type and assign it to the type of slot-th input slot
//				using slot_type = std::remove_reference_t<decltype(std::get<slot>(input_))>;
//				auto src_node_ptr = std::forward<producer<slot_type>*>(source);
//				std::get<slot>(input_) = src_node_ptr.value();
//			}
		}

		[[nodiscard]]auto name() const -> std::string override {
			return "node: "+std::to_string(reinterpret_cast<std::uintptr_t>(this));
		}
	};

	// sink & source
	//
	template<typename Input>
	struct sink : component<std::tuple<Input>, void> {
		//		using input_type = std::tuple<Input>;
		//		using output_type = void;
	};

	template<typename Output>
	struct source : component<std::tuple<>, Output> {
		using input_type = std::tuple<>;
		using output_type = Output;

	 private:
		void connect([[maybe_unused]] const node* source, [[maybe_unused]] int slot) override{
		    // do nothing
		};
	};

	// 3.6.0 concrete_node Concept

	// The requirements that a type `N` must satisfy
	// to be used as a component in a pipeline.
	template<typename N>
	// 3.6.0
	concept concrete_node = requires {
		                        typename N::input_type;
		                        requires std::tuple_size_v<typename N::input_type> >= 0;
		                        typename N::output_type;
		                        requires std::derived_from<N, node>;
		                        requires std::derived_from<N, producer<typename N::output_type>>;
		                        // requires !std::is_abstract_v<N>;
	                        };
	//	concept concrete_node = std::constructible_from<N>;




	class pipeline {
	 public:
		// 3.6.1
		using node_id = std::uint16_t;

		// 3.6.2
		pipeline();
		pipeline(const pipeline&) = delete;
		pipeline(pipeline&&) noexcept;
		auto operator=(const pipeline&) -> pipeline& = delete;
		auto operator=(pipeline&&) noexcept -> pipeline&;
		~pipeline() = default;

		// 3.6.3
		template<typename N, typename... Args>
		    requires concrete_node<N>
		//		             and std::constructible_from<N, Args...>
		auto create_node(Args&&... args) -> node_id {
			using input_type = typename N::input_type;
//			using output_type = typename N::output_type;

			// create a new node
			auto node_x = std::make_unique<N>(std::forward<Args>(args)...);

			auto id_x = this->next_id_++; // increase next_id_ after use the old value

			if (this->nodes_.count(id_x) != 0) {
				throw ppl::pipeline_error(ppl::pipeline_error_kind::slot_already_used);
			}
			this->nodes_.insert({id_x, std::move(node_x)});

			// if source node
			if (std::is_same_v<input_type, std::tuple<>>) {
				this->sources_.insert(id_x);

			}

			// if sink node
			if (std::is_same_v<typename N::output_type, void>) {
				this->sinks_.insert(id_x);

			}

			// get the tuple size of input_type
//			auto x = node_x.get().get_all_input_type_idx();
			auto input_type_id = input_types<input_type>();
			std::vector<std::type_index> input_id_vec (input_type_id.begin(),input_type_id.end());
			auto s = input_id_vec.size();

			// initialize connections_ with size of input_type
			if (s == 0) {
				this->connections_[id_x] = std::vector<node_id>{};
				this->sources_.insert(id_x);
			}
			else {
				this->connections_[id_x] = std::vector<node_id>(s, id_x);
			}
			//	 Note: if connections_[id][x] = id, then the current slot x is not connected to any node

			return id_x;
		}

		void erase_node(node_id n_id){
			if (this->nodes_.count(n_id) == 0){
				throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
//				return ;
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

		[[nodiscard]]auto get_node(node_id n_id) const -> node*{
			if (this->nodes_.count(n_id) == 0){
				throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
				// return nullptr;
			}
			return this->nodes_.at(n_id).get();
		};
		[[nodiscard]] auto get_node(node_id n_id) -> node*{
			if (this->nodes_.count(n_id) == 0){
				throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
				// return nullptr;
			}
			return this->nodes_.at(n_id).get();
		}

		// 3.6.4
		void connect(const node_id src_id, const node_id dst_id, const int slot){
			// if src is sink or dst is source, throw error
			if (this->sinks_.count(src_id) || this->sources_.count(dst_id)){
				throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
			}

			if (this->nodes_.count(src_id) == 0 || this->nodes_.count(dst_id) == 0){
				throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
			}

			auto src_node = get_node(src_id);
			auto dst_node = get_node(dst_id);


			if (this->connections_[dst_id][static_cast<std::size_t>(slot)] != dst_id){
				throw ppl::pipeline_error(ppl::pipeline_error_kind::slot_already_used);
			}
			if (static_cast<std::size_t>(slot) >= this->connections_[dst_id].size()){
				throw ppl::pipeline_error(ppl::pipeline_error_kind::no_such_slot);
			}

			// type mismatching
			auto src_out = src_node->get_output_type();
			auto dst_in = dst_node->get_input_type(slot);
			if (src_out != dst_in){
				throw ppl::pipeline_error(ppl::pipeline_error_kind::connection_type_mismatch);
			}

			// node.connect usage need to be done here
			dst_node->connect(src_node, slot);

			this->connections_[dst_id].at(static_cast<std::size_t>(slot)) = src_id;

			// no need to change source/sink status since they are different class to component

		}

		void disconnect(const node_id src_id, const node_id dst_id){
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

		auto get_dependencies(node_id src) const -> std::vector<std::pair<node_id, int>>{
			if (this->nodes_.count(src) == 0){
				throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
			}
			std::vector<std::pair<node_id, int>> dependencies{};
			if (this->sinks_.count(src)){
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

		// 3.6.5
		auto is_valid() -> bool;

		// Preconditions: is_valid() is true.

		auto step() -> bool;

		// Preconditions: is_valid() is true.
		// Run the pipeline until all sink nodes are closed. Equivalent to while(!step()) {}, but potentially more
		// efficient.
		void run();

		// 3.6.6
		// We specify the output using the [Graphviz "DOT" language][http://graphviz.org/documentation/].
		// Nodes are strings like "id name", where name is the result of a call to node::name() and id is a unique
		// integer, starting at 1, that is incremented for every successful call to create_node for this pipeline. (IDs
		// for nodes that are later removed are skipped.) Edges are specified like "1 node" -> "2 node": that is, put an
		// arrow -> between the names of two nodes. The output should start with the line digraph G {, be followed by a
		// newline-separated sequence of nodes (each indented two spaces), then a blank line, then a newline-separated
		// sequence of edges (again, each indented two spaces), and finally end with a single } and trailing newline.
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
		// Print a graphical representation of the pipeline dependency graph to the given output stream, according to
		// the rules above.
		friend std::ostream& operator<<(std::ostream&, const pipeline&);

	 private:
		node_id next_id_;
		std::unordered_map<node_id, std::unique_ptr<node>> nodes_;
		std::unordered_map<node_id, std::vector<node_id>> connections_; // vector for slots (its local src_id)
		std::unordered_set<node_id> sources_;
		std::unordered_set<node_id> sinks_;
		std::unordered_map<node_id, poll> node_status_{};
	};

	std::ostream& operator<<(std::ostream& os, const pipeline& p);
} // namespace ppl

#endif // COMP6771_PIPELINE_H
