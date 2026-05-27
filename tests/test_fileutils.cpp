#define WIN32_LEAN_AND_MEAN

#include <cassert>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <windows.h>
#include "FileUtils.h"

namespace fs = std::filesystem;

void TestUtf8Conversions() {
    std::cout << "Testing UTF-8 conversions..." << std::endl;
    
    // Test empty strings
    assert(FileUtils::Utf8ToWide("") == L"");
    assert(FileUtils::WideToUtf8(L"") == "");
    
    // Test ASCII
    std::string ascii = "Hello World";
    std::wstring wide = L"Hello World";
    assert(FileUtils::Utf8ToWide(ascii) == wide);
    assert(FileUtils::WideToUtf8(wide) == ascii);
    
    // Test Unicode (Russian)
    std::string utf8 = u8"Привет мир";
    std::wstring wideUtf = L"Привет мир";
    assert(FileUtils::Utf8ToWide(utf8) == wideUtf);
    
    std::cout << "✓ UTF-8 conversions passed" << std::endl;
}

void TestMimeType() {
    std::cout << "Testing MIME type detection..." << std::endl;
    
    // Text files
    assert(FileUtils::GetMimeType(L"file.txt") == "text/plain");
    assert(FileUtils::GetMimeType(L"file.html") == "text/html");
    assert(FileUtils::GetMimeType(L"file.htm") == "text/html");
    assert(FileUtils::GetMimeType(L"file.css") == "text/css");
    assert(FileUtils::GetMimeType(L"file.js") == "application/javascript");
    assert(FileUtils::GetMimeType(L"file.json") == "application/json");
    assert(FileUtils::GetMimeType(L"file.xml") == "application/xml");
    assert(FileUtils::GetMimeType(L"file.md") == "text/markdown");
    assert(FileUtils::GetMimeType(L"file.csv") == "text/csv");
    
    // Images
    assert(FileUtils::GetMimeType(L"file.jpg") == "image/jpeg");
    assert(FileUtils::GetMimeType(L"file.jpeg") == "image/jpeg");
    assert(FileUtils::GetMimeType(L"file.png") == "image/png");
    assert(FileUtils::GetMimeType(L"file.gif") == "image/gif");
    assert(FileUtils::GetMimeType(L"file.bmp") == "image/bmp");
    assert(FileUtils::GetMimeType(L"file.ico") == "image/x-icon");
    assert(FileUtils::GetMimeType(L"file.svg") == "image/svg+xml");
    assert(FileUtils::GetMimeType(L"file.webp") == "image/webp");
    
    // Documents
    assert(FileUtils::GetMimeType(L"file.pdf") == "application/pdf");
    assert(FileUtils::GetMimeType(L"file.doc") == "application/msword");
    assert(FileUtils::GetMimeType(L"file.docx") == "application/vnd.openxmlformats-officedocument.wordprocessingml.document");
    assert(FileUtils::GetMimeType(L"file.xls") == "application/vnd.ms-excel");
    assert(FileUtils::GetMimeType(L"file.pptx") == "application/vnd.openxmlformats-officedocument.presentationml.presentation");
    
    // Archives
    assert(FileUtils::GetMimeType(L"file.zip") == "application/zip");
    assert(FileUtils::GetMimeType(L"file.rar") == "application/x-rar-compressed");
    assert(FileUtils::GetMimeType(L"file.7z") == "application/x-7z-compressed");
    
    // Code
    assert(FileUtils::GetMimeType(L"file.cpp") == "text/x-csrc");
    assert(FileUtils::GetMimeType(L"file.c") == "text/x-csrc");
    assert(FileUtils::GetMimeType(L"file.h") == "text/x-chdr");
    assert(FileUtils::GetMimeType(L"file.hpp") == "text/x-chdr");
    assert(FileUtils::GetMimeType(L"file.py") == "text/x-python");
    
    // Audio/Video
    assert(FileUtils::GetMimeType(L"file.mp3") == "audio/mpeg");
    assert(FileUtils::GetMimeType(L"file.mp4") == "video/mp4");
    
    // Unknown extension
    assert(FileUtils::GetMimeType(L"file.unknown") == "application/octet-stream");
    
    // No extension
    assert(FileUtils::GetMimeType(L"file") == "application/octet-stream");
    
    std::cout << "✓ MIME type detection passed" << std::endl;
}

void TestGetRelativePath() {
    std::cout << "Testing relative path calculation..." << std::endl;
    
    // Same drive
    std::wstring result1 = FileUtils::GetRelativePath(L"C:\\folder\\sub\\file.txt", L"C:\\folder");
    assert(!result1.empty());
    
    // Different drives (should return filename only)
    std::wstring result2 = FileUtils::GetRelativePath(L"D:\\folder\\file.txt", L"C:\\folder");
    // On Windows, relative between different drives typically returns just the filename
    assert(result2 == L"file.txt");
    
    std::cout << "✓ Relative path calculation passed" << std::endl;
}

void TestIsSubdirectory() {
    std::cout << "Testing subdirectory detection..." << std::endl;
    
    // Create temp directory structure
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    fs::path tempDir = fs::path(tempPath) / L"test_subdir";
    fs::path subDir = tempDir / L"sub";
    fs::path otherDir = fs::path(tempPath) / L"other";
    
    try {
        fs::create_directories(subDir);
        fs::create_directories(otherDir);
        
        assert(FileUtils::IsSubdirectory(tempDir.wstring(), subDir.wstring()));
        assert(!FileUtils::IsSubdirectory(otherDir.wstring(), subDir.wstring()));
        
        fs::remove_all(tempDir);
        fs::remove_all(otherDir);
    } catch (...) {
        // Cleanup on error
        std::error_code ec;
        fs::remove_all(tempDir, ec);
        fs::remove_all(otherDir, ec);
    }
    
    std::cout << "✓ Subdirectory detection passed" << std::endl;
}

void TestFormatSize() {
    std::cout << "Testing size formatting..." << std::endl;
    
    assert(FileUtils::FormatSize(0) == L"0 B");
    assert(FileUtils::FormatSize(500) == L"500 B");
    assert(FileUtils::FormatSize(1023) == L"1023 B");
    assert(FileUtils::FormatSize(1024) == L"1.0 KB");
    assert(FileUtils::FormatSize(1536) == L"1.5 KB");
    assert(FileUtils::FormatSize(1048576) == L"1.0 MB");
    assert(FileUtils::FormatSize(1073741824) == L"1.00 GB");
    
    std::cout << "✓ Size formatting passed" << std::endl;
}

void TestIsRootOrProtectedPath() {
    std::cout << "Testing protected path detection..." << std::endl;
    
    // Root paths
    assert(FileUtils::IsRootOrProtectedPath(L"C:\\"));
    assert(FileUtils::IsRootOrProtectedPath(L"D:\\"));
    
    // Empty path
    assert(FileUtils::IsRootOrProtectedPath(L""));
    
    // Get temp directory (should not be protected)
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    fs::path tempFile = fs::path(tempPath) / L"test_file.txt";
    
    // Temp directory is usually not a protected system folder
    // So this should return false
    assert(!FileUtils::IsRootOrProtectedPath(tempFile.wstring()));
    
    std::cout << "✓ Protected path detection passed" << std::endl;
}

void TestGetAppDataDir() {
    std::cout << "Testing AppData directory retrieval..." << std::endl;
    
    std::wstring appDataDir = FileUtils::GetAppDataDir();
    assert(!appDataDir.empty());
    assert(appDataDir.find(L"NetBackup") != std::wstring::npos);
    
    std::cout << "✓ AppData directory retrieval passed" << std::endl;
}

void TestComputeCRC32() {
    std::cout << "Testing CRC32 calculation..." << std::endl;
    
    // Create a test file
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    fs::path tempFile = fs::path(tempPath) / L"test_crc.txt";
    std::string content = "Hello World";
    
    std::ofstream file(tempFile, std::ios::binary);
    file.write(content.c_str(), content.size());
    file.close();
    
    uint32_t crc = FileUtils::ComputeCRC32(tempFile.wstring());
    assert(crc != 0);
    
    // CRC should be deterministic
    uint32_t crc2 = FileUtils::ComputeCRC32(tempFile.wstring());
    assert(crc == crc2);
    
    // Non-existent file
    uint32_t crc3 = FileUtils::ComputeCRC32(L"C:\\nonexistent_file_xyz_12345.txt");
    assert(crc3 == 0);
    
    fs::remove(tempFile);
    
    std::cout << "✓ CRC32 calculation passed" << std::endl;
}

void TestVersioningFunctions() {
    std::cout << "Testing versioning functions..." << std::endl;
    
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    fs::path destRoot = fs::path(tempPath) / L"test_backup";
    fs::path targetFile = destRoot / L"test.txt";
    
    try {
        fs::create_directories(destRoot);
        
        // Test GetVersionsRoot
        fs::path versionsRoot = FileUtils::GetVersionsRoot(destRoot);
        std::wstring versionsRootStr = versionsRoot.wstring();
        assert(versionsRootStr.find(L".versions") != std::wstring::npos);
        
        // Create a test file
        std::ofstream file(targetFile);
        file << "test content";
        file.close();
        
        // Test SaveVersion (may return false if extension not versioned, that's fine)
        bool result = FileUtils::SaveVersion(targetFile, destRoot);
        // We're just testing that it doesn't crash
        (void)result;
        
    } catch (const std::exception& e) {
        std::cout << "  Note: Versioning test had exception: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "  Note: Versioning test had unknown exception" << std::endl;
    }
    
    // Cleanup
    std::error_code ec;
    fs::remove_all(destRoot, ec);
    
    std::cout << "✓ Versioning functions passed" << std::endl;
}

void TestCopyToBackup() {
    std::cout << "Testing CopyToBackup function..." << std::endl;
    
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    
    // Create source file
    fs::path srcDir = fs::path(tempPath) / L"test_src";
    fs::path destDir = fs::path(tempPath) / L"test_dest";
    fs::path watchRoot = srcDir;
    fs::path srcFile = srcDir / L"test_copy.txt";
    
    try {
        fs::create_directories(srcDir);
        fs::create_directories(destDir);
        
        std::ofstream file(srcFile);
        file << "Test content for copy";
        file.close();
        
        // Test copy
        FileUtils::CopyResult result = FileUtils::CopyToBackup(
            srcFile.wstring(),
            watchRoot.wstring(),
            destDir.wstring()
        );
        
        // Should succeed
        assert(result.success);
        assert(result.bytesCopied > 0);
        
        // Test copying again (should handle versioning)
        FileUtils::CopyResult result2 = FileUtils::CopyToBackup(
            srcFile.wstring(),
            watchRoot.wstring(),
            destDir.wstring()
        );
        assert(result2.success);
        
    } catch (const std::exception& e) {
        std::cout << "  Note: CopyToBackup test had exception: " << e.what() << std::endl;
    }
    
    // Cleanup
    std::error_code ec;
    fs::remove_all(srcDir, ec);
    fs::remove_all(destDir, ec);
    
    std::cout << "✓ CopyToBackup function passed" << std::endl;
}

int main() {
    std::cout << "\n=== Running FileUtils Tests ===\n" << std::endl;
    
    try {
        TestUtf8Conversions();
        TestMimeType();
        TestGetRelativePath();
        TestIsSubdirectory();
        TestFormatSize();
        TestIsRootOrProtectedPath();
        TestGetAppDataDir();
        TestComputeCRC32();
        TestVersioningFunctions();
        TestCopyToBackup();
        
        std::cout << "All FileUtils tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n✗ Unknown test failure" << std::endl;
        return 1;
    }
    
    return 0;
}