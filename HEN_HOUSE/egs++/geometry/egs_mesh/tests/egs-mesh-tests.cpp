#include "egs_mesh.h"
#include "egs_vector.h"

#include <cassert>

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

int test_isInside() {
    EGS_Vector o(0, 0, 0);
    // single tetrahedron
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

    if (!mesh.isInside(in)) {
        std::cerr << "expected point to be inside\n";
        return 1;
    }
    if (mesh.isInside(out)) {
        std::cerr << "expected point to be outside\n";
        return 1;
    }

    return 0;
}

int main() {
    int num_failed = 0;
    int num_total = 0;
    int err = 0;

    RUN_TEST(test_unknown_node());
    RUN_TEST(test_isInside());

    std::cerr << num_total - num_failed << " out of " << num_total << " tests passed\n";
    return num_failed;
}
