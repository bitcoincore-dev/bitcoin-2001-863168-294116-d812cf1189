#ifndef BITCOIN_INTERFACES_CAPNP_TEST_FOO_DECL_H
#define BITCOIN_INTERFACES_CAPNP_TEST_FOO_DECL_H

#include <interfaces/capnp/proxy.h>
#include <interfaces/capnp/test/foo.capnp.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace interfaces {
namespace capnp {
namespace test {

struct FooStruct;

class FooInterface
{
public:
    virtual int add(int a, int b) = 0;
    virtual int mapSize(const std::map<std::string, std::string>&) = 0;
    virtual FooStruct pass(FooStruct) = 0;
};

struct FooStruct
{
    std::string name;
    std::vector<int> num_set;
};

std::unique_ptr<FooInterface> MakeFooInterface();

} // namespace test
} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_TEST_FOO_DECL_H
