#include "text_overlay.h"
#include "config.h"
#include "log.h"
#include "vr.h"

#include <windows.h>
#include <cstdint>
#include <vector>

static const char kOverlayKey[] = "framekeyboardsummoner.typedtext";
static const char kOverlayName[] = "Frame Keyboard Summoner";
static const int kWidth = 1024, kHeight = 192; // texture size in pixels

static vr::VROverlayHandle_t g_handle = vr::k_ulOverlayHandleInvalid;
static bool g_placed = false;

static std::wstring Widen(const std::string& s)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

// Draw the box with GDI into a 32-bit DIB, then convert BGRx to the RGBA that SetOverlayRaw wants.
static void Render(const std::string& text, std::vector<uint8_t>& rgba)
{
    const int W = kWidth, H = kHeight;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H; // top-down rows
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dc || !bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        if (dc) DeleteDC(dc);
        rgba.clear();
        return;
    }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    RECT all = { 0, 0, W, H };
    HBRUSH bg = CreateSolidBrush(RGB(20, 20, 24));
    FillRect(dc, &all, bg);
    DeleteObject(bg);
    // 6px mid-gray border, inset 8px so the outer pixels can fade out (alpha ramp below).
    // A thin bright border aliased badly in the headset.
    HBRUSH edge = CreateSolidBrush(RGB(95, 95, 108));
    for (int i = 0; i < 6; ++i) {
        RECT r = { 8 + i, 8 + i, W - 8 - i, H - 8 - i };
        FrameRect(dc, &r, edge);
    }
    DeleteObject(edge);

    SetBkMode(dc, TRANSPARENT);
    HFONT fontCap = CreateFontW(40, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                CLEARTYPE_QUALITY, 0, L"Segoe UI Symbol");
    HFONT fontBody = CreateFontW(92, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                 CLEARTYPE_QUALITY, 0, L"Segoe UI");

    RECT cap = { 20, 10, W - 20, 60 };
    SetTextColor(dc, RGB(150, 150, 165));
    HGDIOBJ oldFont = SelectObject(dc, fontCap);
    DrawTextW(dc, L"Type input (checkmark to confirm)", -1, &cap, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

    RECT body = { 20, 60, W - 20, H - 10 };
    SetTextColor(dc, RGB(255, 255, 255));
    SelectObject(dc, fontBody);
    std::wstring line = Widen(text) + L"_"; // "_" acts as the cursor
    DrawTextW(dc, line.c_str(), (int)line.size(), &body,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    GdiFlush();

    rgba.resize((size_t)W * H * 4);
    const uint8_t* src = static_cast<const uint8_t*>(bits); // B G R x
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)y * W + x;
            // Fade the outer 8px to transparent so the texture edge doesn't alias in the headset.
            int dx = x < W - 1 - x ? x : W - 1 - x;
            int dy = y < H - 1 - y ? y : H - 1 - y;
            int d = dx < dy ? dx : dy;
            int a = d >= 8 ? 235 : 235 * d / 8;
            rgba[i * 4 + 0] = src[i * 4 + 2];
            rgba[i * 4 + 1] = src[i * 4 + 1];
            rgba[i * 4 + 2] = src[i * 4 + 0];
            rgba[i * 4 + 3] = (uint8_t)a;
        }
    }

    SelectObject(dc, oldFont);
    DeleteObject(fontCap);
    DeleteObject(fontBody);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

static bool EnsureOverlay(vr::IVROverlay* o)
{
    if (g_handle != vr::k_ulOverlayHandleInvalid) return true;
    vr::EVROverlayError e = o->CreateOverlay(kOverlayKey, kOverlayName, &g_handle);
    if (e == vr::VROverlayError_KeyInUse) e = o->FindOverlay(kOverlayKey, &g_handle);
    if (e != vr::VROverlayError_None) {
        Log("text box: could not create overlay (error %d)", (int)e);
        g_handle = vr::k_ulOverlayHandleInvalid;
        return false;
    }
    o->SetOverlayWidthInMeters(g_handle, g_cfg.overlay.width);
    o->SetOverlaySortOrder(g_handle, 1000); // draw above other overlays
    return true;
}

// Put the box in front of the gaze, offsets in head space. Either glued to the head, or fixed in
// the world at the head's current pose, which matches where SteamVR opens the keyboard.
static void Place(vr::IVROverlay* o)
{
    const auto& c = g_cfg.overlay;
    vr::HmdMatrix34_t local = {{ { 1, 0, 0, c.offsetX }, { 0, 1, 0, c.offsetY }, { 0, 0, 1, -c.distance } }};
    if (c.followHead) {
        o->SetOverlayTransformTrackedDeviceRelative(g_handle, vr::k_unTrackedDeviceIndex_Hmd, &local);
        return;
    }
    auto* sys = static_cast<vr::IVRSystem*>(VRInterface(vr::IVRSystem_Version));
    if (!sys) { Log("text box: IVRSystem unavailable, cannot read head pose"); return; }
    vr::TrackedDevicePose_t pose[1] = {};
    sys->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, pose, 1);
    if (!pose[0].bPoseIsValid) { Log("text box: head pose invalid"); return; }
    // world = head * local (rotation from the head, position offset along the head's axes)
    const vr::HmdMatrix34_t& h = pose[0].mDeviceToAbsoluteTracking;
    vr::HmdMatrix34_t w;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) w.m[i][j] = h.m[i][j];
        w.m[i][3] = h.m[i][3] + h.m[i][0] * local.m[0][3] + h.m[i][1] * local.m[1][3] + h.m[i][2] * local.m[2][3];
    }
    o->SetOverlayTransformAbsolute(g_handle, vr::TrackingUniverseStanding, &w);
}

void TextOverlayUpdate(vr::IVROverlay* o, const std::string& text)
{
    if (!g_cfg.overlay.enabled || !o || !EnsureOverlay(o)) return;
    if (!g_placed) { Place(o); g_placed = true; }
    std::vector<uint8_t> px;
    Render(text, px);
    if (px.empty()) return;
    vr::EVROverlayError e = o->SetOverlayRaw(g_handle, px.data(), kWidth, kHeight, 4);
    if (e != vr::VROverlayError_None) LogDebug("text box: SetOverlayRaw error %d", (int)e);
    o->ShowOverlay(g_handle);
}

void TextOverlayOpen(vr::IVROverlay* o)
{
    g_placed = false; // re-place at the current gaze every time the keyboard opens
    TextOverlayUpdate(o, "");
}

void TextOverlayHide(vr::IVROverlay* o)
{
    g_placed = false;
    if (g_handle != vr::k_ulOverlayHandleInvalid && o) o->HideOverlay(g_handle);
}

void TextOverlayReset()
{
    g_handle = vr::k_ulOverlayHandleInvalid;
    g_placed = false;
}
