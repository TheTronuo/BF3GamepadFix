#include "bf3/patch/release.hpp"

namespace bf3::patch {
namespace {
const ResourceSpec resources[] = {
    {"synthetic", "synthetic", "Data/test.cas", 100, 2, "fixture.original", "fixture.patched",
     "983987033f0e117011e531dc33ad9bb15290bba41a414d830fb5cbdbcda2ff17",
     "0081ff721996c03f358ecc19e39d2a8ced41e81b76271a79fe9c489182a1fd49", "", "", nullptr, 0, 0}};
const ArchiveSpec archives[] = {
    {"Data/test.cas", 512, "84867454d4a00b8cc5e38dea6f9356eff696f54614526e11a62b4f5c80413d80",
     "509e3069b0251aa90674a04c3b62e037d974045c1425efc7e185f7cfd7d9a9a9"}};
const MetadataSpec metadata[] = {
    {"Data/test.toc", "73907589101a7e8ab83178e7db2997aab7272cd02d364e8e3ecc2beccda4b631"}};
} // namespace
const ResourceSpec* release_resources() noexcept {
    return resources;
}
std::size_t release_resource_count() noexcept {
    return std::size(resources);
}
const ArchiveSpec* release_archives() noexcept {
    return archives;
}
std::size_t release_archive_count() noexcept {
    return std::size(archives);
}
const MetadataSpec* release_metadata() noexcept {
    return metadata;
}
std::size_t release_metadata_count() noexcept {
    return std::size(metadata);
}
} // namespace bf3::patch
