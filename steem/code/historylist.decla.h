#if defined(STEVEN_SEAGAL) && defined(SSE_STRUCTURE_HISTORYLIST_H)
#pragma once
#endif

//---------------------------------------------------------------------------
class THistoryList : public TStemDialog
{
private:
  static LRESULT __stdcall WndProc(HWND,UINT,WPARAM,LPARAM);
  void ManageWindowClasses(bool);
public:
  THistoryList();
  ~THistoryList();

  void Show(),Hide();

  void RefreshHistoryBox();

  int Width,Height;
  bool Maximized;
};

