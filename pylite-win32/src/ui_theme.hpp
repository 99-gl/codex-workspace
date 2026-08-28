#pragma once

namespace pylite_ui {

constexpr COLORREF HexColor(unsigned rgb) {
  return RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
}

struct Theme {
  static constexpr COLORREF AppBackground = HexColor(0xF6F7F9);
  static constexpr COLORREF PanelBackground = HexColor(0xFFFFFF);
  static constexpr COLORREF EditorBackground = HexColor(0xFFFFFF);
  static constexpr COLORREF OutputBackground = HexColor(0xFBFCFE);
  static constexpr COLORREF Border = HexColor(0xE4E7EB);
  static constexpr COLORREF Divider = HexColor(0xE1E4E8);
  static constexpr COLORREF TextPrimary = HexColor(0x24262B);
  static constexpr COLORREF TextSecondary = HexColor(0x747A84);
  static constexpr COLORREF TextMuted = HexColor(0x9AA0A8);
  static constexpr COLORREF HoverBackground = HexColor(0xEEF0F3);
  static constexpr COLORREF SelectedBackground = HexColor(0xE8EDF7);
  static constexpr COLORREF SelectedText = HexColor(0x26344D);
  static constexpr COLORREF PrimaryButton = HexColor(0x292B30);
  static constexpr COLORREF PrimaryButtonHover = HexColor(0x3A3D43);
  static constexpr COLORREF PrimaryButtonText = HexColor(0xFFFFFF);
  static constexpr COLORREF Success = HexColor(0x24945E);
  static constexpr COLORREF Error = HexColor(0xC43C3C);
  static constexpr COLORREF FocusAccent = HexColor(0x5576C7);
  static constexpr COLORREF CurrentLine = HexColor(0xF7F9FC);
};

inline UINT WindowDpi(HWND window) {
  using GetDpiForWindowFn = UINT(WINAPI *)(HWND);
  static auto getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
      GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
  if (getDpiForWindow && window) {
    const UINT dpi = getDpiForWindow(window);
    if (dpi) return dpi;
  }
  HDC dc = GetDC(window);
  const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
  if (dc) ReleaseDC(window, dc);
  return dpi > 0 ? static_cast<UINT>(dpi) : 96U;
}

inline int Scale(int value, UINT dpi) {
  return MulDiv(value, static_cast<int>(dpi), 96);
}

inline HFONT CreateUiFont(UINT dpi, int points = 10, int weight = FW_NORMAL) {
  return CreateFontW(-MulDiv(points, static_cast<int>(dpi), 72), 0, 0, 0,
                     weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                     L"Segoe UI");
}

inline int CALLBACK FindFontProc(const LOGFONTW *, const TEXTMETRICW *, DWORD,
                                 LPARAM found) {
  *reinterpret_cast<bool *>(found) = true;
  return 0;
}

inline bool FontExists(const wchar_t *face) {
  HDC dc = GetDC(nullptr);
  if (!dc) return false;
  LOGFONTW font{};
  font.lfCharSet = DEFAULT_CHARSET;
  wcsncpy_s(font.lfFaceName, face, _TRUNCATE);
  bool found = false;
  EnumFontFamiliesExW(dc, &font,
                      reinterpret_cast<FONTENUMPROCW>(FindFontProc),
                      reinterpret_cast<LPARAM>(&found), 0);
  ReleaseDC(nullptr, dc);
  return found;
}

inline HFONT CreateCodeFont(UINT dpi, int points = 11) {
  const wchar_t *face = FontExists(L"Cascadia Mono") ? L"Cascadia Mono"
                                                       : L"Consolas";
  return CreateFontW(-MulDiv(points, static_cast<int>(dpi), 72), 0, 0, 0,
                     FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
                     face);
}

inline HFONT CreateIconFont(UINT dpi, int points = 11) {
  return CreateFontW(-MulDiv(points, static_cast<int>(dpi), 72), 0, 0, 0,
                     FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                     L"Segoe MDL2 Assets");
}

inline void Fill(HDC dc, const RECT &rect, COLORREF color) {
  HBRUSH brush = CreateSolidBrush(color);
  FillRect(dc, &rect, brush);
  DeleteObject(brush);
}

inline void FillRounded(HDC dc, const RECT &rect, COLORREF color, int radius) {
  HBRUSH brush = CreateSolidBrush(color);
  HPEN pen = CreatePen(PS_NULL, 0, color);
  auto oldBrush = SelectObject(dc, brush);
  auto oldPen = SelectObject(dc, pen);
  RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
  SelectObject(dc, oldBrush);
  SelectObject(dc, oldPen);
  DeleteObject(brush);
  DeleteObject(pen);
}

inline void StrokeRounded(HDC dc, const RECT &rect, COLORREF color, int radius) {
  HPEN pen = CreatePen(PS_SOLID, 1, color);
  auto oldPen = SelectObject(dc, pen);
  auto oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
  RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
  SelectObject(dc, oldBrush);
  SelectObject(dc, oldPen);
  DeleteObject(pen);
}

inline void Text(HDC dc, const std::wstring &value, RECT rect, HFONT font,
                 COLORREF color, UINT format) {
  auto oldFont = SelectObject(dc, font);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, color);
  DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &rect, format);
  SelectObject(dc, oldFont);
}

}  // namespace pylite_ui
