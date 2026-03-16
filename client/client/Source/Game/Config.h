#pragma once
#include <Windows.h>
#include <ShlObj.h>
#include <cstdio>
#include "Structs.h"

namespace Config
{
    inline const char* GetConfigPath()
    {
        static char path[MAX_PATH] = {};
        if (path[0] == 0)
        {
            char appdata[MAX_PATH];
            SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata);
            snprintf(path, MAX_PATH, "%s\\palepale", appdata);
            CreateDirectoryA(path, NULL);
            strcat_s(path, "\\config.bin");
        }
        return path;
    }

    inline bool Save(const Settings& settings)
    {
        FILE* f = NULL;
        fopen_s(&f, GetConfigPath(), "wb");
        if (!f) return false;
        int magic = 0x50414C45; // "PALE"
        int version = 1;
        fwrite(&magic, 4, 1, f);
        fwrite(&version, 4, 1, f);
        fwrite(&settings, sizeof(Settings), 1, f);
        fclose(f);
        return true;
    }

    inline bool Load(Settings& settings)
    {
        FILE* f = NULL;
        fopen_s(&f, GetConfigPath(), "rb");
        if (!f) return false;
        int magic = 0, version = 0;
        fread(&magic, 4, 1, f);
        fread(&version, 4, 1, f);
        if (magic != 0x50414C45 || version != 1)
        {
            fclose(f);
            return false;
        }
        fread(&settings, sizeof(Settings), 1, f);
        fclose(f);
        return true;
    }
}
