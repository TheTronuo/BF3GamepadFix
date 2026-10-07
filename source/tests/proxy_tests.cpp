#include <windows.h>
#include <array>
#include <bcrypt.h>
#include <iostream>

int main() {
    // CTest supplies a dedicated build-directory fixture. Never run this in the
    // game directory: these small CAS files are deliberately synthetic.
    const auto original = LoadLibraryW(L"ControllerMod\\loader\\original-buildinfo.dll");
    const auto proxy = LoadLibraryW(L"Engine.BuildInfo_Win32_Retail_dll.dll");
    if (!original || !proxy) {
        std::cerr << "Proxy load failed: " << GetLastError() << '\n';
        return 1;
    }
    using BuildInfo = void*(__cdecl*)();
    const auto original_info =
        reinterpret_cast<BuildInfo>(GetProcAddress(original, "getBuildInfo"));
    const auto proxy_info = reinterpret_cast<BuildInfo>(GetProcAddress(proxy, "getBuildInfo"));
    if (!original_info || !proxy_info || original_info() != proxy_info() ||
        original_info() != proxy_info())
        return 2;
    CreateDirectoryW(L"Data", nullptr);
    const auto file = CreateFileW(L"Data\\cas_01.cas", GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    std::array<BYTE, 16> expected{1, 2, 3}, actual{};
    DWORD count = 0;
    LARGE_INTEGER zero{};
    if (file == INVALID_HANDLE_VALUE || !WriteFile(file, expected.data(), 16, &count, nullptr) ||
        count != 16 || !SetFilePointerEx(file, zero, nullptr, FILE_BEGIN) ||
        !ReadFile(file, actual.data(), 16, &count, nullptr) || count != 16 || expected != actual ||
        !CloseHandle(file))
        return 3;

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_RSA_ALGORITHM, nullptr, 0) < 0 ||
        BCryptGenerateKeyPair(algorithm, &key, 1024, 0) < 0 || BCryptFinalizeKeyPair(key, 0) < 0)
        return 4;
    BCRYPT_PKCS1_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM};
    std::array<BYTE, 32> hash{};
    std::array<BYTE, 128> signature{};
    ULONG length = 0;
    if (BCryptSignHash(key, &padding, hash.data(), 32, signature.data(), 128, &length,
                       BCRYPT_PAD_PKCS1) < 0 ||
        length != 128)
        return 5;
    const auto valid = BCryptVerifySignature(key, &padding, hash.data(), 32, signature.data(),
                                             length, BCRYPT_PAD_PKCS1);
    signature[0] ^= 1;
    const auto invalid = BCryptVerifySignature(key, &padding, hash.data(), 32, signature.data(),
                                               length, BCRYPT_PAD_PKCS1);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (valid < 0 || invalid >= 0)
        return 6;
    std::cout << "Proxy export forwards exactly; file reads and valid/invalid RSA checks retain "
                 "original behavior.\n";
    return 0;
}
