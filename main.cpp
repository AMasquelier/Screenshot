#include <Windows.h>
#include <iostream>
#include <fstream>
#include <gdiplus.h>
#include <sys/types.h>
#include <string>
#include "tools.h"
#pragma comment (lib,"Gdiplus.lib")

using namespace std;
using namespace Gdiplus;


// Settings
wstring dir_path = L"";
wstring file_name = L"screenshot_";
bool save_file = true;
int file_count = 0;

// ** Found on https://stackoverflow.com/questions/37132196/multi-monitor-screenshots-only-2-monitors-in-c-with-winapi** //
typedef struct Data
{
    int current;
    MONITORINFOEXW* info;
} Data;

BOOL EnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    Data* data = (Data*)dwData;
    data->info[data->current].cbSize = sizeof(MONITORINFOEXW);
    return GetMonitorInfoW(hMonitor, &(data->info[data->current++]));
}

BOOL GetAllMonitorInfo(Data* data)
{
    return EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)(&EnumProc), (LPARAM)(data));
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) // Found on https://stackoverflow.com/questions/13554905/hbitmap-to-jpeg-png-without-cimage-in-c
{
    UINT  num = 0;          // number of image encoders
    UINT  size = 0;         // size of the image encoder array in bytes

    ImageCodecInfo* pImageCodecInfo = NULL;

    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1;  // Failure

    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL)
        return -1;  // Failure

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // Success
        }
    }

    free(pImageCodecInfo);
    return -1;  // Failure
}
// **         ** //

bool file_exist(wstring path)
{
    ifstream f(path.c_str());
    if (f.is_open())
    {
        f.close();
        return true;
    }
    return false;
}

void screenshot(Point a, Point b)
{
    // copy screen to bitmap
    HDC     hScreen = GetDC(NULL);
    HDC     hDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, abs(b.X - a.X), abs(b.Y - a.Y));
    HGDIOBJ old_obj = SelectObject(hDC, hBitmap);
    BOOL    bRet = BitBlt(hDC, 0, 0, abs(b.X - a.X), abs(b.Y - a.Y), hScreen, a.X, a.Y, SRCCOPY);

    // save bitmap to clipboard
    OpenClipboard(NULL);
    EmptyClipboard();
    SetClipboardData(CF_BITMAP, hBitmap);
    CloseClipboard();

    // save bitmap in jpg
    Gdiplus::Bitmap bmp(hBitmap, (HPALETTE)0);
    CLSID pngClsid;
    GetEncoderClsid(L"image/png", &pngClsid);

    wstring path = dir_path + file_name + to_wstring(file_count) + L".png";
    while (file_exist(path))
    {
        file_count++;
        path = dir_path + file_name + to_wstring(file_count) + L".png";
    }
    bmp.Save(path.c_str(), &pngClsid, NULL);

    // clean up
    SelectObject(hDC, old_obj);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);
    DeleteObject(hBitmap);
}

void init()
{
    wifstream f("settings.txt");
    if (f.is_open())
    {
        wstring line; 
        while (getline(f, line))
        {
            if (line.find(L"SAVE=") != string::npos && line.find(L"false") != string::npos) save_file = false;
            if (line.find(L"PATH=") != string::npos && line.length() > 6)
            {
                dir_path = line.substr(5, line.length()-5);
                if (dir_path[dir_path.length() - 1] != L'\\' && dir_path[dir_path.length() - 1] != L'/') dir_path += L"/";
            }
            if (line.find(L"FILENAME=") != string::npos) file_name = line.substr(9, line.length()- 9);
        }
        f.close();
    }
}

int main(int argc, char** argv)
{
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    init();
    
    bool close = false;
    Clock framerate_timer; framerate_timer.start();
    float main_framerate = 20.0f;

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    while (!close)
    {
        if (framerate_timer.duration() >= 1000000.0 / main_framerate)
        {
            double act_fps = ceilf(1000000.0f / framerate_timer.duration());
            framerate_timer.start();

            // Update monitors information
            int cMonitors = GetSystemMetrics(SM_CMONITORS);
            Data data;
            data.current = 0;
            data.info = (MONITORINFOEXW*)calloc(cMonitors, sizeof(MONITORINFOEXW));
            Point top_left = Point(0, 0), bot_right = Point(0, 0);

            if (!GetAllMonitorInfo(&data)) return 1;

            for (int i = 0; i < cMonitors; i++)
            {
                top_left = Point(min(top_left.X, (int)data.info[i].rcMonitor.left), min(top_left.Y, (int)data.info[i].rcMonitor.top));
                bot_right = Point(max(bot_right.X, (int)data.info[i].rcMonitor.right), max(bot_right.Y, (int)data.info[i].rcMonitor.bottom));
            }

            free(data.info);

            // Quit
            if (GetKeyState(VK_SNAPSHOT) & 0x800 && GetKeyState(VK_CONTROL) & 0x800) close = true;
            
            // Screenshot
            if (GetKeyState(VK_SNAPSHOT) & 0x800)
            {
                Clock timer; timer.start();
                POINT p1, p2; GetCursorPos(&p1);
                while (GetKeyState(VK_SNAPSHOT) & 0x800)
                {
                    Clock::sleep(100);
                    GetCursorPos(&p2);
                }

                // Full screen
                if (timer.duration() * 0.001 < 300)
                {
                    screenshot(top_left, bot_right);
                }
                // Cropped
                else 
                {
                    Point p = Point(min(p1.x, p2.x), min(p1.y, p2.y)), q = Point(max(p1.x, p2.x), max(p1.y, p2.y));

                    screenshot(p, q);
                }
            }
        }
        else Clock::sleep(1000.0 / main_framerate - framerate_timer.duration() * 0.001);
    }
}