# RobloxThreadGhost
This is taken from another Code and Repositry all credits goes to 'atrexus'

RobloxThreadGhost is a clean, modern C++ implementation of a Hyperion thread creation bypass for `RobloxPlayerBeta.exe`.

It creates a **suspended remote thread**, registers it into Hyperion’s internal thread whitelist, and then resumes it. This allows the thread to run without being blocked or terminated by the instrumentation hook inside `LdrInitializeThunk`.

---

## How It Works

1. Opens the `RobloxPlayerBeta.exe` process using Windows API.
2. Allocates memory for a message box and its parameters.
3. Creates a suspended remote thread using `CreateRemoteThreadEx`.
4. Fetches the thread's creation time using `GetThreadTimes`.
5. Injects a fake `ThreadEntry` into Hyperion’s internal thread map.
6. Resumes the thread to safely execute bypassed.

---

## 🧬 ThreadEntry Structure

```cpp
struct ThreadEntry {
    void* prev;            // 0x00
    void* next;            // 0x08
    void* self;            // 0x10
    int   reserved;        // 0x18
    DWORD threadId;        // 0x1C
    uint64_t createdAt;    // 0x20
    void* startAddress;    // 0x28
};
```

This entry is injected at RobloxPlayerBeta.dll + 0x286210, where Hyperion performs validation on new threads during the LdrInitializeThunk stage

Disclaimer
This project is for educational and research purposes only.
Using it in real Roblox environments may violate the Terms of Service and lead to account suspension or other consequences.

I do not condone cheating. The intention of this project is to study modern anti-debugging and thread instrumentation mechanisms like those implemented by Hyperion.

Reference
Based on:

Deep dive on Hyperion's thread filtering and instrumentation callback (LdrInitializeThunk) //https://blog.nestra.tech/reverse-engineering-hyperion-selective-thread-spawning/

Analysis of NtCreateThread hooks, thread map layout, and memory validation flow //https://blog.nestra.tech/reverse-engineering-hyperion-selective-thread-spawning/

Credits
Concept from thread-bypass PoC

insights from [[original blog](https://blog.nestra.tech/reverse-engineering-hyperion-selective-thread-spawning/)]

this is taken from another code and repositry all credits goes to atrexus
