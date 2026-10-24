#pragma once
#include <string>

// HTTPS download that does not depend on Qt having a TLS backend.
//
// The Windows build ships the Qt DLLs alone: Qt needs OpenSSL for HTTPS there,
// and without libssl/libcrypto every request dies with "TLS initialization
// failed". Qt is used when it can actually do TLS (normal on Linux), otherwise
// the platform tool (curl.exe on Windows 10+, PowerShell as a fallback).
namespace webdl {
    bool qtTlsAvailable();

    // Downloads url into destPath. Returns an empty string on success, a
    // readable error otherwise. forceExternal skips the Qt path, used to test
    // the fallback on a machine where Qt TLS works.
    std::string download(const std::string& url, const std::string& destPath,
                         bool forceExternal = false);
}
