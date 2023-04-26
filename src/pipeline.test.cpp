#include "./pipeline.h"

#include <catch2/catch.hpp>
#include <sstream>

using namespace ppl;

TEST_CASE("pipeline constructor") {
    ppl::pipeline p{};
	ppl::pipeline p2{};
	ppl::pipeline p3(std::move(p));
	p3 = std::move(p2);
	REQUIRE(!p3.is_valid());

}

TEST_CASE("pipeline ostream") {
	ppl::pipeline p{};
	std::ostringstream oss;
	oss << p;
	REQUIRE_FALSE(oss.str().empty());
}

TEST_CASE("node::producer") {
	ppl::producer<int>* slot0 = nullptr;
	ppl::producer<int>* slot1 = nullptr;
	REQUIRE(slot0 == slot1);
}

struct simple_source : ppl::source<int> {
	int current_value = 0;
	simple_source() = default;
	auto name() const -> std::string override {
		return "SimpleSource";
	}
	auto poll_next() -> ppl::poll override {
		return ppl::poll::closed;
	}
	auto value() const -> const int& override {
		return current_value;
	}
};

struct simple_sink : ppl::sink<int> {
	const ppl::producer<int>* slot0 = nullptr;
	simple_sink() = default;
	auto name() const -> std::string override {
		return "SimpleSink";
	}
	void connect(const ppl::node* src, int slot) override {
		if (slot == 0) {
			slot0 = static_cast<const ppl::producer<int>*>(src);
		}
	}
	auto poll_next() -> ppl::poll override {
		return ppl::poll::ready;
	}
};

TEST_CASE("node::sink") {
	ppl::pipeline p{};
	simple_sink sink{};
	REQUIRE(sink.name() == "SimpleSink");

}

TEST_CASE("node::source") {
	ppl::pipeline p{};
	simple_source source{};
	REQUIRE(source.name() == "SimpleSource");
	REQUIRE(source.poll_next() == ppl::poll::closed);
}

TEST_CASE("node.function") {
	ppl::pipeline p{};
	simple_sink sink{};
	REQUIRE(sink.name() == "SimpleSink");
	REQUIRE(sink.poll_next() == ppl::poll::ready);
}

TEST_CASE("ppl::create") {
	ppl::pipeline p{};
	auto n1 = p.create_node<simple_sink>();
	REQUIRE(n1 >0);
	auto n2 = p.create_node<simple_source>();
	REQUIRE(n2 >0);
	REQUIRE(n2 != n1);
}

TEST_CASE("ppl::erase"){
	ppl::pipeline p{};
	auto n1 = p.create_node<simple_sink>();
	REQUIRE(n1 >0);
	p.erase_node(n1);
}

TEST_CASE("ppl::connect && dependancy"){
	ppl::pipeline p{};
	auto n1 = p.create_node<simple_sink>();
	auto n2 = p.create_node<simple_source>();
	p.connect(n2, n1, 0);
	REQUIRE(!p.get_dependencies(n2).empty());
	p.disconnect(n2, n1);
	REQUIRE(p.get_dependencies(n2).empty());
}

TEST_CASE("validation"){
	ppl::pipeline p{};
	auto n1 = p.create_node<simple_sink>();
	auto n2 = p.create_node<simple_source>();
	p.connect(n2, n1, 0);
	REQUIRE(p.is_valid());
	p.disconnect(n2, n1);
	REQUIRE(!p.is_valid());
}

TEST_CASE("pipeline connect and dis connect") {
	ppl::pipeline p{};
	std::ostringstream oss;
	try {
		p.disconnect(1, 2);
	}
	catch (const std::exception& e) {
		oss << e.what();
	}
	REQUIRE(oss.str() == "invalid node ID");
}

TEST_CASE("ostream"){
	ppl::pipeline p{};
	std::ostringstream oss;
	oss << p;
	REQUIRE_FALSE(oss.str().empty());
}














