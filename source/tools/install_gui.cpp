#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <thread>
#include <vector>
#include <algorithm>
#include <objidl.h>
#include <gdiplus.h>
#include "bf3/hash.hpp"
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
namespace fs = std::filesystem;
int bf3_install_main(int, wchar_t**, const fs::path&);
void bf3_require_closed_game();
namespace {
constexpr UINT done = WM_APP + 1;
HWND main_window, game_box, status_label, install_button, remove_button, browse_button, xbox, ps, progress;
HFONT body_font, title_font, small_font;
HBRUSH background;
int dpi = 96;
int scale(int value) { return MulDiv(value, dpi, 96); }
fs::path game;
bool busy = false, preview = false, preview_installed = false, playstation_selected = false;
struct Result { int code; std::wstring message; };
struct Payload { int id; const wchar_t* path; const char* hash; };
const Payload payload[] = {
#include "payload.inc"
};
std::wstring registry(HKEY root, const wchar_t* key, const wchar_t* value) {
    wchar_t text[32768]{}; DWORD size = sizeof(text);
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, text, &size) != ERROR_SUCCESS) return {};
    return text;
}
bool valid(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path / L"bf3.exe", ec);
}
std::vector<fs::path> discover() {
    std::vector<fs::path> found;
    auto add = [&](const fs::path& p) {
        if (!valid(p)) return;
        auto canonical = fs::weakly_canonical(p);
        if (std::none_of(found.begin(), found.end(), [&](const auto& f) {
            return _wcsicmp(f.c_str(), canonical.c_str()) == 0;
        })) found.push_back(canonical);
    };
    wchar_t exe[32768]{}; GetModuleFileNameW(nullptr, exe, 32768);
    add(fs::path(exe).parent_path()); add(fs::current_path());
    for (auto root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        for (auto key : {L"SOFTWARE\\EA Games\\Battlefield 3", L"SOFTWARE\\WOW6432Node\\EA Games\\Battlefield 3"}) {
            auto path = registry(root, key, L"Install Dir"); if (!path.empty()) add(path);
        }
    }
    auto steam = registry(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
    if (!steam.empty()) {
        std::vector<fs::path> libraries{fs::path(steam)};
        std::ifstream input(fs::path(steam) / "steamapps/libraryfolders.vdf");
        std::string data((std::istreambuf_iterator<char>(input)), {});
        std::regex path_pattern("\"path\"\\s*\"([^\"]+)\"");
        for (auto it = std::sregex_iterator(data.begin(), data.end(), path_pattern); it != std::sregex_iterator(); ++it) {
            auto text = (*it)[1].str();
            for (std::size_t p = 0; (p = text.find("\\\\", p)) != std::string::npos; ++p) text.replace(p, 2, "\\");
            libraries.push_back(fs::u8path(text));
        }
        for (const auto& library : libraries) {
            auto apps = library / "steamapps";
            std::ifstream manifest(apps / "appmanifest_1238820.acf");
            std::string content((std::istreambuf_iterator<char>(manifest)), {});
            std::smatch match;
            if (std::regex_search(content, match, std::regex("\"installdir\"\\s*\"([^\"]+)\""))) {
                fs::path relative = fs::u8path(match[1].str());
                if (!relative.is_absolute() && relative.parent_path().empty()) add(apps / "common" / relative);
            }
            add(apps / "common/Battlefield 3");
        }
    }
    return found;
}
bool recorded() {
    if (preview) return preview_installed;
    return !game.empty() && GetPrivateProfileIntW(L"Install", L"Active", 0,
        (game / "ControllerMod/BF3GamepadFix/state.ini").c_str()) == 1;
}
void refresh() {
    const bool has_game = valid(game), active = recorded();
    SetWindowTextW(status_label, preview ? L"Preview only" : active ? L"Installed" : has_game ? L"Ready" : L"Game not found");
    EnableWindow(install_button, !busy && !active);
    EnableWindow(browse_button, !busy);
    SetWindowTextW(install_button, active ? L"Installed" : has_game || preview ? L"Install" : L"Select game...");
    ShowWindow(progress, busy ? SW_SHOW : SW_HIDE);
    EnableWindow(xbox, !busy); EnableWindow(ps, !busy);
    SendMessageW(xbox, BM_SETCHECK, playstation_selected ? BST_UNCHECKED : BST_CHECKED, 0);
    SendMessageW(ps, BM_SETCHECK, playstation_selected ? BST_CHECKED : BST_UNCHECKED, 0);
}
void save_prompt(const fs::path& config, bool playstation) {
    const auto style = playstation ? L"PS3" : L"Xbox360";
    if (!WritePrivateProfileStringW(L"Controller", L"PromptStyle", style, config.c_str()))
        throw std::runtime_error("Could not save button prompts. Check game folder write access.");
    wchar_t saved[32]{};
    GetPrivateProfileStringW(L"Controller", L"PromptStyle", L"", saved, 32, config.c_str());
    if (wcscmp(saved, style) != 0) throw std::runtime_error("Button prompt settings could not be verified.");
}
void choose_prompt(bool playstation) {
    if (busy) return;
    const bool previous = playstation_selected;
    try {
        if (!preview && recorded() && playstation != previous)
            save_prompt(game / "BF3Controller.ini", playstation);
        playstation_selected = playstation;
    } catch (const std::exception& e) {
        playstation_selected = previous;
        const std::string detail = e.what();
        std::wstring message(detail.begin(), detail.end());
        MessageBoxW(main_window, message.c_str(), L"BF3 Gamepad Fix", MB_OK | MB_ICONWARNING);
    }
    refresh();
}
void select_game() {
    int selected = static_cast<int>(SendMessageW(game_box, CB_GETCURSEL, 0, 0));
    if (selected < 0) return;
    wchar_t text[32768]{}; SendMessageW(game_box, CB_GETLBTEXT, static_cast<WPARAM>(selected), reinterpret_cast<LPARAM>(text));
    game = text;
    wchar_t style[32]{};
    GetPrivateProfileStringW(L"Controller", L"PromptStyle", L"Xbox360", style, 32, (game / "BF3Controller.ini").c_str());
    playstation_selected = _wcsicmp(style, L"PS3") == 0;
    refresh();
}
fs::path extract() {
    wchar_t temp[32768]{};
    if (!GetTempPathW(32768, temp)) throw std::runtime_error("Cannot locate temporary directory");
    auto directory = fs::path(temp) / (L"BF3GamepadFix-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    if (!fs::create_directory(directory)) throw std::runtime_error("Cannot create private payload directory");
    try {
        for (const auto& item : payload) {
            auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(item.id), RT_RCDATA);
            auto loaded = resource ? LoadResource(nullptr, resource) : nullptr;
            auto bytes = loaded ? LockResource(loaded) : nullptr;
            if (!bytes) throw std::runtime_error("Embedded package is damaged");
            auto path = directory / item.path;
            fs::create_directories(path.parent_path());
            std::ofstream output(path, std::ios::binary);
            output.write(static_cast<const char*>(bytes), SizeofResource(nullptr, resource));
            if (!output) throw std::runtime_error("Cannot extract embedded package");
            output.close();
            if (bf3::file_sha256(path) != item.hash) throw std::runtime_error("Embedded package checksum mismatch");
        }
    } catch (...) { std::error_code ec; fs::remove_all(directory, ec); throw; }
    return directory;
}
std::wstring widen(const std::string& text) {
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L' ');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}
int run_command(int argc, wchar_t** args) {
    std::ostringstream output;
    auto old_out = std::cout.rdbuf(output.rdbuf());
    auto old_err = std::cerr.rdbuf(output.rdbuf());
    fs::path package;
    int code = 1;
    try {
#ifdef BF3_PREVIEW_ONLY
        static_cast<void>(argc); static_cast<void>(args);
        throw std::runtime_error("Preview only - game changes are disabled.");
#else
        package = extract();
        code = bf3_install_main(argc, args, package);
#endif
    } catch (const std::exception& e) { output << e.what() << '\n'; }
    std::cout.rdbuf(old_out); std::cerr.rdbuf(old_err);
    if (!package.empty()) { std::error_code ec; fs::remove_all(package, ec); }
    const auto message = output.str();
    auto handle = GetStdHandle(code == 0 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        AttachConsole(ATTACH_PARENT_PROCESS);
        handle = GetStdHandle(code == 0 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
    }
    DWORD written = 0;
    if (!handle || handle == INVALID_HANDLE_VALUE ||
        !WriteFile(handle, message.data(), static_cast<DWORD>(message.size()), &written, nullptr))
        MessageBoxW(nullptr, widen(message).c_str(), L"BF3 Gamepad Fix",
                    MB_OK | (code == 0 ? MB_ICONINFORMATION : MB_ICONWARNING));
    return code;
}
void operate(bool removing) {
    if (busy) return;
    if (preview) { preview_installed = true; refresh(); return; }
    if (!removing && recorded()) return;
    if (!valid(game)) { SendMessageW(main_window, WM_COMMAND, 11, 0); return; }
    if (removing && MessageBoxW(main_window, L"Restore the files from before this patch was installed?", L"Uninstall", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    const auto target = game;
    const bool playstation = playstation_selected;
    const bool settings_only = !removing && recorded();
    busy = true; refresh();
    SetWindowTextW(status_label, settings_only ? L"Saving..." : removing ? L"Restoring..." : L"Installing...");
    SetWindowTextW(install_button, L"Working...");
    SendMessageW(progress, PBM_SETMARQUEE, TRUE, 35);
    std::thread([target, playstation, removing, settings_only] {
        auto result = new Result{1, L""}; fs::path package;
        std::ostringstream output;
        try {
            bf3_require_closed_game();
            if (!settings_only) {
                package = extract();
                std::wstring executable = L"BF3GamepadFix", action = removing ? L"remove" : L"install", path = target.wstring();
                wchar_t* args[]{executable.data(), action.data(), path.data()};
                auto old_out = std::cout.rdbuf(output.rdbuf()); auto old_err = std::cerr.rdbuf(output.rdbuf());
                result->code = bf3_install_main(3, args, package);
                std::cout.rdbuf(old_out); std::cerr.rdbuf(old_err);
            } else result->code = 0;
            if (result->code == 0 && !removing) {
                bf3_require_closed_game();
                save_prompt(target / "BF3Controller.ini", playstation);
            }
            result->message = result->code == 0 ? (removing ? L"Restored" : settings_only ? L"Saved. Restart the game." : L"Installed. Ready to play.") : widen(output.str());
        } catch (const std::exception& e) { result->code = 1; result->message = widen(e.what()); }
        if (!package.empty()) { std::error_code ec; fs::remove_all(package, ec); }
        PostMessageW(main_window, done, 0, reinterpret_cast<LPARAM>(result));
    }).detach();
}
HWND control(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id, HFONT font = nullptr) {
    auto handle = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style, scale(x), scale(y), scale(w), scale(h), main_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SendMessageW(handle, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : body_font), TRUE); return handle;
}
void draw_icon(const DRAWITEMSTRUCT& item) {
    const int resource_id = item.CtlID == 20 ? 1 : 2;
    auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (!resource) return;
    auto bytes = LockResource(LoadResource(nullptr, resource));
    auto stream = SHCreateMemStream(static_cast<const BYTE*>(bytes), SizeofResource(nullptr, resource));
    if (!stream) return;
    {
        Gdiplus::Bitmap image(stream);
        Gdiplus::Graphics graphics(item.hDC);
        graphics.Clear(Gdiplus::Color(250, 250, 250));
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        const auto width = static_cast<Gdiplus::REAL>(item.rcItem.right);
        const auto height = static_cast<Gdiplus::REAL>(item.rcItem.bottom);
        if (image.GetLastStatus() == Gdiplus::Ok && image.GetWidth() && image.GetHeight()) {
            Gdiplus::Rect source(0, 0, static_cast<INT>(image.GetWidth()), static_cast<INT>(image.GetHeight()));
            Gdiplus::BitmapData pixels{};
            if (image.LockBits(&source, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &pixels) == Gdiplus::Ok) {
                int left = source.Width, top = source.Height, right = 0, bottom = 0;
                for (int y = 0; y < source.Height; ++y) {
                    auto row = static_cast<const BYTE*>(pixels.Scan0) + static_cast<std::ptrdiff_t>(y) * pixels.Stride;
                    for (int x = 0; x < source.Width; ++x) if (row[x * 4 + 3] > 32) {
                        left = std::min(left, x); top = std::min(top, y);
                        right = std::max(right, x + 1); bottom = std::max(bottom, y + 1);
                    }
                }
                image.UnlockBits(&pixels);
                if (right > left && bottom > top) source = Gdiplus::Rect(left, top, right - left, bottom - top);
            }
            auto ratio = std::min(width / static_cast<Gdiplus::REAL>(source.Width), height / static_cast<Gdiplus::REAL>(source.Height));
            auto w = static_cast<Gdiplus::REAL>(source.Width) * ratio;
            auto h = static_cast<Gdiplus::REAL>(source.Height) * ratio;
            graphics.DrawImage(&image, Gdiplus::RectF((width - w) / 2, (height - h) / 2, w, h),
                static_cast<Gdiplus::REAL>(source.X), static_cast<Gdiplus::REAL>(source.Y),
                static_cast<Gdiplus::REAL>(source.Width), static_cast<Gdiplus::REAL>(source.Height), Gdiplus::UnitPixel);
        }
    }
    stream->Release();
}
LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_DRAWITEM:
        if (wp == 20 || wp == 21) { draw_icon(*reinterpret_cast<DRAWITEMSTRUCT*>(lp)); return TRUE; }
        break;
    case WM_CREATE: {
        main_window = window;
        install_button = control(L"BUTTON", L"Install", BS_DEFPUSHBUTTON | WS_TABSTOP, 10, 10, 78, 30, 14);
        xbox = control(L"BUTTON", L"Xbox 360", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 98, 14, 16, 22, 12);
        ps = control(L"BUTTON", L"PlayStation 3", BS_AUTORADIOBUTTON | WS_TABSTOP, 220, 14, 16, 22, 13);
        control(L"STATIC", L"", SS_OWNERDRAW | SS_NOTIFY, 114, 17, 24, 16, 20);
        control(L"STATIC", L"", SS_OWNERDRAW | SS_NOTIFY, 236, 17, 24, 16, 21);
        control(L"STATIC", L"Xbox 360", SS_NOTIFY, 141, 17, 70, 18, 22);
        control(L"STATIC", L"PlayStation 3", SS_NOTIFY, 263, 17, 96, 18, 23);
        // The game picker is only shown when discovery needs user input.
        game_box = control(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_GROUP, 12, 12, 1, 1, 10);
        ShowWindow(game_box, SW_HIDE);
        status_label = control(L"STATIC", L"", 0, 0, 0, 1, 1, 0);
        ShowWindow(status_label, SW_HIDE);
        progress = control(PROGRESS_CLASSW, L"", PBS_MARQUEE, 10, 46, 348, 2, 0);
        ShowWindow(progress, SW_HIDE);
        try {
            auto games = discover();
            for (const auto& path : games) SendMessageW(game_box, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(path.c_str()));
            if (!games.empty()) { SendMessageW(game_box, CB_SETCURSEL, 0, 0); select_game(); }
            else { refresh(); }
        } catch (...) { refresh(); }
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 10 && HIWORD(wp) == CBN_SELCHANGE) select_game();
        if (LOWORD(wp) == 11) {
            wchar_t filename[32768]{}; OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = window;
            dialog.lpstrFile = filename; dialog.nMaxFile = 32768; dialog.lpstrFilter = L"Battlefield 3 (bf3.exe)\0bf3.exe\0\0";
            dialog.lpstrTitle = L"Select Battlefield 3 / bf3.exe";
            dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameW(&dialog)) {
                auto path = fs::path(filename).parent_path();
                if (valid(path)) {
                    auto index = SendMessageW(game_box, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(path.c_str()));
                    SendMessageW(game_box, CB_SETCURSEL, static_cast<WPARAM>(index), 0); select_game();
                }
            }
        }
        if (LOWORD(wp) >= 20 && LOWORD(wp) <= 23 && HIWORD(wp) == STN_CLICKED) {
            choose_prompt(LOWORD(wp) == 21 || LOWORD(wp) == 23);
            return 0;
        }
        if (LOWORD(wp) == 12 || LOWORD(wp) == 13) {
            choose_prompt(LOWORD(wp) == 13);
        }
        if (LOWORD(wp) == 14) operate(false);
        if (LOWORD(wp) == 15) operate(true);
        return 0;
    case done: {
        auto result = reinterpret_cast<Result*>(lp); busy = false;
        SendMessageW(progress, PBM_SETMARQUEE, FALSE, 0); refresh();
        if (result->code != 0) MessageBoxW(window, (L"Could not complete the operation.\n\n" + result->message).c_str(), L"BF3 Gamepad Fix", MB_OK | MB_ICONWARNING);
        delete result; return 0;
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC: {
        auto dc = reinterpret_cast<HDC>(wp); SetTextColor(dc, RGB(35, 44, 56)); SetBkColor(dc, RGB(250, 250, 250)); return reinterpret_cast<LRESULT>(background);
    }
    case WM_CLOSE: if (busy) return 0; DestroyWindow(window); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command, int show) {
    if (std::wstring(command) == L"--self-test") {
        fs::path directory;
        try {
            directory = extract();
            const auto config = directory / L"settings-test.ini";
            { std::ofstream output(config); output << "[Controller]\nPromptStyle=Xbox360\nKeep=42\n[Other]\nValue=abc\n"; }
            save_prompt(config, true);
            save_prompt(config, false);
            wchar_t other[32]{};
            GetPrivateProfileStringW(L"Other", L"Value", L"", other, 32, config.c_str());
            if (GetPrivateProfileIntW(L"Controller", L"Keep", 0, config.c_str()) != 42 || wcscmp(other, L"abc") != 0)
                throw std::runtime_error("Prompt switching changed unrelated settings");
            bool rejected = false;
            try { save_prompt(directory / L"missing/settings.ini", true); }
            catch (const std::exception&) { rejected = true; }
            if (!rejected) throw std::runtime_error("Failed settings write was not reported");
            fs::remove_all(directory); return 0;
        } catch (...) {
            if (!directory.empty()) { std::error_code ec; fs::remove_all(directory, ec); }
            return 1;
        }
    }
    int argc = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (args && argc > 1 && (wcscmp(args[1], L"install") == 0 ||
        wcscmp(args[1], L"remove") == 0 || wcscmp(args[1], L"status") == 0)) {
        const auto code = run_command(argc, args);
        LocalFree(args);
        return code;
    }
    if (args) LocalFree(args);
    wchar_t own_path[32768]{}; GetModuleFileNameW(nullptr, own_path, 32768);
    preview = std::wstring(command).find(L"--preview") != std::wstring::npos ||
        fs::path(own_path).stem().wstring() == L"BF3GamepadFix-Preview";
#ifdef BF3_PREVIEW_ONLY
    preview = true;
#endif
    SetProcessDPIAware();
    auto screen = GetDC(nullptr); dpi = GetDeviceCaps(screen, LOGPIXELSX); ReleaseDC(nullptr, screen);
    Gdiplus::GdiplusStartupInput graphics_input;
    ULONG_PTR graphics_token = 0;
    Gdiplus::GdiplusStartup(&graphics_token, &graphics_input, nullptr);
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_PROGRESS_CLASS}; InitCommonControlsEx(&common);
    body_font = CreateFontW(-scale(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    title_font = CreateFontW(-32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    small_font = CreateFontW(-scale(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    background = CreateSolidBrush(RGB(250, 250, 250));
    WNDCLASSW cls{}; cls.hInstance = instance; cls.lpfnWndProc = window_proc; cls.lpszClassName = L"BF3GamepadFixWindow";
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW); cls.hbrBackground = background; RegisterClassW(&cls);
    RECT bounds{0, 0, scale(368), scale(52)}; DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&bounds, style, FALSE);
    auto window = CreateWindowW(cls.lpszClassName, preview ? L"BF3 Gamepad Fix - Preview" : L"BF3 Gamepad Fix", style,
        CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr, instance, nullptr);
    ShowWindow(window, show);
    MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    DeleteObject(body_font); DeleteObject(title_font); DeleteObject(small_font); DeleteObject(background);
    Gdiplus::GdiplusShutdown(graphics_token);
    return 0;
}
