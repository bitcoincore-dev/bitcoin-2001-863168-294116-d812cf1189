#include <interface/capnp/test/foo.h>

#include <util.h>

namespace interface {
namespace capnp {
namespace test {
namespace {

class FooImpl : public FooInterface
{
public:
    int add(int a, int b) override { return a + b; }
    int mapSize(const std::map<std::string, std::string>& map) override { return map.size(); }
    FooStruct pass(FooStruct foo) override { return foo; }
};

} // namespace

std::unique_ptr<FooInterface> MakeFooInterface() { return MakeUnique<FooImpl>(); }

} // namespace test
} // namespace capnp
} // namespace interface
