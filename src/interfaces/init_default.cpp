#include <interfaces/init.h>

#include <util/memory.h>

namespace interfaces {
namespace {
class LocalInitImpl : public LocalInit
{
public:
    LocalInitImpl() : LocalInit(/* exe_name= */ nullptr, /* log_suffix= */ nullptr) {}
};
} // namespace
std::unique_ptr<LocalInit> MakeGuiInit(int argc, char* argv[], InitInterfaces& interfaces)
{
    return MakeUnique<LocalInitImpl>();
}
std::unique_ptr<LocalInit> MakeNodeInit(int argc, char* argv[], InitInterfaces& interfaces)
{
    return MakeUnique<LocalInitImpl>();
}
std::unique_ptr<LocalInit> MakeWalletInit(int argc, char* argv[]) { return MakeUnique<LocalInitImpl>(); }
} // namespace interfaces
