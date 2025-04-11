#include <Windows.h>
#include <TlHelp32.h>
#include <array>
#include <memory>
#include <print>
#include <string>
#include <format>

#include <wincpp/process.hpp> // Custom wrapper for process/memory/thread handling

using namespace wincpp;

constexpr std::uintptr_t kThreadListOffset = 0x286210;

// Utilities
namespace utils {
    std::uint64_t get_thread_creation_time(HANDLE thread_handle) {
        FILETIME create{}, exit{}, kernel{}, user{};
        if (!GetThreadTimes(thread_handle, &create, &exit, &kernel, &user)) {
            throw core::error::from_win32(GetLastError());
        }
        return (static_cast<std::uint64_t>(create.dwHighDateTime) << 32) | create.dwLowDateTime;
    }

    std::shared_ptr<core::handle_t> create_suspended_remote_thread(
        HANDLE process_handle,
        std::uintptr_t start_address,
        std::uintptr_t parameter,
        DWORD& out_tid
    ) {
        auto hThread = CreateRemoteThreadEx(
            process_handle, nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(start_address),
            reinterpret_cast<void*>(parameter),
            CREATE_SUSPENDED, nullptr, &out_tid
        );
        if (!hThread) throw core::error::from_win32(GetLastError());
        return core::handle_t::create(hThread);
    }
}

void inject_whitelist_entry(
    const std::unique_ptr<process_t>& proc,
    std::uintptr_t list_base,
    DWORD thread_id,
    std::uint64_t creation_time,
    std::uintptr_t start_addr
) {
    auto root_node = *proc->memory_factory[list_base];
    auto next_node = *(root_node + 0x8);
    auto entry_count = *(proc->memory_factory[list_base + 0x8]);

    std::array<std::uintptr_t, 7> new_entry = {
        root_node.address(),
        next_node.address(),
        root_node.address(),
        0,
        thread_id,
        creation_time,
        start_addr
    };

    auto entry_alloc = proc->memory_factory.allocate(0x38, memory::protection_flags_t::readwrite, false);
    entry_alloc->write(0, reinterpret_cast<const std::uint8_t*>(new_entry.data()), new_entry.size() * sizeof(std::uintptr_t));

    *(proc->memory_factory[list_base + 0x8]) = entry_count + 1;

    std::array<std::uintptr_t, 3> patched_links = {
        entry_alloc->address(),
        entry_alloc->address(),
        entry_alloc->address()
    };

    next_node.write(0, reinterpret_cast<const std::uint8_t*>(patched_links.data()), sizeof(patched_links));
}

int main() {
    try {
        auto process = process_t::open("RobloxPlayerBeta.exe");

        auto& user32 = process->module_factory["user32.dll"];
        auto& msgbox = user32["MessageBoxIndirectA"];

        auto alloc_block = process->memory_factory.allocate(0x1000, memory::protection_flags_t::readwrite);
        auto text_ptr = alloc_block->address();
        auto caption_ptr = text_ptr + 0x100;

        process->memory_factory.write<std::string>(text_ptr, "Injected from RobloxThreadGhost");
        process->memory_factory.write<std::string>(caption_ptr, "Bypassed Thread");

        MSGBOXPARAMSA params {
            .cbSize = sizeof(MSGBOXPARAMSA),
            .hwndOwner = nullptr,
            .hInstance = reinterpret_cast<HINSTANCE>(user32.base()),
            .lpszText = reinterpret_cast<LPCSTR>(text_ptr),
            .lpszCaption = reinterpret_cast<LPCSTR>(caption_ptr),
            .dwStyle = MB_OK | MB_ICONINFORMATION,
            .lpszIcon = nullptr,
            .dwContextHelpId = 0,
            .lpfnMsgBoxCallback = nullptr,
            .dwLanguageId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)
        };

        auto param_mem = process->memory_factory.allocate<MSGBOXPARAMSA>(memory::protection_flags_t::readwrite);
        param_mem->write(0, params);

        process->thread_factory.suspend_all();

        DWORD thread_id = 0;
        auto remote_thread = utils::create_suspended_remote_thread(
            process->handle->native,
            msgbox.address(),
            param_mem->address(),
            thread_id
        );

        auto creation_time = utils::get_thread_creation_time(remote_thread->native);

        auto& roblox_module = process->module_factory["RobloxPlayerBeta.dll"];
        auto whitelist_base = roblox_module.address() + kThreadListOffset;

        inject_whitelist_entry(
            process,
            whitelist_base,
            thread_id,
            creation_time,
            msgbox.address()
        );

        process->thread_factory.resume_all();

        WaitForSingleObject(remote_thread->native, INFINITE);

        DWORD exit_code = 0;
        if (!GetExitCodeThread(remote_thread->native, &exit_code))
            throw core::error::from_win32(GetLastError());

        std::println("[+] Thread exited with code: {:#x}", exit_code);
    } catch (const core::error& ex) {
        std::println("[!] Exception: {}", ex.what());
    }
    return 0;
}
