
#include "blook/blook.h"

#include "Windows.h"
#include <format>

void test_wrap_function() {
    int evilNum = 1919810;
    const auto wrapped = blook::Function::into_function_pointer(
            [=](int64_t a, int64_t b) { return a + b + evilNum; });

    const auto non_wrapped =
            std::function([=](int64_t a, int64_t b) { return a + b + evilNum; });

    constexpr auto num = 100000000;
    const auto clock = std::chrono::high_resolution_clock();

    const auto s2 = clock.now();
    for (int a = 0; a < num; a++)
        non_wrapped(114, 514);
    const auto x = clock.now() - s2;
    std::cout << std::format("non-wrapped version took {} for {} calls\n", x,
                             num);

    const auto s1 = clock.now();
    for (int a = 0; a < num; a++)
        wrapped(114, 514);
    const auto b = clock.now() - s1;
    std::cout << std::format("wrapped version took {} for {} calls\n", b, num);

    std::cout << std::format("delta: {}, radio: {}\n", b - x,
                             b.count() / (double) x.count());
}

void test_inline_hook() {
    auto process = blook::Process::self();
    auto hook = process->module("USER32.DLL")
            .value()
            ->exports("MessageBoxA")
            ->inline_hook();
    hook->install([=](int64_t a, char *text, char *title, int64_t b) -> int64_t {
        return hook->trampoline_t<int64_t(int64_t, char *, char *, int64_t)>()(
                a, (char *) "oh fuck", text, b);
    });

    MessageBoxA(nullptr, "hi", "hi", 0);

    hook->uninstall();
    MessageBoxA(nullptr, "hi", "hi", 0);
}

void test_section_view() {
    auto process = blook::Process::self();
    auto mod = process->module().value();
    auto rdata = mod->section(".rdata").value();
    auto text = mod->section(".text").value();

    const char *xchar =
            "this_is_some_special_string_that_represents_a_damn_vip_proc";
    auto a_special_variable = xchar;
    std::cout << a_special_variable << std::endl;
    const auto p = rdata.find_one("this_is_some_special_string_").value();
    std::cout << "Found target special string at: " << std::hex << "" << p.data()
              << std::endl;

    auto xref = text.find_xref(p).value();
    std::cout << "Found target usage at: " << std::hex << "" << xref.data()
              << std::endl;

    auto guess_func = xref.guess_function().value().data();
    std::cout << "Target function guessed: " << std::hex << guess_func
              << ", actual: " << &test_section_view << std::endl;
}

struct TestStruct {
    union U {
        TestStruct *s;
        size_t data;
    } slot[100];
};

void test_memory_offsets() {
    TestStruct s{};
    s.slot[0x23].s = new TestStruct();
    s.slot[0x23].s->slot[0x12].s = new TestStruct();
    s.slot[0x23].s->slot[0x12].s->slot[0x4].data = 114514;

    auto ptr = (size_t ***) &s;
    blook::Pointer ptr2 = (void *) &s;

    auto val = *(*(*(ptr + 0x23) + 0x12) + 0x4);
    auto val2 = **ptr2.offsets({0x23, 0x12, 0x4}).value().read<size_t>();

    std::cout << "Normal:" << val << " Blook:" << val2;

    assert(val == val2);
}

void test_disassembly_iterator() {
    using namespace blook;
    MemoryRange range = ((Pointer)(void * )(&test_disassembly_iterator)).range_size(0x512);
    disasm::DisassembleIterator iter(
            range
    );

    int cnt = 0;
    for (const auto &data: iter) {
        if (cnt++ > 5) {
            std::cout << "The fifth instruction mnemonic: " << data->getMnemonic() << std::endl;
            break;
        }
    }

    const char *special_str = "Some other special string...\n";
    std::cout << special_str;

    auto self = Process::self();
    auto mod = self->module().value();
    auto p_special_str = mod->section(".rdata")->find_one(special_str).value();

    auto res = std::ranges::find_if(iter, [=](const auto &c) {
        return std::ranges::contains(c.xrefs(), p_special_str);
    });

    std::cout << "Usage found at: " << (*res).ptr();
}

int main() {
    try {
        test_disassembly_iterator();
    } catch (std::exception &e) {
        std::cerr << e.what();
        abort();
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, // handle to DLL module
                    DWORD fdwReason,    // reason for calling function
                    LPVOID lpvReserved) // reserved
{
    // Perform actions based on the reason for calling.
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            MessageBoxA(nullptr, "hi", "hi", 0);
            blook::misc::initialize_dll_hijacking();

            break;

        case DLL_THREAD_ATTACH:
            // Do thread-specific initialization.
            break;

        case DLL_THREAD_DETACH:
            // Do thread-specific cleanup.
            break;

        case DLL_PROCESS_DETACH:

            if (lpvReserved != nullptr) {
                break; // do not do cleanup if process termination scenario
            }

            // Perform any necessary cleanup.
            break;
    }
    return TRUE; // Successful DLL_PROCESS_ATTACH.
}
