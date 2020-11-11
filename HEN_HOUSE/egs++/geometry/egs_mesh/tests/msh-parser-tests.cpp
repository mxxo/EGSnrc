#include "msh_parser.h"
#include <cassert>

// exception test macros adapted from Arthur O'Dwyer's comment here:
//   https://github.com/google/googletest/issues/952#issuecomment-521361666

#define EXPECT_NO_ERROR(stmt) \
    try { \
        stmt; \
    } catch (const std::runtime_error& err) { \
        std::cerr << "got error message: \"" << err.what() << "\"\n"; \
        return 1; \
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

using namespace msh_parser::internal;

int test_parse_msh_version() {
    // catch empty inputs
    {
        std::istringstream input("");
        EXPECT_ERROR(parse_msh_version(input), "unexpected end of input");
    }
    // bad format header
    {
        std::istringstream input("$MshFmt\n");
        EXPECT_ERROR(parse_msh_version(input), "expected $MeshFormat, got $MshFmt");
    }
    // bad msh version line
    {
         std::istringstream input(
            "$MeshFormat\n"
            "0\n"
            "$EndMeshFormat\n"
        );
        EXPECT_ERROR(parse_msh_version(input), "failed to parse msh version");
    }
    // unknown msh version
    {
        std::istringstream input(
            "$MeshFormat\n"
            "100.2 0 8\n"
            "$EndMeshFormat\n"
        );
        EXPECT_ERROR(parse_msh_version(input), "unsupported msh version `100.2`, the only supported version is 4.1");
    }
    // binary files are unsupported
    {
        std::istringstream input(
            "$MeshFormat\n"
            "4.1 1 8\n"
            "$EndMeshFormat\n"
        );
        EXPECT_ERROR(parse_msh_version(input), "binary msh files are unsupported, please convert this file to ascii and try again");
    }
    // size_t != 8 is unsupported
    {
        std::istringstream input(
            "$MeshFormat\n"
            "4.1 0 4\n"
            "$EndMeshFormat\n"
        );
        EXPECT_ERROR(parse_msh_version(input), "msh file size_t must be 8");
    }
    // eof after version line fails
    {
        std::istringstream input(
            "$MeshFormat\n"
            "4.1 0 8\n"
        );
        EXPECT_ERROR(parse_msh_version(input), "expected $EndMeshFormat, got ``");
    }
    // parse msh v4.1 successfully
    {
        std::istringstream input(
            "$MeshFormat\n"
            "4.1 0 8\n"
            "$EndMeshFormat\n"
        );
        MshVersion vers;
        EXPECT_NO_ERROR(vers = parse_msh_version(input));
        assert(vers == MshVersion::v41);
    }
    // windows line-endings are OK
    {
        std::istringstream input(
            "$MeshFormat\r\n"
            "4.1 0 8\r\n"
            "$EndMeshFormat\r\n"
        );
        MshVersion vers;
        EXPECT_NO_ERROR(vers = parse_msh_version(input));
        assert(vers == MshVersion::v41);
    }
    return 0;
}

// all test cases assume $PhysicalNames header has already been parsed
int test_parse_msh41_groups() {
    // empty section is OK
    {
        std::istringstream input(
            "0\n"
            "$EndPhysicalNames\n"
        );
        std::vector<msh41::PhysicalGroup> groups;
        EXPECT_NO_ERROR(groups = msh41::parse_groups(input));
        assert(groups.size() == 0);
    }
    // missing $EndPhysicalNames tag fails
    {
        std::istringstream input(
            "3\n"
            "1 1 \"a line\"\n"
            "2 2 \"a surface\"\n"
            "3 3 \"a volume\"\n"
        );
        EXPECT_ERROR(msh41::parse_groups(input),
            "unexpected end of file, expected $EndPhysicalNames");
    }
    // bad physical group line fails
    {
        std::istringstream input(
            "1\n"
            "1 \"a line\"\n" // missing tag
            "$EndPhysicalNames\n"
        );
        EXPECT_ERROR(msh41::parse_groups(input),
            "physical group parsing failed: 1 \"a line\"");
    }
    // catch invalid physical group names
    {
        std::istringstream input(
            "1\n"
            "3 1 \"\"\n"
            "$EndPhysicalNames\n"
        );
        EXPECT_ERROR(msh41::parse_groups(input),
            "empty physical group name: 3 1 \"\"");
    }
    // physical group names are quoted
    {
        std::istringstream input(
            "1\n"
            "3 1 Steel\n"
            "$EndPhysicalNames\n"
        );
        EXPECT_ERROR(msh41::parse_groups(input),
            "physical group names must be quoted: 3 1 Steel");
    }
    // closing name quote is required
    {
        std::istringstream input(
            "1\n"
            "3 1 \"Steel\n"
            "$EndPhysicalNames\n"
        );
        EXPECT_ERROR(msh41::parse_groups(input),
            "couldn't find closing quote for physical group: 3 1 \"Steel");
    }
    // only 3D groups are returned
    {
         std::istringstream input(
            "3\n"
            "1 1 \"line\"\n"
            "2 2 \"surface\"\n"
            "3 3 \"volume\"\n"
            "$EndPhysicalNames\n"
        );
        std::vector<msh41::PhysicalGroup> groups;
        EXPECT_NO_ERROR(groups = msh41::parse_groups(input));
        assert(groups.size() == 1);
        std::string expected_name = "volume";
        if (groups.at(0).name != expected_name) {
            std::cerr << "bad physical name parse, expected: " << expected_name
                << "but got: " << groups.at(0).name << "\n";
            return 1;
        }
    }
    // duplicate 3D group tags are caught
    {
         std::istringstream input(
            "4\n"
            "1 1 \"line\"\n"
            "2 2 \"surface\"\n"
            "3 3 \"volume\"\n"
            "3 4 \"volume2\"\n"
            "3 4 \"other volume\"\n" // tag 4 repeated
            "$EndPhysicalNames\n"
        );
        EXPECT_ERROR(msh41::parse_groups(input),
            "$PhysicalNames section parsing failed, found duplicate tag 4");
    }
    // spaces in names are OK
    {
         std::istringstream input(
            "3\n"
            "1 1 \"a line\"\n"
            "2 2 \"a surface\"\n"
            "3 3 \"a volume\"\n"
            "$EndPhysicalNames\n"
        );
        std::vector<msh41::PhysicalGroup> groups;
        EXPECT_NO_ERROR(groups = msh41::parse_groups(input));
        assert(groups.size() == 1);
        std::string expected_name = "a volume";
        if (groups.at(0).name != expected_name) {
            std::cerr << "bad physical name parse, expected: " << expected_name
                << "but got: " << groups.at(0).name << "\n";
            return 1;
        }
    }
    // single letter names are OK
    {
         std::istringstream input(
            "3\n"
            "1 1 \"a line\"\n"
            "2 2 \"a surface\"\n"
            "3 3 \"a\"\n"
            "$EndPhysicalNames\n"
        );
        std::vector<msh41::PhysicalGroup> groups;
        EXPECT_NO_ERROR(groups = msh41::parse_groups(input));
        assert(groups.size() == 1);
        std::string expected_name = "a";
        if (groups.at(0).name != expected_name) {
            std::cerr << "bad physical name parse, expected: " << expected_name
                << "but got: " << groups.at(0).name << "\n";
            return 1;
        }
    }
    // successfully parse a valid physical groups section
    {
         std::istringstream input(
            "5\n"
            "1 1 \"a line\"\n"
            "2 2 \"a surface\"\n"
            "3 3 \"Steel\"\n"
            "3 4 \"Air\"\n"
            "3 5 \"Water\"\n"
            "$EndPhysicalNames\n"
        );
        std::vector<msh41::PhysicalGroup> groups;
        EXPECT_NO_ERROR(groups = msh41::parse_groups(input));
        if (groups.size() != 3) {
            std::cerr << "expected 3 groups, got " << groups.size() << "\n";
            return 1;
        }
        if (! (groups.at(0).name == "Steel" && groups.at(0).tag == 3) &&
              (groups.at(1).name == "Air" && groups.at(1).tag == 4) &&
              (groups.at(2).name == "Water" && groups.at(2).tag == 5))
        {
            std::cerr << "parsed physical groups didn't match reference values\n";
            return 1;
        }
    }
    return 0;
}

int test_parse_msh41_node_bloc() {
    // missing bloc metadata fails
    {
        std::istringstream input(
        //    "1 100 0 1\n"
            "1\n"
            "1 0 0\n"
        );
        EXPECT_ERROR(msh41::parse_node_bloc(input), "Node bloc parsing failed");
    }
    // bad dimension value fails
    {
        std::istringstream input(
            "4 100 0 1\n" // 4d entity?
            "1\n"
            "1 0 0\n"
        );
        EXPECT_ERROR(msh41::parse_node_bloc(input),
            "Node bloc parsing failed for entity 100, got dimension 4,"
            " expected 0, 1, 2, or 3");
    }
    // wrong number of nodes fails
    {
        std::istringstream input(
            "1 100 0 2\n" // 2 nodes given, only 1 present
            "1\n"
            "1 0 0\n"
        );
        EXPECT_ERROR(msh41::parse_node_bloc(input),
            "Node bloc parsing failed during node coordinate section of entity 100");
    }
    // successfully parse a single node bloc
    {
        std::istringstream input(
            "1 1 0 3\n"
            "1\n"
            "2\n"
            "3\n"
            "1 0 0\n"
            "0 1 0\n"
            "0 0 1\n"
        );
        std::string err_msg;
        std::vector<msh41::Node> nodes;
        EXPECT_NO_ERROR(nodes = msh41::parse_node_bloc(input));
        if (nodes.size() != 3) {
            std::cerr << "expected 3 nodes, got " << nodes.size() << "\n";
            return 1;
        }
        auto n0 = nodes.at(0);
        auto n1 = nodes.at(1);
        auto n2 = nodes.at(2);
        if (!(n0.tag == 1 && n0.x == 1.0 && n0.y == 0.0 && n0.z == 0.0 &&
              n1.tag == 2 && n1.x == 0.0 && n1.y == 1.0 && n1.z == 0.0 &&
              n2.tag == 3 && n2.x == 0.0 && n2.y == 0.0 && n2.z == 1.0))
        {
            std::cerr << "parsed nodes didn't match reference value\n";
            return 1;
        }
    }
    return 0;
}

int test_parse_msh41_nodes() {
    // bad input stream fails
    {
        std::ifstream input("bad-file");
        EXPECT_ERROR(msh41::parse_nodes(input),
            "$Nodes section parsing failed, missing metadata");
    }
    // missing section metadata fails eventually (is parsed as the first bloc metadata)
    {
        std::istringstream input(
            // "1 1 1 1\n"
            "1 1 0 1\n"
            "1\n"
            "1 0 0\n"
        );
        EXPECT_ERROR(msh41::parse_nodes(input),
            "$Nodes section parsing failed\nNode bloc parsing failed");
    }
    // wrong num_blocs fails
    {
        std::istringstream input(
            "2 1 1 2\n" // num_blocs = 2 but is really 1
            "1 1 0 1\n"
            "1\n"
            "1 0 0\n"
        );
        EXPECT_ERROR(msh41::parse_nodes(input),
            "$Nodes section parsing failed\nNode bloc parsing failed");
    }
    // node tags must fit into an int
    {
        std::size_t too_large = std::size_t(std::numeric_limits<int>::max());
        too_large += 1;
        std::istringstream input(
            "2 1 1 " + std::to_string(too_large) + "\n"
            "1 1 0 1\n"
            "1\n"
            "1 0 0\n"
        );
        EXPECT_ERROR(msh41::parse_nodes(input),
            "Max node tag is too large (2147483648), limit is 2147483647");
    }
    // wrong num_nodes fails
    {
        std::istringstream input(
            "1 100 1 2\n" // 100 nodes
            "1 1 0 1\n"
            "1\n"
            "1 0 0\n"
        );
        EXPECT_ERROR(msh41::parse_nodes(input),
            "$Nodes section parsing failed, expected 100 nodes but read 1");
    }
    // missing $EndNodes fails
    {
         std::istringstream input(
            "1 1 1 1\n"
            "1 1 0 1\n"
            "1\n"
            "1 0 0\n"
            // "$EndNodes\n"
        );
        EXPECT_ERROR(msh41::parse_nodes(input),
            "$Nodes section parsing failed, expected $EndNodes");
    }
    // duplicate node tags are caught
    {
         std::istringstream input(
            "2 2 1 2\n"
            "1 1 0 1\n"
            "1\n"
            "1 0 0\n"
            "1 2 0 1\n"
            "1\n" // node tag 1 repeated
            "1 0 0\n"
            "$EndNodes\n"
        );
        EXPECT_ERROR(msh41::parse_nodes(input),
            "$Nodes section parsing failed, found duplicate node tag 1");
    }
    // parse multiple blocs successfully
    {
        std::istringstream input(
            "3 7 1 7\n"
            "1 1 0 1\n"
            "1\n"
            "1 0 0\n"
            "1 2 0 2\n"
            "2\n"
            "3\n"
            "0 1 0\n"
            "0 1 0\n"
            "3 2 0 4\n"
            "4\n"
            "5\n"
            "6\n"
            "7\n"
            "0 0 1\n"
            "0 0 1\n"
            "0 0 1\n"
            "0 0 1\n"
            "$EndNodes\n"
        );
        std::vector<msh41::Node> nodes;
        EXPECT_NO_ERROR(nodes = msh41::parse_nodes(input));
        assert(nodes.size() == 7);
        auto n0 = nodes.at(0);
        auto n1 = nodes.at(1);
        auto n6 = nodes.at(6);
        if (!(n0.tag == 1 && n0.x == 1.0 && n0.y == 0.0 && n0.z == 0.0 &&
              n1.tag == 2 && n1.x == 0.0 && n1.y == 1.0 && n1.z == 0.0 &&
              n6.tag == 7 && n6.x == 0.0 && n6.y == 0.0 && n6.z == 1.0))
        {
            std::cerr << "parsed entities didn't match reference value\n";
            return 1;
        }
    }
    return 0;
}

int test_parse_msh41_entities() {
    // bad input stream fails
    {
        std::ifstream input("bad-file");
        EXPECT_ERROR(msh41::parse_entities(input), "$Entities parsing failed");
    }
    // no 3d entities fails
    {
        std::istringstream input(
            "2 1 1 0\n"
            "$EndEntities\n"
        );
        EXPECT_ERROR(msh41::parse_entities(input), "$Entities parsing failed, no volumes found");
    }
    // 3d entity without a physical group fails
    {
        std::istringstream input(
            "0 0 0 1\n"
            "1 0.0 0.0 0.0 1.0 1.0 1.0 0 1\n"
            //                         ^-- num physical groups = 0
            "$EndEntities\n"
        );
        EXPECT_ERROR(msh41::parse_entities(input),
            "$Entities parsing failed, volume 1 was not assigned a physical group");
    }
    // 3d entity with more than one physical group fails
    {
        std::istringstream input(
            "0 0 0 1\n"
            "2 0.0 0.0 0.0 1.0 1.0 1.0 2 1 2\n"
            //                         ^-- num physical groups = 2
            "$EndEntities\n"
        );
        EXPECT_ERROR(msh41::parse_entities(input),
            "$Entities parsing failed, volume 2 has more than one physical group");
    }
    // repeated 3d entity tags fails
    {
        std::istringstream input(
            "0 0 0 2\n"
            "1 0.0 0.0 0.0 1.0 1.0 1.0 1 1\n"
            "1 0.0 0.0 0.0 1.0 1.0 1.0 1 1\n"
        //   ^-- volume tag 1 appears twice
            "$EndEntities\n"
        );
        EXPECT_ERROR(msh41::parse_entities(input),
            "$Entities section parsing failed, found duplicate volume tag 1");
    }
    // num entities mismatch fails
    {
        std::istringstream input(
            "0 0 0 2\n"
            "2 0.0 0.0 0.0 1.0 1.0 1.0 1 1\n"
            "$EndEntities\n"
        );
        EXPECT_ERROR(msh41::parse_entities(input),
            "$Entities parsing failed, expected 2 volumes but got 1");
    }
    // catch duplicate volume tags
    {
        std::istringstream input(
            "0 0 0 2\n"
            "2 0.0 0.0 0.0 1.0 1.0 1.0 1 1\n"
            "2 0.0 0.0 0.0 1.0 1.0 1.0 1 1\n"
            "$EndEntities\n"
        );
        EXPECT_ERROR(msh41::parse_entities(input),
            "$Entities section parsing failed, found duplicate volume tag 2");
    }
    // successfully parse volumes, skipping 0, 1, 2d entities
    {
        std::istringstream input(
            "1 2 3 2\n"
            // 1 0d entity
            "1 0 0 1 0\n"
            // 2 1d entities
            "1 -1e-007 -1e-007 -9.999999994736442e-008 1e-007 1e-007 1.0000001 0 2 2 -1\n"
            "2 -1e-007 -9.999999994736442e-008 0.9999999000000001 1e-007 1.0000001 1.0000001 0 2 1 -3\n"
            // 3 2d entities
            "1 -1e-007 -9.999999994736442e-008 -9.999999994736442e-008 1e-007 1.0000001 1.0000001 0 4 1 2 -3 -4\n"
            "2 0.9999999000000001 -9.999999994736442e-008 -9.999999994736442e-008 1.0000001 1.0000001 1.0000001 0 4 5 6 -7 -8\n"
            "3 -9.999999994736442e-008 -1e-007 -9.999999994736442e-008 1.0000001 1e-007 1.0000001 0 4 9 5 -10 -1\n"
            // 2 3d entities
        //   |-- tag                                           |-- physical group
            "1 -9.99e-008 -9.99e-008 -9.99e-008 1.0 1.0 1.0 1 100 6 1 2 3 4 5 6\n"
            "2 -9.99e-008 -9.99e-008 -9.99e-008 1.0 1.0 1.0 1 200 6 1 2 3 4 5 6\n"
            "$EndEntities\n"
        );
        std::vector<msh41::MeshVolume> vols;
        EXPECT_NO_ERROR(vols = msh41::parse_entities(input));
        if (vols.size() != 2) {
            std::cerr << "expected 2 volumes, got " << vols.size() << "\n";
            return 1;
        }
        if (!(vols.at(0).tag == 1 && vols.at(0).group == 100 &&
              vols.at(1).tag == 2 && vols.at(1).group == 200))
        {
            std::cerr << "parsed entities didn't match reference value\n";
            return 1;
        }
    }
    return 0;
}

int test_parse_msh41_element_bloc() {
    // bad input stream fails
    {
        std::ifstream input("bad-file");
        EXPECT_ERROR(msh41::parse_element_bloc(input), "Element bloc parsing failed");
    }
    // skip lower-dimension elements
    {
        std::istringstream input(
        //  v-- 2d shape
            "2 1 3 2\n"
            "1 1 2 3 4\n"
            "2 2 5 6 3\n"
        );
        std::vector<msh41::Tetrahedron> elts;
        EXPECT_NO_ERROR(elts = msh41::parse_element_bloc(input));
        if (elts.size() != 0) {
            std::cerr << "expected 0 elements, got " << elts.size() << "\n";
            return 1;
        }
    }
    // non-tetrahedral 3d elements fails
    {
         std::istringstream input(
            "3 1 5 1\n"
            //   ^-- 5 is code for hexahedron
            "1 1 2 3 4 5 6\n"
        );
        EXPECT_ERROR(msh41::parse_element_bloc(input),
            "Element bloc parsing failed for entity 1" ", got non-tetrahedral mesh element type 5");
    }
    // missing tetrahedron data fails
    {
         std::istringstream input(
            "3 2 4 3\n"
            "1 1 2 3\n" // only 3/4 nodes given
            "10 10 20 30 40\n"
            "11 5 6 7 8\n"
        );
        EXPECT_ERROR(msh41::parse_element_bloc(input),
            "Element bloc parsing failed for entity 2");
    }
    // successfully parse a tetrahedron element bloc
    {
         std::istringstream input(
            "3 1 4 3\n"
            // ^ ^ ^-- 3 elements
            // | |---- 4 => tetrahedron
            // |------ volume id 1
            "1 1 2 3 4\n"
            "10 10 20 30 40\n"
            "11 5 6 7 8\n"
        );
        std::vector<msh41::Tetrahedron> elts;
        EXPECT_NO_ERROR(elts = msh41::parse_element_bloc(input));
        assert(elts.size() == 3);
        auto e0 = elts.at(0);
        auto e1 = elts.at(1);
        auto e2 = elts.at(2);
        if (!(e0.tag == 1 && e0.volume == 1 &&
               e0.a == 1 && e0.b == 2 && e0.c == 3 && e0.d == 4 &&
              e1.tag == 10 && e1.volume == 1 &&
               e1.a == 10 && e1.b == 20 && e1.c == 30 && e1.d == 40 &&
              e2.tag == 11 && e2.volume == 1 &&
               e2.a == 5 && e2.b == 6 && e2.c == 7 && e2.d == 8))
        {
            std::cerr << "parsed elements didn't match reference value\n";
            return 1;
        }
    }
    return 0;
}

int test_parse_msh41_elements() {
    // bad input stream fails
    {
        std::ifstream input("bad-file");
        EXPECT_ERROR(msh41::parse_elements(input), "$Elements section parsing failed, missing metadata");
    }
    // no elements fails
    {
         std::istringstream input(
            "0 0 0 0\n"
            "$EndElements\n"
         );
        EXPECT_ERROR(msh41::parse_elements(input), "$Elements section parsing failed, no tetrahedral elements were read");
    }
    // no tetrahedral elements fails
    {
        std::istringstream input(
            "1 1 1 2\n"
            "1 10 1 2\n"
            "1 1 2\n"
            "2 2 3\n"
            "$EndElements\n"
        );
        EXPECT_ERROR(msh41::parse_elements(input), "$Elements section parsing failed, no tetrahedral elements were read");
    }
    // missing $EndElements fails
    {
         std::istringstream input(
            "1 2 1 2\n" // 1 bloc, 2 elements, min = 1, max = 2
            "3 1 4 2\n"
            "1 1 2 3 4\n"
            "2 5 6 7 8\n"
            // "$EndElements\n"
        );
        EXPECT_ERROR(msh41::parse_elements(input), "$Elements section parsing failed, expected $EndElements");
    }
    // skip lower-dimension elements
    {
         std::istringstream input(
            "2 4 1 4\n" // 2 blocs, 4 elts, min = 1, max = 4
            "1 10 1 2\n" // 1d element bloc should be skipped
            "1 1 2\n"
            "2 2 3\n"
            "3 50 4 2\n" // tetrahedron bloc should be parsed
            "1 1 2 3 4\n"
            "2 5 6 7 8\n"
            "$EndElements\n"
        );
        std::vector<msh41::Tetrahedron> elts;
        EXPECT_NO_ERROR(elts = msh41::parse_elements(input));
        if (elts.size() != 2) {
            std::cerr << "expected 2 elements, got " << elts.size() << "\n";
            return 1;
        }
        auto e0 = elts.at(0);
        auto e1 = elts.at(1);
        if (!(e0.tag == 1 && e0.volume == 50 &&
               e0.a == 1 && e0.b == 2 && e0.c == 3 && e0.d == 4 &&
              e1.tag == 2 && e1.volume == 50 &&
               e1.a == 5 && e1.b == 6 && e1.c == 7 && e1.d == 8))
        {
            std::cerr << "parsed elements didn't match reference value\n";
            return 1;
        }
    }
    // duplicate tetrahedron tags are caught
    {
         std::istringstream input(
            "2 4 1 4\n"
            "3 1 4 2\n"
            "1 1 2 3 4\n"
            "2 5 6 7 8\n"
            "3 2 4 2\n"
            "1 1 2 3 4\n" // tag 1 again
            "4 5 6 7 8\n"
            "$EndElements\n"
        );
        EXPECT_ERROR(msh41::parse_elements(input),
            "$Elements section parsing failed, found duplicate tetrahedron tag 1");
    }
    // successfully parses multiple element blocs
    {
         std::istringstream input(
            "3 6 1 6\n"
            "1 10 1 2\n"
            "1 1 2\n"
            "2 2 3\n"
            "3 1 4 2\n"   // tetrahedron bloc 1
            "3 1 2 3 4\n"
            "4 5 6 7 8\n"
            "3 2 4 2\n"   // tetrahedron bloc 2
            "5 1 2 3 5\n"
            "6 5 6 7 1\n"
            "$EndElements\n"
        );
        std::vector<msh41::Tetrahedron> elts;
        EXPECT_NO_ERROR(elts = msh41::parse_elements(input));
        if (elts.size() != 4) {
            std::cerr << "expected 4 elements, got " << elts.size() << "\n";
            return 1;
        }
        auto e0 = elts.at(0);
        auto e1 = elts.at(1);
        auto e2 = elts.at(2);
        auto e3 = elts.at(3);
        if (!(e0.tag == 3 && e0.volume == 1 &&
               e0.a == 1 && e0.b == 2 && e0.c == 3 && e0.d == 4 &&
              e1.tag == 4 && e1.volume == 1 &&
               e1.a == 5 && e1.b == 6 && e1.c == 7 && e1.d == 8 &&
              e2.tag == 5 && e2.volume == 2 &&
               e2.a == 1 && e2.b == 2 && e2.c == 3 && e2.d == 5 &&
              e3.tag == 6 && e3.volume == 2 &&
               e3.a == 5 && e3.b == 6 && e3.c == 7 && e3.d == 1))
        {
            std::cerr << "parsed elements didn't match reference value\n";
            return 1;
        }
    }
    return 0;
}

namespace {
    const std::string header =
        "$MeshFormat\n"
        "4.1 0 8\n"
        "$EndMeshFormat\n";

    const std::string entities =
        "$Entities\n"
        "0 0 0 2\n"
        "1 0 0 0 1.0 1.0 1.0 1 1 6 1 2 3 4 5 6\n"
        "2 0 0 0 1.0 1.0 1.0 1 2 6 1 2 3 4 5 6\n"
        "$EndEntities\n";

    const std::string pgroups =
        "$PhysicalNames\n"
        "2\n"
        "3 1 \"Steel\"\n"
        "3 2 \"Water\"\n"
        "$EndPhysicalNames\n";

    const std::string nodes =
        "$Nodes\n"
        "2 5 1 5\n"
        "1 1 0 2\n"
        "1\n"
        "2\n"
        "0 0 0\n"
        "0 1 0\n"
        "1 2 0 3\n"
        "3\n"
        "4\n"
        "5\n"
        "1 0 0\n"
        "1 1 0\n"
        "1 1 1\n"
        "$EndNodes\n";

    const std::string elts =
        "$Elements\n"
         "2 4 1 4\n"
         "3 1 4 2\n"
         "1 1 2 3 4\n"
         "2 1 2 3 5\n"
         "3 2 4 2\n"
         "3 1 2 4 5\n"
         "4 2 3 4 5\n"
         "$EndElements\n";

} // anonymous namespace

int test_parse_msh41_file_errors() {
    // Unknown physical group tags assigned to entities are caught
    {
        std::istringstream input(
            header + nodes + elts +
            "$Entities\n"
            "0 0 0 1\n"
            "1 0.0 0.0 0.0 1.0 1.0 1.0 1 100\n"
            //                           ^
            // physical group tag 100 is not part of $PhysicalNames
            "$EndEntities\n"
            "$PhysicalNames\n"
            "1\n"
            "3 1 \"Steel\"\n"
            // ^ expecting tag == 1
            "$EndPhysicalNames\n"
        );
        EXPECT_ERROR(msh_parser::parse_msh_file(input),
            "msh 4.1 parsing failed\nvolume 1 had unknown physical group tag 100");
    }

    // Unknown volume (entity) tags assigned to elements are caught
    {
        std::istringstream input(
            header + nodes + pgroups +
            "$Entities\n"
            "0 0 0 1\n"
            "1 0.0 0.0 0.0 1.0 1.0 1.0 1 1\n"
            "$EndEntities\n"
            "$Elements\n"
            "1 1 1 1\n"
            "3 100 4 1\n"
            // ^ entity tag 100 is not present in $Entities
            "1 1 2 3 4\n"
            "$EndElements\n"
        );
        EXPECT_ERROR(msh_parser::parse_msh_file(input),
            "msh 4.1 parsing failed\ntetrahedron 1 had unknown volume tag 100");
    }

    return 0;
}

int test_parse_msh41_file() {
    // section errors bubble up
    {
        std::istringstream input(
            "$MeshFormat\n"
            "4.1 0 8\n"
            "$EndMeshFormat\n"
            "$PhysicalNames\n" // missing PhysicalNames content
            "$EndPhysicalNames\n"
        );
        EXPECT_ERROR(msh_parser::parse_msh_file(input),
            "msh 4.1 parsing failed\n$PhysicalNames parsing failed");
    }
    // minimum complete mesh file for EGSnrc
    {
        std::istringstream input(header + entities + pgroups + nodes + elts);
        EGS_Mesh mesh = msh_parser::parse_msh_file(input);
        auto elts = mesh.elements();
        assert(elts.size() == 4);
        assert(elts[0].medium_tag == 1);
        assert(elts[1].medium_tag == 1);
        assert(elts[2].medium_tag == 2);
        assert(elts[3].medium_tag == 2);
        assert(elts[0].a == 1 && elts[0].b == 2 && elts[0].c == 3 && elts[0].d == 4);
        assert(elts[1].a == 1 && elts[1].b == 2 && elts[1].c == 3 && elts[1].d == 5);
        assert(elts[2].a == 1 && elts[2].b == 2 && elts[2].c == 4 && elts[2].d == 5);
        assert(elts[3].a == 2 && elts[3].b == 3 && elts[3].c == 4 && elts[3].d == 5);

        auto nodes = mesh.nodes();
        assert(nodes.size() == 5);
        assert(nodes[0].tag == 1 && nodes[0].x == 0.0 && nodes[0].y == 0.0 && nodes[0].z == 0.0);
        assert(nodes[1].tag == 2 && nodes[1].x == 0.0 && nodes[1].y == 1.0 && nodes[1].z == 0.0);
        assert(nodes[2].tag == 3 && nodes[2].x == 1.0 && nodes[2].y == 0.0 && nodes[2].z == 0.0);
        assert(nodes[3].tag == 4 && nodes[3].x == 1.0 && nodes[3].y == 1.0 && nodes[3].z == 0.0);
        assert(nodes[4].tag == 5 && nodes[4].x == 1.0 && nodes[4].y == 1.0 && nodes[4].z == 1.0);

        auto materials = mesh.materials();
        assert(materials.size() == 2);
        assert(materials[0].tag == 1);
        assert(materials[0].medium_name == "Steel");
        assert(materials[1].tag == 2);
        assert(materials[1].medium_name == "Water");
    }
    return 0;
}

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

int main() {
    int num_failed = 0;
    int num_total = 0;
    int err = 0;

    RUN_TEST(test_parse_msh_version());
    RUN_TEST(test_parse_msh41_entities());
    RUN_TEST(test_parse_msh41_node_bloc());
    RUN_TEST(test_parse_msh41_nodes());
    RUN_TEST(test_parse_msh41_groups());
    RUN_TEST(test_parse_msh41_element_bloc());
    RUN_TEST(test_parse_msh41_elements());
    RUN_TEST(test_parse_msh41_file_errors());
    RUN_TEST(test_parse_msh41_file());

    std::cerr << num_total - num_failed << " out of " << num_total << " tests passed\n";
    return num_failed;
}
