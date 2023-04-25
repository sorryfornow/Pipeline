#ifndef COMP6771_PIPELINE_H
#define COMP6771_PIPELINE_H

#include <functional>
#include <memory>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <tuple>
#include <vector>
#include <queue>
#include <string>
#include <utility>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <variant>
#include <optional>
#include <functional>



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

	// A pipeline error.
	// explicit pipeline_error(pipeline_error_kind kind);
	// Constructs an error with the given reason.

	// auto kind() -> pipeline_error_kind;
	// Returns: the kind of error we were constructed from.

	// auto what() -> const char*;
	// Returns: A string depending on the value of kind():

	// invalid_node_id: Return "invalid node ID"
	// no_such_slot: Return "no such slot"
	// slot_already_used: Return "slot already used"
	// connection_type_mismatch: Return "connection type mismatch"

	// Notes:
	// This is a member function inherited from std::exception.
	// The strings do not end in a newline.

	struct pipeline_error : std::exception {
	 private:
		pipeline_error_kind kind_;
	 public:
		explicit pipeline_error(pipeline_error_kind);
		auto kind() -> pipeline_error_kind;
		[[nodiscard]]const char * what() const noexcept override;
	};

	// A node in a pipeline.
	// A node is a type-erased computation.
	// It is the base class for all component<I, O>s, and exposes any common functionality.
	// You will note that some member functions are specified to be private.
	// This is intentional; it encourages encapsulation by only allowing pipelines to modify node states.
	// However, you can still override private virtual functions in derived classes if you want or need to.
	//
	// As a polymorphic base class, node should also have a public virtual destructor.

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
		// Returns: A human-readable name for the node.
		// Notes: This is a pure virtual function, and must be overriden by derived classes.
		virtual auto name() const -> std::string = 0;
		virtual auto name() -> std::string = 0;
		virtual ~node() = default;

	 private:
		// Process a single tick, preparing the next value.
		// Returns: The state of this node; see poll.
		// Notes: This is a pure virtual function, and must be overriden by derived classes.
		virtual auto poll_next() -> poll = 0;
		virtual auto poll_next() const -> poll = 0;
		// Connect source as the input to the given slot.
		// If source is nullptr, signifies that this node should disconnect the existing connection for slot.
		// A later call to connect with a non-null pointer will later fill that slot again.
		// Preconditions: slot is a valid index, and source is either a pointer to a producer of the correct type, or nullptr.
		// Notes: This is a pure virtual function, and must be overriden by derived classes.
		//
		virtual void connect(const node* source, const int slot) = 0;

		// You may add any other virtual functions you feel you may want here.
//		virtual auto get_input_size() const -> int =0;
//		virtual auto get_output_type() const -> std::string = 0;
//		virtual auto get_input_type(int slot) const -> std::string = 0;
		virtual auto is_source() const -> bool =0;
		virtual auto is_sink() const -> bool =0;

		// virtual auto set_poll(poll p) -> void;

		friend class pipeline;
	};

	// producer
	// This is a very simple type, used to allow inspecting a component as a producer of a given type,
	// and retrieve a value from it. Because sink nodes produce no output,
	// you must specialise the producer type for when Output is void.
	// This specialisation should be identical to the normal template,
	// except that the value() function does not exist.

	// auto value() -> const output_type&;
	// Returns: an immutable reference to the node's constructed value.
	// Preconditions: poll_next() has been called and last returned poll::ready.
	// Notes: This is a pure virtual function; it must be overriden by derived classes.

	// requires(!std::is_void_v<Output>)
	template <typename Output>
	struct producer : node {
		using output_type = Output;
		// you must specialise the producer type for when Output is void.
		virtual auto value() const -> const output_type&;	// only when `Output` is not `void`
		// when `Output` is `void`, this function does not exist
		virtual auto value() -> const output_type&;
		// TODO
	};

	template <>
	struct producer<void> : node {
		using output_type = void;
	};


	// A component is a single computation in a pipeline.
	// It is parameterised on the Input that it takes, and the Output that it generates.
	// An Input must be a std::tuple with one type for each slot.
	// For example, a component<std::tuple<int, char>, double> takes one int and one char as input
	// and produces one double as output.
	// The slots are the positionals of the input tuple type: int is slot 0 and char is slot 1.
	// These are accessible as member type aliases input_type and output_type, respectively.
	// It is up to the pipeline to validate that components are correctly connected
	// such that the types of their inputs and outputs match.

	template <typename Input, typename Output>
//	    requires (std::tuple_size_v<Input> >= 0)
			struct component : producer<Output> {
				using input_type = Input;

//		        input_type input_;
//		        auto get_input_size() const -> int override {
//			        return std::tuple_size_v<input_type>;
//		        }
//		        // self-defined
//		        auto get_output_type() const -> std::string override {
//			        return typeid(Output).name();
//		        }
//
//		        auto get_input_type(const int slot) const -> std::string override {
//			        if (slot < 0 || slot >= static_cast<int>(std::tuple_size_v<input_type>)) {
//				        throw pipeline_error(pipeline_error_kind::no_such_slot);
//			        }
//
//			        return typeid(std::get<slot>(input_)).name();
//		        }

		        poll poll_status_ = poll::empty;

		        auto is_source() const -> bool override {
			        return std::tuple_size_v<input_type> == 0;
		        }

		        auto is_sink() const -> bool override {
			        return std::is_void_v<Output>;
		        }

		        auto poll_next() -> poll override {
			        return this->poll_status_;
		        }

		        auto poll_next() const -> poll override {
			        return this->poll_status_;
		        }


		        // Connect source as the input to the given slot.
		        // If source is nullptr, signifies that this node should disconnect the existing connection for slot.
		        // A later call to connect with a non-null pointer will later fill that slot again.
		        // Preconditions: slot is a valid index, and source is either a pointer to a producer of the correct type, or nullptr.

		        auto connect([[maybe_unused]]const node* source, const int slot) -> void override {
			        if (slot < 0 || slot >= static_cast<int>(std::tuple_size_v<input_type>)) {
				        throw pipeline_error(pipeline_error_kind::no_such_slot);
			        }
//			        if (source == nullptr) {
//				        std::get<slot>(input_) = nullptr;
//
//			        } else {
//				        // erase node type and assign it to the type of slot-th input slot
//
//				        using slot_type = std::remove_reference_t<decltype(std::get<slot>(input_))>;
//				        auto src_node_ptr = std::forward<producer<slot_type>*>(source);
//				        std::get<slot>(input_) = src_node_ptr.value();
//			        }
		        }


		        [[nodiscard]]auto name() const -> std::string override {
			        return "node: "+std::to_string(reinterpret_cast<std::uintptr_t>(this));
		        }

		        [[nodiscard]]auto name() -> std::string override {
			        return "node: "+std::to_string(reinterpret_cast<std::uintptr_t>(this));
		        }


	        };

	// sink & source
	//
	// These are helper types to assist users in implementing common component types.
	// A sink consumes values but does not produce any; it is the end of one branch of a pipeline.
	// For simplicity we assume that a sink will only ever have one input slot.
	// A source produces values but does not consume any; it is the start of a pipeline.
	// Since this type has no slots, we should provide a default implementation for connect
	// (to simplify user code and ensure code can compile).
	template <typename Input>
	struct sink : component<std::tuple<Input>, void> {
		using input_type = std::tuple<Input>;
		using output_type = void;

	};

	template <typename Output>
	struct source : component<std::tuple<>, Output> {
		using input_type = std::tuple<>;
		using output_type = Output;
	 private:
		void connect([[maybe_unused]]const node* source, [[maybe_unused]]int slot) override{
		};
	};


	// 3.6.0 concrete_node Concept
	// You are required to write a custom concept that specifies the requirements for a component to be used in a pipeline. A component should:
	//
	// publish the types it consumes through a public member type input_type;
	// have a std::tuple input_type;
	// publish the type it produces through a public member type output_type;
	// be derived from the node type;
	// also be derived from the appropriate producer type; note that the requirement that a component be derived from node is automatically met if this requirement is met.
	// not be an abstract class (i.e., we can construct it).

	// The requirements that a type `N` must satisfy
	// to be used as a component in a pipeline.
	template <typename N>
	// 3.6.0
	concept concrete_node = requires {
		  typename N::input_type;
		  std::tuple_size_v<typename N::input_type> >= 0;
		  typename N::output_type;
		  std::derived_from<N, node>;
		  std::derived_from<N, producer<typename N::output_type>>;
		  // not be an abstract class (i.e., we can construct it).
		  std::constructible_from<N>;
	  };
//	concept concrete_node = std::constructible_from<N>;


	// pipeline
	// This is the main type, implementing most of the functionality of the system.
	// A pipeline connects arbitrary nodes together in a data stream.
	// Each tick, input is created from all sources, flows through intermediary nodes, and ends up at a sink,
	// assuming that every stage succeeded. Any given node is polled at most once every tick.
	// It is possible for a node to not be polled if a parent node polls as closed.
	// The nodes and connections can be dynamically reconfigured, even between individual process ticks,
	// or after running to completion (which may cause the pipeline to no longer be 'completed').
	// Almost all pipeline operations satisfy at least the "strong exception guarantee":
	// if any exception is thrown by any member function, for any reason,
	// it must appear to the caller as if no pipeline state has changed.
	// The only exceptions to this are step() and run(), which only need to satisfy the "weak exception guarantee":
	// the visible state may change on exception, but no resources should be leaked, and any class invariants must continue to hold.
	class pipeline {

	 public:
		// 3.6.1
		// An opaque handle to a node in the pipeline.
		// May be any type of your choice as long as it is "regular"
		// that is, copyable, default-constructible, and equality-comparable.
		// Note: we expect to be able to create a reasonable number of nodes.
		// Your handle should be able to support at least 256 nodes in the pipeline.
		// We refer to a node_id as "invalid" if it is not a valid handle; that is,
		// it cannot be used to refer to a node that is currently in the pipeline.
		using node_id = std::uint16_t;

		// 3.6.2
		// The pipeline must be default constructible.
		// The pipeline should not be copyable (any attempt to do so should be a compile error).
		// The pipeline should be movable; after auto p2 = std::move(p1);,
		// p2 should manage all the nodes and connections that p1 used to,
		// and p1 should be left in a valid (but unspecified) empty state.
		// In this state, the pipeline should logically contain 0 nodes.
		// You may provide a destructor to clean up if necessary.
		pipeline();
		pipeline(const pipeline &) = delete;
		pipeline(pipeline&&) noexcept;
		auto operator=(const pipeline &) -> pipeline& = delete;
		auto operator=(pipeline &&) noexcept -> pipeline&;
		~pipeline() = default;

		// 3.6.3

		// Preconditions: N is a valid node type (see section 3.6.0),
		// and can be constructed from the arguments args.
		// Allocates memory for a new node with concrete type N,
		// constructed from the provided args parameter pack.
		// The new node is initially completely disconnected.
		//
		// Returns: a node_id that can be used as a handle
		// to refer to the newly constructed node.
		template <typename N, typename... Args>
		    requires concrete_node<N> and std::constructible_from<N, Args...>
		             auto create_node(Args&& ...args) -> node_id;

		// Remove the specified node from the pipeline.
		// Disconnects it from any nodes it is currently connected to.
		//
		// Throws: a pipeline_error for an invalid node ID.
		//
		// Notes: n_id is no longer a valid handle after it is erased.
		void erase_node(node_id n_id);

		// Returns: A pointer to the specified node.
		// If n_id is invalid, returns nullptr instead.
		//
		// Notes: You may need more than one overload
		// for proper const-correctness.
		auto get_node(node_id n_id) const -> node*;
		auto get_node(node_id n_id) -> node*;

		// 3.6.4
		// Connect src's output to dst's input for the given slot.
		//
		// Throws: in order, if either handle is invalid,
		// the destination node's slot is already full,
		// the slot number indicated by slot does not exist,
		// the source output type does not match the destination slot's input type,
		// throw the appropriate pipeline_error.
		void connect(node_id src, node_id dst, int slot);

		// Remove all immediate connections between the given two nodes.
		// If the provided nodes are not connected, nothing is done.
		//
		// Throws: a pipeline_error if either handle is invalid.
		void disconnect(node_id src, node_id dst);

		// Returns: A list of all nodes immediately depending on src.
		// Each element is a pair (node, slot), where src's output
		// is connected to the given slot for the node.
		//
		// Throws: A pipeline_error if source is invalid.
		auto get_dependencies(node_id src) const -> std::vector<std::pair<node_id, int>>;
		auto get_dependencies(node_id src) -> std::vector<std::pair<node_id, int>>;

		// 3.6.5
		// Here, a sink node X is a node that produces no output.
		// That is, X::output_type is void.
		// A source node Y is a node that takes no input.
		// That is, Y::input_type is std::tuple<>.

		// Preconditions: None.
		// Validate that this is a sensible pipeline. In particular:
		// All source slots for all nodes must be filled.
		// All non-sink nodes must have at least one dependent.
		// There is at least 1 source node.
		// There is at least 1 sink node.
		// There are no subpipelines i.e. completely disconnected sections of the dataflow from the main pipeline.
		// There are no cycles.
		//
		// Returns: true if all above conditions hold, and false otherwise.
		auto is_valid() const -> bool;
		auto is_valid() -> bool;

		// Preconditions: is_valid() is true.
		// Perform one tick of the pipeline. Initially source nodes shall be polled, and will prepare a value.
		// According to the poll result:
		// If the node is closed, close all nodes that depend on it.
		// If the node has no value, skip all nodes that depend on it.
		// Otherwise, the node has a value,
		// and all nodes that depend on it should be polled,
		// and so on recursively.
		// The tick ends once every node has been either polled, skipped, or closed.
		//
		// Returns: true if all sink nodes are now closed,
		// or false otherwise.
		//
		// Notes: you are allowed to (but don't have to)
		// avoid polling a node if all its dependent sink nodes are closed.
		auto step() -> bool;

		// Preconditions: is_valid() is true.
		// Run the pipeline until all sink nodes are closed. Equivalent to while(!step()) {}, but potentially more efficient.
		void run();

		// 3.6.6
		// We specify the output using the [Graphviz "DOT" language][http://graphviz.org/documentation/].
		// Nodes are strings like "id name", where name is the result of a call to node::name() and id is a unique integer, starting at 1, that is incremented for every successful call to create_node for this pipeline. (IDs for nodes that are later removed are skipped.) Edges are specified like "1 node" -> "2 node": that is, put an arrow -> between the names of two nodes. The output should start with the line digraph G {, be followed by a newline-separated sequence of nodes (each indented two spaces), then a blank line, then a newline-separated sequence of edges (again, each indented two spaces), and finally end with a single } and trailing newline.
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
		friend std::ostream &operator<<(std::ostream &, const pipeline &);

	 private:

		node_id next_id_;
		std::unordered_map<node_id, std::unique_ptr<node>> nodes_;
		std::unordered_map<node_id, std::vector<node_id>> connections_; // vector for slots (its local src_id)
		std::unordered_set<node_id> sources_;
		std::unordered_set<node_id> sinks_;
		std::unordered_map<node_id,poll> node_status_{};

	};


}

#endif  // COMP6771_PIPELINE_H
