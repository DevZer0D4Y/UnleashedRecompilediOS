#pragma once

struct ModLoader
{
    static inline bool s_isLogTypeConsole;

    static inline std::filesystem::path s_saveFilePath;
    
    static std::filesystem::path ResolvePath(std::string_view path);

    static std::vector<std::filesystem::path>* GetIncludeDirectories(size_t modIndex);

    struct Code
    {
        std::string name;
        bool enabled;
    };

    // Only codes implemented in this executable are available on iOS.
    static std::vector<Code> GetBuiltInCodes();
    static void SaveBuiltInCodes(const std::vector<Code>& codes);

    static void Init();
};
