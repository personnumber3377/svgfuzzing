#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <cstdio>
#include <fstream>

#include <vector>
#include <string>
#include <iostream>


void debug_print(const char* string) {
    /*
    FILE* f = fopen("C:\\Users\\elsku\\hook_hit.txt", "a+");
    if (f) {
        fprintf(f, string);
        fclose(f);
    }
    */

}




BYTE originalBytes[5]; // Original instructions from the function...
uintptr_t target; // Address of the target function...

typedef void (*MyHookedFuncType)(void); // Your replacement function

// __declspec(naked)


typedef HRESULT (__stdcall *ISVGImageFactoryCreate1Proxy_t)(void**, char);

// typedef HRESULT (__thiscall *CreateSVGImage_t)(void*, void**);
typedef void* (__thiscall *CreateSVGImage_t)(void*, void*);
typedef void (__thiscall *DestroyFunc)(void*);
typedef void* (__thiscall *AcquireEffectTree_t)(void* this_ptr, void** out_effect, void* transform_matrix, void* maybe_null);

HMODULE hDll;
ISVGImageFactoryCreate1Proxy_t ISVGImageFactoryCreate1Proxy;

// namespace std;

int fuzz_init() {



    HMODULE hDll = LoadLibraryW(L"MSOSVG.dll");
    if (!hDll) {
        // std::cerr << "Failed to load MSOSVG.DLL" << std::endl;
        debug_print("Failed to load MSOSVG.DD!!!!\n");


        return 1;
    }

    // printf("Holy fuck!!!\n");
    ISVGImageFactoryCreate1Proxy = 
        (ISVGImageFactoryCreate1Proxy_t) GetProcAddress(hDll, "ISVGImageFactoryCreate1Proxy");

    if (!ISVGImageFactoryCreate1Proxy) {
        // std::cerr << "Failed to get function address for ISVGImageFactoryCreate1Proxy\n";
        debug_print("Failed to get function address for ISVGImageFactoryCreate1Proxy\n");
        FreeLibrary(hDll);
        return 1;
    }

    return 0;


}




// Function to read a file into a buffer
bool ReadFileToBuffer(const std::string& filename, std::vector<uint8_t>& buffer) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        // std::cerr << "Error: Could not open file: " << filename << "\n";
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize < 1) { // Ensure there is data
        // std::cerr << "Error: File must not be empty!\n";
        return false;
    }

    debug_print("Succesfully read file to buffer...\n");

    buffer.resize(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    return true;
}


#include <objidl.h>  // IStream
#include <ole2.h>    // CreateStreamOnHGlobal

IStream* CreateMemoryStream(const std::vector<uint8_t>& data) {
    IStream* stream = nullptr;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, data.size());

    if (hMem) {
        void* pMem = GlobalLock(hMem);
        if (pMem) {
            memcpy(pMem, data.data(), data.size());
            GlobalUnlock(hMem);
            HRESULT hr = CreateStreamOnHGlobal(hMem, TRUE, &stream);
            if (FAILED(hr)) {
                // std::cerr << "Failed to create memory stream!" << std::endl;
                return nullptr;
            }
        }
    }
    return stream;
}

// Function to call CreateSVGImage
int fuzz_function(const std::vector<uint8_t>& svgData) {


    void* factory = nullptr;
    void* svgImage = nullptr;
    HRESULT res;
    //printf("Called fuzz function...\n");
    // Call the factory function to get an instance of ISVGImageFactory // NOTE: This actually returns void, therefore do not check res
    // HRESULT res = ISVGImageFactoryCreate1Proxy(&factory);
    debug_print("Calling ISVGImageFactoryCreate1Proxy\n");
    ISVGImageFactoryCreate1Proxy(&factory, 0); // Need to pass in flags maybe???
    //printf("Here is the factory pointer: %p\n", factory);
    //if (FAILED(res) || !factory) {
    if (!factory) {
        // std::cerr << "Error: Failed to get SVGImageFactory!\n";
        debug_print("Error: Failed to get SVGImageFactory!\n");
        return 1;
    }

    // Get the function pointer for CreateSVGImage from the vtable
    void** vtable_ptr = *(void***)factory;
    CreateSVGImage_t create_svg_func = (CreateSVGImage_t)vtable_ptr[3]; // Usually function at index 5

    if (!create_svg_func) {
        // std::cerr << "Error: Failed to retrieve CreateSVGImage function pointer!\n";
        return 1;
    }
    //printf("Address (this + 0x10): %p\n", (void*)((uintptr_t)factory + 0x10));
    void* stream_ptr = *(void**)((uintptr_t)factory + 0x10);
    //printf("Stream Pointer: %p\n", stream_ptr);


    void** stream_location = (void**)((uintptr_t)factory + 0x10);
    debug_print("Calling CreateMemoryStream\n");
    IStream* svgStream = CreateMemoryStream(svgData);  // svgData is your SVG file contents

    *stream_location = svgStream;  // Assign our new memory stream to the factory
    //printf("New Stream Pointer: %p\n", *stream_location);

    *(IStream**)((char*)factory + 0x10) = svgStream;

    debug_print("Calling create_svg_func\n");

    create_svg_func(factory, &svgImage);
    debug_print("Returned from create_svg_func\n");

    if (!svgImage) {
        debug_print("CreateSVGImage failed!\n");
        //// std::cerr << "CreateSVGImage failed!\n";
        return 1;
    }

    debug_print("create_svg_func succeeded!!!\n");

    void** svg_vtable = *(void***)svgImage;
    AcquireEffectTree_t acquire_func = (AcquireEffectTree_t)svg_vtable[3]; // adjust index if necessary // The get the stuff is at index 3 in the vtable...

    if (!acquire_func) {
        debug_print("Error: Could not get AcquireEffectTree from vtable!\n\n");
        // std::cerr << "Error: Could not get AcquireEffectTree from vtable!\n";
        return 1;
    }

    // Setup dummy args
    void* out_effect_ptr = nullptr;

    double identity_matrix[6] = {
        1.0, 0.0,  // first row
        0.0, 1.0,  // second row
        0.0, 0.0   // translation
    };

    // Call the function

    debug_print("Calling acquire_func\n");

    void* result = acquire_func(svgImage, &out_effect_ptr, identity_matrix, nullptr);

    // std::cout << "AcquireEffectTree returned: " << result << "\n";
    // std::cout << "Effect result: " << out_effect_ptr << "\n";

    debug_print("AcquireEffectTree returned!!!\n");

    // Cleanup: Destroy the created SVGImage if possible
    DestroyFunc destroy_func = (DestroyFunc)vtable_ptr[0]; // Assuming first function in vtable is destroy
    if (destroy_func) {
        destroy_func(svgImage);
    }

    return 0;
}

// Main fuzzing function
__declspec(noinline) void __fastcall actual_stuff(char* filename) {
    std::vector<uint8_t> buffer;
    //printf("Reading to file...\n");
    if (!ReadFileToBuffer(filename, buffer)) {
        return;
    }
    //printf("Now calling fuzz...\n");
    fuzz_function(buffer);
    //printf("After\n");
}

// Loop for continuous fuzzing
__declspec(noinline) void __fastcall loop(char* filename) {
    //printf("poopoooooooo\n");
    actual_stuff(filename);
    //printf("After the fact...\n");
}





















// This is your custom function that will be jumped to
void MyHookedFunction() {
    // You can write your own assembly or do something here
    /*
    while(1) {
        MessageBoxW(NULL, L"msosvg!Mso::SVG::SVGImage::AcquireEffectTree Hooked!", L"MSOSVG Hook", MB_OK);
    }
    */

    
    FILE* f = fopen("C:\\Users\\elsku\\hook_hit.txt", "a+");
    if (f) {
        fprintf(f, "Hook triggered!\n");
        fclose(f);
    }


    // Now do the shit....


    DWORD oldProtect;
    debug_print("Patching the old shit back...\n");
    // Copy the original instructions back....
    VirtualProtect((LPVOID)target, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)target, originalBytes, sizeof(originalBytes));
    VirtualProtect((LPVOID)target, 5, oldProtect, &oldProtect);

    /*
    while (1) {
        MessageBoxW(NULL, L"Create1 Proxy Hooked!", L"MSOSVG Hook", MB_OK);
    }
    */
    //MessageBoxW(NULL, L"msosvg!Mso::SVG::SVGImage::AcquireEffectTree Hooked!", L"MSOSVG Hook", MB_OK);
    

    // fuzz_init();

    if (fuzz_init()) {
        debug_print("Initialization failed!!!!\n");
        return;
    }

    // Now jump to our actual fuzzing function maybe....

    while (1) {
        debug_print("Calling loop...\n");
        loop("C:\\Users\\elsku\\final\\input.svg");
    }
    

    // You might crash unless you carefully return or trampoline back
    /*
    __asm {
        ret
    };
    */
}

uintptr_t GetModuleBase(const wchar_t* moduleName) {
    uintptr_t base = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me;
        me.dwSize = sizeof(me);
        if (Module32FirstW(snapshot, &me)) {
            do {
                if (_wcsicmp(me.szModule, moduleName) == 0) {
                    base = (uintptr_t)me.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snapshot, &me));
        }
        CloseHandle(snapshot);
    }
    return base;
}

void LogBytes(void* address, size_t length) {
    BYTE* bytes = reinterpret_cast<BYTE*>(address);
    char buffer[256] = {0};

    FILE* f = fopen("C:\\Users\\elsku\\hook_hit.txt", "a+");
    if (!f) return;

    fprintf(f, "Bytes at %p: ", address);
    for (size_t i = 0; i < length; ++i) {
        fprintf(f, "%02X ", bytes[i]);
    }
    fprintf(f, "\n");

    fclose(f);
}

void PatchFunction() {
    // Wait for MSOSVG.dll to load
    HMODULE hMod;
    debug_print("Waiting for MSOSVG...\n\n");
    while ((hMod = GetModuleHandleW(L"MSOSVG.dll")) == NULL) {
        debug_print("Waiting for MSOSVG...\n\n");
        Sleep(100);
    }
    debug_print("Success!!!\n\n");
    uintptr_t base = (uintptr_t)hMod;
    // uintptr_t target = base + 0x2a30; // RVA of ISVGImageFactoryCreate1Proxy


    target = base + 0x50f0; // 0x2a30; // RVA of msosvg!Mso::SVG::SVGImage::AcquireEffectTree

    // Oof
    



    memcpy(originalBytes, (void*)target, 5);

    // Wait until dll is initialized...
    BYTE expected[3] = { 0x48, 0x8B, 0xC4 }; // The real starting bytes of AcquireEffectTree
    BYTE buffer[3] = { 0 };

    debug_print("Waiting for MSOSVG a bit more...\n\n");

    bool valid = false;
    while (!valid) {
        memcpy(buffer, (void*)target, 3);
        debug_print("Comparing...\n");
        if (memcmp(buffer, expected, 3) == 0) {
            valid = true;
        } else {
            LogBytes((void*)target, 3); // Log those three bytes...
            Sleep(100); // Wait a bit more...
        }
    }

    
    debug_print("Done!\n\n");

    Sleep(1000);
    // Write a 5-byte JMP to our function
    DWORD oldProtect;
    if (VirtualProtect((LPVOID)target, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        uintptr_t relAddr = (uintptr_t)MyHookedFunction - (target + 5); // Relative offset

        BYTE patch[5] = {
            0xE9,                        // JMP rel32
            (BYTE)(relAddr & 0xFF),
            (BYTE)((relAddr >> 8) & 0xFF),
            (BYTE)((relAddr >> 16) & 0xFF),
            (BYTE)((relAddr >> 24) & 0xFF)
        };
        debug_print("Here are the patch bytes:\n");
        LogBytes((void*)patch, 5); // Log those three bytes...
        memcpy((void*)target, patch, sizeof(patch));
        VirtualProtect((LPVOID)target, 5, oldProtect, &oldProtect);
        debug_print("Now patch was applied...\n");
        // MessageBoxW(NULL, L"Patch applied!", L"MSOSVG", MB_OK);
    } else {
        debug_print("Patch failed to apply...\n");
        // MessageBoxW(NULL, L"Failed to change protection!", L"MSOSVG", MB_OK);
    }
    debug_print("End of the patch function......\n");
    // Debug output to file
    std::ofstream log("C:\\Users\\elsku\\msosvg_patch.log", std::ios::app);
    log << "MSOSVG Base: 0x" << std::hex << base << std::endl;
    log << "Target address (to patch): 0x" << std::hex << target << std::endl;
    // log << "Hook function address: 0x" << std::hex << hookFunc << std::endl;
    // log << "Calculated relative JMP offset: 0x" << std::hex << relAddr << std::endl;
    log.close();

}

DWORD WINAPI PayloadThread(LPVOID) {
    debug_print("Payload thread...\n");
    PatchFunction();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    debug_print("Called DllMain...\n");
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, PayloadThread, NULL, 0, NULL);
    }
    return TRUE;
}
