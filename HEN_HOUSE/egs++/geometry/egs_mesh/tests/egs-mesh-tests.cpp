#include "egs_mesh.h"
#include "egs_vector.h"

#include <algorithm>
#include <cassert>
#include <fstream>

#define RUN_TEST(test_fn) \
    std::cerr << "starting test " << #test_fn << std::endl; \
    err = test_fn; \
    num_total++; \
    if (err) { \
        std::cerr << "test FAILED" << std::endl; \
        num_failed++; \
    } else { \
        std::cerr << "test passed" << std::endl; \
    }

#define EXPECT_ERROR(stmt, err_msg) \
    try { \
        stmt; \
        std::cerr << "expected exception with message: \"" << err_msg << "\"\n"; \
        return 1; \
    } catch (const std::runtime_error& err) { \
        if (err.what() != std::string(err_msg)) { \
            std::cerr << "got error message: \"" \
                << err.what() << "\"\nbut expected: \"" << err_msg << "\"\n"; \
            return 1; \
        } \
    }

// we'll use a simple five-element mesh for smoke testing
constexpr auto test_file = "five-tet.msh";

int test_unknown_node() {
    std::vector<EGS_Mesh::Tetrahedron> elt { EGS_Mesh::Tetrahedron(0, 0, 1, 2, 100) };
    // no node 100 in nodes vector
    std::vector<EGS_Mesh::Node> nodes {
        EGS_Mesh::Node(0, 1.0, 1.0, -1.0),
        EGS_Mesh::Node(1, -1.0, 1.0, -1.0),
        EGS_Mesh::Node(2, 0.0, -1.0, -1.0),
        EGS_Mesh::Node(3, 0.0, 0.0, 1.0)
    };
    std::vector<EGS_Mesh::Medium> media { EGS_Mesh::Medium(1, "") };
    EXPECT_ERROR(EGS_Mesh mesh(elt, nodes, media), "No mesh node with tag: 100");
    return 0;
}

int test_boundary() {
    std::ifstream input(test_file);
    auto mesh = EGS_Mesh::parse_msh_file(input);
    // element 0 is surrounded by the other four elements
    if (mesh.is_boundary() != std::vector<bool>{false, true, true, true, true}) {
        return 1;
    }
    return 0;
}

int test_neighbours() {
    std::ifstream input(test_file);
    auto mesh = EGS_Mesh::parse_msh_file(input);
    // element 0 is neighbours with the other four elements
    auto neighbours = mesh.neighbours();
    auto n0 = neighbours.at(0);
    assert(std::count(n0.begin(), n0.end(), 1) == 1);
    assert(std::count(n0.begin(), n0.end(), 2) == 1);
    assert(std::count(n0.begin(), n0.end(), 3) == 1);
    assert(std::count(n0.begin(), n0.end(), 4) == 1);

    for (auto ns = neighbours.begin() + 1; ns != neighbours.end(); ns++) {
        assert(std::count(ns->begin(), ns->end(), 0) == 1);
        assert(std::count(ns->begin(), ns->end(), -1) == 3);
    }

    return 0;
}

int test_isWhere() {
    EGS_Vector o(0, 0, 0);
    // single tetrahedron
    {
        std::vector<EGS_Mesh::Tetrahedron> elt { EGS_Mesh::Tetrahedron(0, 0, 1, 2, 3) };
        std::vector<EGS_Mesh::Node> nodes {
            EGS_Mesh::Node(0, 1.0, 1.0, -1.0),
            EGS_Mesh::Node(1, -1.0, 1.0, -1.0),
            EGS_Mesh::Node(2, 0.0, -1.0, -1.0),
            EGS_Mesh::Node(3, 0.0, 0.0, 1.0)
        };
        std::vector<EGS_Mesh::Medium> media { EGS_Mesh::Medium(1, "") };
        EGS_Mesh mesh(elt, nodes, media);

        EGS_Vector in(0, 0, 0);
        EGS_Vector out(10.0, 10.0, 10.0);

        if (mesh.isWhere(in) != 0) {
            std::cerr << "expected point to be in tetrahedron 0, got: " << mesh.isWhere(in) << "\n";
            return 1;
        }
        if (mesh.isWhere(out) != -1) {
            std::cerr << "expected point to be outside (-1), got: " << mesh.isWhere(out) << "\n";
            return 1;
        }
        if (!mesh.isInside(in)) {
            std::cerr << "expected point to be inside\n";
            return 1;
        }
        if (mesh.isInside(out)) {
            std::cerr << "expected point to be outside\n";
            return 1;
        }
    }

    // two tetrahedrons
    {
        std::vector<EGS_Mesh::Tetrahedron> elt {
            EGS_Mesh::Tetrahedron(0, 0, 1, 2, 3),
            EGS_Mesh::Tetrahedron(1, 1, 2, 3, 4),
        };
        std::vector<EGS_Mesh::Node> nodes {
            EGS_Mesh::Node(0, 1.0, 1.0, -1.0),
            EGS_Mesh::Node(1, -1.0, 1.0, -1.0),
            EGS_Mesh::Node(2, 0.0, -1.0, -1.0),
            EGS_Mesh::Node(3, 0.0, 0.0, 1.0),
            EGS_Mesh::Node(4, -1.0, -1.0, 1.0)
        };
        std::vector<EGS_Mesh::Medium> media { EGS_Mesh::Medium(1, "") };
        EGS_Mesh mesh(elt, nodes, media);

        EGS_Vector in(-0.5, -0.25, 0);
        EGS_Vector out(10.0, 10.0, 10.0);

        if (mesh.isWhere(in) != 1) {
            std::cerr << "expected point to be in tetrahedron 1, got: " << mesh.isWhere(in) << "\n";
            return 1;
        }
        if (mesh.isWhere(out) != -1) {
            std::cerr << "expected point to be outside (-1), got: " << mesh.isWhere(out) << "\n";
            return 1;
        }
        if (!mesh.isInside(in)) {
            std::cerr << "expected point to be inside\n";
            return 1;
        }
        if (mesh.isInside(out)) {
            std::cerr << "expected point to be outside\n";
            return 1;
        }
    }

    return 0;
}

int test_hownear() {
    return 1;
}

int main() {
    int num_failed = 0;
    int num_total = 0;
    int err = 0;

    RUN_TEST(test_unknown_node());
    RUN_TEST(test_isWhere());
    RUN_TEST(test_boundary());
    RUN_TEST(test_neighbours());
    RUN_TEST(test_hownear());

    std::cerr << num_total - num_failed << " out of " << num_total << " tests passed\n";
    return num_failed;
}
