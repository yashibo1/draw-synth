#pragma once

namespace drawsynth
{
    // Bumped by hand with every meaningful change, shown in the UI (see
    // TransportBar). Purely diagnostic: lets a screenshot or a glance at the
    // running plugin confirm which build is actually loaded, since a host
    // can hold an old copy of the DLL in memory even after the file on disk
    // has been replaced.
    constexpr const char* kBuildTag = "build-2026-09-22-1";
}
