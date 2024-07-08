
#include <mmbkpp/obf/hardware/mmc.h>
#include <memepp/convert/fmt.hpp>
#include <fmt/format.h>

int main()
{
    std::vector<mmbkpp::obfhw::mmc_info> mmcs;
    auto e = mmbkpp::obfhw::enum_mmcs(std::back_inserter(mmcs));

    if (e) {
        fmt::print("[Error][{}] {}\n", e.code(), e.message());
    }

    for (const auto& mmc : mmcs) {
        fmt::print("---------------------------------------\n");
        fmt::print("MMC: {}\n", mmc.dev_name);
        fmt::print("  Name: {}\n", mmc.name);
        fmt::print("  CID: {}\n", mmc.cid);
        fmt::print("  CSD: {}\n", mmc.csd);
        fmt::print("  OEMID: {}\n", mmc.oemid);
        fmt::print("  Serial: {}\n", mmc.serial);
        fmt::print("  Manfid: {}\n", mmc.manfid);
        fmt::print("  Date: {}\n", mmc.date);
        fmt::print("  Type: {}\n", mmc.type);
        fmt::print("  Removable: {}\n", static_cast<int>(mmc.removable));
    }
    fmt::print("---------------------------------------\n");

    return 0;
}
